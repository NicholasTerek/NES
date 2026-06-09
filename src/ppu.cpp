#include "nes/ppu.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {

void Ppu::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
    if (!cartridge) {
        throw std::invalid_argument("cannot connect an empty cartridge to the PPU");
    }
    cartridge_ = std::move(cartridge);
}

std::uint8_t Ppu::ppu_read(std::uint16_t address, bool read_only) {
    static_cast<void>(read_only);
    address &= 0x3FFFU;

    std::uint8_t value = 0;
    if (cartridge_ && cartridge_->ppu_read(address, value)) {
        return value;
    }
    if (address <= 0x1FFFU) {
        return pattern_ram_[address];
    }
    if (address <= 0x3EFFU) {
        return nametable_ram_[nametable_index(address)];
    }
    return static_cast<std::uint8_t>(palette_ram_[palette_index(address)] & 0x3FU);
}

void Ppu::ppu_write(std::uint16_t address, std::uint8_t value) {
    address &= 0x3FFFU;

    if (cartridge_ && cartridge_->ppu_write(address, value)) {
        return;
    }
    if (address <= 0x1FFFU) {
        pattern_ram_[address] = value;
        return;
    }
    if (address <= 0x3EFFU) {
        nametable_ram_[nametable_index(address)] = value;
        return;
    }
    palette_ram_[palette_index(address)] = static_cast<std::uint8_t>(value & 0x3FU);
}

void Ppu::reset() {}

std::size_t Ppu::nametable_index(std::uint16_t address) const {
    const auto offset = static_cast<std::uint16_t>((address - 0x2000U) & 0x0FFFU);
    const auto table = static_cast<std::uint8_t>(offset / 0x0400U);
    const auto cell = static_cast<std::size_t>(offset & 0x03FFU);
    const auto mirror = cartridge_ ? cartridge_->mirror() : Mirror::horizontal;

    std::uint8_t physical_table = 0;
    switch (mirror) {
    case Mirror::vertical:
        physical_table = static_cast<std::uint8_t>(table & 0x01U);
        break;
    case Mirror::horizontal:
        physical_table = static_cast<std::uint8_t>(table >> 1U);
        break;
    case Mirror::four_screen:
        physical_table = table;
        break;
    case Mirror::one_screen_low:
        physical_table = 0;
        break;
    case Mirror::one_screen_high:
        physical_table = 1;
        break;
    }
    return static_cast<std::size_t>(physical_table) * 0x0400U + cell;
}

std::size_t Ppu::palette_index(std::uint16_t address) {
    auto index = static_cast<std::uint8_t>(address & 0x001FU);
    if (index == 0x10U || index == 0x14U || index == 0x18U || index == 0x1CU) {
        index = static_cast<std::uint8_t>(index - 0x10U);
    }
    return index;
}

}  // namespace nes
