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
#include "docsmithcpp/text_doc.h"

#include <fstream>
#include <set>
#include <stdexcept>

namespace docsmith
{
namespace fs = std::filesystem;

image text_doc::add_image(std::string name, std::string data, std::string media_type)
{
    image img{name};
    m_resources.add(resource(std::move(name), std::move(data), std::move(media_type)));
    return img;
}

void text_doc::write_resources(const fs::path &dir) const
{
    std::set<std::string> written;
    for(const frame *f : collect<frame>(*this))
    {
        const resource *r = m_resources.find(f->img.uri);
        if(!r || !written.insert(r->name()).second)
            continue;

        // Names come from loaded files too: keep them inside dir.
        const fs::path rel = fs::path(r->name()).lexically_normal();
        if(rel.empty() || rel.is_absolute() || rel.has_root_path() || *rel.begin() == "..")
            throw std::runtime_error("Resource name leaves the target folder: " + r->name());

        const fs::path p = dir / rel;
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        if(!out.write(r->data().data(), static_cast<std::streamsize>(r->data().size())))
            throw std::runtime_error("Could not write " + p.string());
    }
}

} // namespace docsmith
