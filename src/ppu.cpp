#include "nes/ppu.hpp"

#include "nes/cartridge.hpp"

#include <stdexcept>
#include <utility>

namespace nes {
namespace {

constexpr std::uint8_t control_increment_mode = 0x04;
constexpr std::uint8_t control_background_pattern = 0x10;
constexpr std::uint8_t control_enable_nmi = 0x80;
constexpr std::uint8_t mask_grayscale = 0x01;
constexpr std::uint8_t mask_render_background_left = 0x02;
constexpr std::uint8_t mask_render_background = 0x08;
constexpr std::uint8_t mask_render_sprites = 0x10;
constexpr std::uint8_t status_vertical_blank = 0x80;
constexpr std::uint8_t status_sprite_overflow = 0x20;
constexpr std::uint8_t status_sprite_zero_hit = 0x40;

}  // namespace

void Ppu::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
    if (!cartridge) {
        throw std::invalid_argument("cannot connect an empty cartridge to the PPU");
    }
    cartridge_ = std::move(cartridge);
}

std::uint8_t Ppu::cpu_read(std::uint16_t address, bool read_only) {
    const auto selected_register = static_cast<std::uint8_t>(address & 0x0007U);
    if (read_only) {
        switch (selected_register) {
        case 0:
            return control_;
        case 1:
            return mask_;
        case 2:
            return status_;
        case 4:
            return oam_[oam_address_];
        default:
            return open_bus_;
        }
    }

    std::uint8_t value = open_bus_;
    switch (selected_register) {
    case 2:
        value = static_cast<std::uint8_t>((status_ & 0xE0U) | (open_bus_ & 0x1FU));
        status_ = static_cast<std::uint8_t>(status_ & ~status_vertical_blank);
        write_latch_ = false;
        break;
    case 4:
        value = oam_[oam_address_];
        break;
    case 7: {
        const auto address_before_increment = vram_address_;
        const auto fetched = ppu_read(address_before_increment);
        if ((address_before_increment & 0x3FFFU) >= 0x3F00U) {
            value = fetched;
            data_buffer_ = ppu_read(static_cast<std::uint16_t>(address_before_increment - 0x1000U));
        } else {
            value = data_buffer_;
            data_buffer_ = fetched;
        }
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ + ((control_ & control_increment_mode) != 0U ? 32U : 1U)) &
            0x7FFFU);
        break;
    }
    default:
        break;
    }
    open_bus_ = value;
    return value;
}

void Ppu::cpu_write(std::uint16_t address, std::uint8_t value) {
    open_bus_ = value;
    switch (address & 0x0007U) {
    case 0:
        if ((control_ & control_enable_nmi) == 0U && (value & control_enable_nmi) != 0U &&
            (status_ & status_vertical_blank) != 0U) {
            nmi_pending_ = true;
        }
        control_ = value;
        temporary_address_ = static_cast<std::uint16_t>(
            (temporary_address_ & 0xF3FFU) | (static_cast<std::uint16_t>(value & 0x03U) << 10U));
        break;
    case 1:
        mask_ = value;
        break;
    case 3:
        oam_address_ = value;
        break;
    case 4:
        oam_[oam_address_] = value;
        ++oam_address_;
        break;
    case 5:
        if (!write_latch_) {
            fine_x_ = static_cast<std::uint8_t>(value & 0x07U);
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0xFFE0U) | (static_cast<std::uint16_t>(value) >> 3U));
            write_latch_ = true;
        } else {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x8C1FU) |
                (static_cast<std::uint16_t>(value & 0x07U) << 12U) |
                (static_cast<std::uint16_t>(value & 0xF8U) << 2U));
            write_latch_ = false;
        }
        break;
    case 6:
        if (!write_latch_) {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x00FFU) |
                (static_cast<std::uint16_t>(value & 0x3FU) << 8U));
            write_latch_ = true;
        } else {
            temporary_address_ = static_cast<std::uint16_t>(
                (temporary_address_ & 0x7F00U) | static_cast<std::uint16_t>(value));
            vram_address_ = temporary_address_;
            write_latch_ = false;
        }
        break;
    case 7:
        ppu_write(vram_address_, value);
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ + ((control_ & control_increment_mode) != 0U ? 32U : 1U)) &
            0x7FFFU);
        break;
    default:
        break;
    }
}

