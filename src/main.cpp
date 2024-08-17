#include "application.hpp"
#include "audio/audio.hpp"

using namespace milg;

class Milglication : public Application {
public:
    Milglication(int argc, char **argv, const WindowCreateInfo &window_info) : Application(argc, argv, window_info) {
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
        .height = 900,
    };

    Milglication app(argc, argv, window_info);
    app.run();

    return 0;
}
