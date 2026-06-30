#pragma once

#include "../Component.h"

#include <string>

namespace Archura {

struct AudioSourceComponent : public Component {
    std::string clip_path;
    float gain = 1.0f;
    float pitch = 1.0f;
    float ref_distance = 1.0f;
    bool looping = false;
    bool playing = false;
};

} // namespace Archura
