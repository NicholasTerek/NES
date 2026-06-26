#include "nes/mapper.hpp"

namespace nes {

Mapper::Mapper(std::uint8_t program_banks, std::uint8_t character_banks)
    : program_banks_(program_banks), character_banks_(character_banks) {}

std::optional<Mirror> Mapper::mirror() const noexcept {
    return std::nullopt;
}

bool Mapper::program_ram_enabled() const noexcept {
    return true;
}

std::optional<std::uint32_t> Mapper0::cpu_read(std::uint16_t address) {
    if (address < 0x8000U) {
        return std::nullopt;
    }
    return address & (program_banks_ > 1U ? 0x7FFFU : 0x3FFFU);
}

Mapper::WriteMapping Mapper0::cpu_write(std::uint16_t address, std::uint8_t) {
    return {address >= 0x8000U, std::nullopt};
}

std::optional<std::uint32_t> Mapper0::ppu_read(std::uint16_t address) {
    if (address <= 0x1FFFU) {
        return address;
    }
    return std::nullopt;
}

Mapper::WriteMapping Mapper0::ppu_write(std::uint16_t address) {
    if (address <= 0x1FFFU && character_banks_ == 0U) {
        return {true, address};
    }
    return {address <= 0x1FFFU, std::nullopt};
}

void Mapper0::reset() {}

std::optional<std::uint32_t> Mapper2::cpu_read(std::uint16_t address) {
    if (address >= 0x8000U && address <= 0xBFFFU) {
        return static_cast<std::uint32_t>(selected_bank_) * 0x4000U + (address & 0x3FFFU);
    }
    if (address >= 0xC000U) {
        return static_cast<std::uint32_t>(program_banks_ - 1U) * 0x4000U +
               (address & 0x3FFFU);
    }
    return std::nullopt;
}

Mapper::WriteMapping Mapper2::cpu_write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000U) {
        return {};
    }
    selected_bank_ = static_cast<std::uint8_t>(value % program_banks_);
    return {true, std::nullopt};
}

std::optional<std::uint32_t> Mapper2::ppu_read(std::uint16_t address) {
    if (address <= 0x1FFFU) {
        return address;
    }
    return std::nullopt;
}

Mapper::WriteMapping Mapper2::ppu_write(std::uint16_t address) {
    if (address <= 0x1FFFU && character_banks_ == 0U) {
        return {true, address};
    }
    return {address <= 0x1FFFU, std::nullopt};
}

void Mapper2::reset() {
    selected_bank_ = 0;
}

std::optional<std::uint32_t> Mapper1::cpu_read(std::uint16_t address) {
    if (address < 0x8000U) {
        return std::nullopt;
    }

    const auto mode = static_cast<std::uint8_t>((control_ >> 2U) & 0x03U);
    const auto offset = static_cast<std::uint32_t>(address & 0x3FFFU);
    std::uint8_t bank = 0;
    if (mode <= 1U) {
        const auto first = static_cast<std::uint8_t>(program_bank_ & 0x0EU);
        bank = static_cast<std::uint8_t>(first + (address >= 0xC000U ? 1U : 0U));
    } else if (mode == 2U) {
        bank = address < 0xC000U ? 0U
                                 : static_cast<std::uint8_t>(program_bank_ & 0x0FU);
    } else {
        bank = address < 0xC000U ? static_cast<std::uint8_t>(program_bank_ & 0x0FU)
                                 : static_cast<std::uint8_t>(program_banks_ - 1U);
    }
    bank = static_cast<std::uint8_t>(bank % program_banks_);
    return static_cast<std::uint32_t>(bank) * 0x4000U + offset;
}

Mapper::WriteMapping Mapper1::cpu_write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000U) {
        return {};
    }
    if ((value & 0x80U) != 0U) {
        shift_register_ = 0x10;
        control_ = static_cast<std::uint8_t>(control_ | 0x0CU);
        return {true, std::nullopt};
    }

    const bool complete = (shift_register_ & 0x01U) != 0U;
    shift_register_ = static_cast<std::uint8_t>(
        (shift_register_ >> 1U) | ((value & 0x01U) << 4U));
    if (!complete) {
        return {true, std::nullopt};
    }

    const auto data = static_cast<std::uint8_t>(shift_register_ & 0x1FU);
    if (address <= 0x9FFFU) {
        control_ = data;
    } else if (address <= 0xBFFFU) {
        character_bank_0_ = data;
    } else if (address <= 0xDFFFU) {
        character_bank_1_ = data;
    } else {
        program_bank_ = data;
    }
    shift_register_ = 0x10;
    return {true, std::nullopt};
}

std::optional<std::uint32_t> Mapper1::ppu_read(std::uint16_t address) {
    if (address > 0x1FFFU) {
        return std::nullopt;
    }
    return character_address(address);
}

Mapper::WriteMapping Mapper1::ppu_write(std::uint16_t address) {
    if (address > 0x1FFFU) {
        return {};
    }
    if (character_banks_ == 0U) {
        return {true, character_address(address)};
    }
    return {true, std::nullopt};
}

std::optional<Mirror> Mapper1::mirror() const noexcept {
    switch (control_ & 0x03U) {
    case 0:
        return Mirror::one_screen_low;
    case 1:
        return Mirror::one_screen_high;
    case 2:
        return Mirror::vertical;
    default:
        return Mirror::horizontal;
    }
}

bool Mapper1::program_ram_enabled() const noexcept {
    return (program_bank_ & 0x10U) == 0U;
}

void Mapper1::reset() {
    shift_register_ = 0x10;
    control_ = 0x0C;
    character_bank_0_ = 0;
    character_bank_1_ = 0;
    program_bank_ = 0;
}

std::uint32_t Mapper1::character_address(std::uint16_t address) const noexcept {
    const auto four_kilobyte_banks = static_cast<std::uint8_t>(
        character_banks_ == 0U ? 2U : character_banks_ * 2U);
    const bool split = (control_ & 0x10U) != 0U;
    std::uint8_t bank = 0;
    if (!split) {
        bank = static_cast<std::uint8_t>((character_bank_0_ & 0x1EU) +
                                         (address >= 0x1000U ? 1U : 0U));
    } else {
        bank = address < 0x1000U ? character_bank_0_ : character_bank_1_;
    }
    bank = static_cast<std::uint8_t>(bank % four_kilobyte_banks);
    return static_cast<std::uint32_t>(bank) * 0x1000U + (address & 0x0FFFU);
}

}  // namespace nes
