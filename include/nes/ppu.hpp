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
    using Framebuffer = std::array<std::uint8_t, screen_width * screen_height>;

    struct Sprite {
        std::uint8_t y = 0xFF;
        std::uint8_t tile = 0xFF;
        std::uint8_t attributes = 0xFF;
        std::uint8_t x = 0xFF;
        std::uint8_t oam_index = 0xFF;
    };

    struct State {
        std::uint8_t control = 0;
        std::uint8_t mask = 0;
        std::uint8_t status = 0;
        std::uint16_t vram_address = 0;
        std::uint16_t temporary_address = 0;
        std::uint8_t fine_x = 0;
        bool write_latch = false;
        std::uint8_t data_buffer = 0;
        std::int16_t scanline = -1;
        std::int16_t cycle = 0;
        bool frame_complete = false;
        bool odd_frame = false;
        std::uint8_t next_tile_id = 0;
        std::uint8_t next_tile_attribute = 0;
        std::uint8_t next_tile_low = 0;
        std::uint8_t next_tile_high = 0;
        std::uint16_t pattern_shift_low = 0;
        std::uint16_t pattern_shift_high = 0;
        std::uint16_t attribute_shift_low = 0;
        std::uint16_t attribute_shift_high = 0;
        bool nmi_pending = false;
        std::uint8_t oam_address = 0;
        std::uint8_t sprite_count = 0;
        bool sprite_zero_possible = false;
    };

    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address,
                                        bool read_only = false);
    void cpu_write(std::uint16_t address, std::uint8_t value);

    [[nodiscard]] std::uint8_t ppu_read(std::uint16_t address,
                                        bool read_only = false);
    void ppu_write(std::uint16_t address, std::uint8_t value);
    void clock();
    void reset();
    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] const Framebuffer& framebuffer() const noexcept;
    [[nodiscard]] std::uint8_t pixel(std::size_t x, std::size_t y) const;
    void clear_frame_complete() noexcept;
    [[nodiscard]] bool poll_nmi() noexcept;
    [[nodiscard]] std::uint8_t oam_read(std::uint8_t address) const noexcept;
    void oam_write(std::uint8_t address, std::uint8_t value) noexcept;
    [[nodiscard]] const std::array<std::uint8_t, 256>& oam() const noexcept;
    [[nodiscard]] const std::array<Sprite, 8>& active_sprites() const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, 8>& sprite_pattern_low() const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, 8>& sprite_pattern_high() const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, 8>& sprite_x_counters() const noexcept;

private:
    [[nodiscard]] std::size_t nametable_index(std::uint16_t address) const;
    [[nodiscard]] static std::size_t palette_index(std::uint16_t address);
    [[nodiscard]] bool rendering_enabled() const noexcept;
    void fetch_background_data();
    void load_background_shifters();
    void update_background_shifters();
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_scroll_x();
    void transfer_scroll_y();
    void render_pixel();
    void evaluate_sprites();
    void fetch_sprite_patterns(std::int16_t target_scanline);
    void update_sprite_shifters();
    [[nodiscard]] static std::uint8_t reverse_bits(std::uint8_t value) noexcept;

    std::array<std::uint8_t, 8U * 1024U> pattern_ram_{};
    std::array<std::uint8_t, 4U * 1024U> nametable_ram_{};
    std::array<std::uint8_t, 32> palette_ram_{};
    Framebuffer framebuffer_{};
    std::array<std::uint8_t, 256> oam_{};
    std::array<Sprite, 8> active_sprites_{};
    std::array<std::uint8_t, 8> sprite_pattern_low_{};
    std::array<std::uint8_t, 8> sprite_pattern_high_{};
    std::array<std::uint8_t, 8> sprite_x_counters_{};
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
    std::uint8_t oam_address_ = 0;
    std::int16_t scanline_ = -1;
    std::int16_t cycle_ = 0;
    bool frame_complete_ = false;
    bool odd_frame_ = false;
    bool nmi_pending_ = false;

    std::uint8_t next_tile_id_ = 0;
    std::uint8_t next_tile_attribute_ = 0;
    std::uint8_t next_tile_low_ = 0;
    std::uint8_t next_tile_high_ = 0;
    std::uint16_t pattern_shift_low_ = 0;
    std::uint16_t pattern_shift_high_ = 0;
    std::uint16_t attribute_shift_low_ = 0;
    std::uint16_t attribute_shift_high_ = 0;
    std::uint8_t sprite_count_ = 0;
    bool sprite_zero_possible_ = false;
};

}  // namespace nes
