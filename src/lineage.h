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
#pragma once

// Private: a style's chain of parents.

#include <type_traits>
#include <vector>

#include "docsmithcpp/style.h"

namespace docsmith::detail
{

/// The style named sn and its parents, nearest first; empty if sn isn't
/// defined. A parent that isn't defined ends the chain.
template <style_family F>
auto lineage(const style_name<F> &sn, const style_sheet &styles)
{
    using S = std::remove_cvref_t<decltype(*styles.find(sn))>;
    std::vector<const S *> out;
    const S *s = styles.find(sn);
    for(int depth = 0; s && depth != 32; ++depth) // guards against a cycle
    {
        out.push_back(s);
        if constexpr(requires { s->parent; })
            s = styles.find(s->parent);
        else
            s = nullptr;
    }
    return out;
}

} // namespace docsmith::detail
