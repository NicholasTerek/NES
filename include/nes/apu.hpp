#pragma once

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
    };

    void reset() noexcept;
    void clock() noexcept;
    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address,
                                        bool read_only = false) noexcept;
    void cpu_write(std::uint16_t address, std::uint8_t value) noexcept;

    [[nodiscard]] bool irq_pending() const noexcept;
    [[nodiscard]] State state() const noexcept;

private:
    void clock_quarter_frame() noexcept;
    void clock_half_frame() noexcept;

    std::uint64_t cpu_cycle_ = 0;
    std::uint32_t frame_cycle_ = 0;
    std::uint64_t quarter_frame_ticks_ = 0;
    std::uint64_t half_frame_ticks_ = 0;
    bool five_step_mode_ = false;
    bool irq_inhibit_ = false;
    bool frame_irq_ = false;
};

}  // namespace nes
