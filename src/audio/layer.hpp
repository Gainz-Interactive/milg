#pragma once

#include <memory>
#include <milg/layer.hpp>

namespace milg::layers {
    class Audio : public Layer {
    public:
        void on_attach() override;
        void on_detach() override;
        void on_update(float delta) override;
        void on_event(class Event &event) override;
    };
} // namespace milg::layers
