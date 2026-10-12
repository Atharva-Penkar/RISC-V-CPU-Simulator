// Instruction decoding and disassembly.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "rv/isa.hpp"

namespace rv {

/*nullopt when the word is not an RV32I instruction this simulator supports*/
std::optional<DecodedInst> decode(uint32_t word);

/*assembler syntax, e.g. "LW x5, -4[x1]"; branch and JAL targets are shown relative to pc*/
std::string disassemble(const DecodedInst& inst);

std::string toHex(uint32_t value, int digits = 8);

}  // namespace rv
