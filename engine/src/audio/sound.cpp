#include <milg/audio/sound.hpp>

#include <milg/audio/engine.hpp>

#include <stdexcept>

static const ma_uint32 DEFAULT_FLAGS = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;

namespace milg::audio {
    Sound::Sound(char *data, std::size_t size) {
        auto      engine         = get_engine();
        auto      sample_rate    = ma_engine_get_sample_rate(engine);
        auto      channels       = ma_engine_get_channels(engine);
        auto      decoder_config = ma_decoder_config_init(ma_format_f32, channels, sample_rate);
        ma_uint64 frame_count    = 0;
        void     *frames         = NULL;
        auto      res            = ma_decode_memory(data, size, &decoder_config, &frame_count, &frames);
        if (res != MA_SUCCESS) {
            throw std::runtime_error("Decoding sound data failed");
        }
        auto buffer_config = ma_audio_buffer_config_init(ma_format_f32, channels, frame_count, frames, NULL);

        if (ma_audio_buffer_init(&buffer_config, &this->buffer) != MA_SUCCESS) {
            throw std::runtime_error("Initializing sound buffer failed");
        }

        res = ma_sound_init_from_data_source(engine, &buffer, DEFAULT_FLAGS, NULL, &this->sound);
        if (res != MA_SUCCESS) {
            throw std::runtime_error("Loading sound failed");
        }
    }

    Sound::~Sound() {
        ma_sound_uninit(static_cast<ma_sound *>(this->get_handle()));
    }

    ma_node *Sound::get_handle() {
        return &this->sound;
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
