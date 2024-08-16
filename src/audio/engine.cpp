#include "engine.hpp"
#include "node.hpp"

#include <milg.hpp>

#ifndef MA_NO_DEVICE_IO
#define MA_NO_DEVICE_IO
#endif

#ifndef MINIAUDIO_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#endif

#include <SDL.h>
#include <miniaudio.h>
#include <stdexcept>

class Endpoint : public milg::audio::Node {
protected:
    ma_node *get_handle() override {
        auto node_graph = milg::audio::get_node_graph();

        return ma_node_graph_get_endpoint(node_graph);
    }
};

static ma_engine         engine;
static SDL_AudioDeviceID device;
static auto              endpoint = std::make_shared<Endpoint>();

namespace milg::audio {
    void init() {
        auto engine_cfg = ma_engine_config_init();

        engine_cfg.channels   = 2;
        engine_cfg.sampleRate = 44100;
        engine_cfg.noDevice   = MA_TRUE;

        MILG_DEBUG("Initializing multiaudio engine…");

        if (ma_engine_init(&engine_cfg, &engine) != MA_SUCCESS) {
            throw std::runtime_error("Audio engine initialization failed");
        }

        MILG_DEBUG("Initializing SDL audio subsystem…");

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            ma_engine_uninit(&engine);

            throw std::runtime_error("SDL audio subsystem initialization failed");
        }

        SDL_AudioSpec spec = {
            .freq     = (int)ma_engine_get_sample_rate(&engine),
            .format   = AUDIO_F32,
            .channels = (Uint8)ma_engine_get_channels(&engine),
            .samples  = 512,
            .callback =
                [](void *, Uint8 *stream, int len) {
                    auto channels   = ma_engine_get_channels(&engine);
                    auto frame_size = ma_get_bytes_per_frame(ma_format_f32, channels);

                    ma_engine_read_pcm_frames(&engine, stream, (ma_uint32)len / frame_size, NULL);
                },
        };

        MILG_DEBUG("Opening SDL audio device…");

        device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (device == 0) {
            ma_engine_uninit(&engine);
            SDL_QuitSubSystem(SDL_INIT_AUDIO);

            throw std::runtime_error("SDL audio device opening failed");
        }

        MILG_DEBUG("Unpausing SDL audio device…");

        SDL_PauseAudioDevice(device, 0);
    }

    void destroy() {
        SDL_CloseAudioDevice(device);
        ma_engine_uninit(&engine);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    std::shared_ptr<Node> get_endpoint() {
        return endpoint;
    }

    ma_engine *get_engine() {
        return &engine;
    }

    ma_node_graph *get_node_graph() {
        return ma_engine_get_node_graph(get_engine());
    }

    float get_volume() {
        return ma_engine_get_volume(&engine);
    }

    void set_volume(float volume) {
        ma_engine_set_volume(&engine, volume);
    }
} // namespace milg::audio
