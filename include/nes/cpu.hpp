#pragma once

#include "nes/bus_device.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nes {

class Cpu {
public:
    enum Flag : std::uint8_t {
        carry = 1U << 0U,
        zero = 1U << 1U,
        interrupt_disable = 1U << 2U,
        decimal = 1U << 3U,
        break_command = 1U << 4U,
        unused = 1U << 5U,
        overflow = 1U << 6U,
        negative = 1U << 7U,
    };

    struct State {
        std::uint8_t a = 0;
        std::uint8_t x = 0;
        std::uint8_t y = 0;
        std::uint8_t stack_pointer = 0xFD;
        std::uint8_t status = unused | interrupt_disable;
        std::uint16_t program_counter = 0;
        std::uint64_t cycles = 0;
    };

    explicit Cpu(CpuBusDevice& bus);

    void reset();
    void irq();
    void poll_irq();
    void nmi();
    void clock();

    [[nodiscard]] bool instruction_complete() const noexcept;
    [[nodiscard]] const State& state() const noexcept;
    [[nodiscard]] State& state() noexcept;

    [[nodiscard]] bool flag(Flag flag) const noexcept;
    void set_flag(Flag flag, bool value) noexcept;

private:
    enum class AddressMode : std::uint8_t {
        implied,
        accumulator,
        immediate,
        zero_page,
        zero_page_x,
        zero_page_y,
        relative,
        absolute,
        absolute_x,
        absolute_y,
        indirect,
        indexed_indirect,
        indirect_indexed,
    };

    enum class Operation : std::uint8_t {
        adc, and_, asl, bcc, bcs, beq, bit, bmi, bne, bpl, brk, bvc, bvs,
        clc, cld, cli, clv, cmp, cpx, cpy, dec, dex, dey, eor_, inc, inx,
        iny, jmp, jsr, lda, ldx, ldy, lsr, nop, ora, pha, php, pla, plp,
        rol, ror, rti, rts, sbc, sec, sed, sei, sta, stx, sty, tax, tay,
        tsx, txa, txs, tya,
        ahx, alr, anc, arr, atx, axs, dcp, isc, las, lax, rla, rra, sax, shx,
        shy, slo, sre, tas, xaa,
        illegal,
    };

    struct Instruction {
        const char* name;
        Operation operation;
        AddressMode mode;
        std::uint8_t cycles;
        bool page_cycle;
    };

    CpuBusDevice& bus_;
    State state_{};
    std::uint8_t remaining_cycles_ = 0;
    std::uint8_t opcode_ = 0;
    std::uint16_t address_ = 0;
    std::uint16_t dummy_address_ = 0;
    std::int8_t relative_ = 0;
    bool page_crossed_ = false;
    bool interrupt_disable_sampled_ = true;
    bool powered_ = false;
    AddressMode current_mode_ = AddressMode::implied;

    [[nodiscard]] static const std::array<Instruction, 256>& instruction_table();
    [[nodiscard]] std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint16_t read_word(std::uint16_t address);
    void push(std::uint8_t value);
    [[nodiscard]] std::uint8_t pop();
    void service_interrupt(std::uint16_t vector, std::uint8_t cycles);
    void resolve_address(AddressMode mode);
    void execute(Operation operation);
    [[nodiscard]] std::uint8_t operand();
    void store_operand(std::uint8_t value);
    void store_mutation(std::uint8_t original, std::uint8_t value);
    void set_zero_negative(std::uint8_t value) noexcept;
    void branch(bool condition);
};

}  // namespace nes
