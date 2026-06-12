#include "nes/bus.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {

Bus::Bus() : cpu_(*this) {}

void Bus::insert_cartridge(std::shared_ptr<Cartridge> cartridge) {
    if (!cartridge) {
        throw std::invalid_argument("cannot insert an empty cartridge");
    }
    cartridge_ = std::move(cartridge);
    ppu_.connect_cartridge(cartridge_);
}

void Bus::reset() {
    if (cartridge_) {
        cartridge_->reset();
    }
    ppu_.reset();
    cpu_.reset();
    system_clock_counter_ = 0;
}

void Bus::clock() {
    ppu_.clock();
    if (system_clock_counter_ % 3U == 0U) {
        cpu_.clock();
    }
    ++system_clock_counter_;
}

Cpu& Bus::cpu() noexcept {
    return cpu_;
}

const Cpu& Bus::cpu() const noexcept {
    return cpu_;
}

Ppu& Bus::ppu() noexcept {
    return ppu_;
}

const Ppu& Bus::ppu() const noexcept {
    return ppu_;
}

std::uint64_t Bus::system_clock() const noexcept {
    return system_clock_counter_;
}

std::shared_ptr<Cartridge> Bus::cartridge() const noexcept {
    return cartridge_;
}

std::uint8_t Bus::cpu_read(std::uint16_t address, bool read_only) {
    static_cast<void>(read_only);

    std::uint8_t value = 0;
    if (cartridge_ && cartridge_->cpu_read(address, value)) {
        return value;
    }
    if (address <= 0x1FFFU) {
        return cpu_ram_[address & 0x07FFU];
    }
    if (address <= 0x3FFFU) {
        return ppu_.cpu_read(static_cast<std::uint16_t>(address & 0x0007U), read_only);
    }
    return 0;
}

void Bus::cpu_write(std::uint16_t address, std::uint8_t value) {
    if (cartridge_ && cartridge_->cpu_write(address, value)) {
        return;
    }
    if (address <= 0x1FFFU) {
        cpu_ram_[address & 0x07FFU] = value;
        return;
    }
    if (address <= 0x3FFFU) {
        ppu_.cpu_write(static_cast<std::uint16_t>(address & 0x0007U), value);
    }
}

}  // namespace nes
