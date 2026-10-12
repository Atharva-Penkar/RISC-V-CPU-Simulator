#include "rv/cpu.hpp"

namespace rv {

ForwardSource HazardUnit::forward(Reg source, const PendingWrite& exMem, const PendingWrite& memWb) {
    if (source.index() == 0)
        return ForwardSource::RegisterFile;
    if (exMem.valid && exMem.rd == source)
        return ForwardSource::ExMem;
    if (memWb.valid && memWb.rd == source)
        return ForwardSource::MemWb;
    return ForwardSource::RegisterFile;
}

bool HazardUnit::loadUseHazard(const DecodedInst& inId, const Control& idControl, const PendingWrite& loadInEx) {
    if (!loadInEx.valid || loadInEx.rd.index() == 0)
        return false;
    return (idControl.readsRs1 && inId.rs1 == loadInEx.rd) || (idControl.readsRs2 && inId.rs2 == loadInEx.rd);
}

}  // namespace rv
