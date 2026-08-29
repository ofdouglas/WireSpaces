# WireSpaces Embedded C++ Libraries

Reusable C++17 libraries for WireSpaces firmware. The C-compatible WS Core lives in `../core/`; this directory holds higher-level embedded libraries ported from `Design/Firmware`.

**Include convention:** add `cpp/` to your include path, then include by module path:

```cpp
#include "containers/ring_buffer.h"
#include "crc/crc_algorithm.h"
#include "foundation/span.h"
```

## Layout

| Path | Contents |
|------|----------|
| `foundation/` | Array, Span, StaticString, and other compatibility helpers |
| `containers/` | RingBuffer, memory pools, seqlock buffers (planned) |
| `crc/` | CRC algorithms and bitwise implementation |

Production headers and sources live directly in each module directory. Each
module may add a `test/` subdirectory with GoogleTest sources and other support
subdirectories as needed.

## Planned (from README sketch)

Foundational: thin HAL layer, memory pools, seqlock buffers/tables, byte arena allocators, MPSC lock-free queues.

Logical link layer: CAN PDU aggregation, UART HDLC/COBS, Ethernet, CAN-FD, etc.
