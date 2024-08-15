#include <SDL_mouse.h>
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <map>
#include <milg.hpp>

#include "application.hpp"
#include "audio/audio.hpp"
#include "window.hpp"

using namespace milg;

class Milg : public Layer {
public:
    void on_attach() override {
        MILG_INFO("Starting Milg");
    }

    void on_update(float delta) override {
    }

    void on_event(Event &event) override {
    }

    void on_detach() override {
        MILG_INFO("Shutting down Milg");
    }
};

#include "lighting_main.cpp"

class Milglication : public Application {
public:
    Milglication(int argc, char **argv, const WindowCreateInfo &window_info) : Application(argc, argv, window_info) {
        push_layer(new Milg());
        push_layer(new GraphicsLayer());
        push_layer(new layers::Audio());
    }

    ~Milglication() {
    }
};

int main(int argc, char **argv) {
    Logging::init();

    WindowCreateInfo window_info = {
        .title  = "Milg",
        .width  = 1600,
        .height = 1008,
    };

    Milglication app(argc, argv, window_info);

    app.run();

    return 0;
}
