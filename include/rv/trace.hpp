// Observing a simulation: per-cycle pipeline trace and the final state dump.
#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string_view>

#include "rv/decoder.hpp"
#include "rv/memory.hpp"

namespace rv {

class Tracer {
public:
    virtual ~Tracer() = default;
    /*a new clock cycle (pipeline) or instruction (ISS) starts*/
    virtual void beginCycle(std::size_t /*cycle*/) {}
    /*state of one pipeline register at the end of the cycle, or the cycle's hazard events*/
    virtual void stage(std::string_view /*name*/, std::string_view /*description*/) {}
};

/*prints every cycle's pipeline state*/
class TextTracer : public Tracer {
public:
    explicit TextTracer(std::ostream& out) : out_(out) {}

    void beginCycle(std::size_t cycle) override { out_ << "cycle " << cycle << '\n'; }
    void stage(std::string_view name, std::string_view description) override {
        out_ << "  " << name << std::string(name.size() < 8 ? 8 - name.size() : 0, ' ') << description << '\n';
    }

private:
    std::ostream& out_;
};

/*non-zero registers and data memory words; the format of the .expected files in tests/programs*/
inline void dumpState(const RegisterFile& regs, const DataMemory& memory, std::ostream& out) {
    out << "registers\n";
    for (unsigned i = 1; i < 32; i++) {
        const uint32_t value = regs.read(Reg::fromField(i));
        if (value != 0)
            out << "  x" << i << (i < 10 ? " " : "") << " = " << toHex(value) << "  " << static_cast<int32_t>(value) << '\n';
    }
    out << "memory\n";
    for (uint32_t address = 0; address + 4 <= memory.size(); address += 4) {
        const uint32_t value = memory.read(address, 4).value_or(0);
        if (value != 0)
            out << "  [" << toHex(address, 3) << "] = " << toHex(value) << "  " << static_cast<int32_t>(value) << '\n';
    }
}

}  // namespace rv
