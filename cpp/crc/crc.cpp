#include <crc/crc.h>

namespace wirespaces::crc {

// // TODO: use link-seam injection later, so projects can have different CRC implementations

// SaeJ1850::value_type SaeJ1850::compute(const uint8_t* data, size_t size) {
//     return crcBitwise<SaeJ1850>(data, size);
// }

// Crc16Ccitt::value_type Crc16Ccitt::compute(const uint8_t* data, size_t size) {
//     return crcBitwise<Crc16Ccitt>(data, size);
// }



// uint8_t crcSaeJ1850(const uint8_t* data, size_t size) {
//     const uint8_t kPolynomial = 0x1D;
//     const uint8_t kTestBit    = 0x80;
//     uint8_t result = 0xFF;

//     for (size_t index = 0U; index < size; ++index) {
        const uint8_t x = data[index];
//         result ^= x;
//         for (int i = 0; i < 8; i++) {
//             if (result & kTestBit) {
//                 result = (result << 1U) ^ kPolynomial;
//             } else {
//                 result <<= 1U;
//             }
//         }
//     }

//     return static_cast<uint8_t>(~result);
// }

} // namespace wirespaces::crc
