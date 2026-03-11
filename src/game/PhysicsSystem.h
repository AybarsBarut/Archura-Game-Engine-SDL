#pragma once

#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "CollisionEvent.h"
#include <vector>
#include <glm/glm.hpp>

namespace Archura {

    class Scene;

    /**
     * @brief Guclendirilmis Fizik Sistemi.
     *
     * Yenilikler:
     *  - Tamamlanmis AABB Raycast
     *  - Sphere Collider destegi
     *  - Carpisma callback'leri (OnCollisionEnter, OnCollisionExit)
     *  - Debug draw destegi (wireframe collider goruntuleme)
     *  - SetGravity / Simulate kontrolleri
     */
    class PhysicsSystem {
    public:
        void Init(Scene* scene);
        void Update(float deltaTime);
        void Shutdown();

        // ── Ayarlar ─────────────────────────────────────────────────────────────
        void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }
        const glm::vec3& GetGravity() const { return m_Gravity; }

        /// true ise fizik guncellenmez (Debug / Pause amacli)
        void SetSimulating(bool enabled) { m_Simulating = enabled; }
        bool IsSimulating() const { return m_Simulating; }

        /// Wireframe collider debug cizgilerini etkinlestir/devre disi birak
        void SetDebugDraw(bool enabled) { m_DebugDraw = enabled; }
        bool GetDebugDraw() const { return m_DebugDraw; }

        // ── Callbacks ────────────────────────────────────────────────────────────
        /**
         * @brief Iki entity birbirinden ayrildiginda cagrilacak fonksiyonu kaydet.
         *        Birden fazla dinleyici desteklenmez; tekrar cagirilirsa uzerine yazar.
         */
        void SetOnCollisionEnter(CollisionCallback cb) { m_OnEnter = std::move(cb); }
        void SetOnCollisionExit (CollisionCallback cb) { m_OnExit  = std::move(cb); }

        // ── Raycast ──────────────────────────────────────────────────────────────
        /**
         * @brief AABB Raycast: verilen isin ile sahnedeki ilk BoxCollider'i bul.
         * @param origin       Isin baslangic noktasi (dunya koordinati)
         * @param direction    Normallestirmis isin yonu
         * @param maxDistance  Maksimum mesafe
         * @param outEntity    Carpilamayan entity (cikti, nullable)
         * @param outHitPoint  Carpisma noktasi (cikti, nullable)
         * @return true eger bir nesne carpiliga
         */
        bool Raycast(const glm::vec3& origin,
                     const glm::vec3& direction,
                     float            maxDistance,
                     Entity**         outEntity,
                     glm::vec3*       outHitPoint);

        /**
         * @brief Sphere Raycast: kure ile carpismayi test eder.
         */
        bool RaycastSphere(const glm::vec3& origin,
                           const glm::vec3& direction,
                           float            maxDistance,
                           float            sphereRadius,
                           Entity**         outEntity,
                           glm::vec3*       outHitPoint);

    private:
        Scene*    m_Scene     = nullptr;
        glm::vec3 m_Gravity   = glm::vec3(0.0f, -9.81f, 0.0f);
        bool      m_Simulating = true;
        bool      m_DebugDraw  = false;

        void Integrate(float deltaTime);
        void ResolveCollisions();

        // AABB yardimci
        bool CheckAABB(const glm::vec3& posA, const glm::vec3& sizeA,
                       const glm::vec3& posB, const glm::vec3& sizeB);

        // Ray-AABB kesisim (slab yontemi) - carpisma t degerini verir
        bool RayAABB(const glm::vec3& origin, const glm::vec3& invDir,
                     const glm::vec3& boxMin, const glm::vec3& boxMax,
                     float& tMin, float& tMax);

        // Onceki frame carpisma listesi (Enter/Exit tespiti icin)
        struct CollisionPair {
            EntityID a, b;
            bool operator==(const CollisionPair& o) const {
                return (a == o.a && b == o.b) || (a == o.b && b == o.a);
            }
        };
        std::vector<CollisionPair> m_PrevCollisions;
        std::vector<CollisionPair> m_CurrCollisions;

        CollisionCallback m_OnEnter;
        CollisionCallback m_OnExit;
    };

} // namespace Archura
