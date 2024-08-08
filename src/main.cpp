#include <SDL_mouse.h>
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <map>

#include "application.hpp"
#include "audio.hpp"
#include "event.hpp"
#include "layer.hpp"
#include "logging.hpp"
#include "window.hpp"

using namespace milg;

static std::shared_ptr<milg::audio::VocoderNode> vocoder_node;

class Milg : public Layer {
public:
    std::map<std::string, std::shared_ptr<audio::Sound>> sounds;

    void on_attach() override {
        MILG_INFO("Starting Milg");

        auto &asset_store = Application::get().get_asset_store();

        for (auto &[name, asset] : asset_store.get_assets(Asset::Type::SOUND)) {
            this->sounds[name] = std::make_shared<milg::audio::Sound>(asset->get_data(), asset->get_size());
        }
    }

    void on_update(float delta) override {
        ImGui::Begin("Milg");
        if (ImGui::BeginTabBar("##MilgTabBar")) {
            if (ImGui::BeginTabItem("Audio")) {
                ImGui::SeparatorText("Master volume");

                auto master_volume = audio::get_volume();

                if (ImGui::SliderFloat("##master_vol", &master_volume, 0.f, 1.f)) {
                    audio::set_volume(master_volume);
                }

                ImGui::SeparatorText("Loaded sounds");

                int        i              = 0;
                static int selected_index = 0;

                for (auto &[key, sound] : this->sounds) {
                    bool selected = selected_index == i;
                    auto volume   = sound->get_volume();

                    if (ImGui::ArrowButton(std::format("##play_{}", key).c_str(), ImGuiDir_Right)) {
                        vocoder_node->detach_input(1);
                        sound->attach_output<0, 1>(vocoder_node);
                        sound->play();
                    }
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderFloat(std::format("##vol_{}", key).c_str(), &volume, 0.f, 1.f)) {
                        sound->set_volume(volume);
                    }
                    ImGui::SameLine();
                    if (ImGui::Selectable(key.c_str(), selected)) {
                        selected_index = i;
                    }

                    i++;
                }

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
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

    vocoder_node = std::make_shared<milg::audio::VocoderNode>();
    vocoder_node->attach_output<0, 0>(milg::audio::get_endpoint());

    app.run();

    return 0;
}
