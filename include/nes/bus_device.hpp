#pragma once

#include <cstdint>

namespace nes {

class CpuBusDevice {
public:
    virtual ~CpuBusDevice() = default;

    [[nodiscard]] virtual std::uint8_t cpu_read(std::uint16_t address,
                                                bool read_only = false) = 0;
    virtual void cpu_write(std::uint16_t address, std::uint8_t value) = 0;
};

}  // namespace nes
