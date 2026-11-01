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
#include "image_size.h"

#include <cstdint>

namespace docsmith::detail
{
namespace
{
std::uint32_t be16(std::string_view b, std::size_t i)
{ return (static_cast<std::uint8_t>(b[i]) << 8) | static_cast<std::uint8_t>(b[i + 1]); }

std::uint32_t be32(std::string_view b, std::size_t i)
{ return (be16(b, i) << 16) | be16(b, i + 2); }

std::uint32_t le16(std::string_view b, std::size_t i)
{ return static_cast<std::uint8_t>(b[i]) | (static_cast<std::uint8_t>(b[i + 1]) << 8); }

std::optional<image_size> png(std::string_view b)
{
    if(b.size() < 24 || b.substr(12, 4) != "IHDR")
        return std::nullopt;
    image_size s{be32(b, 16), be32(b, 20)};
    // Chunks: length, type, data, CRC. pHYs comes before the image data.
    for(std::size_t i = 8; i + 8 <= b.size();)
    {
        const std::uint32_t len = be32(b, i);
        const std::string_view type = b.substr(i + 4, 4);
        if(type == "IDAT" || type == "IEND" || i + 12 + len > b.size())
            break;
        if(type == "pHYs" && len == 9 && b[i + 16] == 1) // pixels per metre
        {
            if(const auto x = be32(b, i + 8), y = be32(b, i + 12); x && y)
            {
                s.dpi_x = x * 0.0254;
                s.dpi_y = y * 0.0254;
            }
            break;
        }
        i += 12 + len;
    }
    return s;
}

std::optional<image_size> jpeg(std::string_view b)
{
    image_size s;
    for(std::size_t i = 2; i + 4 <= b.size();)
    {
        if(static_cast<std::uint8_t>(b[i]) != 0xFF)
            return std::nullopt;
        const std::uint8_t marker = static_cast<std::uint8_t>(b[i + 1]);
        if(marker == 0xFF) // fill byte
        {
            ++i;
            continue;
        }
        const std::uint32_t len = be16(b, i + 2);
        if(i + 2 + len > b.size())
            return std::nullopt;
        if(marker == 0xE0 && len >= 14 && b.substr(i + 4, 5) == std::string_view("JFIF\0", 5))
        {
            const int units = static_cast<std::uint8_t>(b[i + 11]);
            const double x = be16(b, i + 12), y = be16(b, i + 14);
            if(units != 0 && x && y)
            {
                const double scale = units == 2 ? 2.54 : 1.0; // dots per cm
                s.dpi_x = x * scale;
                s.dpi_y = y * scale;
            }
        }
        // Start of frame: every SOFn except DHT (C4), JPG (C8) and DAC (CC).
        if(marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
        {
            if(len < 7)
                return std::nullopt;
            s.height = be16(b, i + 5);
            s.width = be16(b, i + 7);
            return s;
        }
        i += 2 + len;
    }
    return std::nullopt;
}
} // namespace

std::optional<image_size> read_image_size(std::string_view b)
{
    std::optional<image_size> s;
    if(b.starts_with("\x89PNG\r\n\x1a\n"))
        s = png(b);
    else if(b.size() >= 4 && static_cast<std::uint8_t>(b[0]) == 0xFF &&
        static_cast<std::uint8_t>(b[1]) == 0xD8)
        s = jpeg(b);
    else if(b.size() >= 10 && (b.starts_with("GIF87a") || b.starts_with("GIF89a")))
        s = image_size{le16(b, 6), le16(b, 8)};
    if(s && (s->width == 0 || s->height == 0))
        return std::nullopt;
    return s;
}

} // namespace docsmith::detail
