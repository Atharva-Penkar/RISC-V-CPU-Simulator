#include "rv/json.hpp"

#include <cstdint>
#include <cstdio>
#include <optional>

#include "rv/decoder.hpp"

namespace rv {

namespace {

/*streaming writer: inserts the commas, escapes strings*/
class JsonWriter {
public:
    JsonWriter& beginObject() { return open('{'); }
    JsonWriter& endObject() { return close('}'); }
    JsonWriter& beginArray() { return open('['); }
    JsonWriter& endArray() { return close(']'); }

    JsonWriter& key(std::string_view name) {
        separate();
        quote(name);
        out_ += ':';
        afterKey_ = true;
        return *this;
    }

    JsonWriter& number(int64_t value) { return raw(std::to_string(value)); }
    JsonWriter& number(double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%.4f", value);
        return raw(buffer);
    }
    JsonWriter& boolean(bool value) { return raw(value ? "true" : "false"); }
    JsonWriter& null() { return raw("null"); }
    JsonWriter& string(std::string_view text) {
        separate();
        quote(text);
        return *this;
    }

    template <class T>
    JsonWriter& optionalNumber(const std::optional<T>& value) {
        return value ? number(static_cast<int64_t>(*value)) : null();
    }

    std::string take() { return std::move(out_); }

private:
    JsonWriter& open(char bracket) {
        separate();
        out_ += bracket;
        first_.push_back(true);
        return *this;
    }

    JsonWriter& close(char bracket) {
        out_ += bracket;
        first_.pop_back();
        return *this;
    }

    JsonWriter& raw(std::string_view text) {
        separate();
        out_ += text;
        return *this;
    }

    /*a comma before every element of a container but the first; none between a key and its value*/
    void separate() {
        if (afterKey_) {
            afterKey_ = false;
            return;
        }
        if (first_.empty())
            return;
        if (!first_.back())
            out_ += ',';
        first_.back() = false;
    }

    void quote(std::string_view text) {
        out_ += '"';
        for (const char c : text) {
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\n': out_ += "\\n"; break;
                case '\r': out_ += "\\r"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char escaped[8];
                        std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(c));
                        out_ += escaped;
                    }
                    else
                        out_ += c;
            }
        }
        out_ += '"';
    }

    std::string out_;
    std::vector<bool> first_;
    bool afterKey_ = false;
};

const char* name(Kind kind) {
    switch (kind) {
        case Kind::Alu: return "alu";
        case Kind::AluImm: return "alu-immediate";
        case Kind::Shift: return "shift-immediate";
        case Kind::Load: return "load";
        case Kind::Store: return "store";
        case Kind::Branch: return "branch";
        case Kind::Lui: return "lui";
        case Kind::Auipc: return "auipc";
        case Kind::Jal: return "jal";
        case Kind::Jalr: return "jalr";
        case Kind::System: return "system";
    }
    return "unknown";
}

const char* name(Format format) {
    switch (format) {
        case Format::R: return "R";
        case Format::I: return "I";
        case Format::S: return "S";
        case Format::B: return "B";
        case Format::U: return "U";
        case Format::J: return "J";
    }
    return "?";
}

const char* name(AluOp op) {
    switch (op) {
        case AluOp::Add: return "add";
        case AluOp::Sub: return "sub";
        case AluOp::And: return "and";
        case AluOp::Or: return "or";
        case AluOp::Xor: return "xor";
        case AluOp::Sll: return "sll";
        case AluOp::Srl: return "srl";
        case AluOp::Sra: return "sra";
        case AluOp::Slt: return "slt";
        case AluOp::Sltu: return "sltu";
        case AluOp::Eq: return "eq";
        case AluOp::Ne: return "ne";
        case AluOp::Lt: return "lt";
        case AluOp::Ge: return "ge";
        case AluOp::Ltu: return "ltu";
        case AluOp::Geu: return "geu";
        case AluOp::PassB: return "pass-b";
        case AluOp::Mul: return "mul";
        case AluOp::Mulh: return "mulh";
        case AluOp::Mulhsu: return "mulhsu";
        case AluOp::Mulhu: return "mulhu";
        case AluOp::Div: return "div";
        case AluOp::Divu: return "divu";
        case AluOp::Rem: return "rem";
        case AluOp::Remu: return "remu";
    }
    return "?";
}

