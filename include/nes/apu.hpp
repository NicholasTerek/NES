#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nes {

class Apu {
public:
    struct State {
        std::uint64_t cpu_cycle = 0;
        std::uint32_t frame_cycle = 0;
        std::uint64_t quarter_frame_ticks = 0;
        std::uint64_t half_frame_ticks = 0;
        bool five_step_mode = false;
        bool irq_inhibit = false;
        bool frame_irq = false;
        std::array<std::uint8_t, 2> pulse_length{};
        std::array<std::uint16_t, 2> pulse_period{};
        std::array<std::uint8_t, 2> pulse_level{};
    };

    void reset() noexcept;
    void clock() noexcept;
    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address,
                                        bool read_only = false) noexcept;
    void cpu_write(std::uint16_t address, std::uint8_t value) noexcept;

    [[nodiscard]] bool irq_pending() const noexcept;
    [[nodiscard]] State state() const noexcept;

private:
    struct Pulse {
        bool enabled = false;
        std::uint8_t duty = 0;
        std::uint8_t sequence = 0;
        bool length_halt = false;
        bool constant_volume = false;
        std::uint8_t envelope_period = 0;
        bool envelope_start = false;
        std::uint8_t envelope_divider = 0;
        std::uint8_t envelope_decay = 0;
        std::uint16_t timer_period = 0;
        std::uint16_t timer_counter = 0;
        std::uint8_t length_counter = 0;
        bool sweep_enabled = false;
        std::uint8_t sweep_period = 0;
        bool sweep_negate = false;
        std::uint8_t sweep_shift = 0;
        bool sweep_reload = false;
        std::uint8_t sweep_divider = 0;
    };

    void clock_quarter_frame() noexcept;
    void clock_half_frame() noexcept;
    void write_pulse(std::size_t index, std::uint16_t address,
                     std::uint8_t value) noexcept;
    void clock_pulse_timer(Pulse& pulse) noexcept;
    void clock_envelope(Pulse& pulse) noexcept;
    void clock_sweep(Pulse& pulse, bool first_channel) noexcept;
    [[nodiscard]] std::uint8_t pulse_level(std::size_t index) const noexcept;
    [[nodiscard]] std::int32_t sweep_target(const Pulse& pulse,
                                            bool first_channel) const noexcept;

    std::uint64_t cpu_cycle_ = 0;
    std::uint32_t frame_cycle_ = 0;
    std::uint64_t quarter_frame_ticks_ = 0;
    std::uint64_t half_frame_ticks_ = 0;
    bool five_step_mode_ = false;
    bool irq_inhibit_ = false;
    bool frame_irq_ = false;
    std::array<Pulse, 2> pulse_{};
};

}  // namespace nes
