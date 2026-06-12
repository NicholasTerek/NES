#include "nes/bus.hpp"
#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>

namespace {

using nes::test::expect;

void clock_until(nes::Ppu& ppu, std::int16_t scanline, std::int16_t cycle) {
    constexpr std::uint32_t frame_clocks = 341U * 262U;
    for (std::uint32_t count = 0; count < frame_clocks; ++count) {
        const auto state = ppu.state();
        if (state.scanline == scanline && state.cycle == cycle) {
            return;
        }
        ppu.clock();
    }
    expect(false, "PPU reached the requested timing position");
}

void ppu_steps_across_scanlines_and_frames() {
    nes::Ppu ppu;
    expect(ppu.state().scanline == -1 && ppu.state().cycle == 0,
           "PPU reset begins on the pre-render scanline");
    for (int count = 0; count < 341; ++count) {
        ppu.clock();
    }
    expect(ppu.state().scanline == 0 && ppu.state().cycle == 0,
           "341 PPU clocks advance one scanline");

    for (std::uint32_t count = 341U; count < 341U * 262U; ++count) {
        ppu.clock();
    }
    expect(ppu.state().scanline == -1 && ppu.state().cycle == 0,
           "262 scanlines complete an NTSC frame");
    expect(ppu.state().frame_complete, "PPU marks completed frames");
    ppu.clear_frame_complete();
    expect(!ppu.state().frame_complete, "frame completion can be acknowledged");
}

void vertical_blank_tracks_the_timing_window() {
    nes::Ppu ppu;
    clock_until(ppu, 241, 1);
    ppu.clock();
    expect((ppu.state().status & 0x80U) != 0U, "PPU enters vertical blank at scanline 241");

    clock_until(ppu, -1, 1);
    ppu.clock();
    expect((ppu.state().status & 0x80U) == 0U,
           "pre-render scanline clears the vertical blank flag");
}

void system_clock_runs_the_ppu_three_times_faster() {
    nes::Bus bus;
    bus.reset();
    const auto cpu_cycles = bus.cpu().state().cycles;
    for (int count = 0; count < 9; ++count) {
        bus.clock();
    }
    expect(bus.system_clock() == 9, "bus records master clock cycles");
    expect(bus.cpu().state().cycles - cpu_cycles == 3,
           "CPU receives one clock for every three PPU clocks");
    expect(bus.ppu().state().cycle == 9, "PPU receives every master clock");
}

}  // namespace

int run_ppu_timing_tests() {
    const auto before = nes::test::failures;
    ppu_steps_across_scanlines_and_frames();
    vertical_blank_tracks_the_timing_window();
    system_clock_runs_the_ppu_three_times_faster();
    return nes::test::failures - before;
}
