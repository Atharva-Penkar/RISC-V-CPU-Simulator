#include "rv/memory.hpp"

namespace rv {

namespace {

uint32_t widthOf(uint32_t funct3) { return 1u << (funct3 & 3u); }

}  // namespace

bool DataMemory::valid(uint32_t address, uint32_t width) const {
    return (width == 1 || width == 2 || width == 4) && address % width == 0 && address < bytes_.size()
        && bytes_.size() - address >= width;
}

std::optional<uint32_t> DataMemory::read(uint32_t address, uint32_t width) const {
    if (!valid(address, width))
        return std::nullopt;
    uint32_t value = 0;
    for (uint32_t i = width; i-- > 0;)
        value = value << 8 | bytes_[address + i];
    return value;
}

bool DataMemory::write(uint32_t address, uint32_t width, uint32_t value) {
    if (!valid(address, width))
        return false;
    for (uint32_t i = 0; i < width; i++)
        bytes_[address + i] = static_cast<uint8_t>(value >> (8 * i));
    return true;
}

std::optional<uint32_t> DataMemory::load(uint32_t funct3, uint32_t address) const {
    const uint32_t width = widthOf(funct3);
    const std::optional<uint32_t> value = read(address, width);
    const bool zeroExtend = (funct3 & 4u) != 0 || width == 4;     // LBU, LHU, LW
    if (!value || zeroExtend)
        return value;
    return static_cast<uint32_t>(signExtend(*value, 8 * width));
}

bool DataMemory::store(uint32_t funct3, uint32_t address, uint32_t value) {
    return write(address, widthOf(funct3), value);
}

}  // namespace rv
