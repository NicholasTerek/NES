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
    direct,
    indexed_indirect,
    indirect_indexed,
};

struct ReadCase {
    std::uint8_t opcode;
    std::uint8_t operand_low;
    std::uint8_t operand_high;
    std::uint8_t x;
    std::uint8_t y;
    std::uint16_t target;
    std::uint8_t cycles;
    Mode mode;
    char destination;
    std::string_view name;
};

void configure_read(FlatMemory& memory, const ReadCase& test) {
    memory.bytes[0x8000] = test.opcode;
    memory.bytes[0x8001] = test.mode == Mode::immediate ? 0xA5 : test.operand_low;
    memory.bytes[0x8002] = test.operand_high;
    if (test.mode == Mode::immediate) {
        return;
    }

    memory.bytes[test.target] = 0xA5;
    if (test.mode == Mode::indexed_indirect) {
        const auto pointer = static_cast<std::uint8_t>(test.operand_low + test.x);
        memory.bytes[pointer] = static_cast<std::uint8_t>(test.target & 0xFFU);
        memory.bytes[static_cast<std::uint8_t>(pointer + 1U)] =
            static_cast<std::uint8_t>(test.target >> 8U);
    } else if (test.mode == Mode::indirect_indexed) {
        const auto base = static_cast<std::uint16_t>(test.target - test.y);
        memory.bytes[test.operand_low] = static_cast<std::uint8_t>(base & 0xFFU);
        memory.bytes[static_cast<std::uint8_t>(test.operand_low + 1U)] =
            static_cast<std::uint8_t>(base >> 8U);
    }
}

void verify_read(const ReadCase& test) {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    configure_read(memory, test);
    Cpu cpu(memory);
    reset(cpu);
    cpu.state().x = test.x;
    cpu.state().y = test.y;

    const auto cycles = step(cpu);
    const auto value = test.destination == 'A' ? cpu.state().a
                     : test.destination == 'X' ? cpu.state().x
                                               : cpu.state().y;
    expect(value == 0xA5, test.name);
    expect(cycles == test.cycles, std::string(test.name) + " cycle count");
}

void loads_cover_every_addressing_mode() {
    constexpr std::array cases{
        ReadCase{0xA9, 0x00, 0x00, 0, 0, 0x0000, 2, Mode::immediate, 'A', "LDA immediate"},
        ReadCase{0xA5, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'A', "LDA zero page"},
        ReadCase{0xB5, 0x3C, 0x00, 4, 0, 0x0040, 4, Mode::direct, 'A', "LDA zero page X"},
        ReadCase{0xAD, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'A', "LDA absolute"},
        ReadCase{0xBD, 0xFC, 0x8F, 4, 0, 0x9000, 5, Mode::direct, 'A', "LDA absolute X crossing"},
        ReadCase{0xB9, 0xFB, 0x8F, 0, 5, 0x9000, 5, Mode::direct, 'A', "LDA absolute Y crossing"},
        ReadCase{0xA1, 0x20, 0x00, 4, 0, 0x9000, 6, Mode::indexed_indirect, 'A', "LDA indexed indirect"},
        ReadCase{0xB1, 0x20, 0x00, 0, 5, 0x9000, 6, Mode::indirect_indexed, 'A', "LDA indirect indexed crossing"},
        ReadCase{0xA2, 0x00, 0x00, 0, 0, 0x0000, 2, Mode::immediate, 'X', "LDX immediate"},
        ReadCase{0xA6, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'X', "LDX zero page"},
        ReadCase{0xB6, 0x3C, 0x00, 0, 4, 0x0040, 4, Mode::direct, 'X', "LDX zero page Y"},
        ReadCase{0xAE, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'X', "LDX absolute"},
        ReadCase{0xBE, 0xFC, 0x8F, 0, 4, 0x9000, 5, Mode::direct, 'X', "LDX absolute Y crossing"},
        ReadCase{0xA0, 0x00, 0x00, 0, 0, 0x0000, 2, Mode::immediate, 'Y', "LDY immediate"},
        ReadCase{0xA4, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'Y', "LDY zero page"},
        ReadCase{0xB4, 0x3C, 0x00, 4, 0, 0x0040, 4, Mode::direct, 'Y', "LDY zero page X"},
        ReadCase{0xAC, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'Y', "LDY absolute"},
        ReadCase{0xBC, 0xFC, 0x8F, 4, 0, 0x9000, 5, Mode::direct, 'Y', "LDY absolute X crossing"},
    };
    for (const auto& test : cases) {
        verify_read(test);
    }
}

