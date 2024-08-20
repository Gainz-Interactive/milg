#pragma once

#include <cassert>
#include <map>
#include <memory>
#include <miniaudio.h>

namespace milg::audio {
    class Node : public std::enable_shared_from_this<Node> {
    public:
        template <std::size_t output_bus, std::size_t input_bus> bool attach_output(std::shared_ptr<Node> input) {
            if (!this->map_output<output_bus, input_bus>(input)) {
                return false;
            }
            if (!input->map_input<input_bus, output_bus>(shared_from_this())) {
                return false;
            }

            return ma_node_attach_output_bus(this->get_handle(), output_bus, input->get_handle(), input_bus) ==
                   MA_SUCCESS;
        }

        bool detach_input(std::size_t input_bus) {
            auto iter = this->inputs.find(input_bus);
            if (iter == this->inputs.end()) {
                return false;
            }
            auto [output, output_bus] = iter->second;

            return output->detach_output(output_bus);
        }

        bool detach_output(std::size_t output_bus) {
            auto iter = this->outputs.find(output_bus);
            if (iter == this->outputs.end()) {
                return false;
            }
            auto [input, input_bus] = iter->second;

            input->unmap_input(input_bus);
            this->unmap_output(output_bus);

            return ma_node_detach_output_bus(this->get_handle(), output_bus) == MA_SUCCESS;
        }

    protected:
        template <std::size_t input_bus, std::size_t output_bus> bool map_input(std::shared_ptr<Node> output) {
            auto [_, inserted] = this->inputs.insert({input_bus, {output, output_bus}});

            (void)_;

            return inserted;
        }
        template <std::size_t output_bus, std::size_t input_bus> bool map_output(std::shared_ptr<Node> input) {
            auto [_, inserted] = this->outputs.insert({output_bus, {input, input_bus}});

            (void)_;

            return inserted;
        }

        void unmap_input(std::size_t input_bus) {
            assert(this->inputs.erase(input_bus) != 0);
        }
        void unmap_output(std::size_t output_bus) {
            assert(this->outputs.erase(output_bus) != 0);
        }

        virtual ma_node *get_handle() = 0;

    private:
        std::map<std::size_t, std::pair<std::shared_ptr<Node>, std::size_t>> inputs;
        std::map<std::size_t, std::pair<std::shared_ptr<Node>, std::size_t>> outputs;
    };
} // namespace milg::audio
