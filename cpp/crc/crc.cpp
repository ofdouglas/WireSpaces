#include "crc/crc.h"

namespace crc {

// // TODO: use link-seam injection later, so projects can have different CRC implementations

// SaeJ1850::value_type SaeJ1850::compute(util::Span<const uint8_t> input) {
//     return crcBitwise<SaeJ1850>(input);
// }

// Crc16Ccitt::value_type Crc16Ccitt::compute(util::Span<const uint8_t> input) {
//     return crcBitwise<Crc16Ccitt>(input);
// }



// uint8_t crcSaeJ1850(util::Span<const uint8_t> input) {
//     const uint8_t kPolynomial = 0x1D;
//     const uint8_t kTestBit    = 0x80;
//     uint8_t result = 0xFF;

//     for (auto x : input) {
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

} // namespace crc
