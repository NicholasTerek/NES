#include "nes/bus.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {

Bus::Bus() : cpu_(*this) {
    apu_.set_dmc_reader([this](std::uint16_t address) { return cpu_read(address); });
}

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
    apu_.reset();
    cpu_.reset();
    system_clock_counter_ = 0;
    dma_page_ = 0;
    dma_address_ = 0;
    dma_data_ = 0;
    dma_dummy_ = true;
    dma_transfer_ = false;
    dmc_stall_cycles_ = 0;
    controller_shift_.fill(0);
    controller_strobe_ = false;
    nmi_pending_ = false;
    nmi_delay_boundaries_ = 0;
}

void Bus::clock() {
    ppu_.clock();
    const auto delay_nmi = ppu_.nmi_requires_instruction_delay();
    if (ppu_.poll_nmi()) {
        nmi_pending_ = true;
        nmi_delay_boundaries_ = static_cast<std::uint8_t>(delay_nmi ? 1U : 0U);
    }
    if (system_clock_counter_ % 3U == 0U) {
        apu_.clock();
        dmc_stall_cycles_ = static_cast<std::uint8_t>(
            dmc_stall_cycles_ + apu_.take_cpu_stall_cycles());
        if (dma_transfer_) {
            if (dma_dummy_) {
                if (system_clock_counter_ % 2U == 1U) {
                    dma_dummy_ = false;
                }
            } else if (system_clock_counter_ % 2U == 0U) {
                const auto source = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(dma_page_) << 8U) | dma_address_);
                dma_data_ = cpu_read(source);
            } else {
                ppu_.cpu_write(0x0004, dma_data_);
                ++dma_address_;
                if (dma_address_ == 0U) {
                    dma_transfer_ = false;
                    dma_dummy_ = true;
                }
            }
        } else if (dmc_stall_cycles_ != 0U) {
            --dmc_stall_cycles_;
        } else {
            if (cpu_.instruction_complete()) {
                if (nmi_pending_) {
                    if (nmi_delay_boundaries_ != 0U) {
                        --nmi_delay_boundaries_;
                    } else {
                        cpu_.nmi();
                        nmi_pending_ = false;
                    }
                } else if (apu_.irq_pending()) {
                    cpu_.irq();
                }
            }
            cpu_.clock();
        }
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

Apu& Bus::apu() noexcept {
    return apu_;
}

const Apu& Bus::apu() const noexcept {
    return apu_;
}

std::uint64_t Bus::system_clock() const noexcept {
    return system_clock_counter_;
}

bool Bus::dma_active() const noexcept {
    return dma_transfer_;
}

void Bus::set_controller_state(std::size_t port, std::uint8_t buttons) {
    if (port >= controller_state_.size()) {
        throw std::out_of_range("controller port must be zero or one");
    }
    controller_state_[port] = buttons;
    if (controller_strobe_) {
        controller_shift_[port] = buttons;
    }
}

std::uint8_t Bus::controller_state(std::size_t port) const {
    if (port >= controller_state_.size()) {
        throw std::out_of_range("controller port must be zero or one");
    }
    return controller_state_[port];
}

std::shared_ptr<Cartridge> Bus::cartridge() const noexcept {
    return cartridge_;
}

std::uint8_t Bus::cpu_read(std::uint16_t address, bool read_only) {
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
    if (address == 0x4015U) {
        return apu_.cpu_read(address, read_only);
    }
    if (address == 0x4016U || address == 0x4017U) {
        const auto port = static_cast<std::size_t>(address & 0x0001U);
        const auto serial = controller_strobe_ ? controller_state_[port]
                                               : controller_shift_[port];
        const auto result = static_cast<std::uint8_t>(serial & 0x01U);
        if (!read_only && !controller_strobe_) {
            controller_shift_[port] =
                static_cast<std::uint8_t>((controller_shift_[port] >> 1U) | 0x80U);
        }
        return result;
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
        return;
    }
    if (address == 0x4014U) {
        dma_page_ = value;
        dma_address_ = 0;
        dma_transfer_ = true;
        dma_dummy_ = true;
        return;
    }
    if ((address >= 0x4000U && address <= 0x4013U) || address == 0x4015U ||
        address == 0x4017U) {
        apu_.cpu_write(address, value);
        return;
    }
    if (address == 0x4016U) {
        const bool strobe = (value & 0x01U) != 0U;
        if (strobe || controller_strobe_) {
            controller_shift_ = controller_state_;
        }
        controller_strobe_ = strobe;
    }
}

}  // namespace nes
