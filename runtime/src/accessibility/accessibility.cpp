#include "accessibility/accessibility.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <aurora/env.hpp>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/audio/cue_service.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/game/nsmbw_game.h"
#include "accessibility/hotkeys.h"
#include "accessibility/level/actors.h"
#include "accessibility/level/entrances.h"
#include "accessibility/level/level_monitor.h"
#include "accessibility/level/terrain.h"
#include "accessibility/localization.h"
#include "accessibility/map/world_map.h"
#include "accessibility/prism_runtime.h"
#include "accessibility/screen_reader.h"
#include "accessibility/sound_guide.h"
#include "accessibility/speech.h"
#include "accessibility/ui/menu_reader.h"

namespace a11y {
namespace {

bool g_initialised = false;
bool g_announced = false;
std::chrono::steady_clock::time_point g_lastTick;
constexpr float kMaxFrameSec = 0.1f;  // a hitch must not fire every timer at once
constexpr int kVolumeStep = 10;

// Logged once per entry point: a fault that repeats every frame would flood the log.
void ReportFault(const char* where, bool& logged) {
    if (logged) return;
    logged = true;
    RT_LOGF(RT_TAG_A11Y, "accessibility: exception escaped %s; suppressed for the rest of the run\n", where);
}

void ApplyVolume() {
    audio::CueService::Instance().SetMasterVolume(static_cast<float>(config::Get().cueVolume) / 100.0f);
}

bool InPlayableLevel() {
    return game::InScene(game::Scene::Stage) && !game::OnTitleScreen() && !game::GamePaused();
}

void SilenceGameplayCues() {
    level::terrain::Stop();
    level::StopCues();
}

void RunAction(hotkeys::Action action, const game::PlayerState* player) {
    config::Settings& cfg = config::Get();
    const auto onOff = [](bool v) { return loc::Get(v ? "value_on" : "value_off"); };
    switch (action) {
        case hotkeys::Action::Help:
            speech::Say(loc::Get("help"), true);
            break;
        case hotkeys::Action::Status:
            if (player) speech::Say(level::monitor::Status(*player), true);
            break;
        case hotkeys::Action::Progress:
            if (player) speech::Say(level::monitor::Progress(*player), true);
            else if (game::InScene(game::Scene::WorldMap)) speech::Say(map::Describe(), true);
            break;
        case hotkeys::Action::Nearby:
            if (player) speech::Say(level::DescribeNearby(*player), true);
            break;
        case hotkeys::Action::ToggleTerrain:
            cfg.terrainCues = !cfg.terrainCues;
            config::Save();
            speech::Say(loc::Format("toggle_terrain", {{"state", onOff(cfg.terrainCues)}}), true);
            break;
        case hotkeys::Action::ToggleObjects:
            cfg.objectCues = !cfg.objectCues;
            config::Save();
            speech::Say(loc::Format("toggle_objects", {{"state", onOff(cfg.objectCues)}}), true);
            break;
        case hotkeys::Action::VolumeDown:
        case hotkeys::Action::VolumeUp: {
            const int step = action == hotkeys::Action::VolumeUp ? kVolumeStep : -kVolumeStep;
            cfg.cueVolume = std::clamp(cfg.cueVolume + step, 0, 100);
            config::Save();
            ApplyVolume();
            if (!cfg.gameSounds || !game::sound::PlaySystem(audio::cues::kGameCursorLabel)) {
                audio::CueService::Instance().PlayOneShot(audio::CueChannel::Ui, audio::cues::Confirm());
            }
            speech::Say(loc::Format("volume", {{"n", std::to_string(cfg.cueVolume)}}), true);
            break;
        }
        case hotkeys::Action::Repeat:
            speech::RepeatLast();
            break;
        case hotkeys::Action::SoundGuide:
            sound_guide::Start();
            break;
    }
}

float MeasureFrame() {
    const auto now = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(now - g_lastTick).count();
    g_lastTick = now;
    return std::clamp(sec, 0.0f, kMaxFrameSec);
}

// NSMBW_A11Y_LOG_PLAYER=1: once a second, what the mod reads about player 1 and the terrain.
void LogPlayer(const game::PlayerState& p) {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_LOG_PLAYER");
        return v != nullptr && *v == '1';
    }();
    static int frames = 0;
    if (!on || ++frames % 60 != 0) return;
    if (frames % 300 == 0) {
        RT_LOGF(RT_TAG_A11Y, "nearby: %s | %s\n", level::DescribeNearby(p).c_str(), level::monitor::Progress(p).c_str());
    }
    static bool soundsLogged = false;
    if (!soundsLogged) {
        soundsLogged = true;
        game::sound::LogLabels();
        RT_LOGF(RT_TAG_A11Y, "game sound test: positional %d, system %d\n",
                game::sound::PlayAt("SE_OBJ_ITEM_APPEAR", p.pos.x + 64.0f, p.pos.y) ? 1 : 0,
                game::sound::PlaySystem("SE_SYS_CURSOR") ? 1 : 0);
    }
    static std::uint8_t loggedArea = 0xFF;
    if (game::AreaNo() != loggedArea) {
        loggedArea = game::AreaNo();
        level::entrances::LogAll();
    }
    float ground = 0.0f, wallR = 0.0f, wallL = 0.0f;
    const bool g = game::GroundBelow(p.pos.x, p.pos.y + 8.0f, p.layer, ground);
    const bool wr = game::WallBetween(p.pos.x, p.pos.x + 96.0f, p.pos.y + 8.0f, p.layer, wallR);
    const bool wl = game::WallBetween(p.pos.x, p.pos.x - 96.0f, p.pos.y + 8.0f, p.layer, wallL);
    RT_LOGF(RT_TAG_A11Y,
            "player: pos (%.1f, %.1f) speed (%.2f, %.2f) dir %u layer %u power %d star %d type %d bc %08x "
            "foot %d state %s demo %s | ground %d %.1f wallR %d %.1f wallL %d %.1f unit %08x kind %08x lives %d time %d\n",
            p.pos.x, p.pos.y, p.speed.x, p.speed.y, p.direction, p.layer, p.powerup, p.starTimer, p.playerType,
            p.bgFlags, p.onGround ? 1 : 0, p.stateName.c_str(), p.demoStateName.c_str(), g ? 1 : 0, ground, wr ? 1 : 0, wallR, wl ? 1 : 0,
            wallL, game::UnitType(p.pos.x, p.pos.y - 4.0f, p.layer), game::UnitKind(p.pos.x, p.pos.y - 4.0f, p.layer),
            game::Lives(p.playerType), game::TimerSeconds());
    int listed = 0;
    game::ForEachActor(p.actor, [&](std::uint32_t actor) {
        game::ActorInfo info;
        if (listed >= 12 || !game::ReadActor(actor, info)) return;
        if (std::fabs(info.pos.x - p.pos.x) > 256.0f || std::fabs(info.pos.y - p.pos.y) > 160.0f) return;
        ++listed;
        RT_LOGF(RT_TAG_A11Y, "  actor %08x prof %u group %u kind %u layer %u pos (%.1f, %.1f) visible %d\n", actor,
                info.profile, info.group, info.kind, info.layer, info.pos.x, info.pos.y, info.visible ? 1 : 0);
    });
}

void TickImpl() {
    if (!g_announced) {
        g_announced = true;
        RT_LOGF(RT_TAG_A11Y, "first game frame; per-frame hook is live\n");
        speech::Say(loc::Get("ready"), true);
    }
    const float frameSec = MeasureFrame();

    game::PlayerState player;
    bool havePlayer = false;
    if (InPlayableLevel()) {
        const std::uint32_t actor = game::PlayerActor(0);
        havePlayer = actor != 0 && game::ReadPlayer(actor, player);
        if (havePlayer) LogPlayer(player);
    }

    speech::DrainPosted();
    while (const auto action = hotkeys::Take()) RunAction(*action, havePlayer ? &player : nullptr);

    static bool wasTitle = false;
    const bool title = game::OnTitleScreen();
    if (title && !wasTitle) speech::Say(loc::Get("title_screen"), true);
    wasTitle = title;

    ui::menu::Tick();
    level::monitor::Tick(havePlayer ? &player : nullptr);
    map::Tick();

    if (sound_guide::Tick(frameSec)) {
        SilenceGameplayCues();
    } else if (havePlayer && !game::PlayerOutOfPlay(player.actor) && game::InNormalPlay(player)) {
        level::terrain::Tick(player, frameSec);
        level::TickCues(player, frameSec);
        level::entrances::TickCue(player, frameSec);
    } else {
        SilenceGameplayCues();  // menus, pause, map, death: the cues go quiet
    }

    audio::CueService::Instance().SetFramePeriod(frameSec > 0.0f ? frameSec : audio::kDefaultFramePeriodSec);
    audio::CueService::Instance().Tick();
}

}  // namespace

