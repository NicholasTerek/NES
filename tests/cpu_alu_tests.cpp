#include "test_harness.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using nes::Cpu;
using nes::test::FlatMemory;
using nes::test::expect;
using nes::test::reset;
using nes::test::step;

enum class Mode {
    immediate,
    zero_page,
    zero_page_x,
    absolute,
    absolute_x,
    absolute_y,
    indexed_indirect,
    indirect_indexed,
};

struct AluCase {
    std::uint8_t opcode;
    Mode mode;
    std::uint8_t cycles;
};

void configure_operand(FlatMemory& memory, Cpu& cpu, const AluCase& test,
                       std::uint8_t operand) {
    memory.bytes[0x8000] = test.opcode;
    switch (test.mode) {
    case Mode::immediate:
        memory.bytes[0x8001] = operand;
        break;
    case Mode::zero_page:
        memory.bytes[0x8001] = 0x40;
        memory.bytes[0x0040] = operand;
        break;
    case Mode::zero_page_x:
        cpu.state().x = 4;
        memory.bytes[0x8001] = 0x3C;
        memory.bytes[0x0040] = operand;
        break;
    case Mode::absolute:
        memory.bytes[0x8001] = 0x00;
        memory.bytes[0x8002] = 0x90;
        memory.bytes[0x9000] = operand;
        break;
    case Mode::absolute_x:
        cpu.state().x = 4;
        memory.bytes[0x8001] = 0xFC;
        memory.bytes[0x8002] = 0x8F;
        memory.bytes[0x9000] = operand;
        break;
    case Mode::absolute_y:
        cpu.state().y = 5;
        memory.bytes[0x8001] = 0xFB;
        memory.bytes[0x8002] = 0x8F;
        memory.bytes[0x9000] = operand;
        break;
    case Mode::indexed_indirect:
        cpu.state().x = 4;
        memory.bytes[0x8001] = 0x20;
        memory.bytes[0x0024] = 0x00;
        memory.bytes[0x0025] = 0x90;
        memory.bytes[0x9000] = operand;
        break;
    case Mode::indirect_indexed:
        cpu.state().y = 5;
        memory.bytes[0x8001] = 0x20;
        memory.bytes[0x0020] = 0xFB;
        memory.bytes[0x0021] = 0x8F;
        memory.bytes[0x9000] = operand;
        break;
    }
}

template <typename Prepare, typename Verify>
void verify_family(const std::array<AluCase, 8>& cases, std::string_view name,
                   std::uint8_t operand, Prepare prepare, Verify verify) {
    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        Cpu cpu(memory);
        reset(cpu);
        configure_operand(memory, cpu, test, operand);
        prepare(cpu);
        const auto cycles = step(cpu);
        verify(cpu);
        expect(cycles == test.cycles, std::string(name) + " addressing cycle count");
    }
}

void logic_covers_all_addressing_modes() {
    constexpr std::array and_cases{
        AluCase{0x29, Mode::immediate, 2}, AluCase{0x25, Mode::zero_page, 3},
        AluCase{0x35, Mode::zero_page_x, 4}, AluCase{0x2D, Mode::absolute, 4},
        AluCase{0x3D, Mode::absolute_x, 5}, AluCase{0x39, Mode::absolute_y, 5},
        AluCase{0x21, Mode::indexed_indirect, 6}, AluCase{0x31, Mode::indirect_indexed, 6},
    };
    constexpr std::array eor_cases{
        AluCase{0x49, Mode::immediate, 2}, AluCase{0x45, Mode::zero_page, 3},
        AluCase{0x55, Mode::zero_page_x, 4}, AluCase{0x4D, Mode::absolute, 4},
        AluCase{0x5D, Mode::absolute_x, 5}, AluCase{0x59, Mode::absolute_y, 5},
        AluCase{0x41, Mode::indexed_indirect, 6}, AluCase{0x51, Mode::indirect_indexed, 6},
    };
    constexpr std::array ora_cases{
        AluCase{0x09, Mode::immediate, 2}, AluCase{0x05, Mode::zero_page, 3},
        AluCase{0x15, Mode::zero_page_x, 4}, AluCase{0x0D, Mode::absolute, 4},
        AluCase{0x1D, Mode::absolute_x, 5}, AluCase{0x19, Mode::absolute_y, 5},
        AluCase{0x01, Mode::indexed_indirect, 6}, AluCase{0x11, Mode::indirect_indexed, 6},
    };

    const auto prepare = [](Cpu& cpu) { cpu.state().a = 0xF0; };
    verify_family(and_cases, "AND", 0x3C, prepare, [](const Cpu& cpu) {
        expect(cpu.state().a == 0x30, "AND combines accumulator bits");
    });
    verify_family(eor_cases, "EOR", 0x3C, prepare, [](const Cpu& cpu) {
        expect(cpu.state().a == 0xCC, "EOR combines accumulator bits");
    });
    verify_family(ora_cases, "ORA", 0x3C, prepare, [](const Cpu& cpu) {
        expect(cpu.state().a == 0xFC, "ORA combines accumulator bits");
        expect(cpu.flag(Cpu::negative), "ORA updates negative");
    });
}

