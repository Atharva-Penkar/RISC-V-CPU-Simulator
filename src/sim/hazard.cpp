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

bool HazardUnit::dependsOn(const DecodedInst& inId, const Control& idControl, const PendingWrite& write) {
    if (!write.valid || write.rd.index() == 0)
        return false;
    return (idControl.readsRs1 && inId.rs1 == write.rd) || (idControl.readsRs2 && inId.rs2 == write.rd);
}

bool HazardUnit::loadUseHazard(const DecodedInst& inId, const Control& idControl, const PendingWrite& loadInEx) {
    return dependsOn(inId, idControl, loadInEx);
}

}  // namespace rv
