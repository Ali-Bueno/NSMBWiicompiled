#ifndef NSMBW_ACCESSIBILITY_GAME_GUEST_CALL_H
#define NSMBW_ACCESSIBILITY_GAME_GUEST_CALL_H

#include <cstdint>
#include <cstring>

#include "abi_bridge.h"
#include "isa/ppc_isa_context.h"
#include "memory.h"

namespace a11y::game {

// Calls guest functions from a native hook on the game thread, the way the product code plays
// its menu sounds (nsmbw_kpad_overrides.cpp): the whole context is saved and put back, because a
// call clobbers the caller's volatile registers, LR and CR. It also opens a frame on the guest
// stack below the caller's r1, so arguments passed by pointer (an mVec3_c, an out float) have
// guest memory to live in; the callee's own frames go below that.
class GuestCall {
public:
    static constexpr std::uint32_t kScratchBytes = 0x100;

    GuestCall() : mCtx(CurrentCpuContext()), mSaved(*mCtx) {
        const std::uint32_t callerSp = mCtx->gpr[1];
        mFrame = (callerSp - kFrameBytes) & ~std::uint32_t{0xF};
        MemoryInline::FlatWriteRam32(mFrame, callerSp);  // back chain, like a real stwu frame
        mScratch = mFrame + kLinkAreaBytes;
    }
    ~GuestCall() { *mCtx = mSaved; }
    GuestCall(const GuestCall&) = delete;
    GuestCall& operator=(const GuestCall&) = delete;

    // Guest address of scratch byte `offset` (0 <= offset < kScratchBytes).
    std::uint32_t Scratch(std::uint32_t offset) const { return mScratch + offset; }

    void WriteFloat(std::uint32_t addr, float v) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        MemoryInline::FlatWriteRam32(addr, bits);
    }
    float ReadFloat(std::uint32_t addr) const {
        const std::uint32_t bits = MemoryInline::FlatRead32(addr);
        float v = 0.0f;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    // Arguments for the next Invoke; every call starts from the caller's saved registers.
    void Gpr(int n, std::uint32_t v) { mCtx->gpr[n] = v; }
    void Fpr(int n, double v) { mCtx->fpr[n].d = v; }

    template <std::uint32_t Addr>
    std::uint32_t Invoke() {
        mCtx->gpr[1] = mFrame;
        InvokeDirectCpu<Addr>(mCtx);
        const std::uint32_t r3 = mCtx->gpr[3];
        mResultF1 = mCtx->fpr[1].d;
        Reset();
        return r3;
    }
    double ResultF1() const { return mResultF1; }

private:
    static constexpr std::uint32_t kLinkAreaBytes = 0x10;  // back chain + LR save word + padding
    static constexpr std::uint32_t kFrameBytes = kLinkAreaBytes + kScratchBytes + 0x10;

    void Reset() {
        const std::uint32_t frame = mFrame;
        *mCtx = mSaved;
        mCtx->gpr[1] = frame;
    }

    CpuContext* mCtx;
    const CpuContext mSaved;
    std::uint32_t mFrame = 0;
    std::uint32_t mScratch = 0;
    double mResultF1 = 0.0;
};

}  // namespace a11y::game

#endif  // NSMBW_ACCESSIBILITY_GAME_GUEST_CALL_H
