#include "nes/cartridge.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace nes {
namespace {

constexpr std::size_t header_size = 16;
constexpr std::size_t trainer_size = 512;
constexpr std::size_t program_bank_size = 16 * 1024;
constexpr std::size_t program_ram_bank_size = 8 * 1024;
constexpr std::size_t character_bank_size = 8 * 1024;
constexpr std::uint16_t program_ram_start = 0x6000;
constexpr std::uint16_t program_ram_end = 0x7FFF;
constexpr std::size_t trainer_ram_offset = 0x1000;

bool contains_range(std::size_t image_size, std::size_t offset, std::size_t length) {
    return offset <= image_size && length <= image_size - offset;
}

void validate_mapper_layout(std::uint8_t mapper_id,
                            std::uint8_t program_banks,
                            std::uint8_t character_banks) {
    switch (mapper_id) {
    case 0:
        if (program_banks > 2U || character_banks > 1U) {
            throw std::invalid_argument("NROM image has an unsupported ROM layout");
        }
        break;
    case 2:
        if (program_banks < 2U || character_banks != 0U) {
            throw std::invalid_argument("UxROM image has an unsupported ROM layout");
        }
        break;
    default:
        throw std::invalid_argument("unsupported mapper " + std::to_string(mapper_id));
    }
}

}  // namespace

std::shared_ptr<Cartridge> Cartridge::load(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("could not open ROM: " + path.string());
    }

    const std::vector<std::uint8_t> image(std::istreambuf_iterator<char>(stream), {});
    return from_ines(image);
}

std::shared_ptr<Cartridge> Cartridge::from_ines(std::span<const std::uint8_t> image) {
    if (image.size() < header_size || image[0] != 'N' || image[1] != 'E' ||
        image[2] != 'S' || image[3] != 0x1AU) {
        throw std::invalid_argument("ROM does not contain an iNES header");
    }

    const auto program_banks = image[4];
    const auto character_banks = image[5];
    const auto flags6 = image[6];
    const auto flags7 = image[7];
    const auto program_ram_banks = image[8] == 0U ? 1U : image[8];
    const auto format_bits = static_cast<std::uint8_t>(flags7 & 0x0CU);
    if (format_bits != 0U) {
        throw std::invalid_argument(format_bits == 0x08U
                                        ? "NES 2.0 images are not supported"
                                        : "unrecognized iNES header format");
    }
    if (program_banks == 0U) {
        throw std::invalid_argument("iNES image does not contain program ROM");
    }

    auto cartridge = std::shared_ptr<Cartridge>(new Cartridge());
    cartridge->mapper_id_ = static_cast<std::uint8_t>((flags7 & 0xF0U) | (flags6 >> 4U));
    validate_mapper_layout(cartridge->mapper_id_, program_banks, character_banks);
    cartridge->has_battery_ = (flags6 & 0x02U) != 0U;
    if ((flags6 & 0x08U) != 0U) {
        cartridge->mirror_ = Mirror::four_screen;
    } else {
        cartridge->mirror_ = (flags6 & 0x01U) != 0U ? Mirror::vertical : Mirror::horizontal;
    }

    const auto has_trainer = (flags6 & 0x04U) != 0U;
    std::size_t offset = header_size + (has_trainer ? trainer_size : 0U);
    const auto program_bytes = static_cast<std::size_t>(program_banks) * program_bank_size;
    const auto program_ram_bytes =
        static_cast<std::size_t>(program_ram_banks) * program_ram_bank_size;
    const auto character_bytes = static_cast<std::size_t>(character_banks) * character_bank_size;
    if (!contains_range(image.size(), offset, program_bytes)) {
        throw std::invalid_argument("iNES image has truncated program ROM");
    }
    if (!contains_range(image.size(), offset + program_bytes, character_bytes)) {
        throw std::invalid_argument("iNES image is truncated");
    }

    cartridge->program_ram_.assign(program_ram_bytes, 0);
    if (has_trainer) {
        std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(header_size), trainer_size,
                    cartridge->program_ram_.begin() +
                        static_cast<std::ptrdiff_t>(trainer_ram_offset));
    }

    cartridge->program_memory_.assign(image.begin() + static_cast<std::ptrdiff_t>(offset),
                                      image.begin() + static_cast<std::ptrdiff_t>(offset + program_bytes));
    offset += program_bytes;
    if (character_banks == 0U) {
        cartridge->character_memory_.assign(character_bank_size, 0);
    } else {
        cartridge->character_memory_.assign(
            image.begin() + static_cast<std::ptrdiff_t>(offset),
            image.begin() + static_cast<std::ptrdiff_t>(offset + character_bytes));
    }

    switch (cartridge->mapper_id_) {
    case 0:
        cartridge->mapper_ = std::make_unique<Mapper0>(program_banks, character_banks);
        break;
    case 2:
        cartridge->mapper_ = std::make_unique<Mapper2>(program_banks, character_banks);
        break;
    default:
        throw std::logic_error("validated mapper was not constructed");
    }

    return cartridge;
}

bool Cartridge::cpu_read(std::uint16_t address, std::uint8_t& value) {
    if (address >= program_ram_start && address <= program_ram_end) {
        value = program_ram_[address - program_ram_start];
        return true;
    }

    const auto mapped = mapper_->cpu_read(address);
    if (!mapped) {
        return false;
    }
    value = program_memory_.at(*mapped);
    return true;
}

bool Cartridge::cpu_write(std::uint16_t address, std::uint8_t value) {
    if (address >= program_ram_start && address <= program_ram_end) {
        program_ram_[address - program_ram_start] = value;
        return true;
    }

    const auto mapping = mapper_->cpu_write(address, value);
    if (!mapping.handled) {
        return false;
    }
    if (mapping.address) {
        program_memory_.at(*mapping.address) = value;
    }
    return true;
}

bool Cartridge::ppu_read(std::uint16_t address, std::uint8_t& value) {
    const auto mapped = mapper_->ppu_read(address);
    if (!mapped) {
        return false;
    }
    value = character_memory_.at(*mapped);
    return true;
}

bool Cartridge::ppu_write(std::uint16_t address, std::uint8_t value) {
    const auto mapping = mapper_->ppu_write(address);
    if (!mapping.handled) {
        return false;
    }
    if (mapping.address) {
        character_memory_.at(*mapping.address) = value;
    }
    return true;
}

void Cartridge::reset() {
    mapper_->reset();
}

Mirror Cartridge::mirror() const noexcept {
    return mirror_;
}

std::uint8_t Cartridge::mapper_id() const noexcept {
    return mapper_id_;
}

std::size_t Cartridge::program_size() const noexcept {
    return program_memory_.size();
}

std::size_t Cartridge::program_ram_size() const noexcept {
    return program_ram_.size();
}

std::size_t Cartridge::character_size() const noexcept {
    return character_memory_.size();
}

bool Cartridge::has_battery() const noexcept {
    return has_battery_;
}

}  // namespace nes
