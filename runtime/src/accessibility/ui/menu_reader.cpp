#include "accessibility/ui/menu_reader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include <aurora/env.hpp>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/game/nsmbw_game.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"
#include "accessibility/ui/lyt.h"

namespace a11y::ui::menu {
namespace {

struct Pending {
    std::uint32_t pane = 0;
    std::uint32_t slot = 0;
    bool fresh = false;
};

Pending g_pending;
std::uint32_t g_lastPane = 0;
std::uint32_t g_lastRoot = 0;
std::string g_lastItem;
int g_framesSinceCursor = 0;
// A cursor that has not moved for this long and comes back is a menu reopened, so the header is
// read again (about 3 s at 60 frames per second).
constexpr int kReopenFrames = 180;
// How many ancestors up a button's own label is looked for.
constexpr int kLabelSearchDepth = 3;

bool LogMenus() {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_LOG_MENU");
        return v != nullptr && *v == '1';
    }();
    return on;
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// Pane names of a window's prompt in NSMBW layouts: T_question_00, T_title_00, T_message_00 ...
bool IsHeaderName(const std::string& name) {
    const std::string n = Lower(name);
    return n.find("question") != std::string::npos || n.find("title") != std::string::npos ||
           n.find("mes") != std::string::npos || n.find("msg") != std::string::npos ||
           n.find("info") != std::string::npos;
}

void AppendUnique(std::vector<std::string>& parts, const std::string& text) {
    if (text.empty()) return;
    if (std::find(parts.begin(), parts.end(), text) == parts.end()) parts.push_back(text);
}

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) {
        if (!out.empty()) out += ", ";
        out += p;
    }
    return out;
}

// The button's own label: every distinct text drawn inside the button pane, in layout order
// (a file slot shows its number and its state). A button whose texts sit beside it rather than
// inside it gets the shown text nearest to it, looked for in its parent, then grandparent.
std::string ButtonLabel(std::uint32_t pane) {
    std::vector<std::string> own;
    for (const lyt::TextPane& t : lyt::ShownTexts(pane)) AppendUnique(own, t.text);
    if (!own.empty()) return Join(own);
    float px = 0.0f, py = 0.0f;
    lyt::GlobalPosition(pane, px, py);
    std::uint32_t scope = lyt::Parent(pane);
    for (int depth = 1; depth <= kLabelSearchDepth && scope != 0; ++depth) {
        const std::vector<lyt::TextPane> texts = lyt::ShownTexts(scope);
        if (!texts.empty()) {
            const lyt::TextPane* best = nullptr;
            float bestDist = 0.0f;
            for (const lyt::TextPane& t : texts) {
                const float d = std::hypot(t.x - px, t.y - py);
                if (best == nullptr || d < bestDist) {
                    best = &t;
                    bestDist = d;
                }
            }
            return best->text;
        }
        scope = lyt::Parent(scope);
    }
    return lyt::PaneName(pane);  // nothing drawn: the pane name at least identifies it in logs
}

std::string HeaderText(std::uint32_t root, const std::string& item) {
    std::vector<std::string> parts;
    for (const lyt::TextPane& t : lyt::ShownTexts(root)) {
        if (IsHeaderName(t.name) && t.text != item) AppendUnique(parts, t.text);
    }
    return Join(parts);
}

void DumpLayout(std::uint32_t root) {
    for (const lyt::TextPane& t : lyt::ShownTexts(root)) {
        RT_LOGF(RT_TAG_A11Y, "menu: root %08x text %s (%.0f,%.0f) \"%s\"\n", root, t.name.c_str(), t.x,
                t.y, t.text.c_str());
    }
}

}  // namespace

void OnCursorPane(std::uint32_t pane, std::uint32_t slot) {
    g_pending = {pane, slot, true};
}

void Tick() {
    ++g_framesSinceCursor;
    if (!g_pending.fresh) return;
    g_pending.fresh = false;
    const std::uint32_t pane = g_pending.pane;
    const bool reopened = g_framesSinceCursor > kReopenFrames;
    g_framesSinceCursor = 0;
    if (pane == g_lastPane && !reopened) return;
    g_lastPane = pane;

    const std::uint32_t root = lyt::Root(pane);
    const bool newMenu = root != g_lastRoot || reopened;
    g_lastRoot = root;
    if (LogMenus() && newMenu) DumpLayout(root);

    const std::string item = ButtonLabel(pane);
    RT_LOGF(RT_TAG_A11Y, "menu: slot %u pane %08x %s -> \"%s\"%s\n", g_pending.slot, pane,
            lyt::PaneName(pane).c_str(), item.c_str(), newMenu ? " (new menu)" : "");
    if (!config::Get().menuSpeech) return;
    std::string text = item;
    if (newMenu) {
        std::string header = HeaderText(root, item);
        // The pause window has no title text of its own; the game is stopped inside a level.
        if (header.empty() && game::InScene(game::Scene::Stage) && !game::OnTitleScreen() && game::GamePaused()) {
            header = loc::Get("paused");
        }
        if (!header.empty()) {
            const char last = header.back();
            const bool punctuated = last == '.' || last == '?' || last == '!' || last == ':';
            text = header + (punctuated ? " " : ". ") + item;
        }
    }
    if (text == g_lastItem && !newMenu) return;
    g_lastItem = text;
    speech::Say(text, true);
}

bool Active() {
    return g_framesSinceCursor <= kReopenFrames;
}

}  // namespace a11y::ui::menu
