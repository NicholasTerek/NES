#include "nes/cpu.hpp"
#include "test_harness.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace {

using nes::test::FlatMemory;
using nes::test::drain;
using nes::test::expect;
using nes::test::failures;

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

void branches_follow_status_flags() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x00},  // LDA #$00, Z=1
        std::uint8_t{0xF0}, std::uint8_t{0x02},  // BEQ +2
        std::uint8_t{0xA9}, std::uint8_t{0xFF},  // skipped
        std::uint8_t{0xA9}, std::uint8_t{0x01},  // LDA #$01
        std::uint8_t{0xD0}, std::uint8_t{0xFC},  // BNE -4
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().program_counter == 0x8006, "BEQ takes a positive relative offset");
    drain(cpu);
    expect(cpu.state().a == 0x01, "taken branch skips intervening instructions");
    drain(cpu);
    expect(cpu.state().program_counter == 0x8006, "BNE sign-extends a negative offset");
}

void flag_instructions_set_and_clear_bits() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0x38},  // SEC
        std::uint8_t{0x18},  // CLC
        std::uint8_t{0xF8},  // SED
        std::uint8_t{0xD8},  // CLD
        std::uint8_t{0x58},  // CLI
        std::uint8_t{0x78},  // SEI
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    expect(cpu.flag(nes::Cpu::carry), "SEC sets carry");
    drain(cpu);
    expect(!cpu.flag(nes::Cpu::carry), "CLC clears carry");
    drain(cpu);
    expect(cpu.flag(nes::Cpu::decimal), "SED sets decimal");
    drain(cpu);
    expect(!cpu.flag(nes::Cpu::decimal), "CLD clears decimal");
    drain(cpu);
    expect(!cpu.flag(nes::Cpu::interrupt_disable), "CLI clears interrupt mask");
    drain(cpu);
    expect(cpu.flag(nes::Cpu::interrupt_disable), "SEI sets interrupt mask");
}

void subroutines_preserve_return_address() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0x20}, std::uint8_t{0x06}, std::uint8_t{0x80},  // JSR $8006
        std::uint8_t{0xA9}, std::uint8_t{0x42},                    // LDA #$42
        std::uint8_t{0xEA},                                        // NOP
        std::uint8_t{0xA9}, std::uint8_t{0x99},                    // LDA #$99
        std::uint8_t{0x60},                                        // RTS
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    expect(cpu.state().program_counter == 0x8006, "JSR enters the subroutine");
    drain(cpu);
    expect(cpu.state().a == 0x99, "subroutine instructions execute");
    drain(cpu);
    expect(cpu.state().program_counter == 0x8003, "RTS resumes after the JSR operand");
    drain(cpu);
    expect(cpu.state().a == 0x42, "execution resumes at the caller");
}

void stack_and_interrupt_returns_restore_state() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    memory.bytes[0xFFFE] = 0x00;
    memory.bytes[0xFFFF] = 0x90;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x5A},  // LDA #$5A
        std::uint8_t{0x48},                      // PHA
        std::uint8_t{0xA9}, std::uint8_t{0x00},  // LDA #$00
        std::uint8_t{0x68},                      // PLA
        std::uint8_t{0x00},                      // BRK
        std::uint8_t{0xEA},                      // padding
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);
    memory.bytes[0x9000] = 0x40;  // RTI

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0x5A, "PHA and PLA round-trip the accumulator");
    drain(cpu);
    expect(cpu.state().program_counter == 0x9000, "BRK loads the IRQ vector");
    drain(cpu);
    expect(cpu.state().program_counter == 0x8008, "RTI resumes after BRK padding");
}

void indexed_addressing_wraps_like_hardware() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    memory.bytes[0x0000] = 0x00;
    memory.bytes[0x0001] = 0x90;
    memory.bytes[0x9000] = 0x6C;
    memory.bytes[0x1303] = 0xAB;
    const std::array program{
        std::uint8_t{0xA2}, std::uint8_t{0x04},                    // LDX #$04
        std::uint8_t{0xA1}, std::uint8_t{0xFC},                    // LDA ($FC,X)
        std::uint8_t{0xBD}, std::uint8_t{0xFF}, std::uint8_t{0x12},  // LDA $12FF,X
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0x6C, "indexed-indirect pointers wrap in zero page");
    drain(cpu);
    expect(cpu.state().a == 0xAB, "absolute indexed addressing crosses pages");
}

void indirect_jump_reproduces_page_boundary_bug() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    memory.bytes[0x8000] = 0x6C;  // JMP ($12FF)
    memory.bytes[0x8001] = 0xFF;
    memory.bytes[0x8002] = 0x12;
    memory.bytes[0x12FF] = 0x34;
    memory.bytes[0x1200] = 0x56;
    memory.bytes[0x1300] = 0x99;

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    expect(cpu.state().program_counter == 0x5634,
           "indirect JMP wraps the high-byte read within the page");
}

}  // namespace

int run_cpu_addressing_tests();

int main() {
    reset_uses_vector();
    nmi_pushes_state_and_loads_vector();
    loads_stores_and_transfers_data();
    arithmetic_sets_6502_flags();
    logic_operations_update_accumulator();
    shifts_work_on_registers_and_memory();
    branches_follow_status_flags();
    flag_instructions_set_and_clear_bits();
    subroutines_preserve_return_address();
    stack_and_interrupt_returns_restore_state();
    indexed_addressing_wraps_like_hardware();
    indirect_jump_reproduces_page_boundary_bug();
    static_cast<void>(run_cpu_addressing_tests());

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CPU tests passed\n";
    return EXIT_SUCCESS;
}
