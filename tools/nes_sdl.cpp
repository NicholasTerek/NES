#include "nes/emulator.hpp"
#include "nes/ppu.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr std::array<std::array<std::uint8_t, 3>, 64> palette{{
    {{84, 84, 84}}, {{0, 30, 116}}, {{8, 16, 144}}, {{48, 0, 136}},
    {{68, 0, 100}}, {{92, 0, 48}}, {{84, 4, 0}}, {{60, 24, 0}},
    {{32, 42, 0}}, {{8, 58, 0}}, {{0, 64, 0}}, {{0, 60, 0}},
    {{0, 50, 60}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{152, 150, 152}}, {{8, 76, 196}}, {{48, 50, 236}}, {{92, 30, 228}},
    {{136, 20, 176}}, {{160, 20, 100}}, {{152, 34, 32}}, {{120, 60, 0}},
    {{84, 90, 0}}, {{40, 114, 0}}, {{8, 124, 0}}, {{0, 118, 40}},
    {{0, 102, 120}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{236, 238, 236}}, {{76, 154, 236}}, {{120, 124, 236}}, {{176, 98, 236}},
    {{228, 84, 236}}, {{236, 88, 180}}, {{236, 106, 100}}, {{212, 136, 32}},
    {{160, 170, 0}}, {{116, 196, 0}}, {{76, 208, 32}}, {{56, 204, 108}},
    {{56, 180, 204}}, {{60, 60, 60}}, {{0, 0, 0}}, {{0, 0, 0}},
    {{236, 238, 236}}, {{168, 204, 236}}, {{188, 188, 236}}, {{212, 178, 236}},
    {{236, 174, 236}}, {{236, 174, 212}}, {{236, 180, 176}}, {{228, 196, 144}},
    {{204, 210, 120}}, {{180, 222, 120}}, {{168, 226, 144}}, {{152, 226, 180}},
    {{160, 214, 228}}, {{160, 162, 160}}, {{0, 0, 0}}, {{0, 0, 0}},
}};

struct SdlLifetime {
    SdlLifetime() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~SdlLifetime() { SDL_Quit(); }
};

struct AudioDevice {
    SDL_AudioDeviceID id = 0;
    ~AudioDevice() {
        if (id != 0U) {
            SDL_CloseAudioDevice(id);
        }
    }
};

std::uint8_t controller_state() {
    const auto* keys = SDL_GetKeyboardState(nullptr);
    std::uint8_t state = 0;
    state |= keys[SDL_SCANCODE_X] != 0U ? 0x01U : 0x00U;
    state |= keys[SDL_SCANCODE_Z] != 0U ? 0x02U : 0x00U;
    state |= keys[SDL_SCANCODE_RSHIFT] != 0U ? 0x04U : 0x00U;
    state |= keys[SDL_SCANCODE_RETURN] != 0U ? 0x08U : 0x00U;
    state |= keys[SDL_SCANCODE_UP] != 0U ? 0x10U : 0x00U;
    state |= keys[SDL_SCANCODE_DOWN] != 0U ? 0x20U : 0x00U;
    state |= keys[SDL_SCANCODE_LEFT] != 0U ? 0x40U : 0x00U;
    state |= keys[SDL_SCANCODE_RIGHT] != 0U ? 0x80U : 0x00U;
    return state;
}

std::vector<std::int16_t> filter_audio(const std::vector<float>& samples,
                                       float& previous_input,
                                       float& previous_output) {
    std::vector<std::int16_t> pcm;
    pcm.reserve(samples.size());
    for (const auto sample : samples) {
        const auto filtered = sample - previous_input + 0.995F * previous_output;
        previous_input = sample;
        previous_output = filtered;
        const auto clamped = std::clamp(filtered, -1.0F, 1.0F);
        pcm.push_back(static_cast<std::int16_t>(clamped * 32'767.0F));
    }
    return pcm;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: nes_sdl ROM\n";
        return 1;
    }
    try {
        SdlLifetime sdl;
        using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
        using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
        using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
        Window window(SDL_CreateWindow("NES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       768, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE),
                      SDL_DestroyWindow);
        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }
        Renderer renderer(SDL_CreateRenderer(window.get(), -1,
                                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
                          SDL_DestroyRenderer);
        if (!renderer) {
            throw std::runtime_error(SDL_GetError());
        }
        Texture texture(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGB24,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          static_cast<int>(nes::Ppu::screen_width),
                                          static_cast<int>(nes::Ppu::screen_height)),
                        SDL_DestroyTexture);
        if (!texture) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_RenderSetLogicalSize(renderer.get(), static_cast<int>(nes::Ppu::screen_width),
                                static_cast<int>(nes::Ppu::screen_height));

        SDL_AudioSpec wanted{};
        wanted.freq = 44'100;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = 1;
        wanted.samples = 1'024;
        AudioDevice audio{SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0)};
        if (audio.id == 0U) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_PauseAudioDevice(audio.id, 0);

        auto emulator = nes::Emulator::load(argv[1]);
        std::vector<std::uint8_t> pixels(nes::Ppu::screen_width * nes::Ppu::screen_height * 3U);
        float previous_input = 0.0F;
        float previous_output = 0.0F;
        bool running = true;
        auto next_frame = std::chrono::steady_clock::now();
        while (running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                    running = false;
                }
            }
            emulator.set_controller_state(0, controller_state());
            emulator.run_frame();

            const auto& frame = emulator.bus().ppu().framebuffer();
            for (std::size_t index = 0; index < frame.size(); ++index) {
                const auto& color = palette[frame[index] & 0x3FU];
                pixels[index * 3U] = color[0];
                pixels[index * 3U + 1U] = color[1];
                pixels[index * 3U + 2U] = color[2];
            }
            if (SDL_UpdateTexture(texture.get(), nullptr, pixels.data(),
                                  static_cast<int>(nes::Ppu::screen_width * 3U)) != 0) {
                throw std::runtime_error(SDL_GetError());
            }
            SDL_RenderClear(renderer.get());
            SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
            SDL_RenderPresent(renderer.get());

            const auto pcm = filter_audio(emulator.take_audio_samples(), previous_input,
                                          previous_output);
            if (!pcm.empty() &&
                SDL_QueueAudio(audio.id, pcm.data(),
                               static_cast<Uint32>(pcm.size() * sizeof(std::int16_t))) != 0) {
                throw std::runtime_error(SDL_GetError());
            }
            next_frame += std::chrono::microseconds(16'639);
            std::this_thread::sleep_until(next_frame);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nes_sdl: " << error.what() << '\n';
        return 1;
    }
}
