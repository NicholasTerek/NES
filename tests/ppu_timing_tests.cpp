#include "nes/bus.hpp"
#include "nes/cartridge.hpp"
#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

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

void vertical_blank_raises_one_nmi_request() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2000, 0x80);
    clock_until(ppu, 241, 1);
    ppu.clock();
    expect(ppu.state().nmi_pending, "enabled PPU raises an NMI at vertical blank");
    expect(ppu.poll_nmi(), "PPU exposes its pending NMI to the system bus");
    expect(!ppu.poll_nmi(), "PPU NMI request is consumed exactly once");
}

void enabling_nmi_during_vertical_blank_requests_it_immediately() {
    nes::Ppu ppu;
    clock_until(ppu, 241, 1);
    ppu.clock();
    expect(!ppu.state().nmi_pending, "disabled NMI does not fire at vertical blank");
    ppu.cpu_write(0x2000, 0x80);
    expect(ppu.state().nmi_pending, "enabling NMI during vertical blank requests one");
}

std::shared_ptr<nes::Cartridge> nmi_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U + 8U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = 1;
    std::fill(image.begin() + 16, image.begin() + 16 + 16 * 1024, 0xEA);
    image[16U + 0x3FFAU] = 0x00;
    image[16U + 0x3FFBU] = 0x90;
    image[16U + 0x3FFCU] = 0x00;
    image[16U + 0x3FFDU] = 0x80;
    return nes::Cartridge::from_ines(image);
}

void system_bus_delivers_ppu_nmi_to_the_cpu() {
    nes::Bus bus;
    bus.insert_cartridge(nmi_cartridge());
    bus.reset();
    bus.cpu_write(0x2000, 0x80);
    clock_until(bus.ppu(), 241, 1);
    bus.clock();
    expect(bus.cpu().state().program_counter == 0x9000,
           "system bus delivers the PPU NMI vector to the CPU");
    expect(bus.cpu().state().stack_pointer == 0xFA,
           "CPU pushes its return state when the PPU raises NMI");
}

}  // namespace

int run_ppu_timing_tests() {
    const auto before = nes::test::failures;
    ppu_steps_across_scanlines_and_frames();
    vertical_blank_tracks_the_timing_window();
    system_clock_runs_the_ppu_three_times_faster();
    vertical_blank_raises_one_nmi_request();
    enabling_nmi_during_vertical_blank_requests_it_immediately();
    system_bus_delivers_ppu_nmi_to_the_cpu();
    return nes::test::failures - before;
}
