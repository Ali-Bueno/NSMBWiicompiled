#include "accessibility/hotkeys.h"

#include <SDL3/SDL_events.h>
#include <aurora/event.h>

#include <deque>
#include <mutex>

extern "C" bool NsmbwOptionsMenuIsOpen();  // runtime/src/product/nsmbw_options_menu.cpp

namespace a11y::hotkeys {
namespace {

std::mutex g_mutex;
std::deque<Action> g_queue;
constexpr size_t kMaxQueued = 16;  // a held key must not pile up a backlog of announcements

std::optional<Action> ActionFor(const SDL_KeyboardEvent& key) {
    const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
    switch (key.scancode) {
        case SDL_SCANCODE_F1: return Action::Help;
        case SDL_SCANCODE_F2: return Action::Status;
        case SDL_SCANCODE_F3: return Action::Progress;
        case SDL_SCANCODE_F4: return Action::Nearby;
        case SDL_SCANCODE_F5: return Action::ToggleTerrain;
        case SDL_SCANCODE_F6: return Action::ToggleObjects;
        case SDL_SCANCODE_F7: return Action::VolumeDown;
        case SDL_SCANCODE_F8: return Action::VolumeUp;
        case SDL_SCANCODE_F9: return shift ? Action::SoundGuide : Action::Repeat;
        case SDL_SCANCODE_PAGEDOWN: return Action::GuideNext;
        case SDL_SCANCODE_PAGEUP: return Action::GuidePrev;
        case SDL_SCANCODE_END: return Action::GuideNextCategory;
        case SDL_SCANCODE_HOME: return Action::GuidePrevCategory;
        case SDL_SCANCODE_J: return Action::CameraLeft;
        case SDL_SCANCODE_L: return Action::CameraRight;
        case SDL_SCANCODE_I: return Action::CameraUp;
        case SDL_SCANCODE_K: return Action::CameraDown;
        case SDL_SCANCODE_U: return Action::CameraDescribe;
        case SDL_SCANCODE_O: return Action::CameraHome;
        default: return std::nullopt;
    }
}

}  // namespace

void HandleEvents(const AuroraEvent* events) {
    if (events == nullptr) {
        return;
    }
    for (const AuroraEvent* ev = events; ev->type != AURORA_NONE; ++ev) {
        if (ev->type != AURORA_SDL_EVENT || ev->sdl.type != SDL_EVENT_KEY_DOWN) {
            continue;
        }
        // Held keys repeat only for the tile camera, which steps through tiles.
        const SDL_Scancode sc = ev->sdl.key.scancode;
        const bool cameraKey = sc == SDL_SCANCODE_I || sc == SDL_SCANCODE_J || sc == SDL_SCANCODE_K || sc == SDL_SCANCODE_L;
        if (ev->sdl.key.repeat && !cameraKey) continue;
        // The options menu (and its rebinding screen) owns the keyboard while it is open.
        if (NsmbwOptionsMenuIsOpen()) continue;
        if (const auto action = ActionFor(ev->sdl.key)) {
            std::lock_guard lock(g_mutex);
            if (g_queue.size() < kMaxQueued) {
                g_queue.push_back(*action);
            }
        }
    }
}

std::optional<Action> Take() {
    std::lock_guard lock(g_mutex);
    if (g_queue.empty()) {
        return std::nullopt;
    }
    const Action action = g_queue.front();
    g_queue.pop_front();
    return action;
}

}  // namespace a11y::hotkeys
