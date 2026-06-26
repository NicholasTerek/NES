#include "test_harness.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

using nes::Cpu;
using nes::test::FlatMemory;
using nes::test::expect;
using nes::test::reset;
using nes::test::step;

struct BranchCase {
    std::uint8_t opcode;
    Cpu::Flag flag;
    bool taken_when_set;
};

void branches_cover_all_conditions_and_cycle_paths() {
    constexpr std::array cases{
        BranchCase{0x90, Cpu::carry, false}, BranchCase{0xB0, Cpu::carry, true},
        BranchCase{0xF0, Cpu::zero, true}, BranchCase{0x30, Cpu::negative, true},
        BranchCase{0xD0, Cpu::zero, false}, BranchCase{0x10, Cpu::negative, false},
        BranchCase{0x50, Cpu::overflow, false}, BranchCase{0x70, Cpu::overflow, true},
    };

    for (const auto& test : cases) {
        FlatMemory memory;
        memory.set_reset_vector(0x8000);
        memory.bytes[0x8000] = test.opcode;
        memory.bytes[0x8001] = 0x05;
        Cpu cpu(memory);
        reset(cpu);
        cpu.set_flag(test.flag, test.taken_when_set);
        expect(step(cpu) == 3, "taken branch costs one extra cycle");
        expect(cpu.state().program_counter == 0x8007, "taken branch applies its offset");

        cpu.state().program_counter = 0x8000;
        cpu.set_flag(test.flag, !test.taken_when_set);
        expect(step(cpu) == 2, "untaken branch keeps its base cycle count");
        expect(cpu.state().program_counter == 0x8002, "untaken branch continues in sequence");
    }

    FlatMemory memory;
    memory.set_reset_vector(0x80FD);
    memory.bytes[0x80FD] = 0xD0;
    memory.bytes[0x80FE] = 0x01;
    Cpu cpu(memory);
    reset(cpu);
    cpu.set_flag(Cpu::zero, false);
    expect(step(cpu) == 4, "branch crossing a page costs two extra cycles");
    expect(cpu.state().program_counter == 0x8100, "branch crosses into the next page");
}

void stack_operations_preserve_values_and_status_bits() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    const std::array program{
        std::uint8_t{0x48}, std::uint8_t{0x08}, std::uint8_t{0x68}, std::uint8_t{0x28},
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);
    Cpu cpu(memory);
    reset(cpu);
    cpu.state().a = 0x80;
    cpu.state().status = Cpu::carry | Cpu::unused;

    expect(step(cpu) == 3, "PHA cycle count");
    expect(memory.bytes[0x01FD] == 0x80, "PHA stores the accumulator on the stack");
    expect(step(cpu) == 3, "PHP cycle count");
    expect((memory.bytes[0x01FC] & (Cpu::break_command | Cpu::unused)) ==
               (Cpu::break_command | Cpu::unused),
           "PHP marks break and unused in the stacked status");

    cpu.state().a = 0;
    expect(step(cpu) == 4, "PLA cycle count");
    expect(cpu.state().a == static_cast<std::uint8_t>(Cpu::carry | Cpu::break_command |
                                                      Cpu::unused),
           "PLA removes the most recent stack value");
    expect(step(cpu) == 4, "PLP cycle count");
    expect(cpu.flag(Cpu::unused) && !cpu.flag(Cpu::break_command),
           "PLP normalizes the internal status bits");
}

void subroutines_store_the_exact_return_address() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0x8000] = 0x20;
    memory.bytes[0x8001] = 0x00;
    memory.bytes[0x8002] = 0x90;
    memory.bytes[0x9000] = 0x60;
    Cpu cpu(memory);
    reset(cpu);

    expect(step(cpu) == 6, "JSR cycle count");
    expect(memory.bytes[0x01FD] == 0x80 && memory.bytes[0x01FC] == 0x02,
           "JSR pushes the address of its final operand byte");
    expect(cpu.state().program_counter == 0x9000, "JSR loads its target");
    expect(step(cpu) == 6, "RTS cycle count");
    expect(cpu.state().program_counter == 0x8003, "RTS resumes after the call");
}

