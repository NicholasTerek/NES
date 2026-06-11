#pragma once

#include "nes/bus_device.hpp"
#include "nes/cpu.hpp"
#include "nes/ppu.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace nes {

class Cartridge;

class Bus final : public CpuBusDevice {
public:
    Bus();

    void insert_cartridge(std::shared_ptr<Cartridge> cartridge);
    void reset();
    void clock();

    [[nodiscard]] Cpu& cpu() noexcept;
    [[nodiscard]] const Cpu& cpu() const noexcept;
    [[nodiscard]] Ppu& ppu() noexcept;
    [[nodiscard]] const Ppu& ppu() const noexcept;
    [[nodiscard]] std::shared_ptr<Cartridge> cartridge() const noexcept;

    std::uint8_t cpu_read(std::uint16_t address, bool read_only = false) override;
    void cpu_write(std::uint16_t address, std::uint8_t value) override;

private:
    static constexpr std::size_t cpu_ram_size = 2U * 1024U;

    Cpu cpu_;
    Ppu ppu_;
    std::array<std::uint8_t, cpu_ram_size> cpu_ram_{};
    std::shared_ptr<Cartridge> cartridge_;
};

}  // namespace nes
