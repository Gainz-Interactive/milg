#pragma once

namespace milg {
    class Layer {
    public:
        Layer()          = default;
        virtual ~Layer() = default;

        virtual void on_attach()                  = 0;
        virtual void on_detach()                  = 0;
        virtual void on_update(float delta)       = 0;
        virtual void on_event(class Event &event) = 0;
    };
} // namespace milg
