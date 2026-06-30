#include "nes/cartridge.hpp"
#include "nes/emulator.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> runtime_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U + 8U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = 1;
    image[16] = 0xEA;
    image[17] = 0x4C;
    image[18] = 0x00;
    image[19] = 0x80;
    image[16U + 0x3FFCU] = 0x00;
    image[16U + 0x3FFDU] = 0x80;
    return nes::Cartridge::from_ines(image);
}

void runtime_executes_complete_video_frames() {
    nes::Emulator emulator(runtime_cartridge());
    emulator.run_frame();
    expect(emulator.completed_frames() == 1, "runtime counts completed PPU frames");
    expect(emulator.bus().system_clock() == 341U * 262U,
           "runtime advances exactly one non-rendering NTSC PPU frame");
    expect(emulator.bus().cpu().state().cycles > 29'000U,
           "runtime advances the CPU alongside the PPU");
    expect(!emulator.bus().ppu().state().frame_complete,
           "runtime acknowledges the completed PPU frame");
}

void runtime_collects_audio_and_controller_input() {
    nes::Emulator emulator(runtime_cartridge());
    emulator.set_controller_state(0, 0xA5);
    emulator.set_controller_state(1, 0x5A);
    emulator.run_frames(2);
    expect(emulator.completed_frames() == 2, "runtime can execute multiple frames");
    expect(emulator.bus().controller_state(0) == 0xA5,
           "runtime forwards first controller state to the bus");
    expect(emulator.bus().controller_state(1) == 0x5A,
           "runtime forwards second controller state to the bus");
    const auto audio = emulator.take_audio_samples();
    expect(audio.size() >= 1'460U && audio.size() <= 1'480U,
           "runtime collects one frame-time of APU output per video frame");
    expect(emulator.take_audio_samples().empty(),
           "taking runtime audio drains the collected sample queue");
}

void runtime_rejects_an_empty_cartridge() {
    bool rejected = false;
    try {
        nes::Emulator emulator(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "runtime rejects construction without a cartridge");
}

}  // namespace

int run_emulator_tests() {
    const auto before = nes::test::failures;
    runtime_executes_complete_video_frames();
    runtime_collects_audio_and_controller_input();
    runtime_rejects_an_empty_cartridge();
    return nes::test::failures - before;
}
