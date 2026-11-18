#ifndef NSMBW_ACCESSIBILITY_HOTKEYS_H
#define NSMBW_ACCESSIBILITY_HOTKEYS_H

#include <optional>

struct AuroraEvent;

namespace a11y::hotkeys {

enum class Action {
    Help,          // F1
    Status,        // F2
    Progress,      // F3
    Nearby,        // F4
    ToggleTerrain, // F5
    ToggleObjects, // F6
    VolumeDown,    // F7
    VolumeUp,      // F8
    Repeat,        // F9
    SoundGuide,    // Shift+F9
    GuideNext,     // Page Down (sound guide open)
    GuidePrev,     // Page Up
    GuideNextCategory,  // End
    GuidePrevCategory,  // Home
    CameraLeft,    // J
    CameraRight,   // L
    CameraUp,      // I
    CameraDown,    // K
    CameraDescribe,  // U
    CameraHome,    // O
};

// Host (event) thread: turns key presses into queued actions. Never touches guest state.
void HandleEvents(const AuroraEvent* events);

// Game thread: the next queued action, if any.
std::optional<Action> Take();

}  // namespace a11y::hotkeys

#endif  // NSMBW_ACCESSIBILITY_HOTKEYS_H