std::uint8_t Ppu::ppu_read(std::uint16_t address, bool read_only) {
    static_cast<void>(read_only);
    address &= 0x3FFFU;

    std::uint8_t value = 0;
    if (cartridge_ && cartridge_->ppu_read(address, value)) {
        return value;
    }
    if (address <= 0x1FFFU) {
        return pattern_ram_[address];
    }
    if (address <= 0x3EFFU) {
        return nametable_ram_[nametable_index(address)];
    }
    return static_cast<std::uint8_t>(
        palette_ram_[palette_index(address)] &
        ((mask_ & mask_grayscale) != 0U ? 0x30U : 0x3FU));
}

void Ppu::ppu_write(std::uint16_t address, std::uint8_t value) {
    address &= 0x3FFFU;

    if (cartridge_ && cartridge_->ppu_write(address, value)) {
        return;
    }
    if (address <= 0x1FFFU) {
        pattern_ram_[address] = value;
        return;
    }
    if (address <= 0x3EFFU) {
        nametable_ram_[nametable_index(address)] = value;
        return;
    }
    palette_ram_[palette_index(address)] = static_cast<std::uint8_t>(value & 0x3FU);
}

void Ppu::clock() {
    if (scanline_ == -1 && cycle_ == 1) {
        status_ = static_cast<std::uint8_t>(
            status_ & ~(status_vertical_blank | status_sprite_overflow | status_sprite_zero_hit));
    }
    if (scanline_ == 241 && cycle_ == 1) {
        status_ = static_cast<std::uint8_t>(status_ | status_vertical_blank);
        if ((control_ & control_enable_nmi) != 0U) {
            nmi_pending_ = true;
        }
    }

    if (scanline_ >= -1 && scanline_ < 240 && rendering_enabled()) {
        if ((cycle_ >= 2 && cycle_ < 258) || (cycle_ >= 321 && cycle_ < 338)) {
            update_background_shifters();
            fetch_background_data();
        }
        if (cycle_ == 257) {
            load_background_shifters();
            transfer_scroll_x();
        }
        if (cycle_ == 256) {
            increment_scroll_y();
        }
        if (cycle_ == 338 || cycle_ == 340) {
            next_tile_id_ = ppu_read(
                static_cast<std::uint16_t>(0x2000U | (vram_address_ & 0x0FFFU)));
        }
        if (scanline_ == -1 && cycle_ >= 280 && cycle_ < 305) {
            transfer_scroll_y();
        }
        if (cycle_ == 257) {
            evaluate_sprites();
        }
    }

    if (scanline_ >= 0 && scanline_ < static_cast<std::int16_t>(Ppu::screen_height) &&
        cycle_ >= 1 && cycle_ <= static_cast<std::int16_t>(Ppu::screen_width)) {
        render_background_pixel();
    }

    if (scanline_ == -1 && cycle_ == 339 && odd_frame_ && rendering_enabled()) {
        cycle_ = 0;
        scanline_ = 0;
        return;
    }

    ++cycle_;
    if (cycle_ >= 341) {
        cycle_ = 0;
        ++scanline_;
        if (scanline_ >= 261) {
            scanline_ = -1;
            frame_complete_ = true;
            odd_frame_ = !odd_frame_;
        }
    }
}

