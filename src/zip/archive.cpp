/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include "zip/archive.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <stdexcept>

// miniz otherwise defines zlib's names (inflate, crc32, ...) as macros.
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace docsmith::zip
{
namespace
{

[[noreturn]] void fail(mz_zip_archive &zip, const std::string &what)
{ throw std::runtime_error(what + ": " + mz_zip_get_error_string(mz_zip_get_last_error(&zip))); }

} // namespace

std::vector<entry> read(std::string_view bytes)
{
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        fail(zip, "Could not open archive");

    struct closer
    {
        mz_zip_archive &zip;
        ~closer() { mz_zip_reader_end(&zip); }
    } close{zip};

    std::vector<entry> entries;
    const mz_uint n = mz_zip_reader_get_num_files(&zip);
    entries.reserve(n);
    for(mz_uint i = 0; i < n; ++i)
    {
        mz_zip_archive_file_stat st;
        if(!mz_zip_reader_file_stat(&zip, i, &st))
            fail(zip, "Could not read archive entry " + std::to_string(i));

        entry e{st.m_filename, {}, st.m_method == 0 ? method::store : method::deflate};
        if(!st.m_is_directory)
        {
            std::size_t size = 0;
            void *p = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if(!p)
                fail(zip, "Could not extract archive entry " + e.name);
            e.data.assign(static_cast<const char *>(p), size);
            mz_free(p);
        }
        entries.push_back(std::move(e));
    }
    return entries;
}

struct writer::impl
{ mz_zip_archive zip{}; };

writer::writer() :
    m_impl(std::make_unique<impl>())
{
    if(!mz_zip_writer_init_heap(&m_impl->zip, 0, 0))
        fail(m_impl->zip, "Could not create archive");
}

writer::~writer() { mz_zip_writer_end(&m_impl->zip); }

void writer::add(const std::string &name, std::string_view data, method m)
{
    if(name.ends_with('/'))
    {
        if(!mz_zip_writer_add_mem(&m_impl->zip, name.c_str(), nullptr, 0, 0))
            fail(m_impl->zip, "Could not add archive entry " + name);
        return;
    }

    // Through the callback rather than mz_zip_writer_add_mem, miniz can go back
    // and put the CRC and sizes in the local header, so no entry needs a data
    // descriptor. ODF wants that for the mimetype at least.
    const auto read = [](void *opaque, mz_uint64 offset, void *buf, std::size_t n) -> std::size_t
    {
        const auto &d = *static_cast<const std::string_view *>(opaque);
        if(offset >= d.size())
            return 0;
        const std::size_t count = std::min<std::size_t>(n, d.size() - offset);
        std::memcpy(buf, d.data() + offset, count);
        return count;
    };
    const MZ_TIME_T now = std::time(nullptr);
    const mz_uint level = m == method::store ? MZ_NO_COMPRESSION : MZ_DEFAULT_LEVEL;
    if(!mz_zip_writer_add_read_buf_callback(&m_impl->zip,
           name.c_str(),
           read,
           &data,
           data.size(),
           &now,
           nullptr,
           0,
           level | MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE,
           nullptr,
           0,
           nullptr,
           0))
        fail(m_impl->zip, "Could not add archive entry " + name);
}

std::string writer::finish()
{
    void *p = nullptr;
    std::size_t size = 0;
    if(!mz_zip_writer_finalize_heap_archive(&m_impl->zip, &p, &size))
        fail(m_impl->zip, "Could not finish archive");
    // The buffer is now ours, allocated through the archive's allocator.
    std::string bytes(static_cast<const char *>(p), size);
    m_impl->zip.m_pFree(m_impl->zip.m_pAlloc_opaque, p);
    return bytes;
}

} // namespace docsmith::zip
