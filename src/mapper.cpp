#include "nes/mapper.hpp"

namespace nes {

Mapper::Mapper(std::uint8_t program_banks, std::uint8_t character_banks)
    : program_banks_(program_banks), character_banks_(character_banks) {}

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

}  // namespace nes
