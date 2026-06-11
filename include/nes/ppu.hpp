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

    struct State {
        std::uint8_t control = 0;
        std::uint8_t mask = 0;
        std::uint8_t status = 0;
        std::uint16_t vram_address = 0;
        std::uint16_t temporary_address = 0;
        std::uint8_t fine_x = 0;
        bool write_latch = false;
        std::uint8_t data_buffer = 0;
    };

    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address,
                                        bool read_only = false);
    void cpu_write(std::uint16_t address, std::uint8_t value);

    [[nodiscard]] std::uint8_t ppu_read(std::uint16_t address,
                                        bool read_only = false);
    void ppu_write(std::uint16_t address, std::uint8_t value);
    void reset();
    [[nodiscard]] State state() const noexcept;

private:
    [[nodiscard]] std::size_t nametable_index(std::uint16_t address) const;
    [[nodiscard]] static std::size_t palette_index(std::uint16_t address);

    std::array<std::uint8_t, 8U * 1024U> pattern_ram_{};
    std::array<std::uint8_t, 4U * 1024U> nametable_ram_{};
    std::array<std::uint8_t, 32> palette_ram_{};
    std::shared_ptr<Cartridge> cartridge_;

    std::uint8_t control_ = 0;
    std::uint8_t mask_ = 0;
    std::uint8_t status_ = 0;
    std::uint16_t vram_address_ = 0;
    std::uint16_t temporary_address_ = 0;
    std::uint8_t fine_x_ = 0;
    bool write_latch_ = false;
    std::uint8_t data_buffer_ = 0;
    std::uint8_t open_bus_ = 0;
};

}  // namespace nes
