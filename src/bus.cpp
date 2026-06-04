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
}

void Bus::reset() {
    if (cartridge_) {
        cartridge_->reset();
    }
    cpu_.reset();
}

void Bus::clock() {
    cpu_.clock();
}

Cpu& Bus::cpu() noexcept {
    return cpu_;
}

const Cpu& Bus::cpu() const noexcept {
    return cpu_;
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
    return 0;
}

void Bus::cpu_write(std::uint16_t address, std::uint8_t value) {
    if (cartridge_ && cartridge_->cpu_write(address, value)) {
        return;
    }
    if (address <= 0x1FFFU) {
        cpu_ram_[address & 0x07FFU] = value;
    }
}

}  // namespace nes
