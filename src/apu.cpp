#include "nes/apu.hpp"

namespace nes {

void Apu::reset() noexcept {
    cpu_cycle_ = 0;
    frame_cycle_ = 0;
    quarter_frame_ticks_ = 0;
    half_frame_ticks_ = 0;
    five_step_mode_ = false;
    irq_inhibit_ = false;
    frame_irq_ = false;
}

void Apu::clock() noexcept {
    ++cpu_cycle_;
    ++frame_cycle_;

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
    const auto status = static_cast<std::uint8_t>(frame_irq_ ? 0x40U : 0x00U);
    if (!read_only) {
        frame_irq_ = false;
    }
    return status;
}

void Apu::cpu_write(std::uint16_t address, std::uint8_t value) noexcept {
    if (address == 0x4015U) {
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
    return {cpu_cycle_, frame_cycle_, quarter_frame_ticks_, half_frame_ticks_,
            five_step_mode_, irq_inhibit_, frame_irq_};
}

void Apu::clock_quarter_frame() noexcept {
    ++quarter_frame_ticks_;
}

void Apu::clock_half_frame() noexcept {
    ++half_frame_ticks_;
}

}  // namespace nes
