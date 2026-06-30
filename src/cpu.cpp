#include "nes/cpu.hpp"

namespace nes {

Cpu::Cpu(CpuBusDevice& bus) : bus_(bus) {}

void Cpu::reset() {
    if (!powered_) {
        state_.a = 0;
        state_.x = 0;
        state_.y = 0;
        state_.stack_pointer = 0xFD;
        state_.status = unused | interrupt_disable;
        powered_ = true;
    } else {
        state_.stack_pointer = static_cast<std::uint8_t>(state_.stack_pointer - 3U);
        set_flag(interrupt_disable, true);
        set_flag(unused, true);
    }
    state_.program_counter = read_word(0xFFFC);
    remaining_cycles_ = 8;
    interrupt_disable_sampled_ = true;
}

void Cpu::irq() {
    if (!flag(interrupt_disable)) {
        service_interrupt(0xFFFE, 7);
    }
}

void Cpu::poll_irq() {
    if (!interrupt_disable_sampled_) {
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
        current_mode_ = instruction.mode;
        interrupt_disable_sampled_ = flag(interrupt_disable);
        resolve_address(instruction.mode);
        const bool indexed_store =
            (instruction.operation == Operation::sta ||
             instruction.operation == Operation::ahx ||
             instruction.operation == Operation::shx ||
             instruction.operation == Operation::shy ||
             instruction.operation == Operation::tas) &&
            (instruction.mode == AddressMode::absolute_x ||
             instruction.mode == AddressMode::absolute_y ||
             instruction.mode == AddressMode::indirect_indexed);
        const bool composite_mutation =
            instruction.operation == Operation::dcp ||
            instruction.operation == Operation::isc ||
            instruction.operation == Operation::rla ||
            instruction.operation == Operation::rra ||
            instruction.operation == Operation::slo ||
            instruction.operation == Operation::sre;
        const bool indexed_mutation =
            (instruction.mode == AddressMode::absolute_x &&
             (instruction.operation == Operation::asl ||
              instruction.operation == Operation::dec ||
              instruction.operation == Operation::inc ||
              instruction.operation == Operation::lsr ||
              instruction.operation == Operation::rol ||
              instruction.operation == Operation::ror)) ||
            (composite_mutation &&
             (instruction.mode == AddressMode::absolute_x ||
              instruction.mode == AddressMode::absolute_y ||
              instruction.mode == AddressMode::indirect_indexed));
        if ((instruction.page_cycle && page_crossed_) || indexed_store || indexed_mutation) {
            (void)read(dummy_address_);
        }
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
    push(state_.status);
    set_flag(interrupt_disable, true);
    interrupt_disable_sampled_ = true;
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
        for (const auto opcode : {0x04, 0x44, 0x64}) {
            set(static_cast<std::uint8_t>(opcode), "NOP", O::nop, M::zero_page, 3);
        }
        for (const auto opcode : {0x14, 0x34, 0x54, 0x74, 0xD4, 0xF4}) {
            set(static_cast<std::uint8_t>(opcode), "NOP", O::nop, M::zero_page_x, 4);
        }
        set(0x0C, "NOP", O::nop, M::absolute, 4);
        for (const auto opcode : {0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC}) {
            set(static_cast<std::uint8_t>(opcode), "NOP", O::nop, M::absolute_x, 4, true);
        }

        const auto set_mutation_family = [&set](const char* name, O operation,
                                                std::uint8_t base) {
            set(static_cast<std::uint8_t>(base + 0x03U), name, operation, M::indexed_indirect, 8);
            set(static_cast<std::uint8_t>(base + 0x07U), name, operation, M::zero_page, 5);
            set(static_cast<std::uint8_t>(base + 0x0FU), name, operation, M::absolute, 6);
            set(static_cast<std::uint8_t>(base + 0x13U), name, operation, M::indirect_indexed, 8);
            set(static_cast<std::uint8_t>(base + 0x17U), name, operation, M::zero_page_x, 6);
            set(static_cast<std::uint8_t>(base + 0x1BU), name, operation, M::absolute_y, 7);
            set(static_cast<std::uint8_t>(base + 0x1FU), name, operation, M::absolute_x, 7);
        };
        set_mutation_family("SLO", O::slo, 0x00);
        set_mutation_family("RLA", O::rla, 0x20);
        set_mutation_family("SRE", O::sre, 0x40);
        set_mutation_family("RRA", O::rra, 0x60);
        set_mutation_family("DCP", O::dcp, 0xC0);
        set_mutation_family("ISC", O::isc, 0xE0);

        set(0x83, "SAX", O::sax, M::indexed_indirect, 6);
        set(0x87, "SAX", O::sax, M::zero_page, 3);
        set(0x8F, "SAX", O::sax, M::absolute, 4);
        set(0x97, "SAX", O::sax, M::zero_page_y, 4);
        set(0x93, "AHX", O::ahx, M::indirect_indexed, 6);
        set(0x9F, "AHX", O::ahx, M::absolute_y, 5);
        set(0x9B, "TAS", O::tas, M::absolute_y, 5);
        set(0x9C, "SHY", O::shy, M::absolute_x, 5);
        set(0x9E, "SHX", O::shx, M::absolute_y, 5);
        set(0xA3, "LAX", O::lax, M::indexed_indirect, 6);
        set(0xA7, "LAX", O::lax, M::zero_page, 3);
        set(0xAF, "LAX", O::lax, M::absolute, 4);
        set(0xB3, "LAX", O::lax, M::indirect_indexed, 5, true);
        set(0xB7, "LAX", O::lax, M::zero_page_y, 4);
        set(0xBF, "LAX", O::lax, M::absolute_y, 4, true);
        set(0xBB, "LAS", O::las, M::absolute_y, 4, true);
        set(0x0B, "ANC", O::anc, M::immediate, 2);
        set(0x2B, "ANC", O::anc, M::immediate, 2);
        set(0x4B, "ALR", O::alr, M::immediate, 2);
        set(0x6B, "ARR", O::arr, M::immediate, 2);
        set(0xAB, "ATX", O::atx, M::immediate, 2);
        set(0xCB, "AXS", O::axs, M::immediate, 2);
        set(0x8B, "XAA", O::xaa, M::immediate, 2);
        set(0xEB, "SBC", O::sbc, M::immediate, 2);

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
    case AddressMode::zero_page_y: {
        const auto base = read(state_.program_counter++);
        (void)read(base);
        const auto offset = mode == AddressMode::zero_page_x ? state_.x : state_.y;
        address_ = static_cast<std::uint8_t>(base + offset);
        break;
    }
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
        dummy_address_ = static_cast<std::uint16_t>(
            (base & 0xFF00U) | (address_ & 0x00FFU));
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
        const auto base = read(state_.program_counter++);
        (void)read(base);
        const auto pointer = static_cast<std::uint8_t>(base + state_.x);
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
        dummy_address_ = static_cast<std::uint16_t>(
            (base & 0xFF00U) | (address_ & 0x00FFU));
        page_crossed_ = (base & 0xFF00U) != (address_ & 0xFF00U);
        break;
    }
    }
}

