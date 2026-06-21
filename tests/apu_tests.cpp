#include "nes/apu.hpp"
#include "nes/bus.hpp"
#include "test_harness.hpp"

#include <cstdint>

namespace {

using nes::test::expect;

void clock_apu(nes::Apu& apu, std::uint32_t cycles) {
    for (std::uint32_t cycle = 0; cycle < cycles; ++cycle) {
        apu.clock();
    }
}

void four_step_sequence_clocks_envelopes_and_lengths() {
    nes::Apu apu;
    apu.reset();
    clock_apu(apu, 14'915);
    const auto state = apu.state();
    expect(state.quarter_frame_ticks == 4,
           "four-step APU mode clocks four quarter-frame units");
    expect(state.half_frame_ticks == 2,
           "four-step APU mode clocks two half-frame units");
    expect(state.frame_cycle == 0, "four-step frame timing repeats after its IRQ step");
    expect(state.frame_irq, "four-step APU mode raises the frame interrupt");
}

void status_reads_report_and_acknowledge_frame_interrupts() {
    nes::Apu apu;
    clock_apu(apu, 14'915);
    expect((apu.cpu_read(0x4015, true) & 0x40U) != 0U,
           "read-only APU status reports the frame interrupt");
    expect(apu.irq_pending(), "read-only APU status preserves the frame interrupt");
    expect((apu.cpu_read(0x4015) & 0x40U) != 0U,
           "normal APU status reads return the frame interrupt");
    expect(!apu.irq_pending(), "normal APU status reads acknowledge the frame interrupt");
}

void five_step_sequence_never_raises_a_frame_interrupt() {
    nes::Apu apu;
    apu.cpu_write(0x4017, 0x80);
    expect(apu.state().quarter_frame_ticks == 1 && apu.state().half_frame_ticks == 1,
           "selecting five-step mode immediately clocks frame units");
    clock_apu(apu, 18'641);
    const auto state = apu.state();
    expect(state.quarter_frame_ticks == 5,
           "five-step APU mode clocks four scheduled quarter frames");
    expect(state.half_frame_ticks == 3,
           "five-step APU mode clocks two scheduled half frames");
    expect(!state.frame_irq, "five-step APU mode suppresses frame interrupts");
}

void frame_irq_inhibit_clears_and_suppresses_interrupts() {
    nes::Apu apu;
    clock_apu(apu, 14'915);
    apu.cpu_write(0x4017, 0x40);
    expect(!apu.irq_pending(), "frame IRQ inhibit clears an outstanding interrupt");
    clock_apu(apu, 14'915);
    expect(!apu.irq_pending(), "frame IRQ inhibit suppresses future interrupts");
}

void system_bus_clocks_and_maps_the_apu() {
    nes::Bus bus;
    bus.reset();
    for (std::uint32_t clock = 0; clock < 3U * 3'729U; ++clock) {
        bus.clock();
    }
    expect(bus.apu().state().cpu_cycle == 3'729U,
           "the system bus clocks the APU once per CPU cycle");
    expect(bus.apu().state().quarter_frame_ticks == 1,
           "the connected APU advances its frame sequencer");

    bus.cpu_write(0x4017, 0x80);
    expect(bus.apu().state().five_step_mode, "$4017 selects APU five-step mode");
    expect(bus.cpu_read(0x4015, true) == 0, "$4015 exposes APU status on the CPU bus");
}

}  // namespace

int run_apu_tests() {
    const auto before = nes::test::failures;
    four_step_sequence_clocks_envelopes_and_lengths();
    status_reads_report_and_acknowledge_frame_interrupts();
    five_step_sequence_never_raises_a_frame_interrupt();
    frame_irq_inhibit_clears_and_suppresses_interrupts();
    system_bus_clocks_and_maps_the_apu();
    return nes::test::failures - before;
}
