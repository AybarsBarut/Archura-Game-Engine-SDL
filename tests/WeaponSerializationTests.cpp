#include "core/WeaponData.h"
#include <iostream>
#include <limits>

namespace {
using namespace Archura;
using namespace Archura::Serialization;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " << #condition << '\n'; ++failures; } } while (false)

bool EqualStats(const Weapon::WeaponStats& a, const Weapon::WeaponStats& b) {
    if (a.damage != b.damage || a.fireRate != b.fireRate || a.range != b.range ||
        a.magSize != b.magSize || a.currentMag != b.currentMag || a.totalAmmo != b.totalAmmo ||
        a.reloadTime != b.reloadTime || a.recoilAmount != b.recoilAmount ||
        a.isAutomatic != b.isAutomatic || a.pattern.offsets.size() != b.pattern.offsets.size()) return false;
    for (size_t i = 0; i < a.pattern.offsets.size(); ++i)
        if (a.pattern.offsets[i].x != b.pattern.offsets[i].x || a.pattern.offsets[i].y != b.pattern.offsets[i].y) return false;
    return true;
}
bool Equal(const Weapon& a, const Weapon& b) {
    if (a.type != b.type || !EqualStats(a.stats, b.stats) || a.inventory.size() != b.inventory.size()) return false;
    for (const auto& entry : a.inventory) {
        const auto found = b.inventory.find(entry.first);
        if (found == b.inventory.end() || !EqualStats(entry.second, found->second)) return false;
    }
    return true;
}
Json Encode(const Weapon& weapon) {
    std::ostringstream out;
    CHECK(WriteWeapon(out, weapon));
    Json result;
    CHECK(JsonParser(out.str()).Parse(result));
    return result;
}
void CheckRoundTrip(const Weapon& input) {
    Weapon output;
    output.InitInventory();
    output.isReloading = true;
    output.reloadTimer = 9;
    output.timeSinceLastShot = 9;
    output.currentRecoil = 9;
    output.shotsFired = 9;
    CHECK(ReadWeapon(Encode(input), output));
    CHECK(Equal(input, output));
    CHECK(!output.isReloading && output.reloadTimer == 0 && output.timeSinceLastShot == 0 &&
          output.currentRecoil == 0 && output.shotsFired == 0);
}
void Reject(const Json& data) {
    Weapon destination;
    destination.InitInventory();
    destination.stats.damage = 777;
    destination.isReloading = true;
    destination.shotsFired = 42;
    const Weapon before = destination;
    CHECK(!ReadWeapon(data, destination));
    CHECK(Equal(before, destination));
    CHECK(destination.isReloading && destination.shotsFired == 42);
}
Json Number(double number) { Json v; v.kind = Json::Kind::Number; v.number = number; return v; }

void RoundTrips() {
    Weapon weapon;
    CheckRoundTrip(weapon); // A real default has no inventory.
    weapon.InitInventory();
    CheckRoundTrip(weapon);
    weapon.type = Weapon::WeaponType::Sniper;
    weapon.stats.damage = 0.123456791f;
    weapon.stats.fireRate = 0.123456791f;
    weapon.stats.range = 987.123474f;
    weapon.stats.magSize = 17;
    weapon.stats.currentMag = 3;
    weapon.stats.totalAmmo = 199;
    weapon.stats.reloadTime = 0.345678925f;
    weapon.stats.recoilAmount = 0.234567896f;
    weapon.stats.isAutomatic = false;
    weapon.stats.pattern.offsets = {{0.123456791f, -0.234567896f}, {-3.25f, 8.75f}};
    weapon.inventory[Weapon::WeaponType::Shotgun] = weapon.stats;
    weapon.inventory[Weapon::WeaponType::Sniper] = weapon.stats;
    weapon.inventory[Weapon::WeaponType::Sniper].currentMag = 16;
    weapon.inventory[Weapon::WeaponType::Pistol].pattern.offsets = {{-0.125f, 2.5f}};
    CheckRoundTrip(weapon); // All six enum values, independent active and inventory stats.
    weapon.stats.pattern.offsets.resize(4096, glm::vec2(-0.125f, 2.5f));
    CheckRoundTrip(weapon);
}

void InvalidInputs() {
    Weapon weapon;
    weapon.InitInventory();
    const Json valid = Encode(weapon);
    Reject(Json{});
    for (const auto* key : {"type", "stats", "inventory"}) {
        Json bad = valid; bad.object.erase(key); Reject(bad);
        bad = valid; bad.object[key] = Json{}; Reject(bad);
    }
    for (const auto* key : {"damage", "fire_rate", "range", "mag_size", "current_mag", "total_ammo",
                            "reload_time", "recoil_amount", "automatic", "pattern"}) {
        Json bad = valid; bad.object["stats"].object.erase(key); Reject(bad);
        bad = valid; bad.object["stats"].object[key] = Json{}; Reject(bad);
    }
    for (double value : {-1.0, 6.0, 0.5, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Json bad = valid; bad.object["type"] = Number(value); Reject(bad);
        bad = valid; bad.object["inventory"].array[0].object["type"] = Number(value); Reject(bad);
    }
    for (const auto* key : {"damage", "fire_rate", "range", "reload_time", "recoil_amount"}) {
        for (double value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), 1e100}) {
            Json bad = valid; bad.object["stats"].object[key] = Number(value); Reject(bad);
        }
    }
    for (const auto* key : {"mag_size", "current_mag", "total_ammo"}) {
        for (double value : {-1.0, 1.5, 1e100}) {
            Json bad = valid; bad.object["stats"].object[key] = Number(value); Reject(bad);
        }
    }
    Json bad = valid; bad.object["stats"].object["fire_rate"] = Number(0); Reject(bad);
    bad = valid; bad.object["stats"].object["current_mag"] = Number(31); Reject(bad);
    bad = valid; bad.object["stats"].object["automatic"] = Number(1); Reject(bad);
    bad = valid; bad.object["inventory"].array.push_back(bad.object["inventory"].array.front()); Reject(bad);
    bad = valid; bad.object["inventory"].array.resize(7); Reject(bad);
    bad = valid; bad.object["inventory"].array[0].object.erase("stats"); Reject(bad);
    bad = valid; bad.object["inventory"].array[0].object["stats"].object["damage"] = Number(-1); Reject(bad);
    for (size_t count : {size_t(0), size_t(1), size_t(3)}) {
        bad = valid;
        Json pair; pair.kind = Json::Kind::Array; pair.array.resize(count, Number(1));
        bad.object["stats"].object["pattern"].array.push_back(pair); Reject(bad);
    }
    for (const Json& item : {Json{}, Number(std::numeric_limits<double>::quiet_NaN()), Number(1e100)}) {
        bad = valid;
        Json pair; pair.kind = Json::Kind::Array; pair.array = {item, Number(0)};
        bad.object["stats"].object["pattern"].array.push_back(pair); Reject(bad);
    }
    bad = valid; bad.object["stats"].object["pattern"].array.push_back(Number(1)); Reject(bad);
    bad = valid; bad.object["stats"].object["pattern"].array.resize(4097); Reject(bad);
    Json unknown = valid; unknown.object["future_metadata"] = Json{};
    Weapon decoded; CHECK(ReadWeapon(unknown, decoded));
    Json parsed;
    CHECK(!JsonParser("{\"type\":0,\"type\":1}").Parse(parsed));
    CHECK(!JsonParser("{\"type\":NaN}").Parse(parsed));
}

