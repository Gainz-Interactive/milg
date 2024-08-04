#pragma once

#include <miniaudio.h>
#include <string>

namespace milg::audio {
    class Sound {
    public:
        Sound() = delete;
        Sound(const Sound &) = delete;
        Sound(Sound &&) = default;

        Sound(const std::string &path);

        ~Sound();

        Sound &operator=(const Sound &) = delete;
        Sound &operator=(Sound &&) = default;

        void play();

        float get_volume();
        void set_volume(float volume);
    private:
        ma_sound sound;
    };

    void init();
    void destroy();

    float get_volume();
    void set_volume(float volume);
}
