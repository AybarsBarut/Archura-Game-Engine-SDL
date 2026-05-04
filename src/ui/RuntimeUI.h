#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Archura {

    using UIDocumentHandle = uint32_t;

    struct UIEvent {
        std::string document;
        std::string target;
        std::string action;
    };

    class IRuntimeUIBackend {
    public:
        virtual ~IRuntimeUIBackend() = default;

        virtual bool Init(int width, int height) = 0;
        virtual void Shutdown() = 0;
        virtual void Resize(int width, int height) = 0;
        virtual void Update(float deltaTime) = 0;
        virtual void Render() = 0;

        virtual UIDocumentHandle LoadDocument(const std::string& path) = 0;
        virtual void ShowDocument(UIDocumentHandle handle, bool visible) = 0;
        virtual void CloseDocument(UIDocumentHandle handle) = 0;

        virtual void SetText(const std::string& id, const std::string& value) = 0;
        virtual void SetNumber(const std::string& id, float value) = 0;
        virtual void SetEventCallback(std::function<void(const UIEvent&)> callback) = 0;
    };

    class RuntimeUI {
    public:
        explicit RuntimeUI(std::unique_ptr<IRuntimeUIBackend> backend)
            : m_Backend(std::move(backend)) {}

        bool Init(int width, int height) {
            return m_Backend && m_Backend->Init(width, height);
        }

        void Shutdown() {
            if (m_Backend) m_Backend->Shutdown();
        }

        void Resize(int width, int height) {
            if (m_Backend) m_Backend->Resize(width, height);
        }

        void Update(float deltaTime) {
            if (m_Backend) m_Backend->Update(deltaTime);
        }

        void Render() {
            if (m_Backend) m_Backend->Render();
        }

        UIDocumentHandle LoadDocument(const std::string& name, const std::string& path) {
            if (!m_Backend) return 0;
            UIDocumentHandle handle = m_Backend->LoadDocument(path);
            if (handle != 0) m_Documents[name] = handle;
            return handle;
        }

        void Show(const std::string& name, bool visible = true) {
            auto it = m_Documents.find(name);
            if (m_Backend && it != m_Documents.end()) {
                m_Backend->ShowDocument(it->second, visible);
            }
        }

        void SetText(const std::string& id, const std::string& value) {
            if (m_Backend) m_Backend->SetText(id, value);
        }

        void SetNumber(const std::string& id, float value) {
            if (m_Backend) m_Backend->SetNumber(id, value);
        }

    private:
        std::unique_ptr<IRuntimeUIBackend> m_Backend;
        std::unordered_map<std::string, UIDocumentHandle> m_Documents;
    };

} // namespace Archura
