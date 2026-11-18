#include "accessibility/level/entrances.h"

#include <algorithm>
#include <cmath>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/guest_read.h"
#include "accessibility/level/actors.h"
#include "accessibility/localization.h"
#include "nsmbw_va.h"

namespace a11y::level::entrances {
namespace {

// dCd_c::m_instance (syms.txt) holds dCdFile_c mFiles[4], 944 bytes each (the stride every
// translated caller multiplies by). In a file, getNextGotoP (func_8008E3D0) reads the entrance
// array at +0x20 and its count at +0x90; entries are sNextGotoData, 20 bytes (d_cd_data.hpp).
constexpr std::uint32_t kCdInstance = NSMBW_VA(0x8042A178u);
constexpr std::uint32_t kCdFileStride = 944;
constexpr std::uint32_t kCdNextGotos = 0x20;
constexpr std::uint32_t kCdNextGotoCount = 0x90;
constexpr std::uint32_t kEntrySize = 20;
// Map actors (sprites): block 7 at +0x24, count at +0x94; sMapActorData is 16 bytes: id +0, x +2,
// y +4, area +12. dActorData_c::l_actor_data_tbl (0x8030A340, 40-byte entries) gives each id's
// profile in its first u16.
constexpr std::uint32_t kCdMapActors = 0x24;
constexpr std::uint32_t kCdMapActorCount = 0x94;
constexpr std::uint32_t kMapActorSize = 16;
constexpr std::uint32_t kMapActorX = 2, kMapActorY = 4, kMapActorArea = 12;
constexpr std::uint32_t kActorDataTable = NSMBW_VA(0x8030A340u);
constexpr std::uint32_t kActorDataStride = 40;
constexpr std::uint16_t kGoalPoleProfile = 151;  // fProf EN_GOALPOLE
constexpr int kMaxMapActors = 2048;
constexpr std::uint32_t kEntryX = 0x00, kEntryY = 0x02, kEntryType = 0x0B, kEntryArea = 0x0D, kEntryFlags = 0x10;
constexpr int kMaxEntries = 256;
constexpr int kCourseFiles = 4;
// dScStage_c::mCurrFile (+0x76) picks the file of the area being played.
constexpr std::uint32_t kStageScene = NSMBW_VA(0x8042A4A8u);
constexpr std::uint32_t kStageFile = 0x120E;  // after dScene_c (0x74) and pad[0x1198] (d_s_stage.hpp)

// NextGotoType_e and NextGotoFlags_e (d_cd_data.hpp).
constexpr std::uint8_t kPipeU = 3, kPipeD = 4, kPipeL = 5, kPipeR = 6;
constexpr std::uint8_t kMiniPipeU = 16, kMiniPipeD = 17, kMiniPipeL = 18, kMiniPipeR = 19;
constexpr std::uint16_t kFlagOnlyExit = 0x80;

constexpr float kCueRangeTiles = 6.0f;
constexpr float kCueIntervalSec = 1.6f;
// A pipe's entrance point sits at its opening's corner; the opening is two tiles wide.
constexpr float kOpeningHalfTiles = 1.0f;

float g_cueTimer = 0.0f;

bool KindOf(std::uint8_t type, PipeKind& kind) {
    switch (type) {
        case kPipeU: case kMiniPipeU: kind = PipeKind::Down; return true;   // opening faces up
        case kPipeD: case kMiniPipeD: kind = PipeKind::Up; return true;     // opening on a ceiling
        case kPipeL: case kPipeR: case kMiniPipeL: case kMiniPipeR: kind = PipeKind::Side; return true;
        default: return false;
    }
}

std::uint32_t CurrentFile() {
    std::uint32_t stage = 0, cd = 0;
    std::uint8_t file = 0;
    if (!TryPointer(kStageScene, stage) || !TryU8(stage + kStageFile, file) || file >= kCourseFiles) return 0;
    if (!TryPointer(kCdInstance, cd)) return 0;
    return cd + kCdFileStride * file;
}

const char* HintKey(PipeKind kind) {
    switch (kind) {
        case PipeKind::Down: return "pipe_down";
        case PipeKind::Up: return "pipe_up";
        case PipeKind::Side: return "pipe_side";
    }
    return "pipe_side";
}

}  // namespace

std::vector<Pipe> PipesNear(const game::PlayerState& player, float rangeTiles) {
    std::vector<Pipe> out;
    const std::uint32_t file = CurrentFile();
    std::uint32_t list = 0, count = 0;
    if (file == 0 || !TryPointer(file + kCdNextGotos, list) || !TryU32(file + kCdNextGotoCount, count)) return out;
    const std::uint8_t area = game::AreaNo();
    for (std::uint32_t i = 0; i < count && i < kMaxEntries; ++i) {
        const std::uint32_t e = list + i * kEntrySize;
        std::uint16_t x = 0, y = 0, flags = 0;
        std::uint8_t type = 0, entryArea = 0;
        if (!TryU16(e + kEntryX, x) || !TryU16(e + kEntryY, y) || !TryU8(e + kEntryType, type) ||
            !TryU8(e + kEntryArea, entryArea) || !TryU16(e + kEntryFlags, flags)) {
            break;
        }
        Pipe p;
        if (!KindOf(type, p.kind) || (flags & kFlagOnlyExit) != 0 || entryArea != area) continue;
        p.x = static_cast<float>(x) + kOpeningHalfTiles * game::kTile;
        p.y = -static_cast<float>(y);  // course data counts y downwards
        if (std::fabs(p.x - player.pos.x) / game::kTile > rangeTiles ||
            std::fabs(p.y - player.pos.y) / game::kTile > rangeTiles) {
            continue;
        }
        out.push_back(p);
    }
    std::sort(out.begin(), out.end(), [&](const Pipe& a, const Pipe& b) {
        return std::hypot(a.x - player.pos.x, a.y - player.pos.y) < std::hypot(b.x - player.pos.x, b.y - player.pos.y);
    });
    return out;
}

void TickCue(const game::PlayerState& player, float frameSec) {
    g_cueTimer -= frameSec;
    if (!config::Get().objectCues || g_cueTimer > 0.0f) return;
    g_cueTimer = kCueIntervalSec;
    const std::vector<Pipe> pipes = PipesNear(player, kCueRangeTiles);
    if (pipes.empty()) return;
    const Pipe& p = pipes.front();
    const float dx = (p.x - player.pos.x) / game::kTile, dy = (p.y - player.pos.y) / game::kTile;
    if (config::Get().gameSounds && game::sound::PlayAt(audio::cues::kGamePipeLabel, p.x, p.y)) {
        g_cueTimer = audio::cues::kGamePipeIntervalSec;
        return;
    }
    audio::CueService::Instance().PlayOneShot(audio::CueChannel::Pipe,
                                              audio::cues::Pipe(audio::cues::PanFor(dx, kCueRangeTiles), dy));
}

bool GoalFromCourseData(Vec3& goal) {
    const std::uint32_t file = CurrentFile();
    std::uint32_t list = 0, count = 0;
    if (file == 0 || !TryPointer(file + kCdMapActors, list) || !TryU32(file + kCdMapActorCount, count)) return false;
    const std::uint8_t area = game::AreaNo();
    for (std::uint32_t i = 0; i < count && i < kMaxMapActors; ++i) {
        const std::uint32_t a = list + i * kMapActorSize;
        std::uint16_t id = 0, x = 0, y = 0, profile = 0;
        std::uint8_t actorArea = 0;
        if (!TryU16(a, id) || !TryU16(a + kMapActorX, x) || !TryU16(a + kMapActorY, y) ||
            !TryU8(a + kMapActorArea, actorArea)) {
            return false;
        }
        if (actorArea != area || !TryU16(kActorDataTable + id * kActorDataStride, profile)) continue;
        if (profile == kGoalPoleProfile) {
            goal = {static_cast<float>(x), -static_cast<float>(y), 0.0f};
            return true;
        }
    }
    return false;
}

void LogAll() {
    const std::uint32_t file = CurrentFile();
    std::uint32_t list = 0, count = 0;
    if (file == 0 || !TryPointer(file + kCdNextGotos, list) || !TryU32(file + kCdNextGotoCount, count)) {
        RT_LOGF(RT_TAG_A11Y, "entrances: no course file\n");
        return;
    }
    RT_LOGF(RT_TAG_A11Y, "entrances: file %08x, %u entries, player area %u\n", file, count, game::AreaNo());
    for (std::uint32_t i = 0; i < count && i < kMaxEntries; ++i) {
        const std::uint32_t e = list + i * kEntrySize;
        std::uint16_t x = 0, y = 0, flags = 0;
        std::uint8_t type = 0, area = 0;
        TryU16(e + kEntryX, x);
        TryU16(e + kEntryY, y);
        TryU8(e + kEntryType, type);
        TryU8(e + kEntryArea, area);
        TryU16(e + kEntryFlags, flags);
        RT_LOGF(RT_TAG_A11Y, "  entrance %u: (%u, %u) type %u area %u flags %04x\n", i, x, y, type, area, flags);
    }
}

std::vector<std::string> Describe(const game::PlayerState& player, float rangeTiles) {
    std::vector<std::string> out;
    for (const Pipe& p : PipesNear(player, rangeTiles)) {
        const int dx = static_cast<int>(std::lround((p.x - player.pos.x) / game::kTile));
        const int dy = static_cast<int>(std::lround((p.y - player.pos.y) / game::kTile));
        const std::string where = Where(dx, dy);
        out.push_back(loc::Format("pipe_entry", {{"where", where}, {"hint", loc::Get(HintKey(p.kind))}}));
    }
    return out;
}

}  // namespace a11y::level::entrances
