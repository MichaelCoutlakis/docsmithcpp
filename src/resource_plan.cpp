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
#include "resource_plan.h"

#include <set>
#include <system_error>

namespace docsmith::detail
{
namespace fs = std::filesystem;

resource_plan::resource_plan(
    const text_doc &doc, const fs::path &resource_dir, const fs::path *base)
{
    const fs::path dir = (base ? *base / resource_dir : resource_dir).lexically_normal();
    auto relative = [&](const fs::path &p)
    { return (base ? p.lexically_relative(*base) : p).generic_string(); };

    std::set<std::string> taken; // names used in dir
    for(const frame *f : collect<frame>(doc))
        if(doc.resources().find(f->img.uri))
            taken.insert(f->img.uri);

    for(const frame *f : collect<frame>(doc))
    {
        const std::string &uri = f->img.uri;
        if(src.contains(uri))
            continue;
        std::error_code ec;
        if(doc.resources().find(uri))
        {
            src[uri] = relative(dir / uri);
        }
        else if(fs::is_regular_file(uri, ec))
        {
            const fs::path abs = fs::weakly_canonical(uri, ec);
            const fs::path in_dir = abs.lexically_relative(fs::weakly_canonical(dir, ec));
            if(base && !in_dir.empty() && *in_dir.begin() != "..")
            {
                src[uri] = relative(abs);
                continue;
            }
            const fs::path name = fs::path(uri).filename();
            std::string rel = ("Pictures" / name).generic_string();
            for(int i = 2; taken.contains(rel); ++i)
                rel = (fs::path("Pictures") /
                    (name.stem().string() + "_" + std::to_string(i) + name.extension().string()))
                          .generic_string();
            taken.insert(rel);
            src[uri] = relative(dir / rel);
            if(base)
                copy.emplace_back(abs, dir / rel);
        }
        else
        {
            src[uri] = uri;
        }
    }
}

void resource_plan::write(
    const text_doc &doc, const fs::path &base, const fs::path &resource_dir) const
{
    doc.write_resources(base / resource_dir);
    for(const auto &[from, to] : copy)
    {
        fs::create_directories(to.parent_path());
        fs::copy_file(from, to, fs::copy_options::overwrite_existing);
    }
}

} // namespace docsmith::detail
