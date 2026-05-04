#pragma once

#include "ComponentStorage.h"
#include "System.h"

namespace Archura {

class TransformSoASystem : public System {
public:
  void Init(Scene *scene) override;
  void Update(float deltaTime) override;
  void Shutdown() override;

  void RebuildFromScene();
  void SyncBackToScene();

  ComponentStorage<Transform> &Transforms() { return m_Transforms; }
  ComponentStorage<RigidBody> &Bodies() { return m_Bodies; }

private:
  void IntegrateRigidBodiesSIMD(float deltaTime);

  ComponentStorage<Transform> m_Transforms;
  ComponentStorage<RigidBody> m_Bodies;
};

} // namespace Archura
