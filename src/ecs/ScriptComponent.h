#pragma once

#include <string>

namespace Archura {

    struct ScriptComponent {
        std::string ClassName;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent&) = default;
        ScriptComponent(const std::string& className) : ClassName(className) {}
    };

}