void Ppu::reset() {
    control_ = 0;
    mask_ = 0;
    status_ = 0;
    vram_address_ = 0;
    temporary_address_ = 0;
    fine_x_ = 0;
    write_latch_ = false;
    data_buffer_ = 0;
    open_bus_ = 0;
    oam_address_ = 0;
    scanline_ = -1;
    cycle_ = 0;
    frame_complete_ = false;
    odd_frame_ = false;
    nmi_pending_ = false;
    next_tile_id_ = 0;
    next_tile_attribute_ = 0;
    next_tile_low_ = 0;
    next_tile_high_ = 0;
    pattern_shift_low_ = 0;
    pattern_shift_high_ = 0;
    attribute_shift_low_ = 0;
    attribute_shift_high_ = 0;
    active_sprites_.fill({});
    sprite_count_ = 0;
    sprite_zero_possible_ = false;
    framebuffer_.fill(0);
}

Ppu::State Ppu::state() const noexcept {
    return {control_,          mask_,       status_,         vram_address_,
            temporary_address_, fine_x_,     write_latch_,    data_buffer_,
            scanline_,         cycle_,      frame_complete_, odd_frame_, next_tile_id_,
            next_tile_attribute_, next_tile_low_, next_tile_high_, pattern_shift_low_,
            pattern_shift_high_, attribute_shift_low_, attribute_shift_high_, nmi_pending_,
            oam_address_, sprite_count_, sprite_zero_possible_};
}

void Ppu::clear_frame_complete() noexcept {
    frame_complete_ = false;
}

bool Ppu::poll_nmi() noexcept {
    const auto pending = nmi_pending_;
    nmi_pending_ = false;
    return pending;
}

std::uint8_t Ppu::oam_read(std::uint8_t address) const noexcept {
    return oam_[address];
}

void Ppu::oam_write(std::uint8_t address, std::uint8_t value) noexcept {
    oam_[address] = value;
}

const std::array<std::uint8_t, 256>& Ppu::oam() const noexcept {
    return oam_;
}

const std::array<Ppu::Sprite, 8>& Ppu::active_sprites() const noexcept {
    return active_sprites_;
}

const Ppu::Framebuffer& Ppu::framebuffer() const noexcept {
    return framebuffer_;
}

std::uint8_t Ppu::pixel(std::size_t x, std::size_t y) const {
    return framebuffer_.at(y * screen_width + x);
}

bool Ppu::rendering_enabled() const noexcept {
    return (mask_ & (mask_render_background | mask_render_sprites)) != 0U;
}

void Ppu::fetch_background_data() {
    const auto phase = static_cast<std::uint8_t>((cycle_ - 1) & 0x0007);
    const auto coarse_x = static_cast<std::uint8_t>(vram_address_ & 0x001FU);
    const auto coarse_y = static_cast<std::uint8_t>((vram_address_ >> 5U) & 0x001FU);

    switch (phase) {
    case 0:
        load_background_shifters();
        next_tile_id_ = ppu_read(
            static_cast<std::uint16_t>(0x2000U | (vram_address_ & 0x0FFFU)));
        break;
    case 2: {
        const auto attribute_address = static_cast<std::uint16_t>(
            0x23C0U | (vram_address_ & 0x0C00U) |
            (static_cast<std::uint16_t>(coarse_y >> 2U) << 3U) |
            static_cast<std::uint16_t>(coarse_x >> 2U));
        auto attribute = ppu_read(attribute_address);
        if ((coarse_y & 0x02U) != 0U) {
            attribute = static_cast<std::uint8_t>(attribute >> 4U);
        }
        if ((coarse_x & 0x02U) != 0U) {
            attribute = static_cast<std::uint8_t>(attribute >> 2U);
        }
        next_tile_attribute_ = static_cast<std::uint8_t>(attribute & 0x03U);
        break;
    }
    case 4: {
        const auto pattern_base = static_cast<std::uint16_t>(
            (control_ & control_background_pattern) != 0U ? 0x1000U : 0x0000U);
        const auto fine_y = static_cast<std::uint16_t>((vram_address_ >> 12U) & 0x0007U);
        next_tile_low_ = ppu_read(static_cast<std::uint16_t>(
            pattern_base + static_cast<std::uint16_t>(next_tile_id_) * 16U + fine_y));
        break;
    }
    case 6: {
        const auto pattern_base = static_cast<std::uint16_t>(
            (control_ & control_background_pattern) != 0U ? 0x1000U : 0x0000U);
        const auto fine_y = static_cast<std::uint16_t>((vram_address_ >> 12U) & 0x0007U);
        next_tile_high_ = ppu_read(static_cast<std::uint16_t>(
            pattern_base + static_cast<std::uint16_t>(next_tile_id_) * 16U + fine_y + 8U));
        break;
    }
    case 7:
        increment_scroll_x();
        break;
    default:
        break;
    }
}

