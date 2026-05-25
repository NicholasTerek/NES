#include "nes/cpu.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void drain(nes::Cpu& cpu) {
    do {
        cpu.clock();
    } while (!cpu.instruction_complete());
}

void reset_uses_vector() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x34;
    memory.bytes[0xFFFD] = 0x12;
    nes::Cpu cpu(memory);

    cpu.reset();
    expect(cpu.state().program_counter == 0x1234, "reset reads the reset vector");
    expect(cpu.state().stack_pointer == 0xFD, "reset initializes the stack pointer");
    expect(cpu.flag(nes::Cpu::interrupt_disable), "reset masks IRQs");
}

void nmi_pushes_state_and_loads_vector() {
    FlatMemory memory;
    memory.bytes[0xFFFA] = 0x00;
    memory.bytes[0xFFFB] = 0x80;
    nes::Cpu cpu(memory);
    cpu.state().program_counter = 0x4567;
    cpu.state().stack_pointer = 0xFD;

    cpu.nmi();
    expect(cpu.state().program_counter == 0x8000, "NMI loads its vector");
    expect(memory.bytes[0x01FD] == 0x45, "NMI pushes PC high byte");
    expect(memory.bytes[0x01FC] == 0x67, "NMI pushes PC low byte");
}

void loads_stores_and_transfers_data() {
    FlatMemory memory;
    memory.bytes[0xFFFC] = 0x00;
    memory.bytes[0xFFFD] = 0x80;
    const std::array program{
        std::uint8_t{0xA9}, std::uint8_t{0x42},  // LDA #$42
        std::uint8_t{0x85}, std::uint8_t{0x10},  // STA $10
        std::uint8_t{0xA2}, std::uint8_t{0x7F},  // LDX #$7F
        std::uint8_t{0xE8},                      // INX
        std::uint8_t{0x8A},                      // TXA
    };
    std::copy(program.begin(), program.end(), memory.bytes.begin() + 0x8000);

    nes::Cpu cpu(memory);
    cpu.reset();
    drain(cpu);
    drain(cpu);
    expect(cpu.state().a == 0x42, "LDA loads the accumulator");
    drain(cpu);
    expect(memory.bytes[0x0010] == 0x42, "STA writes through the bus");
    drain(cpu);
    drain(cpu);
    expect(cpu.state().x == 0x80, "INX wraps and updates X");
    expect(cpu.flag(nes::Cpu::negative), "INX updates the negative flag");
    drain(cpu);
    expect(cpu.state().a == 0x80, "TXA transfers X into A");
}

}  // namespace

int main() {
    reset_uses_vector();
    nmi_pushes_state_and_loads_vector();
    loads_stores_and_transfers_data();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CPU tests passed\n";
    return EXIT_SUCCESS;
}
