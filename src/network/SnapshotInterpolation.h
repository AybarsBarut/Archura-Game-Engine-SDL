#pragma once
#include "AuthoritativeWorld.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>

namespace Archura::Net {
// Presentation only. Times are monotonic seconds, never simulation delta-times.
// Four 128Hz ticks of delay; bounded history; no extrapolation or prediction.
class SnapshotInterpolation {
public:
    static constexpr double DelayTicks = 4;
    static constexpr std::size_t Capacity = 32;
    bool Push(const WorldSnapshot& snapshot, double now) {
        if (!std::isfinite(now) || now < 0 || !snapshot.tick ||
            snapshot.players.size() > kMaxWorldPlayers) return false;
        std::uint32_t previous = 1;
        for (const auto& player : snapshot.players) {
            if (player.id <= previous || !std::isfinite(player.x) || !std::isfinite(player.z) ||
                std::abs(player.x) > kWorldExtent || std::abs(player.z) > kWorldExtent) return false;
            previous = player.id;
        }
        if (!m_History.empty() && (snapshot.tick <= m_History.back().tick || now < m_Arrival)) return false;
        if (m_History.empty() || snapshot.tick-m_History.back().tick > 32) {
            Reset(); m_Origin = snapshot.tick;
        }
        m_History.push_back(snapshot); m_Arrival = now;
        if (m_History.size() > Capacity) m_History.pop_front();
        return true;
    }
    WorldSnapshot Sample(double now) {
        if (m_History.empty()) return {};
        const auto& latest = m_History.back();
        const double newest = static_cast<double>(latest.tick-m_Origin);
        if (std::isfinite(now)) {
            const double desired = newest + std::max(0.0, now-m_Arrival)*128.0 - DelayTicks;
            m_Cursor = std::max(m_Cursor, std::min(newest, desired));
        }
        m_Cursor = std::max(m_Cursor, static_cast<double>(m_History.front().tick-m_Origin));
        const WorldSnapshot* lower = &m_History.front();
        const WorldSnapshot* upper = lower;
        for (const auto& snapshot : m_History) {
            upper = &snapshot;
            if (static_cast<double>(snapshot.tick-m_Origin) >= m_Cursor) break;
            lower = upper;
        }
        const double span = static_cast<double>(upper->tick-lower->tick);
        const float alpha = span > 0 ? static_cast<float>(std::clamp(
            (m_Cursor-static_cast<double>(lower->tick-m_Origin))/span, 0.0, 1.0)) : 0;
        // Latest authority owns membership and acknowledgements. Never resurrect peers.
        WorldSnapshot result = latest;
        for (auto& player : result.players) {
            const auto find = [&](const WorldSnapshot& snapshot) -> const WorldPlayer* {
                const auto it = std::lower_bound(snapshot.players.begin(), snapshot.players.end(), player.id,
                    [](const WorldPlayer& value, std::uint32_t id) { return value.id < id; });
                return it != snapshot.players.end() && it->id == player.id ? &*it : nullptr;
            };
            // Find this player's current continuous lifetime/trajectory. After a
            // spawn or teleport hold its first pose until the cursor catches up;
            // falling back to latest would later rewind when interpolation resumes.
            const WorldSnapshot* start = &latest;
            const WorldPlayer* next = find(latest);
            for (auto it = std::next(m_History.rbegin()); it != m_History.rend(); ++it) {
                const auto* current = find(*it);
                const double ticks = static_cast<double>(start->tick-it->tick);
                if (!current || std::hypot(next->x-current->x, next->z-current->z) > kWorldSpeed*ticks/128.0+0.001) break;
                start = &*it; next = current;
            }
            const auto* a = find(*lower); const auto* b = find(*upper);
            if (lower->tick < start->tick) {
                const auto* first = find(*start);
                player.x = first->x; player.z = first->z;
            } else if (a && b) {
                player.x = a->x+(b->x-a->x)*alpha;
                player.z = a->z+(b->z-a->z)*alpha;
            }
        }
        return result;
    }
    void Reset() { m_History.clear(); m_Origin = 0; m_Arrival = 0; m_Cursor = 0; }
    std::size_t Size() const { return m_History.size(); }
private:
    std::deque<WorldSnapshot> m_History;
    std::uint64_t m_Origin = 0;
    double m_Arrival = 0, m_Cursor = 0;
};
} // namespace Archura::Net
