#include <milg/audio.hpp>
#include <milg/milg.hpp>

#include <imgui.h>
#include <map>
#include <memory>

static std::map<std::string, std::shared_ptr<milg::audio::Sound>> sounds;
static std::shared_ptr<milg::audio::VocoderNode>                  vocoder_node;

class AudioLayer : public milg::Layer {
    void on_attach() override {
        milg::audio::set_volume(.5f);

        vocoder_node = std::make_shared<milg::audio::VocoderNode>();
        vocoder_node->attach_output<0, 0>(milg::audio::get_endpoint());

        for (auto &[name, asset] : milg::asset_store::get_assets(milg::Asset::Type::SOUND)) {
            sounds[name] = std::make_shared<milg::audio::Sound>(asset->get_data(), asset->get_size());
        }
    }

    void on_detach() override {
        sounds.clear();
    }

    void on_update(float delta) override {
        if (ImGui::Begin("Audio")) {
            ImGui::SeparatorText("Master volume");

            auto master_volume = milg::audio::get_volume();

            if (ImGui::SliderFloat("##master_vol", &master_volume, 0.f, 1.f)) {
                milg::audio::set_volume(master_volume);
            }

            ImGui::SeparatorText("Loaded sounds");

            int        i              = 0;
            static int selected_index = 0;

            for (auto &[key, sound] : sounds) {
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

            ImGui::End();
        }
    }

    void on_event(milg::Event &event) override {
    }
};

class AudioPlayground : public milg::Application {
public:
    AudioPlayground(int argc, char **argv, const milg::WindowCreateInfo &window_info)
        : Application(argc, argv, window_info) {
        push_layer(new AudioLayer());
    }

    ~AudioPlayground() {
    }
};

int main(int argc, char **argv) {
    milg::Logging::init();

    milg::WindowCreateInfo window_info = {
        .title  = "Milg",
        .width  = 1600,
        .height = 900,
    };

    AudioPlayground app(argc, argv, window_info);
    app.run();

    return 0;
}
