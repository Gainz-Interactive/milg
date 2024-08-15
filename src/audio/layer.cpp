#include "audio.hpp"

#include <imgui.h>
#include <map>
#include <memory>
#include <milg.hpp>

static std::map<std::string, std::shared_ptr<milg::audio::Sound>> sounds;
static std::shared_ptr<milg::audio::VocoderNode>                  vocoder_node;

namespace milg {
    void layers::Audio::on_attach() {
        audio::set_volume(.5f);

        vocoder_node = std::make_shared<milg::audio::VocoderNode>();
        vocoder_node->attach_output<0, 0>(milg::audio::get_endpoint());

        for (auto &[name, asset] : asset_store::get_assets(Asset::Type::SOUND)) {
            sounds[name] = std::make_shared<audio::Sound>(asset->get_data(), asset->get_size());
        }
    }

    void layers::Audio::on_detach() {
        sounds.clear();
    }

    void layers::Audio::on_update(float delta) {
        if (ImGui::Begin("Audio")) {
            ImGui::SeparatorText("Master volume");

            auto master_volume = audio::get_volume();

            if (ImGui::SliderFloat("##master_vol", &master_volume, 0.f, 1.f)) {
                audio::set_volume(master_volume);
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

    void layers::Audio::on_event(Event &event) {
    }
} // namespace milg