void Ppu::load_background_shifters() {
    pattern_shift_low_ = static_cast<std::uint16_t>(
        (pattern_shift_low_ & 0xFF00U) | next_tile_low_);
    pattern_shift_high_ = static_cast<std::uint16_t>(
        (pattern_shift_high_ & 0xFF00U) | next_tile_high_);
    attribute_shift_low_ = static_cast<std::uint16_t>(
        (attribute_shift_low_ & 0xFF00U) |
        ((next_tile_attribute_ & 0x01U) != 0U ? 0x00FFU : 0x0000U));
    attribute_shift_high_ = static_cast<std::uint16_t>(
        (attribute_shift_high_ & 0xFF00U) |
        ((next_tile_attribute_ & 0x02U) != 0U ? 0x00FFU : 0x0000U));
}

void Ppu::update_background_shifters() {
    if ((mask_ & mask_render_background) == 0U) {
        return;
    }
    pattern_shift_low_ = static_cast<std::uint16_t>(pattern_shift_low_ << 1U);
    pattern_shift_high_ = static_cast<std::uint16_t>(pattern_shift_high_ << 1U);
    attribute_shift_low_ = static_cast<std::uint16_t>(attribute_shift_low_ << 1U);
    attribute_shift_high_ = static_cast<std::uint16_t>(attribute_shift_high_ << 1U);
}

void Ppu::increment_scroll_x() {
    if (!rendering_enabled()) {
        return;
    }
    if ((vram_address_ & 0x001FU) == 31U) {
        vram_address_ = static_cast<std::uint16_t>(vram_address_ & ~0x001FU);
        vram_address_ = static_cast<std::uint16_t>(vram_address_ ^ 0x0400U);
    } else {
        ++vram_address_;
    }
}

void Ppu::increment_scroll_y() {
    if (!rendering_enabled()) {
        return;
    }
    if ((vram_address_ & 0x7000U) != 0x7000U) {
        vram_address_ = static_cast<std::uint16_t>(vram_address_ + 0x1000U);
        return;
    }

    vram_address_ = static_cast<std::uint16_t>(vram_address_ & ~0x7000U);
    auto coarse_y = static_cast<std::uint8_t>((vram_address_ >> 5U) & 0x001FU);
    if (coarse_y == 29U) {
        coarse_y = 0;
        vram_address_ = static_cast<std::uint16_t>(vram_address_ ^ 0x0800U);
    } else if (coarse_y == 31U) {
        coarse_y = 0;
    } else {
        ++coarse_y;
    }
    vram_address_ = static_cast<std::uint16_t>(
        (vram_address_ & ~0x03E0U) | (static_cast<std::uint16_t>(coarse_y) << 5U));
}

void Ppu::transfer_scroll_x() {
    if (rendering_enabled()) {
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ & ~0x041FU) | (temporary_address_ & 0x041FU));
    }
}

void Ppu::transfer_scroll_y() {
    if (rendering_enabled()) {
        vram_address_ = static_cast<std::uint16_t>(
            (vram_address_ & ~0x7BE0U) | (temporary_address_ & 0x7BE0U));
    }
}

