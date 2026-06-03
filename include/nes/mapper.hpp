#pragma once

#include <cstdint>
#include <optional>

namespace nes {

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

}  // namespace nes