void InvalidWrites() {
    Weapon valid;
    auto reject = [](const Weapon& weapon) {
        std::ostringstream out; out << "prefix";
        CHECK(!WriteWeapon(out, weapon));
        CHECK(out.str() == "prefix");
    };
    Weapon bad = valid; bad.type = static_cast<Weapon::WeaponType>(6); reject(bad);
    bad = valid; bad.stats.damage = std::numeric_limits<float>::quiet_NaN(); reject(bad);
    bad = valid; bad.stats.fireRate = 0; reject(bad);
    bad = valid; bad.stats.currentMag = 13; reject(bad);
    bad = valid; bad.stats.totalAmmo = -1; reject(bad);
    bad = valid; bad.stats.pattern.offsets = {{0, std::numeric_limits<float>::infinity()}}; reject(bad);
    bad = valid; bad.stats.pattern.offsets.resize(4097); reject(bad);
    bad = valid; bad.inventory[static_cast<Weapon::WeaponType>(-1)] = bad.stats; reject(bad);
    bad = valid; bad.inventory[Weapon::WeaponType::Pistol].reloadTime = -1; reject(bad);
}
} // namespace

int main() {
    RoundTrips();
    InvalidInputs();
    InvalidWrites();
    if (failures) { std::cerr << failures << " weapon serialization checks failed\n"; return 1; }
    std::cout << "Weapon serialization tests passed\n";
    return 0;
}
