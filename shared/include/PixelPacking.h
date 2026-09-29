#pragma once
#include "FramePixelFormat.h"
#include <cstring>
#include <vector>

namespace SynapseX {
// Source is mapped DXGI BGRA, including any driver row padding.
// Caller supplies valid positive dimensions and pitch >= width * 4.
inline void PackBgraRows(const uint8_t* source, size_t pitch, size_t width, size_t height,
                         std::vector<uint8_t>& output,
                         FramePixelFormat format = FramePixelFormat::Bgra32) {
    const size_t rowBytes = width * BytesPerPixel(format);
    output.resize(rowBytes * height);
    for (size_t row = 0; row < height; ++row) {
        const uint8_t* src = source + row * pitch;
        uint8_t* dst = output.data() + row * rowBytes;
        if (format == FramePixelFormat::Bgr24) {
            for (size_t pixel = 0; pixel < width; ++pixel) {
                dst[pixel * 3] = src[pixel * 4];
                dst[pixel * 3 + 1] = src[pixel * 4 + 1];
                dst[pixel * 3 + 2] = src[pixel * 4 + 2];
            }
        } else {
            std::memcpy(dst, src, rowBytes);
        }
    }
}
} // namespace SynapseX
