#include "audio.hpp"
#include "logging.hpp"

#ifndef MA_NO_DEVICE_IO
#define MA_NO_DEVICE_IO
#endif

#ifndef MINIAUDIO_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#endif

#include <miniaudio.h>
#include <SDL2/SDL.h>
#include <stdexcept>

static ma_engine engine;
static SDL_AudioDeviceID device;

namespace milg::audio {
Sound::Sound(const std::string &path) {
    auto res = ma_sound_init_from_file(
        &engine,
        path.c_str(),
        0,
        NULL,
        NULL,
        &this->sound
    );
    if (res != MA_SUCCESS) {
        throw std::invalid_argument("Loading sound from file failed");
    }
}

Sound::~Sound() {
    ma_sound_uninit(&this->sound);
}

void Sound::play() {
    ma_sound_start(&this->sound);
}

float Sound::get_volume() {
    return ma_sound_get_volume(&this->sound);
}

void Sound::set_volume(float volume) {
    ma_sound_set_volume(&this->sound, volume);
}

void init() {
    auto engine_cfg = ma_engine_config_init();

    engine_cfg.channels = 2;
    engine_cfg.sampleRate = 44100;
    engine_cfg.noDevice = MA_TRUE;

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
        .freq = (int)ma_engine_get_sample_rate(&engine),
        .format = AUDIO_F32,
        .channels = (Uint8)ma_engine_get_channels(&engine),
        .samples = 512,
        .callback = [](void *, Uint8 *stream, int len) {
            auto channels = ma_engine_get_channels(&engine);
            auto frame_size = ma_get_bytes_per_frame(ma_format_f32, channels);

            ma_engine_read_pcm_frames(&engine, stream, (ma_uint32)len / frame_size, NULL);
        },
    };

    MILG_DEBUG("Opening SDL audio device…");

    device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (device == 0)
    {
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

float get_volume() {
    return ma_engine_get_volume(&engine);
}

void set_volume(float volume) {
    ma_engine_set_volume(&engine, volume);
}
}
