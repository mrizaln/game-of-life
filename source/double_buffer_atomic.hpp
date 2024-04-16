#ifndef DOUBLE_BUFFER_ATOMIC_HPP_T4PFY34R
#define DOUBLE_BUFFER_ATOMIC_HPP_T4PFY34R

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>

// Single-producer, single-consumer double buffer with atomic swap
template <std::copyable Buffer, bool DynamicAlloc = false>
class DoubleBufferAtomic
{
public:
    using BufferType  = Buffer;
    using BuffersType = std::conditional_t<DynamicAlloc, std::unique_ptr<Buffer[]>, std::array<Buffer, 2>>;

    static bool constexpr s_dynamicAlloc = DynamicAlloc;

    enum class BufferIndex : std::uint8_t
    {
        First  = 0,
        Second = 1,
    };

    enum class BufferUpdateStatus : std::uint16_t
    {
        Idle,
        Updating,
        Done,
    };

    explicit DoubleBufferAtomic(Buffer startState = {})
        requires(!DynamicAlloc)
        : m_buffers{ startState, startState }
    {
    }

    explicit DoubleBufferAtomic(Buffer startState = {})
        requires(DynamicAlloc)
        : m_buffers{ std::make_unique<Buffer[]>(2) }
    {
        m_buffers[0] = startState;
        m_buffers[1] = startState;
    }

    void reset(BufferType&& buf = {})
    {
        m_buffers[0] = buf;
        m_buffers[1] = buf;
    }

    const Buffer& swapBuffers()
    {
        if (m_info != BufferUpdateStatus::Done) {
            return getFront();
        }

        auto            index   = m_index.load();
        BufferIndexPair swapped = { .m_front = index.m_back, .m_back = index.m_front };
        m_index                 = swapped;

        m_info = BufferUpdateStatus::Idle;
        return getBuffer(swapped.m_front);
    }

    void updateBuffer(std::invocable<Buffer&> auto&& update)
    {
        if (m_info != BufferUpdateStatus::Idle) {
            return;
        }
        m_info = BufferUpdateStatus::Updating;

        update(getBuffer(m_index.load().m_back));

        m_info = BufferUpdateStatus::Done;
    }

    const Buffer&      getFront() const { return getBuffer(m_index.load().m_front); }
    BufferUpdateStatus status() const { return m_info.load(); }

private:
    struct BufferIndexPair
    {
        BufferIndex m_front = BufferIndex::First;
        BufferIndex m_back  = BufferIndex::Second;
    };

    static std::size_t toSize(BufferIndex index) { return static_cast<std::size_t>(index); }

    const Buffer& getBuffer(BufferIndex index) const { return m_buffers[toSize(index)]; }
    Buffer&       getBuffer(BufferIndex index) { return m_buffers[toSize(index)]; }

    BuffersType                     m_buffers;
    std::atomic<BufferIndexPair>    m_index;
    std::atomic<BufferUpdateStatus> m_info = BufferUpdateStatus::Idle;
};

#endif /* end of include guard: DOUBLE_BUFFER_ATOMIC_HPP_T4PFY34R */
