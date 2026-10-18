#pragma once

#include "SerializationJson.h"
#include "../game/Weapon.h"
#include <iomanip>
#include <ostream>

namespace Archura::Serialization {
namespace WeaponDataDetail {
inline bool ValidType(Weapon::WeaponType type) {
    const int value = static_cast<int>(type);
    return value >= 0 && value <= 5;
}
inline bool ValidStats(const Weapon::WeaponStats& s) {
    if (!std::isfinite(s.damage) || s.damage < 0 ||
        !std::isfinite(s.fireRate) || s.fireRate <= 0 ||
        !std::isfinite(s.range) || s.range < 0 ||
        !std::isfinite(s.reloadTime) || s.reloadTime < 0 ||
        !std::isfinite(s.recoilAmount) || s.recoilAmount < 0 ||
        s.magSize < 0 || s.currentMag < 0 || s.currentMag > s.magSize ||
        s.totalAmmo < 0 || s.pattern.offsets.size() > 4096) return false;
    for (const auto& offset : s.pattern.offsets)
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) return false;
    return true;
}
inline bool ReadType(const Json& value, Weapon::WeaponType& out) {
    int type = -1;
    if (!value.Find("type") || !IntField(value, "type", type) || type < 0 || type > 5) return false;
    out = static_cast<Weapon::WeaponType>(type);
    return true;
}
inline bool ReadStats(const Json& value, Weapon::WeaponStats& out) {
    if (value.kind != Json::Kind::Object) return false;
    for (const auto* key : {"damage", "fire_rate", "range", "mag_size", "current_mag",
                            "total_ammo", "reload_time", "recoil_amount", "automatic", "pattern"})
        if (!value.Find(key)) return false;
    if (!FloatField(value, "damage", out.damage) ||
        !FloatField(value, "fire_rate", out.fireRate) ||
        !FloatField(value, "range", out.range) ||
        !IntField(value, "mag_size", out.magSize) ||
        !IntField(value, "current_mag", out.currentMag) ||
        !IntField(value, "total_ammo", out.totalAmmo) ||
        !FloatField(value, "reload_time", out.reloadTime) ||
        !FloatField(value, "recoil_amount", out.recoilAmount) ||
        !BoolField(value, "automatic", out.isAutomatic)) return false;
    const auto& pattern = *value.Find("pattern");
    if (pattern.kind != Json::Kind::Array || pattern.array.size() > 4096) return false;
    out.pattern.offsets.clear();
    out.pattern.offsets.reserve(pattern.array.size());
    for (const auto& pair : pattern.array) {
        if (pair.kind != Json::Kind::Array || pair.array.size() != 2) return false;
        glm::vec2 offset;
        for (int i = 0; i < 2; ++i) {
            const auto& v = pair.array[i];
            if (v.kind != Json::Kind::Number || !std::isfinite(v.number) ||
                std::abs(v.number) > std::numeric_limits<float>::max()) return false;
            offset[i] = static_cast<float>(v.number);
        }
        out.pattern.offsets.push_back(offset);
    }
    return ValidStats(out);
}
inline void WriteStats(std::ostream& out, const Weapon::WeaponStats& s) {
    out << "{\"damage\":" << s.damage << ",\"fire_rate\":" << s.fireRate
        << ",\"range\":" << s.range << ",\"mag_size\":" << s.magSize
        << ",\"current_mag\":" << s.currentMag << ",\"total_ammo\":" << s.totalAmmo
        << ",\"reload_time\":" << s.reloadTime << ",\"recoil_amount\":" << s.recoilAmount
        << ",\"automatic\":" << (s.isAutomatic ? "true" : "false") << ",\"pattern\":[";
    bool first = true;
    for (const auto& offset : s.pattern.offsets) {
        if (!first) out << ',';
        first = false;
        out << '[' << offset.x << ',' << offset.y << ']';
    }
    out << "]}";
}
} // namespace WeaponDataDetail

inline bool IsValidWeapon(const Weapon& weapon) {
    if (!WeaponDataDetail::ValidType(weapon.type) ||
        !WeaponDataDetail::ValidStats(weapon.stats) || weapon.inventory.size() > 6) return false;
    for (const auto& entry : weapon.inventory)
        if (!WeaponDataDetail::ValidType(entry.first) || !WeaponDataDetail::ValidStats(entry.second)) return false;
    return true;
}

// Active stats are deliberately independent of the stored inventory entry.
// Transient firing/reload state is neither persisted nor used for validation.
inline bool WriteWeapon(std::ostream& out, const Weapon& weapon) {
    if (!IsValidWeapon(weapon)) return false;
    std::ostringstream encoded;
    encoded.imbue(std::locale::classic());
    encoded << std::setprecision(std::numeric_limits<float>::max_digits10)
        << "{\"type\":" << static_cast<int>(weapon.type) << ",\"stats\":";
    WeaponDataDetail::WriteStats(encoded, weapon.stats);
    encoded << ",\"inventory\":[";
    bool first = true;
    for (const auto& entry : weapon.inventory) {
        if (!first) encoded << ',';
        first = false;
        encoded << "{\"type\":" << static_cast<int>(entry.first) << ",\"stats\":";
        WeaponDataDetail::WriteStats(encoded, entry.second);
        encoded << '}';
    }
    encoded << "]}";
    if (!encoded) return false;
    out << encoded.str();
    return static_cast<bool>(out);
}

inline bool ReadWeapon(const Json& data, Weapon& result) {
    if (data.kind != Json::Kind::Object) return false;
    Weapon parsed;
    const auto* stats = data.Find("stats");
    const auto* inventory = data.Find("inventory");
    if (!WeaponDataDetail::ReadType(data, parsed.type) || !stats ||
        !WeaponDataDetail::ReadStats(*stats, parsed.stats) || !inventory ||
        inventory->kind != Json::Kind::Array || inventory->array.size() > 6) return false;
    for (const auto& entry : inventory->array) {
        Weapon::WeaponType type;
        Weapon::WeaponStats stored;
        const auto* storedStats = entry.Find("stats");
        if (!WeaponDataDetail::ReadType(entry, type) || !storedStats ||
            !WeaponDataDetail::ReadStats(*storedStats, stored) ||
            !parsed.inventory.emplace(type, std::move(stored)).second) return false;
    }
    result = std::move(parsed);
    return true;
}
} // namespace Archura::Serialization
