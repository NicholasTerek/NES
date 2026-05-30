#include "test_harness.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace {

using nes::Cpu;
using nes::test::FlatMemory;
using nes::test::expect;
using nes::test::reset;
using nes::test::step;

struct MemoryCase {
    std::uint8_t opcode;
    std::uint8_t cycles;
    std::uint16_t address;
};

constexpr auto mutation_cases(std::uint8_t zero_page, std::uint8_t zero_page_x,
                              std::uint8_t absolute, std::uint8_t absolute_x) {
    return std::array{
        MemoryCase{zero_page, 5, 0x0040}, MemoryCase{zero_page_x, 6, 0x0044},
        MemoryCase{absolute, 6, 0x9000}, MemoryCase{absolute_x, 7, 0x9004},
    };
}

void configure_memory_instruction(FlatMemory& memory, Cpu& cpu, const MemoryCase& test,
                                  std::uint8_t value) {
    memory.bytes[0x8000] = test.opcode;
    if (test.address == 0x0040) {
        memory.bytes[0x8001] = 0x40;
    } else if (test.address == 0x0044) {
        cpu.state().x = 4;
        memory.bytes[0x8001] = 0x40;
    } else {
        cpu.state().x = test.address == 0x9004 ? 4 : 0;
        memory.bytes[0x8001] = 0x00;
        memory.bytes[0x8002] = 0x90;
    }
    memory.bytes[test.address] = value;
}

template <typename Prepare, typename Verify>
void verify_mutation(const std::array<MemoryCase, 4>& cases, std::uint8_t initial,
                     Prepare prepare, Verify verify, std::string_view cycle_message) {
    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        Cpu cpu(memory);
        reset(cpu);
        configure_memory_instruction(memory, cpu, test, initial);
        prepare(cpu);
        const auto cycles = step(cpu);
        verify(memory, cpu, test.address);
        expect(cycles == test.cycles, cycle_message);
    }
}

void shifts_and_rotates_cover_every_memory_mode() {
    verify_mutation(mutation_cases(0x06, 0x16, 0x0E, 0x1E), 0x81,
                    [](Cpu&) {}, [](const FlatMemory& memory, const Cpu& cpu,
                                   std::uint16_t address) {
                        expect(memory.bytes[address] == 0x02, "ASL writes its result");
                        expect(cpu.flag(Cpu::carry), "ASL moves bit seven into carry");
                    }, "ASL memory cycle count");

    verify_mutation(mutation_cases(0x46, 0x56, 0x4E, 0x5E), 0x01,
                    [](Cpu&) {}, [](const FlatMemory& memory, const Cpu& cpu,
                                   std::uint16_t address) {
                        expect(memory.bytes[address] == 0x00, "LSR writes its result");
                        expect(cpu.flag(Cpu::carry) && cpu.flag(Cpu::zero),
                               "LSR updates carry and zero");
                    }, "LSR memory cycle count");

    verify_mutation(mutation_cases(0x26, 0x36, 0x2E, 0x3E), 0x80,
                    [](Cpu& cpu) { cpu.set_flag(Cpu::carry, true); },
                    [](const FlatMemory& memory, const Cpu& cpu, std::uint16_t address) {
                        expect(memory.bytes[address] == 0x01, "ROL rotates through carry");
                        expect(cpu.flag(Cpu::carry), "ROL moves bit seven into carry");
                    }, "ROL memory cycle count");

    verify_mutation(mutation_cases(0x66, 0x76, 0x6E, 0x7E), 0x01,
                    [](Cpu& cpu) { cpu.set_flag(Cpu::carry, true); },
                    [](const FlatMemory& memory, const Cpu& cpu, std::uint16_t address) {
                        expect(memory.bytes[address] == 0x80, "ROR rotates through carry");
                        expect(cpu.flag(Cpu::carry) && cpu.flag(Cpu::negative),
                               "ROR updates carry and negative");
                    }, "ROR memory cycle count");
}

void increments_and_decrements_cover_every_memory_mode() {
    verify_mutation(mutation_cases(0xE6, 0xF6, 0xEE, 0xFE), 0xFF,
                    [](Cpu&) {}, [](const FlatMemory& memory, const Cpu& cpu,
                                   std::uint16_t address) {
                        expect(memory.bytes[address] == 0x00, "INC wraps at 255");
                        expect(cpu.flag(Cpu::zero), "INC updates zero");
                    }, "INC memory cycle count");

    verify_mutation(mutation_cases(0xC6, 0xD6, 0xCE, 0xDE), 0x00,
                    [](Cpu&) {}, [](const FlatMemory& memory, const Cpu& cpu,
                                   std::uint16_t address) {
                        expect(memory.bytes[address] == 0xFF, "DEC wraps at zero");
                        expect(cpu.flag(Cpu::negative), "DEC updates negative");
                    }, "DEC memory cycle count");
}

void accumulator_shifts_and_register_mutations_update_flags() {
    struct RegisterCase {
        std::uint8_t opcode;
        std::uint8_t Cpu::State::*member;
        std::uint8_t initial;
        std::uint8_t expected;
    };
    constexpr std::array cases{
        RegisterCase{0xE8, &Cpu::State::x, 0xFF, 0x00},
        RegisterCase{0xCA, &Cpu::State::x, 0x00, 0xFF},
        RegisterCase{0xC8, &Cpu::State::y, 0xFF, 0x00},
        RegisterCase{0x88, &Cpu::State::y, 0x00, 0xFF},
    };
    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = test.opcode;
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().*(test.member) = test.initial;
        expect(step(cpu) == 2, "register mutation cycle count");
        expect(cpu.state().*(test.member) == test.expected, "register mutation wraps");
        expect(test.expected == 0 ? cpu.flag(Cpu::zero) : cpu.flag(Cpu::negative),
               "register mutation updates flags");
    }

    constexpr std::array accumulator_cases{
        std::uint8_t{0x0A}, std::uint8_t{0x4A}, std::uint8_t{0x2A}, std::uint8_t{0x6A},
    };
    for (const auto opcode : accumulator_cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = opcode;
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().a = 0x81;
        cpu.set_flag(Cpu::carry, true);
        expect(step(cpu) == 2, "accumulator shift cycle count");
        expect(cpu.state().a != 0x81, "accumulator shift stores in A");
    }
}

}  // namespace

int run_cpu_mutation_tests() {
    const auto before = nes::test::failures;
    shifts_and_rotates_cover_every_memory_mode();
    increments_and_decrements_cover_every_memory_mode();
    accumulator_shifts_and_register_mutations_update_flags();
    return nes::test::failures - before;
}
