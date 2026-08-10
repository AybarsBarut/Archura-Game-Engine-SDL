#pragma once

namespace Archura {

class Scene;

/**
 * @brief System base class - Game logic sistemleri buradan türer
 */
class System {
public:
    System() = default;
    virtual ~System() = default;

    System(const System&) = delete;
    System& operator=(const System&) = delete;
    System(System&&) = delete;
    System& operator=(System&&) = delete;

    // Systems are main-thread affine unless a derived system explicitly owns
    // immutable snapshots and performs a synchronized publish step.
    virtual void Init(Scene* scene) { m_Scene = scene; }
    virtual void Update(float deltaTime) = 0;
    virtual void Shutdown() {}

protected:
    Scene* m_Scene = nullptr;
};

} // namespace Archura
