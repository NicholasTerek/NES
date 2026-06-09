#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace nes {

class Cartridge;

class Ppu {
public:
    static constexpr std::size_t screen_width = 256;
    static constexpr std::size_t screen_height = 240;

    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

    [[nodiscard]] std::uint8_t ppu_read(std::uint16_t address,
                                        bool read_only = false);
    void ppu_write(std::uint16_t address, std::uint8_t value);
    void reset();

private:
    [[nodiscard]] std::size_t nametable_index(std::uint16_t address) const;
    [[nodiscard]] static std::size_t palette_index(std::uint16_t address);

    std::array<std::uint8_t, 8U * 1024U> pattern_ram_{};
    std::array<std::uint8_t, 4U * 1024U> nametable_ram_{};
    std::array<std::uint8_t, 32> palette_ram_{};
    std::shared_ptr<Cartridge> cartridge_;
};

}  // namespace nes
