#include "nes/cartridge.hpp"
#include "nes/emulator.hpp"
#include "nes/ppu.hpp"
#include <emscripten/emscripten.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace {
std::unique_ptr<nes::Emulator> emulator;
std::vector<float> audio_buffer;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int nes_load_rom(const std::uint8_t* data, std::size_t size) {
    try {
        auto cartridge = nes::Cartridge::from_ines(std::span<const std::uint8_t>(data, size));
        emulator = std::make_unique<nes::Emulator>(std::move(cartridge));
        return 1;
    } catch (...) {
        emulator.reset();
        return 0;
    }
}
EMSCRIPTEN_KEEPALIVE void nes_reset() { if (emulator) emulator->reset(); }
EMSCRIPTEN_KEEPALIVE void nes_run_frame() { if (emulator) emulator->run_frame(); }
EMSCRIPTEN_KEEPALIVE void nes_set_controller(std::uint8_t buttons) {
    if (emulator) emulator->set_controller_state(0, buttons);
}
EMSCRIPTEN_KEEPALIVE const std::uint8_t* nes_framebuffer() {
    return emulator ? emulator->bus().ppu().framebuffer().data() : nullptr;
}
EMSCRIPTEN_KEEPALIVE std::size_t nes_framebuffer_size() {
    return nes::Ppu::screen_width * nes::Ppu::screen_height;
}
EMSCRIPTEN_KEEPALIVE const float* nes_audio_samples() {
    if (!emulator) {
        audio_buffer.clear();
        return nullptr;
    }
    audio_buffer = emulator->take_audio_samples();
    return audio_buffer.empty() ? nullptr : audio_buffer.data();
}
EMSCRIPTEN_KEEPALIVE std::size_t nes_audio_samples_size() {
    return audio_buffer.size();
}
}
