#pragma once

#include "nes/bus.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace nes {

class Cartridge;

class Emulator {
public:
    explicit Emulator(std::shared_ptr<Cartridge> cartridge);
    static Emulator load(const std::filesystem::path& path);

    void reset();
    void clock();
    void run_frame();
    void run_frames(std::size_t count);
    void set_controller_state(std::size_t port, std::uint8_t buttons);

    [[nodiscard]] Bus& bus() noexcept;
    [[nodiscard]] const Bus& bus() const noexcept;
    [[nodiscard]] std::uint64_t completed_frames() const noexcept;
    [[nodiscard]] std::vector<float> take_audio_samples();

private:
    void collect_audio();

    Bus bus_;
    std::uint64_t completed_frames_ = 0;
    std::vector<float> audio_samples_;
};

}  // namespace nes
