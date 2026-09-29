#pragma once
#include <cstddef>
#include <cstdint>

namespace SynapseX {
enum class FramePixelFormat : uint8_t { Bgra32, Bgr24 };

constexpr size_t BytesPerPixel(FramePixelFormat format) {
    return format == FramePixelFormat::Bgr24 ? 3 : 4;
}
} // namespace SynapseX
