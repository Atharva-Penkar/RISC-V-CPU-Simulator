// Machine storage: register file, instruction memory and data memory.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "rv/isa.hpp"

namespace rv {

inline constexpr std::size_t kMemoryBytes = 256;    // 64 words of data memory

class RegisterFile {
public:
    uint32_t read(Reg reg) const { return values_[reg.index()]; }

    /*the only write port; x0 is hard-wired to zero*/
    void write(Reg reg, uint32_t value) {
        if (reg.index() != 0)
            values_[reg.index()] = value;
    }

private:
    std::array<uint32_t, 32> values_{};
};

/*read-only view of the program; it must outlive the memory*/
class InstructionMemory {
public:
    explicit InstructionMemory(std::span<const uint32_t> words) : words_(words) {}

    /*true for a word-aligned address inside the program*/
    bool contains(uint32_t address) const { return address % 4 == 0 && address / 4 < words_.size(); }
    uint32_t fetch(uint32_t address) const { return words_[address / 4]; }
    /*address just past the last instruction; reaching it ends the program*/
    uint32_t endAddress() const { return static_cast<uint32_t>(words_.size() * 4); }

private:
    std::span<const uint32_t> words_;
};

enum class MemoryError : uint8_t { None, Misaligned, OutOfRange };

/*byte-addressed and little-endian; accesses must be naturally aligned and in range*/
class DataMemory {
public:
    explicit DataMemory(std::size_t bytes = kMemoryBytes) : bytes_(bytes, 0) {}

    std::size_t size() const { return bytes_.size(); }

    /*bytes accessed by the load or store with this funct3: 1, 2 or 4*/
    static uint32_t accessWidth(uint32_t funct3) { return 1u << (funct3 & 3u); }

    /*why an access of width bytes at address would fail, or MemoryError::None*/
    MemoryError check(uint32_t address, uint32_t width) const;

    /*1, 2 or 4 bytes, zero-extended; nullopt for an invalid access*/
    std::optional<uint32_t> read(uint32_t address, uint32_t width) const;
    bool write(uint32_t address, uint32_t width, uint32_t value);

    /*LB, LH, LW, LBU, LHU selected by funct3, with sign or zero extension*/
    std::optional<uint32_t> load(uint32_t funct3, uint32_t address) const;
    /*SB, SH, SW selected by funct3*/
    bool store(uint32_t funct3, uint32_t address, uint32_t value);

private:
    std::vector<uint8_t> bytes_;
};

}  // namespace rv