const char* name(Fate fate) {
    switch (fate) {
        case Fate::InFlight: return "in-flight";
        case Fate::Retired: return "retired";
        case Fate::Flushed: return "flushed";
        case Fate::Discarded: return "discarded";
        case Fate::Faulted: return "faulted";
        case Fate::Halted: return "halted";
    }
    return "?";
}

const char* name(ForwardSource source) {
    switch (source) {
        case ForwardSource::ExMem: return "EX/MEM";
        case ForwardSource::MemWb: return "MEM/WB";
        case ForwardSource::RegisterFile: break;
    }
    return "register-file";
}

const char* name(BubbleReason reason) { return reason == BubbleReason::Stall ? "stall" : "flush"; }

const char* name(WriteSource source) {
    switch (source) {
        case WriteSource::Alu: return "alu";
        case WriteSource::Memory: return "memory";
        case WriteSource::Link: return "link";
    }
    return "?";
}

const char* name(NextPcSource source) {
    switch (source) {
        case NextPcSource::Sequential: return "pc+4";
        case NextPcSource::BranchTarget: return "branch-target";
        case NextPcSource::JumpTarget: return "jump-target";
        case NextPcSource::Hold: return "hold";
        case NextPcSource::Stopped: return "stopped";
    }
    return "?";
}

const char* name(Outcome outcome) {
    switch (outcome) {
        case Outcome::Finished: return "finished";
        case Outcome::Halted: return "halted";
        case Outcome::Faulted: return "faulted";
        case Outcome::CycleLimit: return "cycle-limit";
    }
    return "?";
}

const char* name(FaultKind kind) {
    switch (kind) {
        case FaultKind::IllegalInstruction: return "illegal-instruction";
        case FaultKind::FetchOutOfRange: return "fetch-out-of-range";
        case FaultKind::FetchMisaligned: return "fetch-misaligned";
        case FaultKind::LoadOutOfRange: return "load-out-of-range";
        case FaultKind::LoadMisaligned: return "load-misaligned";
        case FaultKind::StoreOutOfRange: return "store-out-of-range";
        case FaultKind::StoreMisaligned: return "store-misaligned";
    }
    return "?";
}

constexpr const char* kStageNames[kStageCount] = {"IF", "ID", "EX", "MEM", "WB"};

template <class T>
int64_t n(T value) { return static_cast<int64_t>(value); }

void writeInstruction(JsonWriter& json, uint32_t index, uint32_t word, const std::optional<DecodedInst>& inst,
                      const std::vector<SourceLocation>& sources) {
    json.beginObject();
    json.key("id").number(n(index));
    json.key("pc").number(n(index * 4));
    json.key("word").string(toHex(word));
    json.key("source");
    if (index < sources.size())
        json.beginObject()
            .key("line").number(n(sources[index].line))
            .key("column").number(n(sources[index].column))
            .endObject();
    else
        json.null();
    json.key("valid").boolean(inst.has_value());
    if (!inst) {
        json.endObject();
        return;
    }
    const Control control = controlFor(inst->kind());
    json.key("text").string(disassemble(*inst));
    json.key("mnemonic").string(inst->spec->mnemonic);
    json.key("kind").string(name(inst->kind()));
    json.key("format").string(name(formatOf(inst->kind())));
    json.key("rd");
    control.regWrite ? json.number(n(inst->rd.index())) : json.null();
    json.key("rs1");
    control.readsRs1 ? json.number(n(inst->rs1.index())) : json.null();
    json.key("rs2");
    control.readsRs2 ? json.number(n(inst->rs2.index())) : json.null();
    json.key("imm");
    formatOf(inst->kind()) == Format::R || inst->kind() == Kind::System ? json.null() : json.number(int64_t{inst->imm});
    json.key("aluOp").string(name(inst->spec->alu));
    json.key("control").beginObject()
        .key("regRead1").boolean(control.readsRs1)
        .key("regRead2").boolean(control.readsRs2)
        .key("aluSrc").boolean(control.aluSrc)
        .key("regWrite").boolean(control.regWrite)
        .key("memRead").boolean(control.memRead)
        .key("memWrite").boolean(control.memWrite)
        .key("memToReg").boolean(control.memToReg)
        .key("branch").boolean(control.branch)
        .key("jump").boolean(control.jump)
        .key("halt").boolean(control.halt)
        .endObject();
    json.endObject();
}

