#include "nes/cpu.hpp"

namespace nes {

Cpu::Cpu(CpuBusDevice& bus) : bus_(bus) {}

void Cpu::reset() {
    state_.a = 0;
    state_.x = 0;
    state_.y = 0;
    state_.stack_pointer = 0xFD;
    state_.status = unused | interrupt_disable;
    state_.program_counter = read_word(0xFFFC);
    remaining_cycles_ = 8;
}

void Cpu::irq() {
    if (!flag(interrupt_disable)) {
        service_interrupt(0xFFFE, 7);
    }
}

void Cpu::nmi() {
    service_interrupt(0xFFFA, 8);
}

void Cpu::clock() {
    if (remaining_cycles_ == 0) {
        // Instruction decoding is introduced in the next changeset.
        remaining_cycles_ = 2;
    }
    --remaining_cycles_;
    ++state_.cycles;
}

bool Cpu::instruction_complete() const noexcept {
    return remaining_cycles_ == 0;
}

const Cpu::State& Cpu::state() const noexcept {
    return state_;
}

Cpu::State& Cpu::state() noexcept {
    return state_;
}

bool Cpu::flag(Flag requested) const noexcept {
    return (state_.status & requested) != 0;
}

void Cpu::set_flag(Flag requested, bool value) noexcept {
    if (value) {
        state_.status |= requested;
    } else {
        state_.status &= static_cast<std::uint8_t>(~requested);
    }
}

std::uint8_t Cpu::read(std::uint16_t address) {
    return bus_.cpu_read(address);
}

void Cpu::write(std::uint16_t address, std::uint8_t value) {
    bus_.cpu_write(address, value);
}

std::uint16_t Cpu::read_word(std::uint16_t address) {
    const auto low = static_cast<std::uint16_t>(read(address));
    const auto high = static_cast<std::uint16_t>(read(static_cast<std::uint16_t>(address + 1U)));
    return static_cast<std::uint16_t>((high << 8U) | low);
}

void Cpu::push(std::uint8_t value) {
    write(static_cast<std::uint16_t>(0x0100U | state_.stack_pointer), value);
    --state_.stack_pointer;
}

std::uint8_t Cpu::pop() {
    ++state_.stack_pointer;
    return read(static_cast<std::uint16_t>(0x0100U | state_.stack_pointer));
}

void Cpu::service_interrupt(std::uint16_t vector, std::uint8_t cycles) {
    push(static_cast<std::uint8_t>((state_.program_counter >> 8U) & 0xFFU));
    push(static_cast<std::uint8_t>(state_.program_counter & 0xFFU));
    set_flag(break_command, false);
    set_flag(unused, true);
    set_flag(interrupt_disable, true);
    push(state_.status);
    state_.program_counter = read_word(vector);
    remaining_cycles_ = cycles;
}

}  // namespace nes