void Cpu::execute(Operation operation) {
    switch (operation) {
    case Operation::clc:
        set_flag(carry, false);
        break;
    case Operation::cld:
        set_flag(decimal, false);
        break;
    case Operation::cli:
        set_flag(interrupt_disable, false);
        break;
    case Operation::clv:
        set_flag(overflow, false);
        break;
    case Operation::sec:
        set_flag(carry, true);
        break;
    case Operation::sed:
        set_flag(decimal, true);
        break;
    case Operation::sei:
        set_flag(interrupt_disable, true);
        break;
    case Operation::bcc:
        branch(!flag(carry));
        break;
    case Operation::bcs:
        branch(flag(carry));
        break;
    case Operation::beq:
        branch(flag(zero));
        break;
    case Operation::bmi:
        branch(flag(negative));
        break;
    case Operation::bne:
        branch(!flag(zero));
        break;
    case Operation::bpl:
        branch(!flag(negative));
        break;
    case Operation::bvc:
        branch(!flag(overflow));
        break;
    case Operation::bvs:
        branch(flag(overflow));
        break;
    case Operation::jmp:
        state_.program_counter = address_;
        break;
    case Operation::jsr: {
        const auto return_address = static_cast<std::uint16_t>(state_.program_counter - 1U);
        push(static_cast<std::uint8_t>(return_address >> 8U));
        push(static_cast<std::uint8_t>(return_address & 0xFFU));
        state_.program_counter = address_;
        break;
    }
    case Operation::rts: {
        const auto low = static_cast<std::uint16_t>(pop());
        const auto high = static_cast<std::uint16_t>(pop());
        state_.program_counter = static_cast<std::uint16_t>(((high << 8U) | low) + 1U);
        break;
    }
    case Operation::brk:
        ++state_.program_counter;
        push(static_cast<std::uint8_t>(state_.program_counter >> 8U));
        push(static_cast<std::uint8_t>(state_.program_counter & 0xFFU));
        push(static_cast<std::uint8_t>(state_.status | break_command | unused));
        set_flag(break_command, false);
        set_flag(interrupt_disable, true);
        state_.program_counter = read_word(0xFFFE);
        break;
    case Operation::rti: {
        state_.status = pop();
        set_flag(break_command, false);
        set_flag(unused, true);
        interrupt_disable_sampled_ = flag(interrupt_disable);
        const auto low = static_cast<std::uint16_t>(pop());
        const auto high = static_cast<std::uint16_t>(pop());
        state_.program_counter = static_cast<std::uint16_t>((high << 8U) | low);
        break;
    }
    case Operation::pha:
        push(state_.a);
        break;
    case Operation::php:
        push(static_cast<std::uint8_t>(state_.status | break_command | unused));
        break;
    case Operation::pla:
        state_.a = pop();
        set_zero_negative(state_.a);
        break;
    case Operation::plp:
        state_.status = pop();
        set_flag(break_command, false);
        set_flag(unused, true);
        break;
    case Operation::adc: {
        const auto value = operand();
        const auto sum = static_cast<std::uint16_t>(state_.a) + value + (flag(carry) ? 1U : 0U);
        const auto result = static_cast<std::uint8_t>(sum & 0xFFU);
        set_flag(carry, sum > 0xFFU);
        set_flag(overflow, ((~(state_.a ^ value) & (state_.a ^ result)) & 0x80U) != 0);
        state_.a = result;
        set_zero_negative(state_.a);
        break;
    }
    case Operation::sbc: {
        const auto value = operand();
        const auto inverted = static_cast<std::uint16_t>(value ^ 0xFFU);
        const auto sum = static_cast<std::uint16_t>(state_.a) + inverted + (flag(carry) ? 1U : 0U);
        const auto result = static_cast<std::uint8_t>(sum & 0xFFU);
        set_flag(carry, (sum & 0xFF00U) != 0);
        set_flag(overflow, (((state_.a ^ result) & (state_.a ^ value)) & 0x80U) != 0);
        state_.a = result;
        set_zero_negative(state_.a);
        break;
    }
    case Operation::and_:
        state_.a = static_cast<std::uint8_t>(state_.a & operand());
        set_zero_negative(state_.a);
        break;
    case Operation::eor_:
        state_.a = static_cast<std::uint8_t>(state_.a ^ operand());
        set_zero_negative(state_.a);
        break;
    case Operation::ora:
        state_.a = static_cast<std::uint8_t>(state_.a | operand());
        set_zero_negative(state_.a);
        break;
    case Operation::bit: {
        const auto value = operand();
        set_flag(zero, (state_.a & value) == 0);
        set_flag(overflow, (value & 0x40U) != 0);
        set_flag(negative, (value & 0x80U) != 0);
        break;
    }
    case Operation::nop:
        if (current_mode_ != AddressMode::implied) {
            (void)operand();
        }
        break;
    case Operation::anc:
        state_.a = static_cast<std::uint8_t>(state_.a & operand());
        set_zero_negative(state_.a);
        set_flag(carry, flag(negative));
        break;
    case Operation::alr:
        state_.a = static_cast<std::uint8_t>(state_.a & operand());
        set_flag(carry, (state_.a & 0x01U) != 0U);
        state_.a = static_cast<std::uint8_t>(state_.a >> 1U);
        set_zero_negative(state_.a);
        break;
    case Operation::arr: {
        const auto combined = static_cast<std::uint8_t>(state_.a & operand());
        state_.a = static_cast<std::uint8_t>(
            (combined >> 1U) | (flag(carry) ? 0x80U : 0U));
        set_zero_negative(state_.a);
        set_flag(carry, (state_.a & 0x40U) != 0U);
        set_flag(overflow, ((state_.a >> 6U) ^ (state_.a >> 5U)) & 0x01U);
        break;
    }
    case Operation::atx:
        state_.a = operand();
        state_.x = state_.a;
        set_zero_negative(state_.a);
        break;
    case Operation::axs: {
        const auto value = operand();
        const auto left = static_cast<std::uint8_t>(state_.a & state_.x);
        state_.x = static_cast<std::uint8_t>(left - value);
        set_flag(carry, left >= value);
        set_zero_negative(state_.x);
        break;
    }
    case Operation::xaa:
        state_.a = static_cast<std::uint8_t>(state_.x & operand());
        set_zero_negative(state_.a);
        break;
    case Operation::asl: {
        const auto value = operand();
        set_flag(carry, (value & 0x80U) != 0);
        const auto result = static_cast<std::uint8_t>(value << 1U);
        store_mutation(value, result);
        set_zero_negative(result);
        break;
    }
    case Operation::lsr: {
        const auto value = operand();
        set_flag(carry, (value & 0x01U) != 0);
        const auto result = static_cast<std::uint8_t>(value >> 1U);
        store_mutation(value, result);
        set_zero_negative(result);
        break;
    }
    case Operation::rol: {
        const auto value = operand();
        const auto previous_carry = flag(carry) ? 1U : 0U;
        set_flag(carry, (value & 0x80U) != 0);
        const auto result = static_cast<std::uint8_t>((value << 1U) | previous_carry);
        store_mutation(value, result);
        set_zero_negative(result);
        break;
    }
    case Operation::ror: {
        const auto value = operand();
        const auto previous_carry = flag(carry) ? 0x80U : 0U;
        set_flag(carry, (value & 0x01U) != 0);
        const auto result = static_cast<std::uint8_t>((value >> 1U) | previous_carry);
        store_mutation(value, result);
        set_zero_negative(result);
        break;
    }
    case Operation::cmp:
    case Operation::cpx:
    case Operation::cpy: {
        const auto left = operation == Operation::cmp ? state_.a
                        : operation == Operation::cpx ? state_.x
                                                      : state_.y;
        const auto right = operand();
        const auto difference = static_cast<std::uint8_t>(left - right);
        set_flag(carry, left >= right);
        set_zero_negative(difference);
        break;
    }
    case Operation::lda:
        state_.a = operand();
        set_zero_negative(state_.a);
        break;
    case Operation::ldx:
        state_.x = operand();
        set_zero_negative(state_.x);
        break;
    case Operation::ldy:
        state_.y = operand();
        set_zero_negative(state_.y);
        break;
    case Operation::lax:
        state_.a = operand();
        state_.x = state_.a;
        set_zero_negative(state_.a);
        break;
    case Operation::las:
        state_.a = static_cast<std::uint8_t>(operand() & state_.stack_pointer);
        state_.x = state_.a;
        state_.stack_pointer = state_.a;
        set_zero_negative(state_.a);
        break;
    case Operation::sta:
        write(address_, state_.a);
        break;
    case Operation::stx:
        write(address_, state_.x);
        break;
    case Operation::sty:
        write(address_, state_.y);
        break;
    case Operation::sax:
        write(address_, static_cast<std::uint8_t>(state_.a & state_.x));
        break;
    case Operation::ahx:
    case Operation::tas: {
        auto source = static_cast<std::uint8_t>(state_.a & state_.x);
        if (operation == Operation::tas) {
            state_.stack_pointer = source;
        }
        const auto high_plus_one = static_cast<std::uint8_t>((dummy_address_ >> 8U) + 1U);
        const auto value = static_cast<std::uint8_t>(source & high_plus_one);
        const auto destination = page_crossed_
            ? static_cast<std::uint16_t>((static_cast<std::uint16_t>(value) << 8U) |
                                         (address_ & 0x00FFU))
            : address_;
        write(destination, value);
        break;
    }
    case Operation::shx:
    case Operation::shy: {
        const auto source = operation == Operation::shx ? state_.x : state_.y;
        const auto high_plus_one = static_cast<std::uint8_t>((dummy_address_ >> 8U) + 1U);
        const auto value = static_cast<std::uint8_t>(source & high_plus_one);
        const auto destination = page_crossed_
            ? static_cast<std::uint16_t>((static_cast<std::uint16_t>(value) << 8U) |
                                         (address_ & 0x00FFU))
            : address_;
        write(destination, value);
        break;
    }
    case Operation::tax:
        state_.x = state_.a;
        set_zero_negative(state_.x);
        break;
    case Operation::tay:
        state_.y = state_.a;
        set_zero_negative(state_.y);
        break;
    case Operation::tsx:
        state_.x = state_.stack_pointer;
        set_zero_negative(state_.x);
        break;
    case Operation::txa:
        state_.a = state_.x;
        set_zero_negative(state_.a);
        break;
    case Operation::txs:
        state_.stack_pointer = state_.x;
        break;
    case Operation::tya:
        state_.a = state_.y;
        set_zero_negative(state_.a);
        break;
    case Operation::inc: {
        const auto original = operand();
        const auto value = static_cast<std::uint8_t>(original + 1U);
        store_mutation(original, value);
        set_zero_negative(value);
        break;
    }
    case Operation::dec: {
        const auto original = operand();
        const auto value = static_cast<std::uint8_t>(original - 1U);
        store_mutation(original, value);
        set_zero_negative(value);
        break;
    }
    case Operation::slo: {
        const auto original = operand();
        set_flag(carry, (original & 0x80U) != 0U);
        const auto value = static_cast<std::uint8_t>(original << 1U);
        store_mutation(original, value);
        state_.a = static_cast<std::uint8_t>(state_.a | value);
        set_zero_negative(state_.a);
        break;
    }
    case Operation::rla: {
        const auto original = operand();
        const auto previous_carry = flag(carry) ? 1U : 0U;
        set_flag(carry, (original & 0x80U) != 0U);
        const auto value = static_cast<std::uint8_t>((original << 1U) | previous_carry);
        store_mutation(original, value);
        state_.a = static_cast<std::uint8_t>(state_.a & value);
        set_zero_negative(state_.a);
        break;
    }
    case Operation::sre: {
        const auto original = operand();
        set_flag(carry, (original & 0x01U) != 0U);
        const auto value = static_cast<std::uint8_t>(original >> 1U);
        store_mutation(original, value);
        state_.a = static_cast<std::uint8_t>(state_.a ^ value);
        set_zero_negative(state_.a);
        break;
    }
    case Operation::rra: {
        const auto original = operand();
        const auto previous_carry = flag(carry) ? 0x80U : 0U;
        set_flag(carry, (original & 0x01U) != 0U);
        const auto value = static_cast<std::uint8_t>((original >> 1U) | previous_carry);
        store_mutation(original, value);
        const auto sum = static_cast<std::uint16_t>(state_.a) + value +
            (flag(carry) ? 1U : 0U);
        const auto result = static_cast<std::uint8_t>(sum & 0xFFU);
        set_flag(carry, sum > 0xFFU);
        set_flag(overflow, ((~(state_.a ^ value) & (state_.a ^ result)) & 0x80U) != 0U);
        state_.a = result;
        set_zero_negative(state_.a);
        break;
    }
    case Operation::dcp: {
        const auto original = operand();
        const auto value = static_cast<std::uint8_t>(original - 1U);
        store_mutation(original, value);
        const auto difference = static_cast<std::uint8_t>(state_.a - value);
        set_flag(carry, state_.a >= value);
        set_zero_negative(difference);
        break;
    }
    case Operation::isc: {
        const auto original = operand();
        const auto value = static_cast<std::uint8_t>(original + 1U);
        store_mutation(original, value);
        const auto inverted = static_cast<std::uint16_t>(value ^ 0xFFU);
        const auto sum = static_cast<std::uint16_t>(state_.a) + inverted +
            (flag(carry) ? 1U : 0U);
        const auto result = static_cast<std::uint8_t>(sum & 0xFFU);
        set_flag(carry, (sum & 0xFF00U) != 0U);
        set_flag(overflow, (((state_.a ^ result) & (state_.a ^ value)) & 0x80U) != 0U);
        state_.a = result;
        set_zero_negative(state_.a);
        break;
    }
    case Operation::inx:
        ++state_.x;
        set_zero_negative(state_.x);
        break;
    case Operation::iny:
        ++state_.y;
        set_zero_negative(state_.y);
        break;
    case Operation::dex:
        --state_.x;
        set_zero_negative(state_.x);
        break;
    case Operation::dey:
        --state_.y;
        set_zero_negative(state_.y);
        break;
    default:
        break;
    }
}

