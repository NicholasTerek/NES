#include "nes/apu.hpp"

#include <array>

namespace nes {
namespace {

constexpr std::array<std::uint8_t, 32> length_table{
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

constexpr std::array<std::array<std::uint8_t, 8>, 4> duty_table{{
    {{0, 1, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 1, 1, 0, 0, 0}},
    {{1, 0, 0, 1, 1, 1, 1, 1}},
}};

constexpr std::array<std::uint8_t, 32> triangle_table{
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

constexpr std::array<std::uint16_t, 16> noise_period_table{
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1'016, 2'034, 4'068,
};

}  // namespace

void Apu::reset() noexcept {
    cpu_cycle_ = 0;
    frame_cycle_ = 0;
    quarter_frame_ticks_ = 0;
    half_frame_ticks_ = 0;
    five_step_mode_ = false;
    irq_inhibit_ = false;
    frame_irq_ = false;
    pulse_ = {};
    triangle_ = {};
    noise_ = {};
    noise_.shift_register = 1;
    noise_.timer_period = noise_period_table[0];
}

void Apu::clock() noexcept {
    ++cpu_cycle_;
    ++frame_cycle_;
    clock_triangle_timer();
    if ((cpu_cycle_ & 0x01U) == 0U) {
        clock_pulse_timer(pulse_[0]);
        clock_pulse_timer(pulse_[1]);
        clock_noise_timer();
    }

    if (!five_step_mode_) {
        switch (frame_cycle_) {
        case 3'729:
        case 11'186:
            clock_quarter_frame();
            break;
        case 7'457:
            clock_quarter_frame();
            clock_half_frame();
            break;
        case 14'915:
            clock_quarter_frame();
            clock_half_frame();
            if (!irq_inhibit_) {
                frame_irq_ = true;
            }
            frame_cycle_ = 0;
            break;
        default:
            break;
        }
        return;
    }

    switch (frame_cycle_) {
    case 3'729:
    case 11'186:
        clock_quarter_frame();
        break;
    case 7'457:
    case 18'641:
        clock_quarter_frame();
        clock_half_frame();
        if (frame_cycle_ == 18'641U) {
            frame_cycle_ = 0;
        }
        break;
    default:
        break;
    }
}

std::uint8_t Apu::cpu_read(std::uint16_t address, bool read_only) noexcept {
    if (address != 0x4015U) {
        return 0;
    }
    auto status = static_cast<std::uint8_t>(frame_irq_ ? 0x40U : 0x00U);
    if (pulse_[0].length_counter != 0U) {
        status = static_cast<std::uint8_t>(status | 0x01U);
    }
    if (pulse_[1].length_counter != 0U) {
        status = static_cast<std::uint8_t>(status | 0x02U);
    }
    if (triangle_.length_counter != 0U) {
        status = static_cast<std::uint8_t>(status | 0x04U);
    }
    if (noise_.length_counter != 0U) {
        status = static_cast<std::uint8_t>(status | 0x08U);
    }
    if (!read_only) {
        frame_irq_ = false;
    }
    return status;
}

void Apu::cpu_write(std::uint16_t address, std::uint8_t value) noexcept {
    if (address >= 0x4000U && address <= 0x4007U) {
        const auto index = static_cast<std::size_t>((address - 0x4000U) / 4U);
        write_pulse(index, static_cast<std::uint16_t>(address & 0x0003U), value);
        return;
    }
    if (address == 0x4008U) {
        triangle_.control = (value & 0x80U) != 0U;
        triangle_.linear_reload = static_cast<std::uint8_t>(value & 0x7FU);
        return;
    }
    if (address == 0x400AU) {
        triangle_.timer_period = static_cast<std::uint16_t>(
            (triangle_.timer_period & 0x0700U) | value);
        return;
    }
    if (address == 0x400BU) {
        triangle_.timer_period = static_cast<std::uint16_t>(
            (triangle_.timer_period & 0x00FFU) |
            (static_cast<std::uint16_t>(value & 0x07U) << 8U));
        if (triangle_.enabled) {
            triangle_.length_counter = length_table[value >> 3U];
        }
        triangle_.linear_reload_flag = true;
        return;
    }
    if (address == 0x400CU) {
        noise_.length_halt = (value & 0x20U) != 0U;
        noise_.constant_volume = (value & 0x10U) != 0U;
        noise_.envelope_period = static_cast<std::uint8_t>(value & 0x0FU);
        return;
    }
    if (address == 0x400EU) {
        noise_.mode = (value & 0x80U) != 0U;
        noise_.timer_period = noise_period_table[value & 0x0FU];
        return;
    }
    if (address == 0x400FU) {
        if (noise_.enabled) {
            noise_.length_counter = length_table[value >> 3U];
        }
        noise_.envelope_start = true;
        return;
    }
    if (address == 0x4015U) {
        pulse_[0].enabled = (value & 0x01U) != 0U;
        pulse_[1].enabled = (value & 0x02U) != 0U;
        triangle_.enabled = (value & 0x04U) != 0U;
        noise_.enabled = (value & 0x08U) != 0U;
        if (!pulse_[0].enabled) {
            pulse_[0].length_counter = 0;
        }
        if (!pulse_[1].enabled) {
            pulse_[1].length_counter = 0;
        }
        if (!triangle_.enabled) {
            triangle_.length_counter = 0;
        }
        if (!noise_.enabled) {
            noise_.length_counter = 0;
        }
        return;
    }
    if (address != 0x4017U) {
        return;
    }

    five_step_mode_ = (value & 0x80U) != 0U;
    irq_inhibit_ = (value & 0x40U) != 0U;
    if (irq_inhibit_) {
        frame_irq_ = false;
    }
    frame_cycle_ = 0;
    if (five_step_mode_) {
        clock_quarter_frame();
        clock_half_frame();
    }
}

bool Apu::irq_pending() const noexcept {
    return frame_irq_;
}

Apu::State Apu::state() const noexcept {
    State state{cpu_cycle_, frame_cycle_, quarter_frame_ticks_, half_frame_ticks_,
                five_step_mode_, irq_inhibit_, frame_irq_};
    for (std::size_t index = 0; index < pulse_.size(); ++index) {
        state.pulse_length[index] = pulse_[index].length_counter;
        state.pulse_period[index] = pulse_[index].timer_period;
        state.pulse_level[index] = pulse_level(index);
    }
    state.triangle_length = triangle_.length_counter;
    state.triangle_linear = triangle_.linear_counter;
    state.triangle_period = triangle_.timer_period;
    state.triangle_level = triangle_level();
    state.noise_length = noise_.length_counter;
    state.noise_period = noise_.timer_period;
    state.noise_shift = noise_.shift_register;
    state.noise_level = noise_level();
    return state;
}

void Apu::clock_quarter_frame() noexcept {
    ++quarter_frame_ticks_;
    clock_envelope(pulse_[0]);
    clock_envelope(pulse_[1]);
    if (triangle_.linear_reload_flag) {
        triangle_.linear_counter = triangle_.linear_reload;
    } else if (triangle_.linear_counter != 0U) {
        --triangle_.linear_counter;
    }
    if (!triangle_.control) {
        triangle_.linear_reload_flag = false;
    }
    clock_noise_envelope();
}

void Apu::clock_half_frame() noexcept {
    ++half_frame_ticks_;
    for (auto& pulse : pulse_) {
        if (!pulse.length_halt && pulse.length_counter != 0U) {
            --pulse.length_counter;
        }
    }
    if (!triangle_.control && triangle_.length_counter != 0U) {
        --triangle_.length_counter;
    }
    if (!noise_.length_halt && noise_.length_counter != 0U) {
        --noise_.length_counter;
    }
    clock_sweep(pulse_[0], true);
    clock_sweep(pulse_[1], false);
}

void Apu::write_pulse(std::size_t index, std::uint16_t address,
                      std::uint8_t value) noexcept {
    auto& pulse = pulse_[index];
    switch (address) {
    case 0:
        pulse.duty = static_cast<std::uint8_t>((value >> 6U) & 0x03U);
        pulse.length_halt = (value & 0x20U) != 0U;
        pulse.constant_volume = (value & 0x10U) != 0U;
        pulse.envelope_period = static_cast<std::uint8_t>(value & 0x0FU);
        break;
    case 1:
        pulse.sweep_enabled = (value & 0x80U) != 0U;
        pulse.sweep_period = static_cast<std::uint8_t>((value >> 4U) & 0x07U);
        pulse.sweep_negate = (value & 0x08U) != 0U;
        pulse.sweep_shift = static_cast<std::uint8_t>(value & 0x07U);
        pulse.sweep_reload = true;
        break;
    case 2:
        pulse.timer_period = static_cast<std::uint16_t>(
            (pulse.timer_period & 0x0700U) | value);
        break;
    case 3:
        pulse.timer_period = static_cast<std::uint16_t>(
            (pulse.timer_period & 0x00FFU) |
            (static_cast<std::uint16_t>(value & 0x07U) << 8U));
        if (pulse.enabled) {
            pulse.length_counter = length_table[value >> 3U];
        }
        pulse.sequence = 0;
        pulse.envelope_start = true;
        break;
    default:
        break;
    }
}

void Apu::clock_pulse_timer(Pulse& pulse) noexcept {
    if (pulse.timer_counter == 0U) {
        pulse.timer_counter = pulse.timer_period;
        pulse.sequence = static_cast<std::uint8_t>((pulse.sequence + 1U) & 0x07U);
    } else {
        --pulse.timer_counter;
    }
}

void Apu::clock_envelope(Pulse& pulse) noexcept {
    if (pulse.envelope_start) {
        pulse.envelope_start = false;
        pulse.envelope_decay = 15;
        pulse.envelope_divider = pulse.envelope_period;
        return;
    }
    if (pulse.envelope_divider != 0U) {
        --pulse.envelope_divider;
        return;
    }
    pulse.envelope_divider = pulse.envelope_period;
    if (pulse.envelope_decay != 0U) {
        --pulse.envelope_decay;
    } else if (pulse.length_halt) {
        pulse.envelope_decay = 15;
    }
}

std::int32_t Apu::sweep_target(const Pulse& pulse, bool first_channel) const noexcept {
    const auto change = static_cast<std::int32_t>(pulse.timer_period >> pulse.sweep_shift);
    const auto period = static_cast<std::int32_t>(pulse.timer_period);
    if (!pulse.sweep_negate) {
        return period + change;
    }
    return period - change - (first_channel ? 1 : 0);
}

void Apu::clock_sweep(Pulse& pulse, bool first_channel) noexcept {
    const auto target = sweep_target(pulse, first_channel);
    if (pulse.sweep_divider == 0U && pulse.sweep_enabled && pulse.sweep_shift != 0U &&
        pulse.timer_period >= 8U && target >= 0 && target <= 0x07FF) {
        pulse.timer_period = static_cast<std::uint16_t>(target);
    }
    if (pulse.sweep_divider == 0U || pulse.sweep_reload) {
        pulse.sweep_divider = pulse.sweep_period;
        pulse.sweep_reload = false;
    } else {
        --pulse.sweep_divider;
    }
}

std::uint8_t Apu::pulse_level(std::size_t index) const noexcept {
    const auto& pulse = pulse_[index];
    const auto target = sweep_target(pulse, index == 0U);
    if (!pulse.enabled || pulse.length_counter == 0U || pulse.timer_period < 8U ||
        target > 0x07FF || duty_table[pulse.duty][pulse.sequence] == 0U) {
        return 0;
    }
    return pulse.constant_volume ? pulse.envelope_period : pulse.envelope_decay;
}

void Apu::clock_triangle_timer() noexcept {
    if (triangle_.timer_counter == 0U) {
        triangle_.timer_counter = triangle_.timer_period;
        if (triangle_.enabled && triangle_.length_counter != 0U &&
            triangle_.linear_counter != 0U && triangle_.timer_period >= 2U) {
            triangle_.sequence = static_cast<std::uint8_t>((triangle_.sequence + 1U) & 0x1FU);
        }
    } else {
        --triangle_.timer_counter;
    }
}

void Apu::clock_noise_timer() noexcept {
    if (noise_.timer_counter != 0U) {
        --noise_.timer_counter;
        return;
    }
    noise_.timer_counter = noise_.timer_period;
    const auto tap = static_cast<std::uint8_t>(noise_.mode ? 6U : 1U);
    const auto feedback = static_cast<std::uint16_t>(
        (noise_.shift_register & 0x01U) ^ ((noise_.shift_register >> tap) & 0x01U));
    noise_.shift_register = static_cast<std::uint16_t>(
        (noise_.shift_register >> 1U) | (feedback << 14U));
}

void Apu::clock_noise_envelope() noexcept {
    if (noise_.envelope_start) {
        noise_.envelope_start = false;
        noise_.envelope_decay = 15;
        noise_.envelope_divider = noise_.envelope_period;
        return;
    }
    if (noise_.envelope_divider != 0U) {
        --noise_.envelope_divider;
        return;
    }
    noise_.envelope_divider = noise_.envelope_period;
    if (noise_.envelope_decay != 0U) {
        --noise_.envelope_decay;
    } else if (noise_.length_halt) {
        noise_.envelope_decay = 15;
    }
}

std::uint8_t Apu::triangle_level() const noexcept {
    if (!triangle_.enabled || triangle_.length_counter == 0U ||
        triangle_.linear_counter == 0U) {
        return 0;
    }
    return triangle_table[triangle_.sequence];
}

std::uint8_t Apu::noise_level() const noexcept {
    if (!noise_.enabled || noise_.length_counter == 0U ||
        (noise_.shift_register & 0x01U) != 0U) {
        return 0;
    }
    return noise_.constant_volume ? noise_.envelope_period : noise_.envelope_decay;
}

}  // namespace nes
