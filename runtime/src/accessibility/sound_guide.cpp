#include "accessibility/sound_guide.h"

#include <string>
#include <vector>

#include "accessibility/a11y_config.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"

namespace a11y::sound_guide {
namespace {

using audio::CueChannel;
using audio::CueSpec;
namespace cues = audio::cues;

// Profiles whose radar signatures the guide demonstrates (fProf IDs, actor_names.inc).
constexpr std::uint16_t kGoomba = 51, kKoopa = 54, kSpiny = 57;
constexpr float kHumDemoSec = 1.6f;  // held sounds play this long

struct Entry {
    const char* phraseKey;
    cues::Composite (*oneShot)() = nullptr;
    CueSpec (*hum)() = nullptr;          // a held sound, on `channel`
    CueChannel channel = CueChannel::Ui;
    const char* gameLabel = nullptr;     // the game's own sound, when the cue uses one
};

struct Category {
    const char* phraseKey;
    std::vector<Entry> entries;
};

const std::vector<Category>& Categories() {
    static const std::vector<Category> categories = {
        {"guide_cat_terrain",
         {
             {"guide_wall_step", nullptr, [] { return cues::WallHum(cues::WallKind::Step, 0.8f, 0.8f); }, CueChannel::WallRight},
             {"guide_wall_high", nullptr, [] { return cues::WallHum(cues::WallKind::HighJump, 0.8f, 0.8f); }, CueChannel::WallRight},
             {"guide_wall_tall", nullptr, [] { return cues::WallHum(cues::WallKind::Wall, 0.8f, 0.8f); }, CueChannel::WallRight},
             {"guide_wall_knock", [] { return cues::WallKnock(0.8f); }},
             {"guide_ceiling", nullptr, [] { return cues::CeilingHum(0.7f); }, CueChannel::Ceiling},
             {"guide_pit", [] { return cues::EdgePit(0.5f); }},
             {"guide_drop", [] { return cues::EdgeDrop(0.5f); }},
             {"guide_step", [] { return cues::EdgeStep(0.5f); }},
             {"guide_hazard", [] { return cues::EdgeHazard(0.5f); }},
             {"guide_platform_above", [] { return cues::PlatformAbove(3); }},
         }},
        {"guide_cat_air",
         {
             {"guide_ground", nullptr, [] { return cues::GroundBelow(2, 0); }, CueChannel::Ground},
             {"guide_ground_far", nullptr, [] { return cues::GroundBelow(6, 0); }, CueChannel::Ground},
             {"guide_ground_hazard", nullptr, [] { return cues::GroundBelow(3, 2); }, CueChannel::Ground},
             {"guide_air_pit", [] { return cues::Pit(); }},
             {"guide_platform_under", [] { return cues::PlatformUnder(); }},
             {"guide_ground_after_pit", [] { return cues::GroundAfterPit(); }},
             {"guide_landing", [] { return cues::Landing(); }},
         }},
        {"guide_cat_enemies",
         {
             {"guide_enemy_goomba", [] { return cues::EnemySignature(kGoomba, false, 0.5f, 0.0f, 0.8f); }},
             {"guide_enemy_koopa", [] { return cues::EnemySignature(kKoopa, false, 0.5f, 0.0f, 0.8f); }},
             {"guide_enemy_above", [] { return cues::EnemySignature(kGoomba, false, 0.5f, 5.0f, 0.8f); }},
             {"guide_danger", [] { return cues::EnemySignature(kSpiny, true, 0.5f, 0.0f, 0.8f); }},
             {"guide_boss", [] { return cues::Boss(0.5f, 0.0f); }},
             {"guide_jump_now", [] { return cues::JumpNow(0.5f); }},
             {"guide_jump_high", [] { return cues::JumpHigh(0.5f); }},
             {"guide_jump_over", [] { return cues::JumpOver(0.5f); }},
             {"guide_stomp", [] { return cues::StompTarget(0.0f); }},
             {"guide_danger_below", [] { return cues::DangerBelow(0.0f); }},
         }},
        {"guide_cat_objects",
         {
             {"guide_item", [] { return cues::Item(-0.5f, 0.0f, 1.0f); }, nullptr, CueChannel::Ui, cues::kGameItemLabel},
             {"guide_under_item", [] { return cues::UnderItem(3.0f); }},
             {"guide_pipe", [] { return cues::Pipe(-0.6f, 0.0f); }, nullptr, CueChannel::Ui, cues::kGamePipeLabel},
             {"guide_goal", [] { return cues::Goal(0.7f); }},
         }},
    };
    return categories;
}

// No speech-finished signal is available from every backend, so the sound waits for an estimate
// of the phrase's length (about 14 characters per second, plus a margin).
constexpr float kSecondsPerChar = 0.07f;
constexpr float kSpeechMarginSec = 0.4f;

bool g_open = false;
int g_category = 0, g_entry = 0;
float g_playIn = -1.0f;    // seconds until the current sound plays; negative = nothing pending
float g_humLeft = 0.0f;    // seconds the current held demo still sounds
CueChannel g_humChannel = CueChannel::Ui;

void StopHum() {
    if (g_humLeft > 0.0f) audio::CueService::Instance().Stop(g_humChannel);
    g_humLeft = 0.0f;
}

void PlayCurrent() {
    const Entry& e = Categories()[g_category].entries[g_entry];
    if (e.gameLabel != nullptr && config::Get().gameSounds && game::sound::PlaySystem(e.gameLabel)) return;
    if (e.hum != nullptr) {
        audio::CueService::Instance().SetSustained(e.channel, e.hum());
        g_humChannel = e.channel;
        g_humLeft = kHumDemoSec;
        return;
    }
    if (e.oneShot != nullptr) cues::Play(e.oneShot());
}

void Announce(bool withCategory) {
    StopHum();
    const Category& c = Categories()[g_category];
    std::string text;
    if (withCategory) {
        text = loc::Format("guide_category", {{"name", loc::Get(c.phraseKey)}, {"n", std::to_string(c.entries.size())}}) + " ";
    }
    text += loc::Format("guide_entry", {{"name", loc::Get(c.entries[g_entry].phraseKey)},
                                        {"i", std::to_string(g_entry + 1)},
                                        {"n", std::to_string(c.entries.size())}});
    speech::Say(text, true);
    g_playIn = kSpeechMarginSec + kSecondsPerChar * static_cast<float>(text.size());
}

}  // namespace

void Toggle() {
    g_open = !g_open;
    StopHum();
    g_playIn = -1.0f;
    if (!g_open) {
        speech::Say(loc::Get("guide_closed"), true);
        return;
    }
    g_category = 0;
    g_entry = 0;
    speech::Say(loc::Get("sound_guide"), true);
    Announce(true);
}

bool IsOpen() { return g_open; }

void Next(int delta) {
    if (!g_open) return;
    const int n = static_cast<int>(Categories()[g_category].entries.size());
    g_entry = ((g_entry + delta) % n + n) % n;
    Announce(false);
}

void NextCategory(int delta) {
    if (!g_open) return;
    const int n = static_cast<int>(Categories().size());
    g_category = ((g_category + delta) % n + n) % n;
    g_entry = 0;
    Announce(true);
}

void Replay() {
    if (!g_open) return;
    StopHum();
    g_playIn = 0.0f;
}

bool Tick(float frameSec) {
    if (!g_open) return false;
    if (g_humLeft > 0.0f) {
        g_humLeft -= frameSec;
        if (g_humLeft <= 0.0f) audio::CueService::Instance().Stop(g_humChannel);
    }
    if (g_playIn >= 0.0f) {
        g_playIn -= frameSec;
        if (g_playIn < 0.0f) PlayCurrent();
    }
    return true;
}

}  // namespace a11y::sound_guide
