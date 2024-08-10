#pragma once

#include "node.hpp"

#include <miniaudio.h>
#include <string>

namespace milg::audio {
    class Sound : public Node {
    public:
        Sound()              = delete;
        Sound(const Sound &) = delete;
        Sound(Sound &&)      = default;

        Sound(const std::string &path);

        ~Sound();

        Sound &operator=(const Sound &) = delete;
        Sound &operator=(Sound &&)      = default;

        ma_node *get_handle() override;

        bool play();

        float get_volume();
        void  set_volume(float volume);

    private:
        std::unique_ptr<ma_sound> sound;
    };
} // namespace milg::audio
