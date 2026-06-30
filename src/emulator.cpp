#include "nes/emulator.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {

Emulator::Emulator(std::shared_ptr<Cartridge> cartridge) {
    if (!cartridge) {
        throw std::invalid_argument("emulator requires a cartridge");
    }
    bus_.insert_cartridge(std::move(cartridge));
    reset();
}

Emulator Emulator::load(const std::filesystem::path& path) {
    return Emulator(Cartridge::load(path));
}

void Emulator::reset() {
    bus_.reset();
    completed_frames_ = 0;
    audio_samples_.clear();
}

void Emulator::clock() {
    bus_.clock();
}

void Emulator::run_frame() {
    bus_.ppu().clear_frame_complete();
    while (!bus_.ppu().state().frame_complete) {
        clock();
    }
    bus_.ppu().clear_frame_complete();
    ++completed_frames_;
    collect_audio();
}

void Emulator::run_frames(std::size_t count) {
    for (std::size_t frame = 0; frame < count; ++frame) {
        run_frame();
    }
}

void Emulator::set_controller_state(std::size_t port, std::uint8_t buttons) {
    bus_.set_controller_state(port, buttons);
}

Bus& Emulator::bus() noexcept {
    return bus_;
}

const Bus& Emulator::bus() const noexcept {
    return bus_;
}

std::uint64_t Emulator::completed_frames() const noexcept {
    return completed_frames_;
}

std::vector<float> Emulator::take_audio_samples() {
    collect_audio();
    auto samples = std::move(audio_samples_);
    audio_samples_.clear();
    return samples;
}

void Emulator::collect_audio() {
    while (const auto sample = bus_.apu().pop_sample()) {
        audio_samples_.push_back(*sample);
    }
}

}  // namespace nes
