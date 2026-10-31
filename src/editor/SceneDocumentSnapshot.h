#pragma once

#include "../core/GameSaveManager.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <ostream>
#include <streambuf>
#include <string>
#include <utility>

namespace Archura {
// CPU-only authoring comparison using the existing codec. Retains no Scene/GPU
// owners, never loads assets, and treats unsupported/unbounded output as unknown.
inline std::optional<std::string> CaptureSceneDocument(Scene* scene,
                                                     size_t limit = 32u * 1024u * 1024u) {
    class Buffer final : public std::streambuf {
    public:
        explicit Buffer(size_t limit) : limit(limit) {}
        std::string data;
    private:
        std::streamsize xsputn(const char* text, std::streamsize count) override {
            if (count < 0 || static_cast<size_t>(count) > limit - data.size()) return 0;
            data.append(text, static_cast<size_t>(count));
            return count;
        }
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
            if (data.size() == limit) return traits_type::eof();
            data.push_back(traits_type::to_char_type(ch));
            return ch;
        }
        size_t limit;
    } buffer(limit);
    std::ostream stream(&buffer);
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    if (!GameSaveManager::WriteEntities(stream, scene, false, false) || !stream) return std::nullopt;
    return std::move(buffer.data);
}
} // namespace Archura
