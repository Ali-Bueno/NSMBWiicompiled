#include "accessibility/sound_guide.h"

#include <string>

#include "accessibility/a11y_config.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"

namespace a11y::sound_guide {
namespace {

using audio::CueChannel;
using namespace audio::cues;

struct Entry {
    const char* phraseKey;
    CueChannel channel;
    audio::CueSpec (*make)();
    const char* gameLabel = nullptr;  // the game's own sound, when the cue uses one
};

const Entry kEntries[] = {
    {"guide_wall", CueChannel::WallRight, [] { return WallTick(0.85f); }},
    {"guide_pit", CueChannel::Edge, [] { return Pit(0.6f); }},
    {"guide_drop", CueChannel::Edge, [] { return Drop(0.6f); }},
    {"guide_step", CueChannel::Edge, [] { return StepUp(0.6f); }},
    {"guide_hazard", CueChannel::Hazard, [] { return Hazard(0.6f); }},
    {"guide_ground", CueChannel::Ground, [] { audio::CueSpec c = GroundBelow(2.0f); c.durationSec = 0.5f; return c; }},
    {"guide_enemy", CueChannel::Enemy0, [] { return Enemy(0.5f, 0.0f, 1.0f); }},
    {"guide_item", CueChannel::Item, [] { return Item(-0.5f, 0.0f, 1.0f); }, kGameItemLabel},
    {"guide_goal", CueChannel::Goal, [] { return Goal(0.7f); }},
    {"guide_pipe", CueChannel::Pipe, [] { return Pipe(-0.6f, 0.0f); }, kGamePipeLabel},
};
constexpr int kCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
// No speech-finished signal is available from every backend, so the pause before each cue is
// estimated from the phrase length (about 14 characters per second, plus a margin).
constexpr float kSecondsPerChar = 0.07f;
constexpr float kSpeechMarginSec = 0.4f;
constexpr float kAfterCueSec = 1.0f;

int g_index = -1;
bool g_cuePending = false;
float g_wait = 0.0f;

void SayEntry() {
    const std::string text = loc::Get(kEntries[g_index].phraseKey);
    speech::Say(text, g_index == 0 ? false : true);
    g_wait = kSpeechMarginSec + kSecondsPerChar * static_cast<float>(text.size());
    g_cuePending = true;
}

}  // namespace

void Start() {
    const std::string intro = loc::Get("sound_guide");
    speech::Say(intro, true);
    g_index = 0;
    g_cuePending = false;
    g_wait = kSpeechMarginSec + kSecondsPerChar * static_cast<float>(intro.size());
}

bool Tick(float frameSec) {
    if (g_index < 0) return false;
    g_wait -= frameSec;
    if (g_wait > 0.0f) return true;
    if (g_index >= kCount) {
        g_index = -1;
        return false;
    }
    if (!g_cuePending) {
        SayEntry();
        return true;
    }
    const Entry& e = kEntries[g_index];
    if (e.gameLabel == nullptr || !config::Get().gameSounds || !game::sound::PlaySystem(e.gameLabel)) {
        audio::CueService::Instance().PlayOneShot(e.channel, e.make());
    }
    g_cuePending = false;
    g_wait = kAfterCueSec;
    ++g_index;
    return true;
}

}  // namespace a11y::sound_guide
