#pragma once

#include "node.hpp"

#include <memory>

namespace milg::audio {
    class VocoderNode : public Node
    {
    public:
        VocoderNode();
        VocoderNode(const VocoderNode &) = delete;
        VocoderNode(VocoderNode &&) = default;

        VocoderNode &operator =(const VocoderNode &) = delete;
        VocoderNode &operator =(VocoderNode &&) = default;

        ~VocoderNode();
    protected:
        ma_node *get_handle() override;
    private:
        std::unique_ptr<struct ma_vocoder_node> node;
    };
}
