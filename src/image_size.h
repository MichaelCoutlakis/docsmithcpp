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

// Private: an image's size in pixels and its resolution, read from the file's
// header (PNG, JPEG, GIF) with no image library.

#include <optional>
#include <string_view>

namespace docsmith::detail
{

struct image_size
{
    unsigned width{}; //!< Pixels
    unsigned height{};
    double dpi_x{96}; //!< 96 when the file doesn't say
    double dpi_y{96};

    double width_inches() const { return width / dpi_x; }
    double height_inches() const { return height / dpi_y; }
};

/// The size of a PNG (IHDR, pHYs), JPEG (SOF, JFIF density) or GIF image, or
/// nullopt for other formats and damaged files.
std::optional<image_size> read_image_size(std::string_view bytes);

} // namespace docsmith::detail
