#pragma once

#include <milg/audio/node.hpp>
#include <milg/core/types.hpp>

#include <miniaudio.h>

namespace milg::audio {
    class Sound : public Node {
    public:
        Sound()              = delete;
        Sound(const Sound &) = delete;
        Sound(Sound &&)      = default;

        Sound(const std::shared_ptr<Bytes> &bytes);

        ~Sound();

        Sound &operator=(const Sound &) = delete;
        Sound &operator=(Sound &&)      = default;

        ma_node *get_handle() override;

        bool play();

        float get_volume();
        void  set_volume(float volume);

    private:
        ma_audio_buffer buffer;
        ma_sound        sound;
    };
} // namespace milg::audio
