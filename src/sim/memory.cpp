#include "rv/memory.hpp"

#include "rv/decoder.hpp"

namespace rv {

InstructionMemory::InstructionMemory(std::span<const uint32_t> words) : words_(words) {
    decoded_.reserve(words.size());
    for (const uint32_t word : words)
        decoded_.push_back(decode(word));
}

MemoryError DataMemory::check(uint32_t address, uint32_t width) const {
    if (width != 1 && width != 2 && width != 4)
        return MemoryError::Misaligned;
    if (address >= bytes_.size() || bytes_.size() - address < width)
        return MemoryError::OutOfRange;
    if (address % width != 0)
        return MemoryError::Misaligned;
    return MemoryError::None;
}

std::optional<uint32_t> DataMemory::read(uint32_t address, uint32_t width) const {
    if (check(address, width) != MemoryError::None)
        return std::nullopt;
    uint32_t value = 0;
    for (uint32_t i = width; i-- > 0;)
        value = value << 8 | bytes_[address + i];
    return value;
}

bool DataMemory::write(uint32_t address, uint32_t width, uint32_t value) {
    if (check(address, width) != MemoryError::None)
        return false;
    for (uint32_t i = 0; i < width; i++)
        bytes_[address + i] = static_cast<uint8_t>(value >> (8 * i));
    return true;
}

std::optional<uint32_t> DataMemory::load(uint32_t funct3, uint32_t address) const {
    const uint32_t width = accessWidth(funct3);
    const std::optional<uint32_t> value = read(address, width);
    const bool zeroExtend = (funct3 & 4u) != 0 || width == 4;     // LBU, LHU, LW
    if (!value || zeroExtend)
        return value;
    return static_cast<uint32_t>(signExtend(*value, 8 * width));
}

bool DataMemory::store(uint32_t funct3, uint32_t address, uint32_t value) {
    return write(address, accessWidth(funct3), value);
}

}  // namespace rv
