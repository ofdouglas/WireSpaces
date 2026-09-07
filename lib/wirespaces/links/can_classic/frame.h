/*
 * @file  frame.hpp
 * @brief Classic CAN frame representation.
 */

#pragma once

#include <cstdint>

namespace wirespaces::links::can_classic {

struct Frame {
    uint32_t id{0U};  // TODO: more compact option?
    uint8_t data[8U]{};
    uint8_t dlc{0U};
    bool is_extended{false};
    bool is_valid{false};
};

}  // namespace wirespaces::links::can_classic