void Init() {
    if (g_initialised) return;
    g_initialised = true;
    try {
        config::Load();
        loc::Init();
        ScreenReader::Instance().Initialise();
        audio::CueService::Instance().Start();
        ApplyVolume();
        g_lastTick = std::chrono::steady_clock::now();
        RT_LOGF(RT_TAG_A11Y, "accessibility initialised (reader: %s, cues: %s)\n",
                ScreenReader::Instance().BackendName(), audio::CueService::Instance().Available() ? "on" : "off");
    } catch (...) {
        static bool logged = false;
        ReportFault("Init", logged);
    }
}

void HandleEvents(const AuroraEvent* events) {
    if (!g_initialised) return;
    try {
        hotkeys::HandleEvents(events);
    } catch (...) {
        static bool logged = false;
        ReportFault("HandleEvents", logged);
    }
}

void Shutdown() {
    if (!g_initialised) return;
    try {
        audio::CueService::Instance().Shutdown();
        ScreenReader::Instance().Shutdown();
        UnloadPrism();
    } catch (...) {
        static bool logged = false;
        ReportFault("Shutdown", logged);
    }
    g_initialised = false;
}

}  // namespace a11y

extern "C" void NsmbwAccessibilityGameTick() {
    if (!a11y::g_initialised) return;
    try {
        a11y::TickImpl();
    } catch (...) {
        static bool logged = false;
        a11y::ReportFault("Tick", logged);
    }
}

// SDK SC_LANG_* values (decomp include/lib/revolution/SC/scapi.h) the PAL disc has text for.
extern "C" std::uint32_t NsmbwA11yGameLanguage() {
    constexpr std::uint32_t kEnglish = 1, kGerman = 2, kFrench = 3, kSpanish = 4, kItalian = 5;
    std::string code = a11y::config::Get().gameLanguage;
    if (code.empty() || code == "auto") code = a11y::loc::CurrentLanguage();
    if (code == "es") return kSpanish;
    if (code == "fr") return kFrench;
    if (code == "de") return kGerman;
    if (code == "it") return kItalian;
    return kEnglish;
}

extern "C" void NsmbwA11yInputSample(std::uint32_t hold, std::uint32_t trig) {
    a11y::game::RecordInput(hold, trig);
}

extern "C" void NsmbwA11yOnCursorPane(std::uint32_t pane, std::uint32_t slot) {
    try {
        a11y::ui::menu::OnCursorPane(pane, slot);
    } catch (...) {
    }
}
