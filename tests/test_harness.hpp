#pragma once

#include "nes/cpu.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace nes::test {

class FlatMemory final : public CpuBusDevice {
public:
    std::array<std::uint8_t, 65'536> bytes{};

    std::uint8_t cpu_read(std::uint16_t address, bool) override {
        return bytes[address];
    }

    void cpu_write(std::uint16_t address, std::uint8_t value) override {
        bytes[address] = value;
    }

    void set_reset_vector(std::uint16_t address) {
        bytes[0xFFFC] = static_cast<std::uint8_t>(address & 0xFFU);
        bytes[0xFFFD] = static_cast<std::uint8_t>(address >> 8U);
    }
};

inline int failures = 0;

inline void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

inline void drain(Cpu& cpu) {
    do {
        cpu.clock();
    } while (!cpu.instruction_complete());
}

inline std::uint64_t step(Cpu& cpu) {
    const auto start = cpu.state().cycles;
    drain(cpu);
    return cpu.state().cycles - start;
}

inline void reset(Cpu& cpu) {
    cpu.reset();
    drain(cpu);
}

}  // namespace nes::test
