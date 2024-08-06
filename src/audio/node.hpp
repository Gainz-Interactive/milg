#pragma once

#include <map>
#include <memory>
#include <miniaudio.h>

namespace milg::audio {
    class Node: public std::enable_shared_from_this<Node> {
    public:
        template<std::size_t bus>
        bool attach_to(std::shared_ptr<Node> input) {
            if (this->outputs.contains(bus)) {
                return false;
            }
            if (!input->attach_output<bus>(shared_from_this())) {
                return false;
            }

            this->outputs.insert({ bus, input });

            return true;
        }
    protected:
        template<std::size_t bus>
        bool attach_output(std::shared_ptr<Node> output) {
            if (this->inputs.contains(bus)) {
                return false;
            }

            this->inputs.insert({ bus, output });

            return true;
        }
    private:
        std::map<std::size_t, std::shared_ptr<Node>> inputs;
        std::map<std::size_t, std::shared_ptr<Node>> outputs;
    };
}
