#include "test_harness.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using nes::Cpu;
using nes::test::FlatMemory;
using nes::test::expect;
using nes::test::reset;
using nes::test::step;

void unofficial_nops_consume_operands_and_bus_cycles() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0x8000] = 0x1C;  // NOP $20FC,X
    memory.bytes[0x8001] = 0xFC;
    memory.bytes[0x8002] = 0x20;
    Cpu cpu(memory);
    reset(cpu);
    cpu.state().x = 4;
    memory.clear_access_log();

    expect(step(cpu) == 5, "page-crossing unofficial NOP uses its extra cycle");
    expect(cpu.state().program_counter == 0x8003,
           "unofficial NOP consumes its absolute operand");
    expect(memory.reads.size() == 5 && memory.reads[3] == 0x2000 &&
               memory.reads[4] == 0x2100,
           "unofficial NOP performs provisional and resolved reads");
}

void combined_mutations_apply_both_operations() {
    struct Case {
        std::uint8_t opcode;
        std::uint8_t accumulator;
        std::uint8_t memory;
        bool carry;
        std::uint8_t expected_accumulator;
        std::uint8_t expected_memory;
    };
    constexpr std::array cases{
        Case{0x07, 0x01, 0x81, false, 0x03, 0x02},  // SLO
        Case{0x27, 0xF0, 0x80, false, 0x00, 0x00},  // RLA
        Case{0x47, 0xFF, 0x01, false, 0xFF, 0x00},  // SRE
        Case{0x67, 0x10, 0x03, false, 0x12, 0x01},  // RRA
        Case{0xC7, 0x10, 0x11, false, 0x10, 0x10},  // DCP
        Case{0xE7, 0x10, 0x0F, true, 0x00, 0x10},   // ISC
    };

    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = test.opcode;
        memory.bytes[0x8001] = 0x40;
        memory.bytes[0x0040] = test.memory;
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().a = test.accumulator;
        cpu.set_flag(Cpu::carry, test.carry);
        memory.clear_access_log();

        expect(step(cpu) == 5, "unofficial zero-page mutation cycle count");
        expect(cpu.state().a == test.expected_accumulator,
               "unofficial mutation updates the accumulator");
        expect(memory.bytes[0x0040] == test.expected_memory,
               "unofficial mutation updates memory");
        expect(memory.writes.size() == 2 && memory.writes[0].second == test.memory &&
                   memory.writes[1].second == test.expected_memory,
               "unofficial mutation preserves the 6502 dummy write");
    }
}

void unofficial_loads_stores_and_immediates_update_state() {
    {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = 0xA7;  // LAX $40
        memory.bytes[0x8001] = 0x40;
        memory.bytes[0x0040] = 0xA5;
        Cpu cpu(memory);
        reset(cpu);
        expect(step(cpu) == 3 && cpu.state().a == 0xA5 && cpu.state().x == 0xA5,
               "LAX loads both A and X");
    }
    {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = 0x8F;  // SAX $2040
        memory.bytes[0x8001] = 0x40;
        memory.bytes[0x8002] = 0x20;
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().a = 0xF3;
        cpu.state().x = 0x3F;
        expect(step(cpu) == 4 && memory.bytes[0x2040] == 0x33,
               "SAX stores the intersection of A and X");
    }
    {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        const std::array program{
            std::uint8_t{0x0B}, std::uint8_t{0x80},  // ANC #$80
            std::uint8_t{0x4B}, std::uint8_t{0x03},  // ALR #$03
            std::uint8_t{0xAB}, std::uint8_t{0xA5},  // ATX #$A5
            std::uint8_t{0xCB}, std::uint8_t{0x05},  // AXS #$05
        };
        for (std::size_t index = 0; index < program.size(); ++index) {
            memory.bytes[0x8000U + index] = program[index];
        }
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().a = 0xFF;
        step(cpu);
        expect(cpu.state().a == 0x80 && cpu.flag(Cpu::carry),
               "ANC copies its sign result into carry");
        cpu.state().a = 0xFF;
        step(cpu);
        expect(cpu.state().a == 0x01 && cpu.flag(Cpu::carry),
               "ALR combines AND with a logical shift");
        step(cpu);
        expect(cpu.state().a == 0xA5 && cpu.state().x == 0xA5,
               "ATX loads its immediate into A and X");
        step(cpu);
        expect(cpu.state().x == 0xA0 && cpu.flag(Cpu::carry),
               "AXS subtracts its immediate from A intersected with X");
    }
}

void unstable_store_family_uses_high_byte_masking() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0x8000] = 0x9C;  // SHY $20F0,X
    memory.bytes[0x8001] = 0xF0;
    memory.bytes[0x8002] = 0x20;
    Cpu cpu(memory);
    reset(cpu);
    cpu.state().x = 0x0F;
    cpu.state().y = 0xA5;

    expect(step(cpu) == 5, "SHY indexed store cycle count");
    expect(memory.bytes[0x20FF] == 0x21,
           "SHY masks Y with one above the base high byte");
}

}  // namespace

int run_cpu_unofficial_tests() {
    const auto before = nes::test::failures;
    unofficial_nops_consume_operands_and_bus_cycles();
    combined_mutations_apply_both_operations();
    unofficial_loads_stores_and_immediates_update_state();
    unstable_store_family_uses_high_byte_masking();
    return nes::test::failures - before;
}
