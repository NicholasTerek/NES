#include "nes/apu.hpp"
#include "nes/cartridge.hpp"
#include "nes/emulator.hpp"
#include "nes/ppu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    std::filesystem::path rom;
    std::size_t frames = 1;
    std::filesystem::path frame_output;
    std::filesystem::path audio_output;
    std::uint8_t controller_one = 0;
    std::uint8_t controller_two = 0;
};

constexpr std::array<std::array<std::uint8_t, 3>, 64> nes_palette{{
    {{84, 84, 84}}, {{0, 30, 116}}, {{8, 16, 144}}, {{48, 0, 136}},
    {{68, 0, 100}}, {{92, 0, 48}}, {{84, 4, 0}}, {{60, 24, 0}},
    {{32, 42, 0}}, {{8, 58, 0}}, {{0, 64, 0}}, {{0, 60, 0}},
    {{0, 50, 60}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{152, 150, 152}}, {{8, 76, 196}}, {{48, 50, 236}}, {{92, 30, 228}},
    {{136, 20, 176}}, {{160, 20, 100}}, {{152, 34, 32}}, {{120, 60, 0}},
    {{84, 90, 0}}, {{40, 114, 0}}, {{8, 124, 0}}, {{0, 118, 40}},
    {{0, 102, 120}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{236, 238, 236}}, {{76, 154, 236}}, {{120, 124, 236}}, {{176, 98, 236}},
    {{228, 84, 236}}, {{236, 88, 180}}, {{236, 106, 100}}, {{212, 136, 32}},
    {{160, 170, 0}}, {{116, 196, 0}}, {{76, 208, 32}}, {{56, 204, 108}},
    {{56, 180, 204}}, {{60, 60, 60}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{236, 238, 236}}, {{168, 204, 236}}, {{188, 188, 236}}, {{212, 178, 236}},
    {{236, 174, 236}}, {{236, 174, 212}}, {{236, 180, 176}}, {{228, 196, 144}},
    {{204, 210, 120}}, {{180, 222, 120}}, {{168, 226, 144}}, {{152, 226, 180}},
    {{160, 214, 228}}, {{160, 162, 160}}, {{0, 0, 0}}, {{0, 0, 0}},
}};

[[noreturn]] void usage_error(std::string_view message) {
    throw std::invalid_argument(std::string(message) +
                                "\nusage: nes_runner ROM [--frames N] [--frame FILE.ppm] "
                                "[--audio FILE.wav] [--controller1 BYTE] [--controller2 BYTE]");
}

std::size_t parse_count(const std::string& text) {
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed, 0);
    if (consumed != text.size() || value == 0U ||
        value > std::numeric_limits<std::size_t>::max()) {
        usage_error("frame count must be a positive integer");
    }
    return static_cast<std::size_t>(value);
}

std::uint8_t parse_buttons(const std::string& text) {
    std::size_t consumed = 0;
    const auto value = std::stoul(text, &consumed, 0);
    if (consumed != text.size() || value > 0xFFU) {
        usage_error("controller state must fit in one byte");
    }
    return static_cast<std::uint8_t>(value);
}

Options parse_options(int argc, char** argv) {
    if (argc < 2) {
        usage_error("a ROM path is required");
    }
    Options options;
    options.rom = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string option = argv[index];
        if (index + 1 >= argc) {
            usage_error("option is missing its value: " + option);
        }
        const std::string value = argv[++index];
        if (option == "--frames") {
            options.frames = parse_count(value);
        } else if (option == "--frame") {
            options.frame_output = value;
        } else if (option == "--audio") {
            options.audio_output = value;
        } else if (option == "--controller1") {
            options.controller_one = parse_buttons(value);
        } else if (option == "--controller2") {
            options.controller_two = parse_buttons(value);
        } else {
            usage_error("unknown option: " + option);
        }
    }
    return options;
}

void write_frame(const std::filesystem::path& path, const nes::Ppu::Framebuffer& frame) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create frame output: " + path.string());
    }
    output << "P6\n" << nes::Ppu::screen_width << ' ' << nes::Ppu::screen_height << "\n255\n";
    for (const auto palette_index : frame) {
        const auto& color = nes_palette[palette_index & 0x3FU];
        output.write(reinterpret_cast<const char*>(color.data()),
                     static_cast<std::streamsize>(color.size()));
    }
    if (!output) {
        throw std::runtime_error("could not write frame output: " + path.string());
    }
}

void write_u16(std::ostream& output, std::uint16_t value) {
    const std::array bytes{static_cast<char>(value & 0xFFU),
                           static_cast<char>((value >> 8U) & 0xFFU)};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    const std::array bytes{static_cast<char>(value & 0xFFU),
                           static_cast<char>((value >> 8U) & 0xFFU),
                           static_cast<char>((value >> 16U) & 0xFFU),
                           static_cast<char>((value >> 24U) & 0xFFU)};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_audio(const std::filesystem::path& path, const std::vector<float>& samples) {
    if (samples.size() > (std::numeric_limits<std::uint32_t>::max() - 44U) / 2U) {
        throw std::runtime_error("audio output is too large for a WAV file");
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create audio output: " + path.string());
    }
    const auto data_bytes = static_cast<std::uint32_t>(samples.size() * 2U);
    output.write("RIFF", 4);
    write_u32(output, 36U + data_bytes);
    output.write("WAVEfmt ", 8);
    write_u32(output, 16);
    write_u16(output, 1);
    write_u16(output, 1);
    write_u32(output, nes::Apu::output_sample_rate);
    write_u32(output, nes::Apu::output_sample_rate * 2U);
    write_u16(output, 2);
    write_u16(output, 16);
    output.write("data", 4);
    write_u32(output, data_bytes);

    float previous_input = 0.0F;
    float previous_output = 0.0F;
    for (const auto sample : samples) {
        const auto filtered = sample - previous_input + 0.995F * previous_output;
        previous_input = sample;
        previous_output = filtered;
        const auto clamped = std::clamp(filtered, -1.0F, 1.0F);
        const auto pcm = static_cast<std::int16_t>(std::lround(clamped * 32'767.0F));
        write_u16(output, static_cast<std::uint16_t>(pcm));
    }
    if (!output) {
        throw std::runtime_error("could not write audio output: " + path.string());
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        auto emulator = nes::Emulator::load(options.rom);
        emulator.set_controller_state(0, options.controller_one);
        emulator.set_controller_state(1, options.controller_two);
        emulator.run_frames(options.frames);
        auto audio = emulator.take_audio_samples();

        if (!options.frame_output.empty()) {
            write_frame(options.frame_output, emulator.bus().ppu().framebuffer());
        }
        if (!options.audio_output.empty()) {
            write_audio(options.audio_output, audio);
        }
        std::cout << "completed " << emulator.completed_frames() << " frame(s), "
                  << emulator.bus().cpu().state().cycles << " CPU cycles, "
                  << audio.size() << " audio samples, mapper "
                  << static_cast<unsigned>(emulator.bus().cartridge()->mapper_id()) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nes_runner: " << error.what() << '\n';
        return 1;
    }
}