void Ppu::render_background_pixel() {
    std::uint8_t background_pixel = 0;
    std::uint8_t background_palette = 0;
    const auto left_edge_visible = cycle_ > 8 || (mask_ & mask_render_background_left) != 0U;
    if ((mask_ & mask_render_background) != 0U && left_edge_visible) {
        const auto bit = static_cast<std::uint16_t>(0x8000U >> fine_x_);
        const auto low_pixel = static_cast<std::uint8_t>((pattern_shift_low_ & bit) != 0U);
        const auto high_pixel = static_cast<std::uint8_t>((pattern_shift_high_ & bit) != 0U);
        background_pixel = static_cast<std::uint8_t>((high_pixel << 1U) | low_pixel);

        const auto low_palette = static_cast<std::uint8_t>((attribute_shift_low_ & bit) != 0U);
        const auto high_palette = static_cast<std::uint8_t>((attribute_shift_high_ & bit) != 0U);
        background_palette = static_cast<std::uint8_t>((high_palette << 1U) | low_palette);
    }
    if (background_pixel == 0U) {
        background_palette = 0;
    }

    const auto palette_address = static_cast<std::uint16_t>(
        0x3F00U + static_cast<std::uint16_t>(background_palette) * 4U + background_pixel);
    const auto index = static_cast<std::size_t>(scanline_) * screen_width +
                       static_cast<std::size_t>(cycle_ - 1);
    framebuffer_[index] = ppu_read(palette_address);
}

void Ppu::evaluate_sprites() {
    active_sprites_.fill({});
    sprite_count_ = 0;
    sprite_zero_possible_ = false;
    status_ = static_cast<std::uint8_t>(status_ & ~status_sprite_overflow);

    const auto target_scanline = static_cast<std::int16_t>(scanline_ + 1);
    if (target_scanline < 0 || target_scanline >= static_cast<std::int16_t>(screen_height)) {
        return;
    }
    const auto sprite_height = static_cast<std::int16_t>((control_ & 0x20U) != 0U ? 16 : 8);
    std::uint8_t visible_count = 0;
    for (std::uint8_t index = 0; index < 64U; ++index) {
        const auto offset = static_cast<std::size_t>(index) * 4U;
        const auto top = static_cast<std::uint8_t>(oam_[offset] + 1U);
        const auto row = static_cast<std::int16_t>(
            target_scanline - static_cast<std::int16_t>(top));
        if (row < 0 || row >= sprite_height) {
            continue;
        }

        if (visible_count < 8U) {
            active_sprites_[visible_count] = {
                oam_[offset], oam_[offset + 1U], oam_[offset + 2U], oam_[offset + 3U], index};
            if (index == 0U) {
                sprite_zero_possible_ = true;
            }
        }
        ++visible_count;
    }
    sprite_count_ = static_cast<std::uint8_t>(visible_count > 8U ? 8U : visible_count);
    if (visible_count > 8U) {
        status_ = static_cast<std::uint8_t>(status_ | status_sprite_overflow);
    }
}

std::size_t Ppu::nametable_index(std::uint16_t address) const {
    const auto offset = static_cast<std::uint16_t>((address - 0x2000U) & 0x0FFFU);
    const auto table = static_cast<std::uint8_t>(offset / 0x0400U);
    const auto cell = static_cast<std::size_t>(offset & 0x03FFU);
    const auto mirror = cartridge_ ? cartridge_->mirror() : Mirror::horizontal;

    std::uint8_t physical_table = 0;
    switch (mirror) {
    case Mirror::vertical:
        physical_table = static_cast<std::uint8_t>(table & 0x01U);
        break;
    case Mirror::horizontal:
        physical_table = static_cast<std::uint8_t>(table >> 1U);
        break;
    case Mirror::four_screen:
        physical_table = table;
        break;
    case Mirror::one_screen_low:
        physical_table = 0;
        break;
    case Mirror::one_screen_high:
        physical_table = 1;
        break;
    }
    return static_cast<std::size_t>(physical_table) * 0x0400U + cell;
}

std::size_t Ppu::palette_index(std::uint16_t address) {
    auto index = static_cast<std::uint8_t>(address & 0x001FU);
    if (index == 0x10U || index == 0x14U || index == 0x18U || index == 0x1CU) {
        index = static_cast<std::uint8_t>(index - 0x10U);
    }
    return index;
}

}  // namespace nes
