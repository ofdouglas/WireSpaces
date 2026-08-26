# WireSpaces Embedded C++ Libraries

Reusable C++17 libraries for WireSpaces firmware. The C-compatible WS Core lives in `../core/`; this directory holds higher-level embedded libraries ported from `Design/Firmware`.

**Include convention:** add `cpp/` to your include path, then include by component path:

```cpp
#include "data_structures/ring_buffer.h"
#include "crc/crc_algorithm.h"
#include "util/span.h"
```

## Layout

| Path | Contents |
|------|----------|
| `util/` | Span, StaticString, and other small helpers |
| `data_structures/` | RingBuffer, memory pools, seqlock buffers (planned) |
| `crc/` | CRC algorithms and bitwise implementation |

Each component may add a `test/` subdirectory with GoogleTest sources when tests are wired into CMake.

## Planned (from README sketch)

Foundational: thin HAL layer, memory pools, seqlock buffers/tables, byte arena allocators, MPSC lock-free queues.

Logical link layer: CAN PDU aggregation, UART HDLC/COBS, Ethernet, CAN-FD, etc.
