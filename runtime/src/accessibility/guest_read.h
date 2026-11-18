#ifndef NSMBW_ACCESSIBILITY_GUEST_READ_H
#define NSMBW_ACCESSIBILITY_GUEST_READ_H

#include <cmath>
#include <cstdint>
#include <cstring>

#include "memory.h"

// Non-throwing, byte-swapped guest reads. Every accessor fails (returns false) on an unmapped
// address, so a stale pointer costs one skipped frame instead of a crash.
namespace a11y {

inline bool TryU32(std::uint32_t addr, std::uint32_t& out) {
    return Memory::TryRead32(addr, out);
}

inline bool TryPointer(std::uint32_t addr, std::uint32_t& out) {
    return Memory::TryRead32(addr, out) && out != 0 && Memory::Contains(out, 4);
}

inline bool TryU8(std::uint32_t addr, std::uint8_t& out) {
    std::uint32_t word = 0;
    if (!Memory::TryRead32(addr & ~std::uint32_t{3}, word)) {
        return false;
    }
    const std::uint32_t shift = (3u - (addr & 3u)) * 8u;
    out = static_cast<std::uint8_t>((word >> shift) & 0xFFu);
    return true;
}

inline bool TryU16(std::uint32_t addr, std::uint16_t& out) {
    std::uint32_t word = 0;
    if ((addr & 1u) != 0 || !Memory::TryRead32(addr & ~std::uint32_t{3}, word)) {
        return false;
    }
    out = static_cast<std::uint16_t>((addr & 2u) ? (word & 0xFFFFu) : (word >> 16));
    return true;
}

inline bool TryFloat(std::uint32_t addr, float& out) {
    std::uint32_t bits = 0;
    if (!Memory::TryRead32(addr, bits)) {
        return false;
    }
    std::memcpy(&out, &bits, sizeof(out));
    return std::isfinite(out);
}

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline bool TryVec3(std::uint32_t addr, Vec3& out) {
    return TryFloat(addr, out.x) && TryFloat(addr + 4, out.y) && TryFloat(addr + 8, out.z);
}

inline bool TryWriteFloat(std::uint32_t addr, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return Memory::TryWrite32(addr, bits);
}

}  // namespace a11y

#endif  // NSMBW_ACCESSIBILITY_GUEST_READ_H
