#include "accessibility/game/game_sound.h"

#include <cstring>
#include <string>
#include <unordered_map>

#include "accessibility/a11y_log.h"
#include "accessibility/game/guest_call.h"
#include "accessibility/guest_read.h"
#include "nsmbw_va.h"

namespace a11y::game::sound {
namespace {

// dAudio::g_pSndObjMap (syms.txt): the shared map sound object actors play their sounds through
// (d_actor.cpp: g_pSndObjMap->startSound(SE_OBJ_CMN_SPLASH, pos, 0)). It is an nw4r SoundActor,
// so the archive's label lookup can run on it.
constexpr std::uint32_t kSndObjMap = NSMBW_VA(0x8042A040u);
// SndAudioMgr::sInstance, as nsmbw_kpad_overrides.cpp reads it for the options menu sounds.
constexpr std::uint32_t kSndAudioMgr = NSMBW_VA(0x8042A768u);
constexpr std::uint32_t kInvalidId = 0xFFFFFFFFu;
constexpr std::uint32_t kLabelMax = GuestCall::kScratchBytes - 1;
constexpr std::uint32_t kSystemSeArg = 1;  // the second argument every startSystemSe call passes
constexpr std::uint32_t kPlayerNo = 0;

std::unordered_map<std::string, std::uint32_t> g_ids;

std::uint32_t IdFor(const char* label) {
    const auto it = g_ids.find(label);
    if (it != g_ids.end()) return it->second;
    std::uint32_t actor = 0;
    if (!TryPointer(kSndObjMap, actor)) return kInvalidId;  // the sound system is not up yet
    const size_t len = std::strlen(label);
    if (len > kLabelMax) return kInvalidId;
    GuestCall call;
    const std::uint32_t text = call.Scratch(0);
    for (size_t i = 0; i <= len; ++i) MemoryInline::FlatWrite8(text + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(label[i]));
    call.Gpr(3, actor);
    call.Gpr(4, text);
    const std::uint32_t id = call.Invoke<NSMBW_VA(0x80275750u)>();  // SoundActor::detail_ConvertLabelStringToSoundId
    if (id != kInvalidId) g_ids.emplace(label, id);  // a failure may only mean "not loaded yet"
    return id;
}

}  // namespace

bool PlaySystem(const char* label) {
    const std::uint32_t id = IdFor(label);
    std::uint32_t mgr = 0;
    if (id == kInvalidId || !TryPointer(kSndAudioMgr, mgr)) return false;
    GuestCall call;
    call.Gpr(3, mgr);
    call.Gpr(4, id);
    call.Gpr(5, kSystemSeArg);
    call.Invoke<NSMBW_VA(0x801954C0u)>();  // SndAudioMgr::startSystemSe
    return true;
}

bool PlayAt(const char* label, float x, float y) {
    const std::uint32_t id = IdFor(label);
    std::uint32_t actor = 0;
    if (id == kInvalidId || !TryPointer(kSndObjMap, actor)) return false;
    GuestCall call;
    const std::uint32_t pos = call.Scratch(0), screen = call.Scratch(16);
    call.WriteFloat(pos, x);
    call.WriteFloat(pos + 4, y);
    call.WriteFloat(pos + 8, 0.0f);
    call.Gpr(3, screen);  // the VEC2 is returned through a hidden first argument
    call.Gpr(4, pos);
    call.Invoke<NSMBW_VA(0x8006A4B0u)>();  // dAudio::cvtSndObjctPos(const mVec3_c&)
    call.Gpr(3, actor);
    call.Gpr(4, id);
    call.Gpr(5, screen);
    call.Gpr(6, kPlayerNo);
    call.Invoke<NSMBW_VA(0x80198D70u)>();  // SndObjctCmnMap::startSound(id, const VEC2&, playerNo)
    return true;
}

void LogLabels() {
    for (const char* label : {"SE_SYS_CURSOR", "SE_SYS_INVALID", "SE_PLY_DOKAN_IN_OUT", "SE_OBJ_ITEM_APPEAR"}) {
        RT_LOGF(RT_TAG_A11Y, "game sound: %s -> %d\n", label, static_cast<int>(IdFor(label)));
    }
}

}  // namespace a11y::game::sound
