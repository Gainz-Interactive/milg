#pragma once

#include "node.hpp"

#include <memory>
#include <miniaudio.h>

namespace milg::audio {
    void init();
    void destroy();

    std::shared_ptr<Node> get_endpoint();
    ma_engine *get_engine();
    ma_node_graph *get_node_graph();

    float get_volume();
    void set_volume(float volume);
}
