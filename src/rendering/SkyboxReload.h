#pragma once
#include "../ecs/Component.h"
#include <utility>

namespace Archura {
// One request is one attempt. Failed file loads retain the existing cubemap;
// another edit or the Reload button explicitly requests the next attempt.
template<class Loader>
bool ConsumeSkyboxReloadRequest(SkyboxComponent& component, Loader&& loader) {
    if (!component.shouldReload) return false;
    component.shouldReload = false;
    return std::forward<Loader>(loader)(component.facePaths);
}
} // namespace Archura
