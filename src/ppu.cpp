#include "nes/ppu.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {
namespace {

constexpr std::uint8_t control_increment_mode = 0x04;
constexpr std::uint8_t mask_grayscale = 0x01;
constexpr std::uint8_t status_vertical_blank = 0x80;

}  // namespace

void Ppu::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
    if (!cartridge) {
        throw std::invalid_argument("cannot connect an empty cartridge to the PPU");
    }
    cartridge_ = std::move(cartridge);
}

std::uint8_t Ppu::cpu_read(std::uint16_t address, bool read_only) {
    const auto selected_register = static_cast<std::uint8_t>(address & 0x0007U);
    if (read_only) {
        switch (selected_register) {
        case 0:
            return control_;
        case 1:
            return mask_;
        case 2:
            return status_;
        default:
            return open_bus_;
        }
    }

    std::uint8_t value = open_bus_;
    switch (selected_register) {
    case 2:
        value = static_cast<std::uint8_t>((status_ & 0xE0U) | (open_bus_ & 0x1FU));
        status_ = static_cast<std::uint8_t>(status_ & ~status_vertical_blank);
        write_latch_ = false;
        break;
    case 7: {
        const auto address_before_increment = vram_address_;
        const auto fetched = ppu_read(address_before_increment);
        if ((address_before_increment & 0x3FFFU) >= 0x3F00U) {
            value = fetched;
            data_buffer_ = ppu_read(static_cast<std::uint16_t>(address_before_increment - 0x1000U));
        } else {
            value = data_buffer_;
            data_buffer_ = fetched;
        }
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ + ((control_ & control_increment_mode) != 0U ? 32U : 1U)) &
            0x7FFFU);
        break;
    }
    default:
        break;
    }
    open_bus_ = value;
    return value;
}

void Ppu::cpu_write(std::uint16_t address, std::uint8_t value) {
    open_bus_ = value;
    switch (address & 0x0007U) {
    case 0:
        control_ = value;
        temporary_address_ = static_cast<std::uint16_t>(
            (temporary_address_ & 0xF3FFU) | (static_cast<std::uint16_t>(value & 0x03U) << 10U));
        break;
    case 1:
        mask_ = value;
        break;
    case 5:
        if (!write_latch_) {
            fine_x_ = static_cast<std::uint8_t>(value & 0x07U);
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0xFFE0U) | (static_cast<std::uint16_t>(value) >> 3U));
            write_latch_ = true;
        } else {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x8C1FU) |
                (static_cast<std::uint16_t>(value & 0x07U) << 12U) |
                (static_cast<std::uint16_t>(value & 0xF8U) << 2U));
            write_latch_ = false;
        }
        break;
    case 6:
        if (!write_latch_) {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x00FFU) |
                (static_cast<std::uint16_t>(value & 0x3FU) << 8U));
            write_latch_ = true;
        } else {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x7F00U) | static_cast<std::uint16_t>(value));
            vram_address_ = temporary_address_;
            write_latch_ = false;
        }
        break;
    case 7:
        ppu_write(vram_address_, value);
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ + ((control_ & control_increment_mode) != 0U ? 32U : 1U)) &
            0x7FFFU);
        break;
    default:
        break;
    }
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
    return static_cast<std::uint8_t>(
        palette_ram_[palette_index(address)] &
        ((mask_ & mask_grayscale) != 0U ? 0x30U : 0x3FU));
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

void Ppu::reset() {
    control_ = 0;
    mask_ = 0;
    status_ = 0;
    vram_address_ = 0;
    temporary_address_ = 0;
    fine_x_ = 0;
    write_latch_ = false;
    data_buffer_ = 0;
    open_bus_ = 0;
}

Ppu::State Ppu::state() const noexcept {
    return {control_,         mask_,         status_,     vram_address_,
            temporary_address_, fine_x_,      write_latch_, data_buffer_};
}

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
