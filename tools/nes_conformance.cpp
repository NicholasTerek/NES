#include "nes/emulator.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::uint16_t status_address = 0x6000;
constexpr std::uint16_t signature_address = 0x6001;
constexpr std::uint16_t message_address = 0x6004;
constexpr std::uint8_t running = 0x80;
constexpr std::uint8_t reset_requested = 0x81;

struct Options {
    std::filesystem::path rom;
    std::size_t maximum_frames = 1'800;
};

[[noreturn]] void usage_error(std::string_view message) {
    throw std::invalid_argument(std::string(message) +
                                "\nusage: nes_conformance ROM [--frames N]");
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

Options parse_options(int argc, char** argv) {
    if (argc < 2) {
        usage_error("a ROM path is required");
    }
    Options options;
    options.rom = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string option = argv[index];
        if (option != "--frames" || index + 1 >= argc) {
            usage_error("unknown or incomplete option: " + option);
        }
        options.maximum_frames = parse_count(argv[++index]);
    }
    return options;
}

bool has_blargg_signature(nes::Bus& bus) {
    return bus.cpu_read(signature_address, true) == 0xDEU &&
           bus.cpu_read(signature_address + 1U, true) == 0xB0U &&
           bus.cpu_read(signature_address + 2U, true) == 0x61U;
}

std::string read_message(nes::Bus& bus) {
    std::string message;
    for (std::uint32_t address = message_address; address <= 0x7FFFU; ++address) {
        const auto byte = bus.cpu_read(static_cast<std::uint16_t>(address), true);
        if (byte == 0U) {
            break;
        }
        if (byte == '\n' || byte == '\r' || byte == '\t' ||
            (byte >= 0x20U && byte <= 0x7EU)) {
            message.push_back(static_cast<char>(byte));
        }
    }
    return message;
}

int run(const Options& options) {
    auto emulator = nes::Emulator::load(options.rom);
    bool signature_seen = false;
    bool reset_performed = false;

    for (std::size_t frame = 1; frame <= options.maximum_frames; ++frame) {
        emulator.run_frame();
        auto& bus = emulator.bus();
        if (!has_blargg_signature(bus)) {
            continue;
        }
        signature_seen = true;
        const auto status = bus.cpu_read(status_address, true);
        if (status == reset_requested && !reset_performed) {
            emulator.run_frames(7);
            emulator.reset();
            reset_performed = true;
            continue;
        }
        if (status == running || status == reset_requested) {
            continue;
        }

        const auto message = read_message(bus);
        std::cout << options.rom.filename().string() << ": status "
                  << static_cast<unsigned>(status) << " after " << frame << " frame(s)\n";
        if (!message.empty()) {
            std::cout << message;
            if (message.back() != '\n') {
                std::cout << '\n';
            }
        }
        return status == 0U ? 0 : 1;
    }

    if (!signature_seen) {
        std::cerr << options.rom.filename().string()
                  << ": no Blargg test signature after " << options.maximum_frames
                  << " frame(s)\n";
    } else {
        std::cerr << options.rom.filename().string() << ": test did not finish after "
                  << options.maximum_frames << " frame(s)\n";
        const auto message = read_message(emulator.bus());
        if (!message.empty()) {
            std::cerr << message;
            if (message.back() != '\n') {
                std::cerr << '\n';
            }
        }
    }
    const auto& state = emulator.bus().cpu().state();
    std::cerr << std::hex << std::uppercase << std::setfill('0')
              << "CPU PC=$" << std::setw(4) << state.program_counter
              << " A=$" << std::setw(2) << static_cast<unsigned>(state.a)
              << " X=$" << std::setw(2) << static_cast<unsigned>(state.x)
              << " Y=$" << std::setw(2) << static_cast<unsigned>(state.y)
              << " P=$" << std::setw(2) << static_cast<unsigned>(state.status)
              << " SP=$" << std::setw(2) << static_cast<unsigned>(state.stack_pointer)
              << std::dec << " cycles=" << state.cycles << '\n';
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(parse_options(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "nes_conformance: " << error.what() << '\n';
        return 2;
    }
}
