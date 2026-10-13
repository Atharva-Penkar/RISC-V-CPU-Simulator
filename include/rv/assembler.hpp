// Two-pass RV32I assembler.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <version>

#include "rv/isa.hpp"

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#else
#include <variant>
#endif

namespace rv {

/*---------------------------------------------------------------------------------------------
  Result: std::expected when the standard library has it (C++23), otherwise a minimal stand-in
  with the same interface (has_value, operator bool, operator*, operator->, error).
---------------------------------------------------------------------------------------------*/
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
template <class T, class E>
using Result = std::expected<T, E>;

template <class E>
std::unexpected<E> fail(E error) { return std::unexpected<E>(std::move(error)); }
#else
template <class E>
struct Failure { E error; };

template <class E>
Failure<E> fail(E error) { return {std::move(error)}; }

template <class T, class E>
class Result {
public:
    Result(T value) : value_(std::in_place_index<0>, std::move(value)) {}
    template <class F>
    Result(Failure<F> failure) : value_(std::in_place_index<1>, E(std::move(failure.error))) {}

    bool has_value() const { return value_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    T& operator*() { return std::get<0>(value_); }
    const T& operator*() const { return std::get<0>(value_); }
    T* operator->() { return &std::get<0>(value_); }
    const T* operator->() const { return &std::get<0>(value_); }
    const E& error() const { return std::get<1>(value_); }

private:
    std::variant<T, E> value_;
};
#endif

/*an error at a 1-based position in the assembly source*/
struct Diagnostic {
    std::size_t line = 0;
    std::size_t column = 0;
    std::string message;
};

using Program = std::vector<uint32_t>;

/*1-based position of an instruction's mnemonic in the assembly source*/
struct SourceLocation {
    std::size_t line = 0;
    std::size_t column = 0;
};

/*machine code plus a source map: sources[i] is where instruction i (address 4*i) was written*/
struct AssembledProgram {
    Program words;
    std::vector<SourceLocation> sources;
};

/*assembles the whole source; on failure returns every error found, sorted by position.
syntax: statements end at ';' or a new line, '#' starts a comment, "label:" may stand alone
or precede an instruction; mnemonics, registers and labels are case-insensitive*/
Result<Program, std::vector<Diagnostic>> assemble(std::string_view source);

/*assemble() that also returns the source map*/
Result<AssembledProgram, std::vector<Diagnostic>> assembleWithSourceMap(std::string_view source);

/*packs the fields of an instruction into its 32-bit encoding*/
uint32_t encode(const DecodedInst& inst);

/*"file:line:column: error: message" followed by the source line and a caret under the column*/
std::string formatDiagnostic(const Diagnostic& diagnostic, std::string_view source, std::string_view fileName);

}  // namespace rv
