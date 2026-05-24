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
        opcode_ = read(state_.program_counter++);
        const auto& instruction = instruction_table()[opcode_];
        remaining_cycles_ = instruction.cycles;
        resolve_address(instruction.mode);
        execute(instruction.operation);
        if (instruction.page_cycle && page_crossed_) {
            ++remaining_cycles_;
        }
        set_flag(unused, true);
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

const std::array<Cpu::Instruction, 256>& Cpu::instruction_table() {
    static const auto table = [] {
        std::array<Instruction, 256> result{};
        result.fill({"???", Operation::illegal, AddressMode::implied, 2, false});
        const auto set = [&result](std::uint8_t opcode, const char* name, Operation operation,
                                   AddressMode mode, std::uint8_t cycles,
                                   bool page_cycle = false) {
            result[opcode] = {name, operation, mode, cycles, page_cycle};
        };

        using M = AddressMode;
        using O = Operation;

        set(0x00, "BRK", O::brk, M::implied, 7); set(0x01, "ORA", O::ora, M::indexed_indirect, 6);
        set(0x05, "ORA", O::ora, M::zero_page, 3); set(0x06, "ASL", O::asl, M::zero_page, 5);
        set(0x08, "PHP", O::php, M::implied, 3); set(0x09, "ORA", O::ora, M::immediate, 2);
        set(0x0A, "ASL", O::asl, M::accumulator, 2); set(0x0D, "ORA", O::ora, M::absolute, 4);
        set(0x0E, "ASL", O::asl, M::absolute, 6); set(0x10, "BPL", O::bpl, M::relative, 2);
        set(0x11, "ORA", O::ora, M::indirect_indexed, 5, true); set(0x15, "ORA", O::ora, M::zero_page_x, 4);
        set(0x16, "ASL", O::asl, M::zero_page_x, 6); set(0x18, "CLC", O::clc, M::implied, 2);
        set(0x19, "ORA", O::ora, M::absolute_y, 4, true); set(0x1D, "ORA", O::ora, M::absolute_x, 4, true);
        set(0x1E, "ASL", O::asl, M::absolute_x, 7); set(0x20, "JSR", O::jsr, M::absolute, 6);
        set(0x21, "AND", O::and_, M::indexed_indirect, 6); set(0x24, "BIT", O::bit, M::zero_page, 3);
        set(0x25, "AND", O::and_, M::zero_page, 3); set(0x26, "ROL", O::rol, M::zero_page, 5);
        set(0x28, "PLP", O::plp, M::implied, 4); set(0x29, "AND", O::and_, M::immediate, 2);
        set(0x2A, "ROL", O::rol, M::accumulator, 2); set(0x2C, "BIT", O::bit, M::absolute, 4);
        set(0x2D, "AND", O::and_, M::absolute, 4); set(0x2E, "ROL", O::rol, M::absolute, 6);
        set(0x30, "BMI", O::bmi, M::relative, 2); set(0x31, "AND", O::and_, M::indirect_indexed, 5, true);
        set(0x35, "AND", O::and_, M::zero_page_x, 4); set(0x36, "ROL", O::rol, M::zero_page_x, 6);
        set(0x38, "SEC", O::sec, M::implied, 2); set(0x39, "AND", O::and_, M::absolute_y, 4, true);
        set(0x3D, "AND", O::and_, M::absolute_x, 4, true); set(0x3E, "ROL", O::rol, M::absolute_x, 7);
        set(0x40, "RTI", O::rti, M::implied, 6); set(0x41, "EOR", O::eor_, M::indexed_indirect, 6);
        set(0x45, "EOR", O::eor_, M::zero_page, 3); set(0x46, "LSR", O::lsr, M::zero_page, 5);
        set(0x48, "PHA", O::pha, M::implied, 3); set(0x49, "EOR", O::eor_, M::immediate, 2);
        set(0x4A, "LSR", O::lsr, M::accumulator, 2); set(0x4C, "JMP", O::jmp, M::absolute, 3);
        set(0x4D, "EOR", O::eor_, M::absolute, 4); set(0x4E, "LSR", O::lsr, M::absolute, 6);
        set(0x50, "BVC", O::bvc, M::relative, 2); set(0x51, "EOR", O::eor_, M::indirect_indexed, 5, true);
        set(0x55, "EOR", O::eor_, M::zero_page_x, 4); set(0x56, "LSR", O::lsr, M::zero_page_x, 6);
        set(0x58, "CLI", O::cli, M::implied, 2); set(0x59, "EOR", O::eor_, M::absolute_y, 4, true);
        set(0x5D, "EOR", O::eor_, M::absolute_x, 4, true); set(0x5E, "LSR", O::lsr, M::absolute_x, 7);
        set(0x60, "RTS", O::rts, M::implied, 6); set(0x61, "ADC", O::adc, M::indexed_indirect, 6);
        set(0x65, "ADC", O::adc, M::zero_page, 3); set(0x66, "ROR", O::ror, M::zero_page, 5);
        set(0x68, "PLA", O::pla, M::implied, 4); set(0x69, "ADC", O::adc, M::immediate, 2);
        set(0x6A, "ROR", O::ror, M::accumulator, 2); set(0x6C, "JMP", O::jmp, M::indirect, 5);
        set(0x6D, "ADC", O::adc, M::absolute, 4); set(0x6E, "ROR", O::ror, M::absolute, 6);
        set(0x70, "BVS", O::bvs, M::relative, 2); set(0x71, "ADC", O::adc, M::indirect_indexed, 5, true);
        set(0x75, "ADC", O::adc, M::zero_page_x, 4); set(0x76, "ROR", O::ror, M::zero_page_x, 6);
        set(0x78, "SEI", O::sei, M::implied, 2); set(0x79, "ADC", O::adc, M::absolute_y, 4, true);
        set(0x7D, "ADC", O::adc, M::absolute_x, 4, true); set(0x7E, "ROR", O::ror, M::absolute_x, 7);
        set(0x81, "STA", O::sta, M::indexed_indirect, 6); set(0x84, "STY", O::sty, M::zero_page, 3);
        set(0x85, "STA", O::sta, M::zero_page, 3); set(0x86, "STX", O::stx, M::zero_page, 3);
        set(0x88, "DEY", O::dey, M::implied, 2); set(0x8A, "TXA", O::txa, M::implied, 2);
        set(0x8C, "STY", O::sty, M::absolute, 4); set(0x8D, "STA", O::sta, M::absolute, 4);
        set(0x8E, "STX", O::stx, M::absolute, 4); set(0x90, "BCC", O::bcc, M::relative, 2);
        set(0x91, "STA", O::sta, M::indirect_indexed, 6); set(0x94, "STY", O::sty, M::zero_page_x, 4);
        set(0x95, "STA", O::sta, M::zero_page_x, 4); set(0x96, "STX", O::stx, M::zero_page_y, 4);
        set(0x98, "TYA", O::tya, M::implied, 2); set(0x99, "STA", O::sta, M::absolute_y, 5);
        set(0x9A, "TXS", O::txs, M::implied, 2); set(0x9D, "STA", O::sta, M::absolute_x, 5);
        set(0xA0, "LDY", O::ldy, M::immediate, 2); set(0xA1, "LDA", O::lda, M::indexed_indirect, 6);
        set(0xA2, "LDX", O::ldx, M::immediate, 2); set(0xA4, "LDY", O::ldy, M::zero_page, 3);
        set(0xA5, "LDA", O::lda, M::zero_page, 3); set(0xA6, "LDX", O::ldx, M::zero_page, 3);
        set(0xA8, "TAY", O::tay, M::implied, 2); set(0xA9, "LDA", O::lda, M::immediate, 2);
        set(0xAA, "TAX", O::tax, M::implied, 2); set(0xAC, "LDY", O::ldy, M::absolute, 4);
        set(0xAD, "LDA", O::lda, M::absolute, 4); set(0xAE, "LDX", O::ldx, M::absolute, 4);
        set(0xB0, "BCS", O::bcs, M::relative, 2); set(0xB1, "LDA", O::lda, M::indirect_indexed, 5, true);
        set(0xB4, "LDY", O::ldy, M::zero_page_x, 4); set(0xB5, "LDA", O::lda, M::zero_page_x, 4);
        set(0xB6, "LDX", O::ldx, M::zero_page_y, 4); set(0xB8, "CLV", O::clv, M::implied, 2);
        set(0xB9, "LDA", O::lda, M::absolute_y, 4, true); set(0xBA, "TSX", O::tsx, M::implied, 2);
        set(0xBC, "LDY", O::ldy, M::absolute_x, 4, true); set(0xBD, "LDA", O::lda, M::absolute_x, 4, true);
        set(0xBE, "LDX", O::ldx, M::absolute_y, 4, true); set(0xC0, "CPY", O::cpy, M::immediate, 2);
        set(0xC1, "CMP", O::cmp, M::indexed_indirect, 6); set(0xC4, "CPY", O::cpy, M::zero_page, 3);
        set(0xC5, "CMP", O::cmp, M::zero_page, 3); set(0xC6, "DEC", O::dec, M::zero_page, 5);
        set(0xC8, "INY", O::iny, M::implied, 2); set(0xC9, "CMP", O::cmp, M::immediate, 2);
        set(0xCA, "DEX", O::dex, M::implied, 2); set(0xCC, "CPY", O::cpy, M::absolute, 4);
        set(0xCD, "CMP", O::cmp, M::absolute, 4); set(0xCE, "DEC", O::dec, M::absolute, 6);
        set(0xD0, "BNE", O::bne, M::relative, 2); set(0xD1, "CMP", O::cmp, M::indirect_indexed, 5, true);
        set(0xD5, "CMP", O::cmp, M::zero_page_x, 4); set(0xD6, "DEC", O::dec, M::zero_page_x, 6);
        set(0xD8, "CLD", O::cld, M::implied, 2); set(0xD9, "CMP", O::cmp, M::absolute_y, 4, true);
        set(0xDD, "CMP", O::cmp, M::absolute_x, 4, true); set(0xDE, "DEC", O::dec, M::absolute_x, 7);
        set(0xE0, "CPX", O::cpx, M::immediate, 2); set(0xE1, "SBC", O::sbc, M::indexed_indirect, 6);
        set(0xE4, "CPX", O::cpx, M::zero_page, 3); set(0xE5, "SBC", O::sbc, M::zero_page, 3);
        set(0xE6, "INC", O::inc, M::zero_page, 5); set(0xE8, "INX", O::inx, M::implied, 2);
        set(0xE9, "SBC", O::sbc, M::immediate, 2); set(0xEA, "NOP", O::nop, M::implied, 2);
        set(0xEC, "CPX", O::cpx, M::absolute, 4); set(0xED, "SBC", O::sbc, M::absolute, 4);
        set(0xEE, "INC", O::inc, M::absolute, 6); set(0xF0, "BEQ", O::beq, M::relative, 2);
        set(0xF1, "SBC", O::sbc, M::indirect_indexed, 5, true); set(0xF5, "SBC", O::sbc, M::zero_page_x, 4);
        set(0xF6, "INC", O::inc, M::zero_page_x, 6); set(0xF8, "SED", O::sed, M::implied, 2);
        set(0xF9, "SBC", O::sbc, M::absolute_y, 4, true); set(0xFD, "SBC", O::sbc, M::absolute_x, 4, true);
        set(0xFE, "INC", O::inc, M::absolute_x, 7);

        // Common unofficial NOP encodings are consumed with their real operand lengths.
        for (const auto opcode : {0x1A, 0x3A, 0x5A, 0x7A, 0xDA, 0xFA}) {
            set(static_cast<std::uint8_t>(opcode), "NOP", O::nop, M::implied, 2);
        }
        for (const auto opcode : {0x80, 0x82, 0x89, 0xC2, 0xE2}) {
            set(static_cast<std::uint8_t>(opcode), "NOP", O::nop, M::immediate, 2);
        }

        return result;
    }();
    return table;
}

