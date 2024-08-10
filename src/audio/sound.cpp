#include "sound.hpp"
#include "engine.hpp"

#include <stdexcept>

static const ma_uint32 DEFAULT_FLAGS = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;

namespace milg::audio {
    Sound::Sound(const std::string &path) : sound(std::make_unique<ma_sound>()) {
        auto engine = get_engine();
        auto res    = ma_sound_init_from_file(engine, path.c_str(), DEFAULT_FLAGS, NULL, NULL,
                                              static_cast<ma_sound *>(this->get_handle()));
        if (res != MA_SUCCESS) {
            throw std::invalid_argument("Loading sound from file failed");
        }
    }

    Sound::~Sound() {
        ma_sound_uninit(static_cast<ma_sound *>(this->get_handle()));
    }

    ma_node *Sound::get_handle() {
        return this->sound.get();
    }

    bool Sound::play() {
        return ma_sound_start(static_cast<ma_sound *>(this->get_handle())) == MA_SUCCESS;
    }

    float Sound::get_volume() {
        return ma_sound_get_volume(static_cast<ma_sound *>(this->get_handle()));
    }

    void Sound::set_volume(float volume) {
        ma_sound_set_volume(static_cast<ma_sound *>(this->get_handle()), volume);
    }
} // namespace milg::audio
