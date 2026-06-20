#pragma once

#include "nes/bus_device.hpp"
#include "nes/cpu.hpp"
#include "nes/ppu.hpp"

#include <array>
#include <cstddef>
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
    [[nodiscard]] std::uint64_t system_clock() const noexcept;
    [[nodiscard]] bool dma_active() const noexcept;
    void set_controller_state(std::size_t port, std::uint8_t buttons);
    [[nodiscard]] std::uint8_t controller_state(std::size_t port) const;
    [[nodiscard]] std::shared_ptr<Cartridge> cartridge() const noexcept;

    std::uint8_t cpu_read(std::uint16_t address, bool read_only = false) override;
    void cpu_write(std::uint16_t address, std::uint8_t value) override;

private:
    static constexpr std::size_t cpu_ram_size = 2U * 1024U;

    Cpu cpu_;
    Ppu ppu_;
    std::array<std::uint8_t, cpu_ram_size> cpu_ram_{};
    std::shared_ptr<Cartridge> cartridge_;
    std::uint64_t system_clock_counter_ = 0;
    std::uint8_t dma_page_ = 0;
    std::uint8_t dma_address_ = 0;
    std::uint8_t dma_data_ = 0;
    bool dma_dummy_ = true;
    bool dma_transfer_ = false;
    std::array<std::uint8_t, 2> controller_state_{};
    std::array<std::uint8_t, 2> controller_shift_{};
    bool controller_strobe_ = false;
};

}  // namespace nes
