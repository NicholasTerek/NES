#include "nes/apu.hpp"
#include "nes/bus.hpp"
#include "test_harness.hpp"

#include <cmath>
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
    clock_apu(apu, 29'830);
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
    clock_apu(apu, 29'830);
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
    clock_apu(apu, 3);
    expect(apu.state().quarter_frame_ticks == 1 && apu.state().half_frame_ticks == 1,
           "selecting five-step mode immediately clocks frame units");
    clock_apu(apu, 37'282);
    const auto state = apu.state();
    expect(state.quarter_frame_ticks == 5,
           "five-step APU mode clocks four scheduled quarter frames");
    expect(state.half_frame_ticks == 3,
           "five-step APU mode clocks two scheduled half frames");
    expect(!state.frame_irq, "five-step APU mode suppresses frame interrupts");
}

void frame_irq_inhibit_clears_and_suppresses_interrupts() {
    nes::Apu apu;
    clock_apu(apu, 29'830);
    apu.cpu_write(0x4017, 0x40);
    expect(!apu.irq_pending(), "frame IRQ inhibit clears an outstanding interrupt");
    clock_apu(apu, 29'830);
    expect(!apu.irq_pending(), "frame IRQ inhibit suppresses future interrupts");
}

void system_bus_clocks_and_maps_the_apu() {
    nes::Bus bus;
    bus.reset();
    for (std::uint32_t clock = 0; clock < 3U * 7'457U; ++clock) {
        bus.clock();
    }
    expect(bus.apu().state().cpu_cycle == 7'457U,
           "the system bus clocks the APU once per CPU cycle");
    expect(bus.apu().state().quarter_frame_ticks == 1,
           "the connected APU advances its frame sequencer");

    bus.cpu_write(0x4017, 0x80);
    for (int clock = 0; clock < 18; ++clock) {
        bus.clock();
    }
    expect(bus.apu().state().five_step_mode, "$4017 selects APU five-step mode");
    expect(bus.cpu_read(0x4015, true) == 0, "$4015 exposes APU status on the CPU bus");
}

void pulse_registers_load_length_and_generate_a_duty_wave() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x01);
    apu.cpu_write(0x4000, 0x1F);
    apu.cpu_write(0x4002, 0x08);
    apu.cpu_write(0x4003, 0xF8);
    expect(apu.state().pulse_length[0] == 30,
           "pulse length reload uses the hardware length table");
    expect((apu.cpu_read(0x4015, true) & 0x01U) != 0U,
           "APU status reports an active first pulse channel");

    apu.clock();
    apu.clock();
    expect(apu.state().pulse_level[0] == 15,
           "pulse timer advances into the configured duty waveform");
}

void half_frames_clock_pulse_length_and_sweep_units() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x03);
    apu.cpu_write(0x4000, 0x1F);
    apu.cpu_write(0x4002, 0x00);
    apu.cpu_write(0x4003, 0x01);
    apu.cpu_write(0x4001, 0x81);
    apu.cpu_write(0x4004, 0x1F);
    apu.cpu_write(0x4006, 0x00);
    apu.cpu_write(0x4007, 0x01);
    apu.cpu_write(0x4005, 0x89);
    const auto before = apu.state();
    clock_apu(apu, 14'913);
    const auto after = apu.state();
    expect(after.pulse_length[0] + 1U == before.pulse_length[0],
           "half-frame clocks decrement pulse length counters");
    expect(after.pulse_period[0] > before.pulse_period[0],
           "positive pulse sweep raises the first timer period");
    expect(after.pulse_period[1] < before.pulse_period[1],
           "negated pulse sweep lowers the second timer period");
}

void channel_enable_bits_clear_pulse_lengths() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x03);
    apu.cpu_write(0x4003, 0xF8);
    apu.cpu_write(0x4007, 0xF8);
    apu.cpu_write(0x4015, 0x02);
    expect(apu.state().pulse_length[0] == 0,
           "disabling pulse one immediately clears its length counter");
    expect(apu.state().pulse_length[1] != 0,
           "pulse two remains active when its enable bit stays set");
}

void triangle_linear_counter_gates_its_waveform() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x04);
    apu.cpu_write(0x4008, 0x82);
    apu.cpu_write(0x400A, 0x02);
    apu.cpu_write(0x400B, 0xF8);
    expect(apu.state().triangle_length == 30,
           "triangle high timer write loads its length counter");
    expect(apu.state().triangle_level == 0,
           "triangle stays silent until the linear counter reloads");
    clock_apu(apu, 7'457);
    expect(apu.state().triangle_linear == 2,
           "quarter-frame reloads the triangle linear counter");
    clock_apu(apu, 4);
    expect(apu.state().triangle_level != 0,
           "triangle timer produces its 32-step waveform when both counters are active");
}

void noise_channel_clocks_its_feedback_register() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x08);
    apu.cpu_write(0x400C, 0x1A);
    apu.cpu_write(0x400E, 0x80);
    apu.cpu_write(0x400F, 0xF8);
    expect(apu.state().noise_length == 30,
           "noise length reload uses the shared hardware length table");
    const auto before = apu.state().noise_shift;
    apu.clock();
    apu.clock();
    expect(apu.state().noise_shift != before,
           "noise timer advances the short-mode feedback register");
    expect(apu.state().noise_level == 10,
           "noise output uses the configured constant volume");
    expect((apu.cpu_read(0x4015, true) & 0x08U) != 0U,
           "APU status reports an active noise channel");
}