struct StoreCase {
    std::uint8_t opcode;
    std::uint8_t operand_low;
    std::uint8_t operand_high;
    std::uint8_t x;
    std::uint8_t y;
    std::uint16_t target;
    std::uint8_t cycles;
    Mode mode;
    char source;
    std::string_view name;
};

void verify_store(const StoreCase& test) {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0x8000] = test.opcode;
    memory.bytes[0x8001] = test.operand_low;
    memory.bytes[0x8002] = test.operand_high;
    if (test.mode == Mode::indexed_indirect) {
        const auto pointer = static_cast<std::uint8_t>(test.operand_low + test.x);
        memory.bytes[pointer] = static_cast<std::uint8_t>(test.target & 0xFFU);
        memory.bytes[static_cast<std::uint8_t>(pointer + 1U)] =
            static_cast<std::uint8_t>(test.target >> 8U);
    } else if (test.mode == Mode::indirect_indexed) {
        const auto base = static_cast<std::uint16_t>(test.target - test.y);
        memory.bytes[test.operand_low] = static_cast<std::uint8_t>(base & 0xFFU);
        memory.bytes[static_cast<std::uint8_t>(test.operand_low + 1U)] =
            static_cast<std::uint8_t>(base >> 8U);
    }

    Cpu cpu(memory);
    reset(cpu);
    cpu.state().a = 0xA5;
    cpu.state().x = test.source == 'X' ? 0xA5 : test.x;
    cpu.state().y = test.source == 'Y' ? 0xA5 : test.y;
    const auto cycles = step(cpu);
    expect(memory.bytes[test.target] == 0xA5, test.name);
    expect(cycles == test.cycles, std::string(test.name) + " cycle count");
}

void stores_cover_every_addressing_mode() {
    constexpr std::array cases{
        StoreCase{0x85, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'A', "STA zero page"},
        StoreCase{0x95, 0x3C, 0x00, 4, 0, 0x0040, 4, Mode::direct, 'A', "STA zero page X"},
        StoreCase{0x8D, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'A', "STA absolute"},
        StoreCase{0x9D, 0xFC, 0x8F, 4, 0, 0x9000, 5, Mode::direct, 'A', "STA absolute X"},
        StoreCase{0x99, 0xFB, 0x8F, 0, 5, 0x9000, 5, Mode::direct, 'A', "STA absolute Y"},
        StoreCase{0x81, 0x20, 0x00, 4, 0, 0x9000, 6, Mode::indexed_indirect, 'A', "STA indexed indirect"},
        StoreCase{0x91, 0x20, 0x00, 0, 5, 0x9000, 6, Mode::indirect_indexed, 'A', "STA indirect indexed"},
        StoreCase{0x86, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'X', "STX zero page"},
        StoreCase{0x96, 0x3C, 0x00, 0, 4, 0x0040, 4, Mode::direct, 'X', "STX zero page Y"},
        StoreCase{0x8E, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'X', "STX absolute"},
        StoreCase{0x84, 0x40, 0x00, 0, 0, 0x0040, 3, Mode::direct, 'Y', "STY zero page"},
        StoreCase{0x94, 0x3C, 0x00, 4, 0, 0x0040, 4, Mode::direct, 'Y', "STY zero page X"},
        StoreCase{0x8C, 0x00, 0x90, 0, 0, 0x9000, 4, Mode::direct, 'Y', "STY absolute"},
    };
    for (const auto& test : cases) {
        verify_store(test);
    }
}

}  // namespace

int run_cpu_addressing_tests() {
    const auto before = nes::test::failures;
    loads_cover_every_addressing_mode();
    stores_cover_every_addressing_mode();
    return nes::test::failures - before;
}