void writeSlot(JsonWriter& json, const StageSlot& slot) {
    switch (slot.kind) {
        case StageSlot::Kind::Empty:
            json.null();
            return;
        case StageSlot::Kind::Bubble:
            json.beginObject().key("bubble").string(name(slot.bubble)).endObject();
            return;
        case StageSlot::Kind::Instruction:
            json.beginObject().key("instance").number(n(slot.instance));
            if (slot.stalled)
                json.key("stalled").boolean(true);
            json.endObject();
            return;
    }
}

void writeCycle(JsonWriter& json, const CycleRecord& cycle) {
    json.beginObject();
    json.key("cycle").number(n(cycle.cycle));
    json.key("pc").number(n(cycle.pc));

    json.key("stages").beginObject();
    for (std::size_t s = 0; s < kStageCount; s++) {
        json.key(kStageNames[s]);
        writeSlot(json, cycle.stages[s]);
    }
    json.endObject();

    json.key("forwarding").beginArray();
    for (const ForwardRecord& f : cycle.forwards)
        json.beginObject()
            .key("operand").string(f.operand == Operand::Rs1 ? "rs1" : "rs2")
            .key("from").string(name(f.from))
            .key("register").number(n(f.reg.index()))
            .key("value").number(n(f.value))
            .key("producer").number(n(f.producer))
            .key("consumer").number(n(f.consumer))
            .endObject();
    json.endArray();

    json.key("stall");
    if (cycle.stall)
        json.beginObject()
            .key("reason").string(cycle.stall->reason == StallReason::LoadUse ? "load-use" : "no-forwarding")
            .key("waiting").number(n(cycle.stall->waiting))
            .key("producer").number(n(cycle.stall->producer))
            .key("register").number(n(cycle.stall->reg.index()))
            .endObject();
    else
        json.null();

    json.key("busy").beginArray();
    for (const BusyRecord& busy : cycle.busy)
        json.beginObject()
            .key("stage").string(kStageNames[static_cast<std::size_t>(busy.stage)])
            .key("instance").number(n(busy.instance))
            .key("cycle").number(n(busy.cycle))
            .key("of").number(n(busy.latency))
            .endObject();
    json.endArray();

    json.key("flush");
    if (cycle.flush) {
        json.beginObject().key("by").number(n(cycle.flush->by)).key("squashed").beginArray();
        for (const uint32_t instance : cycle.flush->squashed)
            json.number(n(instance));
        json.endArray().key("target").number(n(cycle.flush->target)).endObject();
    }
    else
        json.null();

    json.key("execute");
    if (const auto& ex = cycle.execute) {
        json.beginObject()
            .key("instance").number(n(ex->instance))
            .key("op").string(name(ex->op))
            .key("a").number(n(ex->a))
            .key("aSource").string(ex->aFromPc ? "pc" : "rs1")
            .key("b").number(n(ex->b))
            .key("bSource").string(ex->bFromImmediate ? "imm" : "rs2")
            .key("result").number(n(ex->result));
        json.key("branch");
        if (ex->branchTaken)
            json.beginObject().key("taken").boolean(*ex->branchTaken).endObject();
        else
            json.null();
        json.key("target").optionalNumber(ex->target);
        json.endObject();
    }
    else
        json.null();

    json.key("memory");
    if (const auto& mem = cycle.memory) {
        json.beginObject()
            .key("instance").number(n(mem->instance))
            .key("access").string(mem->store ? "store" : "load")
            .key("address").number(n(mem->address))
            .key("width").number(n(mem->width))
            .key("value").number(n(mem->value));
        if (mem->store)
            json.key("previous").number(n(mem->previous));
        json.endObject();
    }
    else
        json.null();

    json.key("registerWrite");
    if (const auto& write = cycle.registerWrite)
        json.beginObject()
            .key("instance").number(n(write->instance))
            .key("register").number(n(write->reg.index()))
            .key("value").number(n(write->value))
            .key("previous").number(n(write->previous))
            .key("source").string(name(write->source))
            .endObject();
    else
        json.null();

    json.key("nextPc").beginObject()
        .key("source").string(name(cycle.nextPcSource))
        .key("value").number(n(cycle.nextPc))
        .endObject();
    json.endObject();
}

