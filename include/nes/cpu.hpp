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
    void nmi();
    void clock();

    [[nodiscard]] bool instruction_complete() const noexcept;
    [[nodiscard]] const State& state() const noexcept;
    [[nodiscard]] State& state() noexcept;

    [[nodiscard]] bool flag(Flag flag) const noexcept;
    void set_flag(Flag flag, bool value) noexcept;

private:
    CpuBusDevice& bus_;
    State state_{};
    std::uint8_t remaining_cycles_ = 0;

    [[nodiscard]] std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint16_t read_word(std::uint16_t address);
    void push(std::uint8_t value);
    [[nodiscard]] std::uint8_t pop();
    void service_interrupt(std::uint16_t vector, std::uint8_t cycles);
};

}  // namespace nes
