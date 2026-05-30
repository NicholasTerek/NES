#include "nes/cpu.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

class FlatMemory final : public nes::CpuBusDevice {
public:
    std::array<std::uint8_t, 65'536> bytes{};

    std::uint8_t cpu_read(std::uint16_t address, bool) override {
        return bytes[address];
    }

    void cpu_write(std::uint16_t address, std::uint8_t value) override {
        bytes[address] = value;
    }
};

template <typename Integer>
bool parse_number(std::string_view text, Integer& value) {
    const auto base = text.starts_with("0x") || text.starts_with("0X") ? 16 : 10;
    if (base == 16) {
        text.remove_prefix(2);
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void drain(nes::Cpu& cpu) {
    do {
        cpu.clock();
    } while (!cpu.instruction_complete());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: nes_cpu_functional IMAGE START_PC SUCCESS_PC "
                     "[MAX_INSTRUCTIONS] [ADDRESS=BYTE ...]\n";
        return 2;
    }

    std::uint16_t start = 0;
    std::uint16_t success = 0;
    std::uint64_t limit = 100'000'000;
    if (!parse_number(std::string_view{argv[2]}, start) ||
        !parse_number(std::string_view{argv[3]}, success) ||
        (argc >= 5 && !parse_number(std::string_view{argv[4]}, limit))) {
        std::cerr << "invalid numeric argument\n";
        return 2;
    }

    FlatMemory memory;
    std::ifstream image(argv[1], std::ios::binary);
    if (!image) {
        std::cerr << "could not open image\n";
        return 2;
    }
    image.read(reinterpret_cast<char*>(memory.bytes.data()),
               static_cast<std::streamsize>(memory.bytes.size()));
    if (image.bad() || (image.gcount() == 0 && !image.eof())) {
        std::cerr << "could not read image\n";
        return 2;
    }

    for (int index = 5; index < argc; ++index) {
        const std::string_view patch{argv[index]};
        const auto separator = patch.find('=');
        std::uint16_t address = 0;
        std::uint16_t value = 0;
        if (separator == std::string_view::npos ||
            !parse_number(patch.substr(0, separator), address) ||
            !parse_number(patch.substr(separator + 1), value) || value > 0xFFU) {
            std::cerr << "invalid memory patch\n";
            return 2;
        }
        memory.bytes[address] = static_cast<std::uint8_t>(value);
    }

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    cpu.state().program_counter = start;

    for (std::uint64_t instruction = 0; instruction < limit; ++instruction) {
        const auto before = cpu.state().program_counter;
        drain(cpu);
        const auto after = cpu.state().program_counter;
        if (after == before) {
            if (after == success) {
                std::cout << "functional test passed at 0x" << std::hex << after
                          << " after " << std::dec << instruction + 1 << " instructions\n";
                return 0;
            }
            std::cerr << "functional test trapped at 0x" << std::hex << after << '\n';
            return 1;
        }
    }

    std::cerr << "instruction limit reached\n";
    return 1;
}
