#include "nes/cpu.hpp"

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

}  // namespace

int main() {
    reset_uses_vector();
    nmi_pushes_state_and_loads_vector();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CPU tests passed\n";
    return EXIT_SUCCESS;
}