void Cpu::resolve_address(AddressMode mode) {
    page_crossed_ = false;
    switch (mode) {
    case AddressMode::implied:
    case AddressMode::accumulator:
        break;
    case AddressMode::immediate:
        address_ = state_.program_counter++;
        break;
    case AddressMode::zero_page:
        address_ = read(state_.program_counter++);
        break;
    case AddressMode::zero_page_x:
        address_ = static_cast<std::uint8_t>(read(state_.program_counter++) + state_.x);
        break;
    case AddressMode::zero_page_y:
        address_ = static_cast<std::uint8_t>(read(state_.program_counter++) + state_.y);
        break;
    case AddressMode::relative:
        relative_ = static_cast<std::int8_t>(read(state_.program_counter++));
        break;
    case AddressMode::absolute:
        address_ = read_word(state_.program_counter);
        state_.program_counter = static_cast<std::uint16_t>(state_.program_counter + 2U);
        break;
    case AddressMode::absolute_x:
    case AddressMode::absolute_y: {
        const auto base = read_word(state_.program_counter);
        state_.program_counter = static_cast<std::uint16_t>(state_.program_counter + 2U);
        const auto offset = mode == AddressMode::absolute_x ? state_.x : state_.y;
        address_ = static_cast<std::uint16_t>(base + offset);
        page_crossed_ = (base & 0xFF00U) != (address_ & 0xFF00U);
        break;
    }
    case AddressMode::indirect: {
        const auto pointer = read_word(state_.program_counter);
        state_.program_counter = static_cast<std::uint16_t>(state_.program_counter + 2U);
        const auto low = read(pointer);
        const auto high_address = static_cast<std::uint16_t>(
            (pointer & 0xFF00U) | static_cast<std::uint8_t>(pointer + 1U));
        address_ = static_cast<std::uint16_t>((static_cast<std::uint16_t>(read(high_address)) << 8U) | low);
        break;
    }
    case AddressMode::indexed_indirect: {
        const auto pointer = static_cast<std::uint8_t>(read(state_.program_counter++) + state_.x);
        const auto low = read(pointer);
        const auto high = read(static_cast<std::uint8_t>(pointer + 1U));
        address_ = static_cast<std::uint16_t>((static_cast<std::uint16_t>(high) << 8U) | low);
        break;
    }
    case AddressMode::indirect_indexed: {
        const auto pointer = read(state_.program_counter++);
        const auto low = read(pointer);
        const auto high = read(static_cast<std::uint8_t>(pointer + 1U));
        const auto base = static_cast<std::uint16_t>((static_cast<std::uint16_t>(high) << 8U) | low);
        address_ = static_cast<std::uint16_t>(base + state_.y);
        page_crossed_ = (base & 0xFF00U) != (address_ & 0xFF00U);
        break;
    }
    }
}

void Cpu::execute(Operation) {
    // Operations are added in focused, independently tested changesets.
}

}  // namespace nes
