#pragma once

namespace SynapseX::detail {

// Construct only after a successful AcquireNextFrame. All pointers are borrowed
// and stay valid for this scope. Template parameters allow cleanup to be checked
// without creating a D3D device; production uses the actual DXGI/D3D11 interfaces.
template <class Duplication, class Context, class Texture>
class DxgiReadbackScope {
public:
    DxgiReadbackScope(Duplication* duplication, Context* context, Texture* staging) noexcept
        : m_duplication(duplication), m_context(context), m_staging(staging) {}

    ~DxgiReadbackScope() noexcept {
        Unmap();
        ReleaseFrame();
    }

    DxgiReadbackScope(const DxgiReadbackScope&) = delete;
    DxgiReadbackScope& operator=(const DxgiReadbackScope&) = delete;
    DxgiReadbackScope(DxgiReadbackScope&&) = delete;
    DxgiReadbackScope& operator=(DxgiReadbackScope&&) = delete;

    void MarkMapped() noexcept { m_mapped = true; }

    void Unmap() noexcept {
        if (!m_mapped) return;
        m_mapped = false;
        m_context->Unmap(m_staging, 0);
    }

    void ReleaseFrame() noexcept {
        if (!m_frameHeld) return;
        m_frameHeld = false;
        m_duplication->ReleaseFrame();
    }

private:
    Duplication* m_duplication;
    Context* m_context;
    Texture* m_staging;
    bool m_frameHeld = true;
    bool m_mapped = false;
};

} // namespace SynapseX::detail