void header(JsonWriter& json, std::string_view format, bool ok) {
    json.key("format").string(format);
    json.key("version").number(int64_t{kJsonFormatVersion});
    json.key("ok").boolean(ok);
}

}  // namespace

std::string toJson(const Timeline& timeline) {
    JsonWriter json;
    json.beginObject();
    header(json, "rv-timeline", true);
    json.key("model").string("pipeline");
    json.key("forwarding").boolean(timeline.config().forwarding);
    const Latencies& latencies = timeline.config().latencies;
    json.key("latencies").beginObject()
        .key("multiply").number(n(latencies.multiply))
        .key("divide").number(n(latencies.divide))
        .key("load").number(n(latencies.load))
        .key("store").number(n(latencies.store))
        .endObject();

    json.key("program").beginObject().key("instructions").beginArray();
    for (uint32_t i = 0; i < timeline.words().size(); i++)
        writeInstruction(json, i, timeline.words()[i], timeline.decoded()[i], timeline.sources());
    json.endArray().endObject();

    json.key("instances").beginArray();
    for (const InstanceRecord& instance : timeline.instances())
        json.beginObject()
            .key("id").number(n(instance.id))
            .key("instruction").optionalNumber(instance.instruction)
            .key("pc").number(n(instance.pc))
            .key("occurrence").number(n(instance.occurrence))
            .key("fetchCycle").number(n(instance.fetchCycle))
            .key("fate").string(name(instance.fate))
            .key("endCycle").number(n(instance.endCycle))
            .endObject();
    json.endArray();

    json.key("cycles").beginArray();
    for (const CycleRecord& cycle : timeline.cycles())
        writeCycle(json, cycle);
    json.endArray();

    const RunResult& result = timeline.result();
    json.key("outcome").beginObject()
        .key("status").string(name(result.outcome))
        .key("cycle").number(n(result.stats.cycles))
        .key("instance").optionalNumber(timeline.stopInstance());
    json.key("haltPc");
    result.outcome == Outcome::Halted ? json.number(n(result.haltPc)) : json.null();
    json.key("fault");
    if (result.fault)
        json.beginObject()
            .key("kind").string(name(result.fault->kind))
            .key("pc").number(n(result.fault->pc))
            .key("address").number(n(result.fault->address))
            .endObject();
    else
        json.null();
    json.endObject();

    const RunStats& stats = result.stats;
    json.key("stats").beginObject()
        .key("cycles").number(n(stats.cycles))
        .key("instructions").number(n(stats.retired))
        .key("cpi").number(stats.cpi())
        .key("stalls").number(n(stats.stalls))
        .key("executeStalls").number(n(stats.executeStalls))
        .key("memoryStalls").number(n(stats.memoryStalls))
        .key("flushes").number(n(stats.flushes))
        .key("redirects").number(n(stats.redirects))
        .endObject();

    json.key("final").beginObject().key("registers").beginArray();
    for (unsigned r = 0; r < 32; r++)
        json.number(n(timeline.finalRegisters().read(Reg::fromField(r))));
    json.endArray();
    const DataMemory& memory = timeline.finalMemory();
    json.key("memory").beginObject().key("bytes").number(n(memory.size())).key("words").beginArray();
    for (uint32_t address = 0; address + 4 <= memory.size(); address += 4)
        json.number(n(memory.read(address, 4).value_or(0)));
    json.endArray().endObject().endObject();

    json.endObject();
    return json.take();
}

std::string toJson(const AssembledProgram& program) {
    JsonWriter json;
    json.beginObject();
    header(json, "rv-program", true);
    json.key("instructions").beginArray();
    for (uint32_t i = 0; i < program.words.size(); i++)
        writeInstruction(json, i, program.words[i], decode(program.words[i]), program.sources);
    json.endArray();
    json.endObject();
    return json.take();
}

std::string diagnosticsToJson(std::string_view format, const std::vector<Diagnostic>& diagnostics) {
    JsonWriter json;
    json.beginObject();
    header(json, format, false);
    json.key("error").beginObject().key("stage").string("assemble").key("diagnostics").beginArray();
    for (const Diagnostic& d : diagnostics)
        json.beginObject()
            .key("line").number(n(d.line))
            .key("column").number(n(d.column))
            .key("message").string(d.message)
            .endObject();
    json.endArray().endObject();
    json.endObject();
    return json.take();
}

}  // namespace rv
