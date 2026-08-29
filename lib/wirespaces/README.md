# WireSpaces C++ Library

Portable C++17 runtime and reusable modules for WireSpaces targets and host simulation. Add `lib/` to the compiler include path and use package-qualified includes:

```cpp
#include <wirespaces/core/dispatch.h>
#include <wirespaces/containers/ring_buffer.h>
#include <wirespaces/links/uart_hdlc/encoder.h>
#include <wirespaces/services/ping/ping.h>
```

## Layout

| Path | Contents |
|------|----------|
| `core/` | Canonical packet types, host identity, routing, dispatch, and Endpoint receivers |
| `foundation/` | Array, Span, StaticString, and compatibility helpers |
| `containers/` | Fixed-capacity containers |
| `crc/` | CRC algorithms and bitwise implementation |
| `hal/` | Portable hardware abstraction contracts |
| `links/` | Logical Link implementations |
| `runtime/` | Convenience and umbrella headers |
| `services/` | Reusable Endpoint services |
| `transports/` | Transport implementations |

Production headers and sources live directly in each module. Tests remain beside the module they exercise. PC-side tooling is a separate Python package under `tools/python/`; language-neutral schemas and generated artifacts should be shared between the C++ library and those tools.
