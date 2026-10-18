#pragma once
#include "SerializationJson.h"
#include "../ecs/Entity.h"
#include "../game/AudioSource.h"
#include "../ecs/components/AudioSourceComponent.h"
#include "../game/SurfaceProperty.h"
#include <optional>
#include <ostream>
#include <unordered_map>

namespace Archura::Serialization {
// Data components common to .scene saves and project files. Runtime-only script
// instance IDs are deliberately reset when a component is constructed.
struct EntityData {
    EntityID id = 0, parent = 0;
    std::string persistentID, parentPersistentID;
    bool engineOwned = false;
    std::optional<BoxCollider> collider;
    std::optional<LightComponent> light;
    std::optional<RigidBody> rigidBody;
    std::optional<Health> health;
    std::optional<ScriptComponent> script;
    std::optional<SpawnPoint> spawn;
    std::optional<SkyboxComponent> skybox;
    std::optional<AudioSource> audio;
    std::optional<AudioSourceComponent> audioComponent;
    std::optional<SurfaceProperty> surface;
};
inline void Vec(std::ostream& out, const glm::vec3& v) { out << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
inline bool HasOnlySupportedComponents(Entity* entity) {
    const size_t known = static_cast<size_t>(entity->HasComponent<Transform>()) +
        entity->HasComponent<MeshRenderer>() + entity->HasComponent<BoxCollider>() +
        entity->HasComponent<LightComponent>() + entity->HasComponent<RigidBody>() +
        entity->HasComponent<Health>() + entity->HasComponent<ScriptComponent>() +
        entity->HasComponent<SpawnPoint>() + entity->HasComponent<SkyboxComponent>() +
        entity->HasComponent<AudioSource>() + entity->HasComponent<AudioSourceComponent>() +
        entity->HasComponent<SurfaceProperty>();
    return known == entity->GetComponentCount();
}
inline void WriteComponents(std::ostream& out, Entity* entity) {
    out << ",\n      \"engine_owned\":" << (entity->IsEngineOwned() ? "true" : "false");
    out << ",\n      \"persistent_id\":\"" << entity->GetPersistentID()
        << "\",\"parent_persistent_id\":\""
        << (entity->GetParent() ? entity->GetParent()->GetPersistentID() : "")
        << "\",\"components\":{";
    bool first = true;
    auto key = [&](const char* name) { if (!first) out << ','; first = false; out << '"' << name << "\":{"; };
    if (auto* c = entity->GetComponent<BoxCollider>()) { key("collider"); out << "\"size\":"; Vec(out,c->size);out << ",\"center\":";Vec(out,c->center);out << ",\"shape\":" << static_cast<int>(c->shape) << ",\"trigger\":" << (c->isTrigger?"true":"false") << '}'; }
    if (auto* c = entity->GetComponent<LightComponent>()) { key("light"); out << "\"type\":" << static_cast<int>(c->type) << ",\"color\":";Vec(out,c->color);out << ",\"intensity\":" << c->intensity << ",\"range\":" << c->range << '}'; }
    if (auto* c = entity->GetComponent<RigidBody>()) { key("rigid_body"); out << "\"velocity\":";Vec(out,c->velocity);out << ",\"force\":";Vec(out,c->force);out << ",\"mass\":" << c->mass << ",\"drag\":" << c->drag << ",\"restitution\":" << c->restitution << ",\"friction\":" << c->friction << ",\"gravity\":" << (c->useGravity?"true":"false") << ",\"kinematic\":" << (c->isKinematic?"true":"false") << ",\"continuous\":" << (c->continuous?"true":"false") << '}'; }
    if (auto* c = entity->GetComponent<Health>()) { key("health");out << "\"current\":" << c->current << ",\"max\":" << c->max << ",\"dead\":" << (c->isDead?"true":"false") << '}'; }
    if (auto* c = entity->GetComponent<ScriptComponent>()) { key("script");out << "\"class_name\":\"" << Escape(c->className) << "\"}"; }
    if (auto* c = entity->GetComponent<SpawnPoint>()) { key("spawn");out << "\"team_id\":" << c->teamId << '}'; }
    if (auto* c = entity->GetComponent<SkyboxComponent>()) { key("skybox");out << "\"faces\":[";for(size_t i=0;i<c->facePaths.size();++i){if(i)out<<',';out<<'"'<<Escape(c->facePaths[i])<<'"';}out << "]}"; }
    if (auto* c = entity->GetComponent<AudioSource>()) { key("audio");out << "\"clip\":\"" << Escape(c->clipName) << "\",\"volume\":" << c->volume << ",\"loop\":" << (c->loop?"true":"false") << ",\"spatial\":" << (c->is3D?"true":"false") << ",\"min_distance\":" << c->minDistance << ",\"max_distance\":" << c->maxDistance << '}'; }
    if (auto* c = entity->GetComponent<AudioSourceComponent>()) { key("audio_source");out << "\"clip\":\"" << Escape(c->clip_path) << "\",\"gain\":" << c->gain << ",\"pitch\":" << c->pitch << ",\"ref_distance\":" << c->ref_distance << ",\"loop\":" << (c->looping?"true":"false") << '}'; }
    if (auto* c = entity->GetComponent<SurfaceProperty>()) { key("surface");out << "\"type\":" << static_cast<int>(c->type) << '}'; }
    out << '}';
}
inline bool IDField(const Json& data, const char* name, EntityID& id) {
    const Json* value=data.Find(name);if(!value)return true;
    if(value->kind!=Json::Kind::Number || value->number<0 || value->number>std::numeric_limits<EntityID>::max() || std::floor(value->number)!=value->number)return false;
    id=static_cast<EntityID>(value->number);return true;
}
inline bool ReadComponents(const Json& data, EntityData& result) {
    const bool hasPersistentID = data.Find("persistent_id") != nullptr;
    if (hasPersistentID != (data.Find("parent_persistent_id") != nullptr)) return false;
    if (hasPersistentID &&
        (!StringField(data, "persistent_id", result.persistentID, true) ||
         !StringField(data, "parent_persistent_id", result.parentPersistentID, true) ||
         !Entity::IsValidPersistentID(result.persistentID) ||
         (!result.parentPersistentID.empty() && !Entity::IsValidPersistentID(result.parentPersistentID)))) return false;
    if (!BoolField(data, "engine_owned", result.engineOwned)) return false;
    if(!IDField(data,"id",result.id)||!IDField(data,"parent_id",result.parent))return false;
    const Json* components=data.Find("components");if(!components)return true;
    if(components->kind!=Json::Kind::Object)return false;
    for(const auto& pair:components->object){
        const auto& key=pair.first;const auto& c=pair.second;
        if(c.kind!=Json::Kind::Object)return false;
        if(key=="collider") { BoxCollider v;int shape=0;if(!Vec3Field(c,"size",v.size)||!Vec3Field(c,"center",v.center)||!IntField(c,"shape",shape)||shape<0||shape>1||!BoolField(c,"trigger",v.isTrigger))return false;v.shape=static_cast<BoxCollider::Shape>(shape);result.collider=v; }
        else if(key=="light") { LightComponent v;int type=1;if(!Vec3Field(c,"color",v.color)||!FloatField(c,"intensity",v.intensity)||!FloatField(c,"range",v.range)||!IntField(c,"type",type)||type<0||type>2||v.intensity<0||v.range<0)return false;v.type=static_cast<LightComponent::Type>(type);result.light=v; }
        else if(key=="rigid_body") { RigidBody v;if(!Vec3Field(c,"velocity",v.velocity)||!Vec3Field(c,"force",v.force)||!FloatField(c,"mass",v.mass)||!FloatField(c,"drag",v.drag)||!FloatField(c,"restitution",v.restitution)||!FloatField(c,"friction",v.friction)||!BoolField(c,"gravity",v.useGravity)||!BoolField(c,"kinematic",v.isKinematic)||!BoolField(c,"continuous",v.continuous))return false;result.rigidBody=v; }
        else if(key=="health") { Health v;if(!FloatField(c,"current",v.current)||!FloatField(c,"max",v.max)||!BoolField(c,"dead",v.isDead))return false;result.health=v; }
        else if(key=="script") { ScriptComponent v;if(!StringField(c,"class_name",v.className,true))return false;result.script=std::move(v); }
        else if(key=="spawn") { SpawnPoint v;if(!IntField(c,"team_id",v.teamId))return false;result.spawn=v; }
        else if(key=="skybox") { SkyboxComponent v;const auto* faces=c.Find("faces");if(!faces||faces->kind!=Json::Kind::Array||faces->array.size()!=6)return false;for(size_t i=0;i<6;++i){if(faces->array[i].kind!=Json::Kind::String)return false;v.facePaths[i]=faces->array[i].string;}result.skybox=std::move(v); }
        else if(key=="audio") { AudioSource v;if(!StringField(c,"clip",v.clipName,true)||!FloatField(c,"volume",v.volume)||!FloatField(c,"min_distance",v.minDistance)||!FloatField(c,"max_distance",v.maxDistance)||!BoolField(c,"loop",v.loop)||!BoolField(c,"spatial",v.is3D)||v.volume<0||v.volume>1||v.minDistance<0||v.maxDistance<v.minDistance)return false;result.audio=std::move(v); }
        else if(key=="audio_source") { AudioSourceComponent v;if(!StringField(c,"clip",v.clip_path,true)||!FloatField(c,"gain",v.gain)||!FloatField(c,"pitch",v.pitch)||!FloatField(c,"ref_distance",v.ref_distance)||!BoolField(c,"loop",v.looping)||v.gain<0||v.pitch<=0||v.ref_distance<0)return false;result.audioComponent=std::move(v); }
        else if(key=="surface") { int type=0;if(!IntField(c,"type",type)||type<0||type>5)return false;result.surface=SurfaceProperty(static_cast<SurfaceType>(type)); }
        else return false; // Unknown serialized components must never disappear silently.
    }
    return true;
}
inline void ApplyComponents(Entity* entity, const EntityData& data) {
    entity->SetOwnership(data.engineOwned ? EntityOwnership::Engine : EntityOwnership::Authored);
    if(data.collider)*entity->AddComponent<BoxCollider>()=*data.collider;
    if(data.light)*entity->AddComponent<LightComponent>()=*data.light;
    if(data.rigidBody)*entity->AddComponent<RigidBody>()=*data.rigidBody;
    if(data.health)*entity->AddComponent<Health>()=*data.health;
    if(data.script)*entity->AddComponent<ScriptComponent>()=*data.script;
    if(data.spawn)*entity->AddComponent<SpawnPoint>()=*data.spawn;
    if(data.skybox)*entity->AddComponent<SkyboxComponent>()=*data.skybox;
    if(data.audio)*entity->AddComponent<AudioSource>()=*data.audio;
    if(data.audioComponent)*entity->AddComponent<AudioSourceComponent>()=*data.audioComponent;
    if(data.surface)*entity->AddComponent<SurfaceProperty>()=*data.surface;
}
inline std::string IdentityKey(const EntityData& data) {
    return data.persistentID.empty() ? (data.id ? std::to_string(data.id) : "") : data.persistentID;
}
inline std::string ParentKey(const EntityData& data) {
    return data.persistentID.empty() ? (data.parent ? std::to_string(data.parent) : "") : data.parentPersistentID;
}
template<class Data> bool ValidateHierarchy(const std::vector<Data>& loaded) {
    // Bound hierarchy depth before linking to protect recursive world-transform
    // evaluation and avoid quadratic walks on hostile deeply nested input.
    std::unordered_map<std::string, std::string> parents;
    for (const auto& data : loaded) {
        const auto id = IdentityKey(data.extra), parent = ParentKey(data.extra);
        if (!parent.empty() && id.empty()) return false;
        if (!id.empty() && !parents.emplace(id, parent).second) return false;
    }
    for (const auto& pair : parents) {
        std::string cursor = pair.first;
        for (unsigned depth = 0; !cursor.empty(); ++depth) {
            if (depth >= 128) return false;
            const auto found = parents.find(cursor);
            if (found == parents.end()) return false;
            cursor = found->second;
        }
    }
    return true;
}
template<class Data> bool LinkHierarchy(Scene* scene, const std::vector<Data>& loaded, const std::vector<EntityHandle>& created) {
    std::unordered_map<std::string,Entity*> ids;
    for(size_t i=0;i<loaded.size();++i) { const auto id=IdentityKey(loaded[i].extra);if(!id.empty() && !ids.emplace(id,scene->GetEntity(created[i])).second)return false; }
    if (!ValidateHierarchy(loaded)) return false;
    for(size_t i=0;i<loaded.size();++i) { const auto key=ParentKey(loaded[i].extra);if(!key.empty()){auto parent=ids.find(key);if(parent==ids.end()||!scene->GetEntity(created[i])->TrySetParent(parent->second))return false;} }
    return true;
}
} // namespace Archura::Serialization