void arithmetic_covers_all_addressing_modes() {
    constexpr std::array adc_cases{
        AluCase{0x69, Mode::immediate, 2}, AluCase{0x65, Mode::zero_page, 3},
        AluCase{0x75, Mode::zero_page_x, 4}, AluCase{0x6D, Mode::absolute, 4},
        AluCase{0x7D, Mode::absolute_x, 5}, AluCase{0x79, Mode::absolute_y, 5},
        AluCase{0x61, Mode::indexed_indirect, 6}, AluCase{0x71, Mode::indirect_indexed, 6},
    };
    constexpr std::array sbc_cases{
        AluCase{0xE9, Mode::immediate, 2}, AluCase{0xE5, Mode::zero_page, 3},
        AluCase{0xF5, Mode::zero_page_x, 4}, AluCase{0xED, Mode::absolute, 4},
        AluCase{0xFD, Mode::absolute_x, 5}, AluCase{0xF9, Mode::absolute_y, 5},
        AluCase{0xE1, Mode::indexed_indirect, 6}, AluCase{0xF1, Mode::indirect_indexed, 6},
    };

    verify_family(adc_cases, "ADC", 0x50, [](Cpu& cpu) {
        cpu.state().a = 0x50;
        cpu.set_flag(Cpu::carry, false);
    }, [](const Cpu& cpu) {
        expect(cpu.state().a == 0xA0, "ADC calculates the result");
        expect(cpu.flag(Cpu::overflow), "ADC detects signed overflow");
        expect(!cpu.flag(Cpu::carry), "ADC distinguishes carry from overflow");
    });

    verify_family(sbc_cases, "SBC", 0x10, [](Cpu& cpu) {
        cpu.state().a = 0x50;
        cpu.set_flag(Cpu::carry, true);
    }, [](const Cpu& cpu) {
        expect(cpu.state().a == 0x40, "SBC calculates the result");
        expect(cpu.flag(Cpu::carry), "SBC reports no borrow with carry");
    });
}

void compare_covers_all_addressing_modes() {
    constexpr std::array cmp_cases{
        AluCase{0xC9, Mode::immediate, 2}, AluCase{0xC5, Mode::zero_page, 3},
        AluCase{0xD5, Mode::zero_page_x, 4}, AluCase{0xCD, Mode::absolute, 4},
        AluCase{0xDD, Mode::absolute_x, 5}, AluCase{0xD9, Mode::absolute_y, 5},
        AluCase{0xC1, Mode::indexed_indirect, 6}, AluCase{0xD1, Mode::indirect_indexed, 6},
    };
    verify_family(cmp_cases, "CMP", 0x50, [](Cpu& cpu) {
        cpu.state().a = 0x50;
    }, [](const Cpu& cpu) {
        expect(cpu.flag(Cpu::zero), "CMP detects equality");
        expect(cpu.flag(Cpu::carry), "CMP sets carry for greater or equal");
        expect(!cpu.flag(Cpu::negative), "CMP sets subtraction sign");
    });
}

void bit_and_index_comparisons_cover_their_modes() {
    for (const auto opcode : {std::uint8_t{0x24}, std::uint8_t{0x2C}}) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = opcode;
        memory.bytes[0x8001] = opcode == 0x24 ? 0x40 : 0x00;
        memory.bytes[0x8002] = 0x90;
        memory.bytes[opcode == 0x24 ? 0x0040 : 0x9000] = 0xC0;
        Cpu cpu(memory);
        reset(cpu);
        cpu.state().a = 0x3F;
        step(cpu);
        expect(cpu.flag(Cpu::zero), "BIT sets zero from A AND operand");
        expect(cpu.flag(Cpu::negative), "BIT copies operand bit seven");
        expect(cpu.flag(Cpu::overflow), "BIT copies operand bit six");
    }

    constexpr std::array cases{
        AluCase{0xE0, Mode::immediate, 2}, AluCase{0xE4, Mode::zero_page, 3},
        AluCase{0xEC, Mode::absolute, 4}, AluCase{0xC0, Mode::immediate, 2},
        AluCase{0xC4, Mode::zero_page, 3}, AluCase{0xCC, Mode::absolute, 4},
    };
    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        Cpu cpu(memory);
        reset(cpu);
        configure_operand(memory, cpu, test, 0x50);
        cpu.state().x = 0x50;
        cpu.state().y = 0x50;
        const auto cycles = step(cpu);
        expect(cpu.flag(Cpu::zero) && cpu.flag(Cpu::carry), "CPX and CPY compare registers");
        expect(cycles == test.cycles, "CPX and CPY cycle count");
    }
}

}  // namespace

int run_cpu_alu_tests() {
    const auto before = nes::test::failures;
    logic_covers_all_addressing_modes();
    arithmetic_covers_all_addressing_modes();
    compare_covers_all_addressing_modes();
    bit_and_index_comparisons_cover_their_modes();
    return nes::test::failures - before;
}
