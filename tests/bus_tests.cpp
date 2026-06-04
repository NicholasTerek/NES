#include "nes/bus.hpp"
#include "nes/cartridge.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> nrom_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U + 8U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = 1;
    image[16] = 0xA9;
    image[16U + 0x3FFCU] = 0x00;
    image[16U + 0x3FFDU] = 0x80;
    return nes::Cartridge::from_ines(image);
}

void internal_ram_repeats_through_its_cpu_window() {
    nes::Bus bus;
    bus.cpu_write(0x0002, 0x5A);
    expect(bus.cpu_read(0x0002) == 0x5A, "bus reads internal CPU RAM");
    expect(bus.cpu_read(0x0802) == 0x5A, "first CPU RAM mirror repeats data");
    expect(bus.cpu_read(0x1002) == 0x5A, "second CPU RAM mirror repeats data");
    expect(bus.cpu_read(0x1802) == 0x5A, "third CPU RAM mirror repeats data");

    bus.cpu_write(0x17FF, 0xA5);
    expect(bus.cpu_read(0x07FF) == 0xA5, "writes use the mirrored RAM address");
}

void cartridge_owns_the_program_rom_window() {
    nes::Bus bus;
    bus.insert_cartridge(nrom_cartridge());
    expect(bus.cpu_read(0x8000) == 0xA9, "bus routes high CPU reads to cartridge");
    expect(bus.cpu_read(0xC000) == 0xA9, "bus preserves NROM program mirroring");
    bus.cpu_write(0x8000, 0x00);
    expect(bus.cpu_read(0x8000) == 0xA9, "program ROM ignores CPU writes");
}

void reset_vector_flows_from_cartridge_to_cpu() {
    nes::Bus bus;
    const auto cartridge = nrom_cartridge();
    bus.insert_cartridge(cartridge);
    expect(bus.cartridge() == cartridge, "bus retains the inserted cartridge");
    bus.reset();
    nes::test::drain(bus.cpu());
    expect(bus.cpu().state().program_counter == 0x8000,
           "CPU reset vector is read through the cartridge bus");
}

void unmapped_cpu_addresses_have_open_bus_defaults() {
    nes::Bus bus;
    expect(bus.cpu_read(0x4000) == 0, "unmapped CPU reads return zero");
    bus.cpu_write(0x4000, 0xFF);
    expect(bus.cpu_read(0x4000) == 0, "unmapped CPU writes have no effect");
}

}  // namespace

int run_bus_tests() {
    const auto before = nes::test::failures;
    internal_ram_repeats_through_its_cpu_window();
    cartridge_owns_the_program_rom_window();
    reset_vector_flows_from_cartridge_to_cpu();
    unmapped_cpu_addresses_have_open_bus_defaults();
    return nes::test::failures - before;
}
