#include "PixelPacking.h"
#include "PacketHeader.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace SynapseX;

int main() {
    try {
        // Three pixels per row, two rows, four padding bytes; alpha varies.
        const uint8_t source[] = {
            0, 0, 255, 0, 0, 255, 0, 17, 255, 0, 0, 255, 91, 92, 93, 94,
            255, 255, 255, 3, 1, 2, 3, 4, 5, 6, 7, 8, 95, 96, 97, 98,
        };
        const std::vector<uint8_t> expectedBgr = {
            0, 0, 255, 0, 255, 0, 255, 0, 0,
            255, 255, 255, 1, 2, 3, 5, 6, 7,
        };
        std::vector<uint8_t> output;
        PackBgraRows(source, 16, 3, 2, output, FramePixelFormat::Bgr24);
        if (output != expectedBgr) throw std::runtime_error("BGR packing changed pixels or row order");
        PackBgraRows(source, 16, 3, 2, output);
        std::vector<uint8_t> expectedBgra(source, source + 12);
        expectedBgra.insert(expectedBgra.end(), source + 16, source + 28);
        if (output != expectedBgra) throw std::runtime_error("Default BGRA packing changed");
        PackBgraRows(source, 4, 1, 1, output, FramePixelFormat::Bgr24);
        if (output != std::vector<uint8_t>({0, 0, 255})) throw std::runtime_error("Single pixel packing failed");
        static_assert(BytesPerPixel(FramePixelFormat::Bgra32) == 4);
        static_assert(BytesPerPixel(FramePixelFormat::Bgr24) == 3);
        static_assert(ProtocolMagic(FramePixelFormat::Bgra32) == 0x5358);
        static_assert(ProtocolMagic(FramePixelFormat::Bgr24) == 0x5342);
        static_assert(sizeof(PacketHeader) == 24);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
