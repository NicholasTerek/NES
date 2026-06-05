#pragma once

#include "nes/mapper.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace nes {

enum class Mirror {
    horizontal,
    vertical,
    four_screen,
    one_screen_low,
    one_screen_high,
};

class Cartridge {
public:
    static std::shared_ptr<Cartridge> load(const std::filesystem::path& path);
    static std::shared_ptr<Cartridge> from_ines(std::span<const std::uint8_t> image);

    [[nodiscard]] bool cpu_read(std::uint16_t address, std::uint8_t& value);
    [[nodiscard]] bool cpu_write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] bool ppu_read(std::uint16_t address, std::uint8_t& value);
    [[nodiscard]] bool ppu_write(std::uint16_t address, std::uint8_t value);

    void reset();

    [[nodiscard]] Mirror mirror() const noexcept;
    [[nodiscard]] std::uint8_t mapper_id() const noexcept;
    [[nodiscard]] std::size_t program_size() const noexcept;
    [[nodiscard]] std::size_t program_ram_size() const noexcept;
    [[nodiscard]] std::size_t character_size() const noexcept;
    [[nodiscard]] bool has_battery() const noexcept;

private:
    Cartridge() = default;

    Mirror mirror_ = Mirror::horizontal;
    std::uint8_t mapper_id_ = 0;
    std::vector<std::uint8_t> program_memory_;
    std::vector<std::uint8_t> program_ram_;
    std::vector<std::uint8_t> character_memory_;
    bool has_battery_ = false;
    std::unique_ptr<Mapper> mapper_;
};

}  // namespace nes
