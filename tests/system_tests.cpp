#include "nes/cartridge.hpp"
#include "nes/emulator.hpp"
#include "test_harness.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> integration_cartridge() {
    constexpr std::size_t header = 16;
    constexpr std::size_t program_size = 16U * 1024U;
    constexpr std::size_t character_size = 8U * 1024U;
    std::vector<std::uint8_t> image(header + program_size + character_size, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = 1;

    const std::array<std::uint8_t, 82> program{
        0xA2, 0x00,              // LDX #$00
        0x8A,                    // TXA
        0x9D, 0x00, 0x02,        // STA $0200,X
        0xE8,                    // INX
        0xD0, 0xF9,              // BNE $8002
        0xA9, 0x02,              // LDA #$02
        0x8D, 0x14, 0x40,        // STA $4014
        0xA9, 0x80,              // LDA #$80
        0x8D, 0x00, 0x20,        // STA $2000
        0xA9, 0x3F,              // LDA #$3F
        0x8D, 0x06, 0x20,        // STA $2006
        0xA9, 0x00,              // LDA #$00
        0x8D, 0x06, 0x20,        // STA $2006
        0xA9, 0x0F,              // LDA #$0F
        0x8D, 0x07, 0x20,        // STA $2007
        0xA9, 0x30,              // LDA #$30
        0x8D, 0x07, 0x20,        // STA $2007
        0xA9, 0x1E,              // LDA #$1E
        0x8D, 0x01, 0x20,        // STA $2001
        0xA9, 0x01,              // LDA #$01
        0x8D, 0x15, 0x40,        // STA $4015
        0xA9, 0x1F,              // LDA #$1F
        0x8D, 0x00, 0x40,        // STA $4000
        0xA9, 0x08,              // LDA #$08
        0x8D, 0x02, 0x40,        // STA $4002
        0xA9, 0xF8,              // LDA #$F8
        0x8D, 0x03, 0x40,        // STA $4003
        0xA9, 0x01,              // LDA #$01
        0x8D, 0x16, 0x40,        // STA $4016
        0xA9, 0x00,              // LDA #$00
        0x8D, 0x16, 0x40,        // STA $4016
        0xAD, 0x16, 0x40,        // LDA $4016
        0x85, 0x00,              // STA $00
        0x4C, 0x4F, 0x80,        // JMP $804F
    };
    std::copy(program.begin(), program.end(),
              image.begin() + static_cast<std::ptrdiff_t>(header));
    image[header + 0x3FFAU] = 0x4F;
    image[header + 0x3FFBU] = 0x80;
    image[header + 0x3FFCU] = 0x00;
    image[header + 0x3FFDU] = 0x80;
    image[header + 0x3FFEU] = 0x4F;
    image[header + 0x3FFFU] = 0x80;
    std::fill_n(image.begin() + static_cast<std::ptrdiff_t>(header + program_size), 8, 0xFF);
    return nes::Cartridge::from_ines(image);
}

void integration_rom_drives_every_console_subsystem() {
    nes::Emulator emulator(integration_cartridge());
    emulator.set_controller_state(0, 0x01);
    emulator.run_frames(2);

    auto& bus = emulator.bus();
    expect(bus.cpu_read(0x0000, true) == 0x01,
           "integration ROM reads the latched controller A button");
    expect(bus.ppu().oam_read(0x00) == 0x00 && bus.ppu().oam_read(0xFF) == 0xFF,
           "integration ROM fills CPU RAM and transfers it through OAM DMA");
    expect((bus.ppu().state().control & 0x80U) != 0U,
           "integration ROM enables vertical-blank NMIs");
    expect((bus.ppu().state().mask & 0x1EU) == 0x1EU,
           "integration ROM enables background and sprite rendering");
    expect((bus.apu().cpu_read(0x4015, true) & 0x01U) != 0U,
           "integration ROM leaves the pulse channel active");
    expect(std::find(bus.ppu().framebuffer().begin(), bus.ppu().framebuffer().end(), 0x30) !=
               bus.ppu().framebuffer().end(),
           "integration ROM renders pattern data through its programmed palette");

    const auto audio = emulator.take_audio_samples();
    expect(std::any_of(audio.begin(), audio.end(),
                       [](float sample) { return sample > 0.001F; }),
           "integration ROM produces non-silent mixed APU output");
}

void complete_system_reset_is_repeatable() {
    nes::Emulator emulator(integration_cartridge());
    emulator.set_controller_state(0, 0x01);
    emulator.run_frame();
    emulator.reset();
    expect(emulator.completed_frames() == 0, "system reset clears runtime frame accounting");
    expect(emulator.bus().system_clock() == 0, "system reset restores the master clock");
    expect(emulator.bus().cpu().state().program_counter == 0x8000,
           "system reset reloads the cartridge reset vector");
    expect(emulator.bus().ppu().state().scanline == -1,
           "system reset restores the PPU pre-render scanline");
    expect(emulator.bus().apu().state().cpu_cycle == 0,
           "system reset restores the APU clock domain");
    emulator.run_frame();
    expect(emulator.completed_frames() == 1,
           "the complete system executes normally after a reset");
}

}  // namespace

int run_system_tests() {
    const auto before = nes::test::failures;
    integration_rom_drives_every_console_subsystem();
    complete_system_reset_is_repeatable();
    return nes::test::failures - before;
}