std::uint8_t Cpu::operand() {
    if (current_mode_ == AddressMode::accumulator) {
        return state_.a;
    }
    return read(address_);
}

void Cpu::store_operand(std::uint8_t value) {
    if (current_mode_ == AddressMode::accumulator) {
        state_.a = value;
    } else {
        write(address_, value);
    }
}

void Cpu::store_mutation(std::uint8_t original, std::uint8_t value) {
    if (current_mode_ == AddressMode::accumulator) {
        state_.a = value;
    } else {
        write(address_, original);
        write(address_, value);
    }
}

void Cpu::set_zero_negative(std::uint8_t value) noexcept {
    set_flag(zero, value == 0);
    set_flag(negative, (value & 0x80U) != 0);
}

void Cpu::branch(bool condition) {
    if (!condition) {
        return;
    }

    ++remaining_cycles_;
    const auto original = state_.program_counter;
    const auto target = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(state_.program_counter) + relative_);
    (void)read(original);
    if ((original & 0xFF00U) != (target & 0xFF00U)) {
        const auto intermediate = static_cast<std::uint16_t>(
            (original & 0xFF00U) | (target & 0x00FFU));
        (void)read(intermediate);
        ++remaining_cycles_;
    }
    state_.program_counter = target;
}

}  // namespace nes
