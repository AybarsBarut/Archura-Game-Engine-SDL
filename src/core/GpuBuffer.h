#pragma once

#include <vector>
#include <cstdint>
#include <cassert>

namespace Archura {

/**
 * @brief Tip guevenli GPU veri tamponu (typed array wrapper).
 *
 * CPU tarafindaki verileri GPU'ya gondermek icin kolaylatirici:
 *   - Add() ile veri eklenir
 *   - Data() ile raw pointer alinir (glBufferData / glBufferSubData icin)
 *   - ByteSize() ile byte cinsinden boyut alinir
 *
 * Kullanimi:
 * @code
 *   TypedBuffer<float> verts(1000);
 *   verts.Add(x); verts.Add(y); verts.Add(z);
 *   glBufferData(GL_ARRAY_BUFFER, verts.ByteSize(), verts.Data(), GL_DYNAMIC_DRAW);
 *
 *   // Kismi guncelleme:
 *   verts.Reset();
 *   // ... yeni veri ekle ...
 *   glBufferSubData(GL_ARRAY_BUFFER, 0, verts.ByteSize(), verts.Data());
 * @endcode
 */
template<typename T>
class TypedBuffer {
public:
    explicit TypedBuffer(size_t reserveCount = 256) {
        m_Data.reserve(reserveCount);
    }

    /// Tek bir deger ekle
    void Add(T value) {
        m_Data.push_back(value);
    }

    /// Birden fazla deger tek satirda ekle (variadic helper)
    template<typename... Args>
    void Add(T first, Args... rest) {
        m_Data.push_back(first);
        Add(static_cast<T>(rest)...);
    }

    /// Tampon icerigini temizle (kapasite korunur)
    void Reset() { m_Data.clear(); }

    /// Ham veri pointer'i (glBufferData / glBufferSubData icin)
    const T* Data() const { return m_Data.data(); }
    T*       Data()       { return m_Data.data(); }

    /// Eleman sayisi
    size_t Count() const { return m_Data.size(); }

    /// Byte cinsinden boyut
    size_t ByteSize() const { return m_Data.size() * sizeof(T); }

    /// Bos mu?
    bool IsEmpty() const { return m_Data.empty(); }

    /// Dogrudan erisim
    T& operator[](size_t index) { return m_Data[index]; }
    const T& operator[](size_t index) const { return m_Data[index]; }

private:
    std::vector<T> m_Data;
};

// ---------------------------------------------------------------------------
// Kolaylik icin tip taklitci (aliaslar):
// ---------------------------------------------------------------------------
using Uint8Buffer   = TypedBuffer<uint8_t>;
using Int16Buffer   = TypedBuffer<int16_t>;
using Uint16Buffer  = TypedBuffer<uint16_t>;
using Int32Buffer   = TypedBuffer<int32_t>;
using Uint32Buffer  = TypedBuffer<uint32_t>;
using Float32Buffer = TypedBuffer<float>;
using Float64Buffer = TypedBuffer<double>;

} // namespace Archura