void triangle_and_noise_disable_bits_clear_lengths() {
    nes::Apu apu;
    apu.cpu_write(0x4015, 0x0C);
    apu.cpu_write(0x400B, 0xF8);
    apu.cpu_write(0x400F, 0xF8);
    apu.cpu_write(0x4015, 0x00);
    expect(apu.state().triangle_length == 0,
           "disabling triangle clears its length counter");
    expect(apu.state().noise_length == 0,
           "disabling noise clears its length counter");
}

void dmc_fetches_samples_and_stalls_the_cpu() {
    nes::Apu apu;
    std::uint16_t fetched_address = 0;
    apu.set_dmc_reader([&fetched_address](std::uint16_t address) {
        fetched_address = address;
        return std::uint8_t{0xFF};
    });
    apu.cpu_write(0x4010, 0x0F);
    apu.cpu_write(0x4011, 0x40);
    apu.cpu_write(0x4012, 0x02);
    apu.cpu_write(0x4013, 0x00);
    apu.cpu_write(0x4015, 0x10);
    apu.clock();
    expect(fetched_address == 0xC080,
           "DMC sample address register selects a byte in CPU program space");
    expect(apu.take_cpu_stall_cycles() == 4,
           "each DMC memory fetch requests four CPU stall cycles");
    expect(apu.state().dmc_address == 0xC081,
           "DMC advances its memory reader after fetching a sample byte");

    clock_apu(apu, 54);
    expect(apu.state().dmc_output == 0x42,
           "DMC output unit applies sample bits in two-level steps");
}

void dmc_loop_and_interrupt_controls_follow_status_registers() {
    nes::Apu interrupting;
    interrupting.set_dmc_reader([](std::uint16_t) { return std::uint8_t{0}; });
    interrupting.cpu_write(0x4010, 0x80);
    interrupting.cpu_write(0x4013, 0x00);
    interrupting.cpu_write(0x4015, 0x10);
    interrupting.clock();
    expect(interrupting.state().dmc_irq, "final DMC fetch raises an enabled interrupt");
    expect((interrupting.cpu_read(0x4015) & 0x80U) != 0U,
           "APU status reports DMC interrupts without acknowledging them");
    expect(interrupting.irq_pending(), "DMC interrupt remains asserted after a status read");
    interrupting.cpu_write(0x4015, 0x00);
    expect(!interrupting.irq_pending(), "$4015 writes acknowledge the DMC interrupt");

    nes::Apu looping;
    looping.set_dmc_reader([](std::uint16_t) { return std::uint8_t{0}; });
    looping.cpu_write(0x4010, 0xC0);
    looping.cpu_write(0x4013, 0x00);
    looping.cpu_write(0x4015, 0x10);
    looping.clock();
    expect(looping.state().dmc_bytes_remaining == 1,
           "DMC loop restarts the configured sample after its final fetch");
    expect(!looping.state().dmc_irq, "looping DMC playback does not raise an interrupt");
}

void nonlinear_mixer_combines_active_channels() {
    nes::Apu apu;
    apu.cpu_write(0x4011, 0x40);
    const auto dmc_only = apu.mixed_output();
    expect(dmc_only > 0.0F && dmc_only < 1.0F,
           "nonlinear mixer converts DMC level into normalized audio");

    apu.cpu_write(0x4015, 0x01);
    apu.cpu_write(0x4000, 0x1F);
    apu.cpu_write(0x4002, 0x08);
    apu.cpu_write(0x4003, 0xF8);
    apu.clock();
    apu.clock();
    expect(apu.mixed_output() > dmc_only,
           "pulse output is combined with the triangle-noise-DMC path");
}

void audio_resampler_buffers_stable_rate_output() {
    nes::Apu apu;
    clock_apu(apu, 40'585);
    expect(apu.buffered_samples() == 1'000,
           "APU resampler produces 44.1 kHz output from the NTSC CPU clock");
    const auto first = apu.pop_sample();
    expect(first.has_value() && std::abs(*first) < 0.0001F,
           "silent APU channels buffer silent audio samples");
    expect(apu.buffered_samples() == 999,
           "popping audio advances the output sample queue");
    apu.clear_samples();
    expect(apu.buffered_samples() == 0, "audio sample queue can be drained by a frontend");
}

}  // namespace

int run_apu_tests() {
    const auto before = nes::test::failures;
    four_step_sequence_clocks_envelopes_and_lengths();
    status_reads_report_and_acknowledge_frame_interrupts();
    five_step_sequence_never_raises_a_frame_interrupt();
    frame_irq_inhibit_clears_and_suppresses_interrupts();
    system_bus_clocks_and_maps_the_apu();
    pulse_registers_load_length_and_generate_a_duty_wave();
    half_frames_clock_pulse_length_and_sweep_units();
    channel_enable_bits_clear_pulse_lengths();
    triangle_linear_counter_gates_its_waveform();
    noise_channel_clocks_its_feedback_register();
    triangle_and_noise_disable_bits_clear_lengths();
    dmc_fetches_samples_and_stalls_the_cpu();
    dmc_loop_and_interrupt_controls_follow_status_registers();
    nonlinear_mixer_combines_active_channels();
    audio_resampler_buffers_stable_rate_output();
    return nes::test::failures - before;
}
