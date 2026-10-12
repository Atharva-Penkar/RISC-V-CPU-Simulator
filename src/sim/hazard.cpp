#include "rv/cpu.hpp"

namespace rv {

bool HazardUnit::mustStall(const DecodedInst& inst, const Control& control) const {
    return (control.readsRs1 && pending_[inst.rs1.index()].has_value())
        || (control.readsRs2 && pending_[inst.rs2.index()].has_value());
}

void HazardUnit::issue(const DecodedInst& inst, const Control& control, uint64_t tag) {
    if (control.regWrite && inst.rd.index() != 0)  // x0 is never written, so never pending
        pending_[inst.rd.index()] = tag;
}

void HazardUnit::writeBack(Reg rd, uint64_t tag) {
    if (pending_[rd.index()] == tag)
        pending_[rd.index()].reset();
}

}  // namespace rv
