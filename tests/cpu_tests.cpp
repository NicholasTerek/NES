#include "nes/cpu.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

class FlatMemory final : public nes::CpuBusDevice {
public:
    std::array<std::uint8_t, 65'536> bytes{};

    std::uint8_t cpu_read(std::uint16_t address, bool) override {
        return bytes[address];
    }

    void cpu_write(std::uint16_t address, std::uint8_t value) override {
        bytes[address] = value;
    }
};

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void drain(nes::Cpu& cpu) {
    do {
        cpu.clock();
    } while (!cpu.instruction_complete());
}

void reset_uses_vector() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x34;
    memory.bytes[0xFFFD] = 0x12;
    nes::Cpu cpu(memory);

    cpu.reset();
    expect(cpu.state().program_counter == 0x1234, "reset reads the reset vector");
    expect(cpu.state().stack_pointer == 0xFD, "reset initializes the stack pointer");
    expect(cpu.flag(nes::Cpu::interrupt_disable), "reset masks IRQs");
}

void nmi_pushes_state_and_loads_vector() {
    FlatMemory memory;
    memory.bytes[0xFFFA] = 0x00;
    memory.bytes[0xFFFB] = 0x80;
    nes::Cpu cpu(memory);
    cpu.state().program_counter = 0x4567;
    cpu.state().stack_pointer = 0xFD;

    cpu.nmi();
    expect(cpu.state().program_counter == 0x8000, "NMI loads its vector");
    expect(memory.bytes[0x01FD] == 0x45, "NMI pushes PC high byte");
    expect(memory.bytes[0x01FC] == 0x67, "NMI pushes PC low byte");
}

void loads_stores_and_transfers_data() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x42},  // LDA #$42
        std::uint8_t{0x85}, std::uint8_t{0x10},  // STA $10
        std::uint8_t{0xA2}, std::uint8_t{0x7F},  // LDX #$7F
        std::uint8_t{0xE8},                      // INX
        std::uint8_t{0x8A},                      // TXA
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0x42, "LDA loads the accumulator");
    drain(cpu);
    expect(memory.bytes[0x0010] == 0x42, "STA writes through the bus");
    drain(cpu);
    drain(cpu);
    expect(cpu.state().x == 0x80, "INX wraps and updates X");
    expect(cpu.flag(nes::Cpu::negative), "INX updates the negative flag");
    drain(cpu);
    expect(cpu.state().a == 0x80, "TXA transfers X into A");
}

void arithmetic_sets_6502_flags() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x50},  // LDA #$50
        std::uint8_t{0x69}, std::uint8_t{0x50},  // ADC #$50
        std::uint8_t{0xE9}, std::uint8_t{0x01},  // SBC #$01
        std::uint8_t{0xC9}, std::uint8_t{0x9F},  // CMP #$9F
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0xA0, "ADC stores the low byte of the sum");
    expect(cpu.flag(nes::Cpu::overflow), "ADC detects signed overflow");
    expect(cpu.flag(nes::Cpu::negative), "ADC updates the negative flag");
    expect(!cpu.flag(nes::Cpu::carry), "ADC leaves carry clear below 256");

    cpu.set_flag(nes::Cpu::carry, true);
    drain(cpu);
    expect(cpu.state().a == 0x9F, "SBC uses an inverted borrow");
    expect(cpu.flag(nes::Cpu::carry), "SBC sets carry when no borrow occurs");
    drain(cpu);
    expect(cpu.flag(nes::Cpu::zero), "CMP sets zero for equal values");
}

void logic_operations_update_accumulator() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0xF0},  // LDA #$F0
        std::uint8_t{0x29}, std::uint8_t{0x3C},  // AND #$3C -> $30
        std::uint8_t{0x49}, std::uint8_t{0x0F},  // EOR #$0F -> $3F
        std::uint8_t{0x09}, std::uint8_t{0x80},  // ORA #$80 -> $BF
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0xBF, "AND, EOR, and ORA compose correctly");
    expect(cpu.flag(nes::Cpu::negative), "logic operations update flags");
}

void shifts_work_on_registers_and_memory() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    memory.bytes[0x0020] = 0x81;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x81},  // LDA #$81
        std::uint8_t{0x0A},                      // ASL A -> $02, C=1
        std::uint8_t{0x6A},                      // ROR A -> $81, C=0
        std::uint8_t{0x46}, std::uint8_t{0x20},  // LSR $20 -> $40, C=1
        std::uint8_t{0x26}, std::uint8_t{0x20},  // ROL $20 -> $81
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0x02, "ASL shifts the accumulator");
    expect(cpu.flag(nes::Cpu::carry), "ASL captures the high bit");
    drain(cpu);
    expect(cpu.state().a == 0x81, "ROR rotates carry into bit seven");
    drain(cpu);
    expect(memory.bytes[0x0020] == 0x40, "LSR writes shifted memory");
    drain(cpu);
    expect(memory.bytes[0x0020] == 0x81, "ROL rotates through carry");
}

}  // namespace

int main() {
    reset_uses_vector();
    nmi_pushes_state_and_loads_vector();
    loads_stores_and_transfers_data();
    arithmetic_sets_6502_flags();
    logic_operations_update_accumulator();
    shifts_work_on_registers_and_memory();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CPU tests passed\n";
    return EXIT_SUCCESS;
}
