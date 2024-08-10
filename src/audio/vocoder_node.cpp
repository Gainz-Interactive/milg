#include "vocoder_node.hpp"
#include "engine.hpp"

#define VOCLIB_IMPLEMENTATION
#include <voclib.h>

static ma_waveform         waveform;
static ma_data_source_node dsnode;

namespace milg::audio {
    struct ma_vocoder_node {
        ma_node_base    base;
        voclib_instance voclib;
    };

    static void on_process(ma_node *node, const float **frames_in, ma_uint32 *n_frames_in, float **frames_out,
                           ma_uint32 *n_frames_out) {
        ma_vocoder_node *vocoder_node = (ma_vocoder_node *)node;

        voclib_process(&vocoder_node->voclib, frames_in[0], frames_in[1], frames_out[0], *n_frames_out);
    }

    static ma_node_vtable node_vtable = {on_process, NULL, 2, 1, 0};

    VocoderNode::VocoderNode() : node(std::make_unique<ma_vocoder_node>()) {
        auto engine      = get_engine();
        auto sample_rate = ma_engine_get_sample_rate(engine);
        auto channels    = ma_engine_get_channels(engine);

        voclib_initialize(&this->node->voclib, 16, 6, sample_rate, channels);

        auto           node_graph        = get_node_graph();
        ma_uint32      input_channels[]  = {1, channels};
        ma_uint32      output_channels[] = {channels};
        ma_node_config config            = ma_node_config_init();

        config.vtable          = &node_vtable;
        config.pInputChannels  = input_channels;
        config.pOutputChannels = output_channels;

        ma_node_init(node_graph, &config, NULL, &this->node->base);
        ma_node_set_output_bus_volume(&this->node->base, 0, 5);

        auto waveform_config =
            ma_waveform_config_init(ma_format_f32, 1, sample_rate, ma_waveform_type_sawtooth, 2.0, 100);
        ma_waveform_init(&waveform_config, &waveform);
        auto sourceNodeConfig = ma_data_source_node_config_init(&waveform);
        ma_data_source_node_init(node_graph, &sourceNodeConfig, NULL, &dsnode);

        ma_node_attach_output_bus(&dsnode, 0, &this->node->base, 0);
    }

    VocoderNode::~VocoderNode() {
        ma_node_uninit(&this->node->base, NULL);
    }

    ma_node *VocoderNode::get_handle() {
        return &this->node->base;
    }
} // namespace milg::audio
