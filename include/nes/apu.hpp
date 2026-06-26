#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

namespace nes {

class Apu {
public:
    static constexpr std::uint32_t cpu_frequency = 1'789'773;
    static constexpr std::uint32_t output_sample_rate = 44'100;

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
        std::uint8_t triangle_length = 0;
        std::uint8_t triangle_linear = 0;
        std::uint16_t triangle_period = 0;
        std::uint8_t triangle_level = 0;
        std::uint8_t noise_length = 0;
        std::uint16_t noise_period = 0;
        std::uint16_t noise_shift = 1;
        std::uint8_t noise_level = 0;
        std::uint16_t dmc_address = 0xC000;
        std::uint16_t dmc_bytes_remaining = 0;
        std::uint8_t dmc_output = 0;
        bool dmc_irq = false;
    };

    void reset() noexcept;
    void clock() noexcept;
    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address,
                                        bool read_only = false) noexcept;
    void cpu_write(std::uint16_t address, std::uint8_t value) noexcept;
    void set_dmc_reader(std::function<std::uint8_t(std::uint16_t)> reader);
    [[nodiscard]] std::uint8_t take_cpu_stall_cycles() noexcept;
    [[nodiscard]] float mixed_output() const noexcept;
    [[nodiscard]] std::size_t buffered_samples() const noexcept;
    [[nodiscard]] std::optional<float> pop_sample() noexcept;
    void clear_samples() noexcept;

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

    struct Triangle {
        bool enabled = false;
        bool control = false;
        std::uint8_t linear_reload = 0;
        bool linear_reload_flag = false;
        std::uint8_t linear_counter = 0;
        std::uint16_t timer_period = 0;
        std::uint16_t timer_counter = 0;
        std::uint8_t sequence = 0;
        std::uint8_t length_counter = 0;
    };

    struct Noise {
        bool enabled = false;
        bool length_halt = false;
        bool constant_volume = false;
        std::uint8_t envelope_period = 0;
        bool envelope_start = false;
        std::uint8_t envelope_divider = 0;
        std::uint8_t envelope_decay = 0;
        bool mode = false;
        std::uint16_t timer_period = 4;
        std::uint16_t timer_counter = 0;
        std::uint16_t shift_register = 1;
        std::uint8_t length_counter = 0;
    };

    struct Dmc {
        bool enabled = false;
        bool irq_enabled = false;
        bool loop = false;
        bool irq = false;
        std::uint16_t timer_period = 428;
        std::uint16_t timer_counter = 0;
        std::uint8_t output_level = 0;
        std::uint16_t sample_address = 0xC000;
        std::uint16_t sample_length = 1;
        std::uint16_t current_address = 0xC000;
        std::uint16_t bytes_remaining = 0;
        std::uint8_t sample_buffer = 0;
        bool buffer_empty = true;
        std::uint8_t shift_register = 0;
        std::uint8_t bits_remaining = 0;
        bool silence = true;
    };

    void clock_quarter_frame() noexcept;
    void clock_half_frame() noexcept;
    void write_pulse(std::size_t index, std::uint16_t address,
                     std::uint8_t value) noexcept;
    void clock_pulse_timer(Pulse& pulse) noexcept;
    void clock_envelope(Pulse& pulse) noexcept;
    void clock_sweep(Pulse& pulse, bool first_channel) noexcept;
    void clock_triangle_timer() noexcept;
    void clock_noise_timer() noexcept;
    void clock_noise_envelope() noexcept;
    void clock_dmc() noexcept;
    void restart_dmc_sample() noexcept;
    void queue_output_sample() noexcept;
    [[nodiscard]] std::uint8_t pulse_level(std::size_t index) const noexcept;
    [[nodiscard]] std::uint8_t triangle_level() const noexcept;
    [[nodiscard]] std::uint8_t noise_level() const noexcept;
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
    Triangle triangle_{};
    Noise noise_{};
    Dmc dmc_{};
    std::function<std::uint8_t(std::uint16_t)> dmc_reader_;
    std::uint8_t pending_cpu_stall_ = 0;
    std::uint32_t sample_phase_ = 0;
    std::deque<float> samples_;
};

}  // namespace nes