void interrupts_push_hardware_accurate_frames() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0xFFFE] = 0x00;
    memory.bytes[0xFFFF] = 0x90;
    memory.bytes[0xFFFA] = 0x00;
    memory.bytes[0xFFFB] = 0xA0;
    Cpu cpu(memory);
    reset(cpu);

    const auto masked_state = cpu.state();
    cpu.irq();
    expect(cpu.state().program_counter == masked_state.program_counter &&
               cpu.state().stack_pointer == masked_state.stack_pointer,
           "masked IRQ does not alter CPU state");

    cpu.set_flag(Cpu::interrupt_disable, false);
    cpu.state().program_counter = 0x4567;
    const auto irq_cycles = cpu.state().cycles;
    cpu.irq();
    nes::test::drain(cpu);
    expect(cpu.state().cycles - irq_cycles == 7, "IRQ consumes seven cycles");
    expect(cpu.state().program_counter == 0x9000, "IRQ loads its vector");
    expect(memory.bytes[0x01FD] == 0x45 && memory.bytes[0x01FC] == 0x67,
           "IRQ pushes the interrupted program counter");
    expect((memory.bytes[0x01FB] & Cpu::break_command) == 0,
           "hardware IRQ clears break in the stacked status");

    cpu.state().program_counter = 0x2345;
    cpu.state().stack_pointer = 0xFD;
    const auto nmi_cycles = cpu.state().cycles;
    cpu.nmi();
    nes::test::drain(cpu);
    expect(cpu.state().cycles - nmi_cycles == 8, "NMI consumes eight cycles");
    expect(cpu.state().program_counter == 0xA000, "NMI loads its vector regardless of IRQ mask");
}

void irq_polling_uses_the_pre_instruction_interrupt_mask() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0xFFFE] = 0x00;
    memory.bytes[0xFFFF] = 0x90;
    memory.bytes[0x8000] = 0x58;  // CLI
    memory.bytes[0x8001] = 0xEA;  // NOP
    Cpu cpu(memory);
    reset(cpu);

    step(cpu);
    cpu.poll_irq();
    expect(cpu.state().program_counter == 0x8001,
           "CLI delays recognition of a pending IRQ by one instruction");
    step(cpu);
    cpu.poll_irq();
    expect(cpu.state().program_counter == 0x9000,
           "IRQ is recognized after the instruction following CLI");
    expect((memory.bytes[0x01FB] & Cpu::interrupt_disable) == 0,
           "IRQ pushes the interrupt mask as sampled before service");
}

void rti_polls_the_restored_interrupt_mask_without_cli_latency() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0xFFFE] = 0x00;
    memory.bytes[0xFFFF] = 0x90;
    memory.bytes[0x8000] = 0x40;  // RTI
    Cpu cpu(memory);
    reset(cpu);
    cpu.state().stack_pointer = 0xFA;
    memory.bytes[0x01FB] = static_cast<std::uint8_t>(Cpu::unused | Cpu::interrupt_disable);
    memory.bytes[0x01FC] = 0x34;
    memory.bytes[0x01FD] = 0x12;

    step(cpu);
    cpu.poll_irq();
    expect(cpu.state().program_counter == 0x1234,
           "RTI immediately polls the restored interrupt mask");
}

void break_and_return_restore_the_program_counter() {
    FlatMemory memory;
    memory.set_reset_vector(0x8000);
    memory.bytes[0xFFFE] = 0x00;
    memory.bytes[0xFFFF] = 0x9000 >> 8U;
    memory.bytes[0x8000] = 0x00;
    memory.bytes[0x8001] = 0xEA;
    memory.bytes[0x9000] = 0x40;
    Cpu cpu(memory);
    reset(cpu);

    expect(step(cpu) == 7, "BRK cycle count");
    expect(memory.bytes[0x01FD] == 0x80 && memory.bytes[0x01FC] == 0x02,
           "BRK pushes the address after its padding byte");
    expect((memory.bytes[0x01FB] & (Cpu::break_command | Cpu::unused)) ==
               (Cpu::break_command | Cpu::unused),
           "BRK marks its stacked status frame");
    expect(step(cpu) == 6, "RTI cycle count");
    expect(cpu.state().program_counter == 0x8002, "RTI restores the interrupted address");
}

}  // namespace

int run_cpu_control_tests() {
    const auto before = nes::test::failures;
    branches_cover_all_conditions_and_cycle_paths();
    stack_operations_preserve_values_and_status_bits();
    subroutines_store_the_exact_return_address();
    interrupts_push_hardware_accurate_frames();
    irq_polling_uses_the_pre_instruction_interrupt_mask();
    rti_polls_the_restored_interrupt_mask_without_cli_latency();
    break_and_return_restore_the_program_counter();
    return nes::test::failures - before;
}
