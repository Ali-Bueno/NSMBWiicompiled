// Accessibility: report the pane the selection cursor is put on.
//
// Every NSMBW menu places its cursor with dGameCom::SelectCursorSetup(pane, slot, forceTop)
// (0x800B44D0; callers: Pausewindow_c, dYesNoWindow_c, the world-map menu, the REL menus). This
// override is a structural copy of the translated body (build/nsmbw/functions/func_800B44D0.cpp):
// it calls dSelectCursor_c::SetPane (0x8010C900) and the inlined SetAlpha exactly as before. The
// only addition is the line marked "A11Y", which hands the pane to the menu reader
// (runtime/src/accessibility/ui/menu_reader.cpp) before anything is clobbered.
#include "nsmbw_va.h"  // NSMBW_VA / NSMBW_FUNC: guest addresses are PAL rev 1, mapped per version
#include <cstdint>
#include "hle_stubs.h"
#include "ppc_runtime.h"
#include "abi_bridge.h"
#include "memory.h"

extern "C" void NsmbwA11yOnCursorPane(uint32_t pane, uint32_t slot);  // accessibility.cpp

extern "C" void NsmbwA11ySelectCursorSetup_800B44D0(CpuContext* MKW_RESTRICT ctx)
{
    NsmbwA11yOnCursorPane(ctx->gpr[3], ctx->gpr[4]); // A11Y
    uint32_t r0 = ctx->gpr[0];
    uint32_t r1 = ctx->gpr[1];
    uint32_t r3 = ctx->gpr[3];
    uint32_t r4 = ctx->gpr[4];
    uint32_t r5 = ctx->gpr[5];
    uint32_t r6 = ctx->gpr[6];
    uint32_t r13 = ctx->gpr[13];
    uint32_t r29 = ctx->gpr[29];
    uint32_t r30 = ctx->gpr[30];
    uint32_t r31 = ctx->gpr[31];

    goto loc_800B44D0;

loc_800B44D0:
{
    MemoryInline::FlatWriteRam32((r1 + -32), r1);
    r1 = (r1 + -32);
    r0 = ctx->lr;
    r6 = r5;
    MemoryInline::FlatWriteRam32((r1 + 36), r0);
    MemoryInline::FlatWriteRam32((r1 + 28), r31);
    MemoryInline::FlatWriteRam32((r1 + 24), r30);
    r30 = r4;
    r5 = r30;
    MemoryInline::FlatWriteRam32((r1 + 20), r29);
    r29 = r3;
    r4 = r29;
    r31 = MemoryInline::FlatRead32((r13 + -21464));
    r3 = r31;
    ctx->lr = NSMBW_VA(0x800B4508u);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[6] = r6;
    ctx->gpr[29] = r29;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    InvokeDirectCpu<NSMBW_VA(0x8010C900u)>(ctx);
    r0 = ctx->gpr[0];
    r1 = ctx->gpr[1];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    r5 = ctx->gpr[5];
    r6 = ctx->gpr[6];
    r29 = ctx->gpr[29];
    r30 = ctx->gpr[30];
    r31 = ctx->gpr[31];
    r3 = r31;
    r4 = r29;
    r5 = r30;
    // inline leaf 0x8010CAB0 (12 guest instruction(s))
    r0 = (r5 * 612);
    r5 = MemoryInline::FlatRead8((r4 + 185));
    r4 = (r3 + r0);
    r3 = MemoryInline::FlatRead32((r4 + 736));
    MemoryInline::FlatWrite8((r3 + 184), static_cast<uint8_t>(r5));
    r3 = MemoryInline::FlatRead32((r4 + 740));
    MemoryInline::FlatWrite8((r3 + 184), static_cast<uint8_t>(r5));
    r3 = MemoryInline::FlatRead32((r4 + 744));
    MemoryInline::FlatWrite8((r3 + 184), static_cast<uint8_t>(r5));
    r3 = MemoryInline::FlatRead32((r4 + 748));
    MemoryInline::FlatWrite8((r3 + 184), static_cast<uint8_t>(r5));
    // end of inlined leaf 0x8010CAB0
    r0 = MemoryInline::FlatRead32((r1 + 36));
    r31 = MemoryInline::FlatRead32((r1 + 28));
    r30 = MemoryInline::FlatRead32((r1 + 24));
    r29 = MemoryInline::FlatRead32((r1 + 20));
    ctx->lr = r0;
    r1 = (r1 + 32);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[6] = r6;
    ctx->gpr[29] = r29;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    return;
}

}

PPC_NATIVE_OVERRIDE_VOID(800B44D0, NsmbwA11ySelectCursorSetup_800B44D0, (CpuContext* ctx), (ctx));
