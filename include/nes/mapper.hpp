#pragma once

#include <cstdint>
#include <optional>

namespace nes {

enum class Mirror {
    horizontal,
    vertical,
    four_screen,
    one_screen_low,
    one_screen_high,
};

class Mapper {
public:
    struct WriteMapping {
        bool handled = false;
        std::optional<std::uint32_t> address;
    };

    Mapper(std::uint8_t program_banks, std::uint8_t character_banks);
    virtual ~Mapper() = default;

    Mapper(const Mapper&) = delete;
    Mapper& operator=(const Mapper&) = delete;

    [[nodiscard]] virtual std::optional<std::uint32_t> cpu_read(std::uint16_t address) = 0;
    [[nodiscard]] virtual WriteMapping cpu_write(std::uint16_t address,
                                                 std::uint8_t value) = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> ppu_read(std::uint16_t address) = 0;
    [[nodiscard]] virtual WriteMapping ppu_write(std::uint16_t address) = 0;
    [[nodiscard]] virtual std::optional<Mirror> mirror() const noexcept;
    [[nodiscard]] virtual bool program_ram_enabled() const noexcept;
    virtual void reset() = 0;

protected:
    std::uint8_t program_banks_;
    std::uint8_t character_banks_;
};

class Mapper0 final : public Mapper {
public:
    using Mapper::Mapper;

    [[nodiscard]] std::optional<std::uint32_t> cpu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping cpu_write(std::uint16_t address,
                                         std::uint8_t value) override;
    [[nodiscard]] std::optional<std::uint32_t> ppu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping ppu_write(std::uint16_t address) override;
    void reset() override;
};

class Mapper2 final : public Mapper {
public:
    using Mapper::Mapper;

    [[nodiscard]] std::optional<std::uint32_t> cpu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping cpu_write(std::uint16_t address,
                                         std::uint8_t value) override;
    [[nodiscard]] std::optional<std::uint32_t> ppu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping ppu_write(std::uint16_t address) override;
    void reset() override;

private:
    std::uint8_t selected_bank_ = 0;
};

class Mapper1 final : public Mapper {
public:
    using Mapper::Mapper;

    [[nodiscard]] std::optional<std::uint32_t> cpu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping cpu_write(std::uint16_t address,
                                         std::uint8_t value) override;
    [[nodiscard]] std::optional<std::uint32_t> ppu_read(std::uint16_t address) override;
    [[nodiscard]] WriteMapping ppu_write(std::uint16_t address) override;
    [[nodiscard]] std::optional<Mirror> mirror() const noexcept override;
    [[nodiscard]] bool program_ram_enabled() const noexcept override;
    void reset() override;

private:
    [[nodiscard]] std::uint32_t character_address(std::uint16_t address) const noexcept;

    std::uint8_t shift_register_ = 0x10;
    std::uint8_t control_ = 0x0C;
    std::uint8_t character_bank_0_ = 0;
    std::uint8_t character_bank_1_ = 0;
    std::uint8_t program_bank_ = 0;
};

}  // namespace nes
