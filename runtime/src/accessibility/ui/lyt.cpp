#include "accessibility/ui/lyt.h"

#include <cstdio>

#include <aurora/env.hpp>

#include "accessibility/a11y_log.h"
#include "accessibility/guest_read.h"
#include "accessibility/guest_text.h"

namespace a11y::ui::lyt {
namespace {

// nw4r::lyt::Pane (decomp lyt_pane.h): ut::LinkList node at +0x04, parent +0x0C, child list
// (count +0x10, sentinel node +0x14), global matrix +0x84, alpha +0xB8, flags +0xBB, name +0xBC.
constexpr std::uint32_t kPaneLinkNode = 0x04;
constexpr std::uint32_t kPaneParent = 0x0C;
constexpr std::uint32_t kPaneChildSentinel = 0x14;
constexpr std::uint32_t kPaneGlobalMtx = 0x84;
constexpr std::uint32_t kPaneAlpha = 0xB8;
constexpr std::uint32_t kPaneFlags = 0xBB;
constexpr std::uint8_t kPaneFlagVisible = 0x01;
constexpr std::uint32_t kPaneName = 0xBC;
constexpr std::uint32_t kPaneNameChars = 16;  // char mName[17], NUL-terminated
// nw4r::lyt::TextBox (lyt_textBox.h): text buffer pointer +0xD8, buffer bytes +0xFC, length +0xFE.
constexpr std::uint32_t kTextBuf = 0xD8;
constexpr std::uint32_t kTextBufBytes = 0xFC;
constexpr std::uint32_t kTextLen = 0xFE;
// The 3x4 global matrix holds the translation in its last column: [0][3] and [1][3].
constexpr std::uint32_t kMtxTransX = 3 * 4;
constexpr std::uint32_t kMtxTransY = 7 * 4;
// Deep enough for any game layout; a corrupt tree cannot loop us forever.
constexpr int kMaxDepth = 24;
constexpr size_t kMaxChildren = 256;
constexpr int kMaxPanesPerWalk = 2048;  // far more than any NSMBW layout holds
// NSMBW layouts name every text box "T_..." (pictures "P_", nulls "N_", windows "W_").
constexpr char kTextBoxPrefix[] = "T_";
// nw4r's tag escape (0x1A) starts an inline command such as a button icon; skipped, not spoken.
constexpr char16_t kTagEscape = 0x1A;
constexpr std::uint16_t kFullWidthFirst = 0xFF01;  // U+FF01..U+FF5E mirror ASCII 0x21..0x7E
constexpr std::uint16_t kFullWidthLast = 0xFF5E;
constexpr std::uint16_t kFullWidthOffset = 0xFF01 - 0x21;

bool TagLog() {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_LOG_MENU");
        return v != nullptr && *v == '1';
    }();
    return on;
}

bool LooksLikeTextBox(std::uint32_t pane, const std::string& name) {
    if (name.rfind(kTextBoxPrefix, 0) != 0) {
        return false;
    }
    std::uint32_t buf = 0;
    std::uint16_t bytes = 0, len = 0;
    return TryPointer(pane + kTextBuf, buf) && TryU16(pane + kTextBufBytes, bytes) &&
           TryU16(pane + kTextLen, len) && len * 2u <= bytes;
}

std::string ReadText(std::uint32_t pane) {
    std::uint32_t buf = 0;
    std::uint16_t len = 0;
    if (!TryPointer(pane + kTextBuf, buf) || !TryU16(pane + kTextLen, len) || len == 0) {
        return {};
    }
    std::u16string text;
    text.reserve(len);
    for (std::uint32_t i = 0; i < len; ++i) {
        std::uint16_t c = 0;
        if (!TryU16(buf + i * 2, c) || c == 0) break;
        if (c == kTagEscape) {
            // Tag layout: 0x1A, then a unit whose high byte is the tag's length in bytes.
            std::uint16_t header = 0;
            if (!TryU16(buf + (i + 1) * 2, header)) break;
            const std::uint32_t tagBytes = header >> 8;
            if (TagLog()) {
                std::string hex;
                for (std::uint32_t k = 0; k < 6 && i + k < len; ++k) {
                    std::uint16_t u = 0;
                    TryU16(buf + (i + k) * 2, u);
                    char b[8];
                    std::snprintf(b, sizeof(b), "%04x ", u);
                    hex += b;
                }
                RT_LOGF(RT_TAG_A11Y, "lyt: tag in %08x: %s\n", pane, hex.c_str());
            }
            i += tagBytes > 2 ? (tagBytes / 2) - 1 : 1;
            continue;
        }
        // Full-width forms (the HUD-style digits "１", "０５") read as their plain ASCII twins.
        if (c >= kFullWidthFirst && c <= kFullWidthLast) c = static_cast<std::uint16_t>(c - kFullWidthOffset);
        text.push_back(static_cast<char16_t>(c));
    }
    std::string utf8 = Utf16ToUtf8(text);
    // Collapse layout line breaks into spaces: a screen reader reads one phrase.
    for (char& ch : utf8) {
        if (ch == '\n' || ch == '\r') ch = ' ';
    }
    const size_t first = utf8.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    const size_t last = utf8.find_last_not_of(' ');
    return utf8.substr(first, last - first + 1);
}

void Collect(std::uint32_t pane, int depth, int& budget, std::vector<TextPane>& out) {
    if (depth > kMaxDepth || --budget < 0) return;  // a stale or cyclic tree cannot run away
    std::uint8_t flags = 0, alpha = 0;
    if (!TryU8(pane + kPaneFlags, flags) || (flags & kPaneFlagVisible) == 0) return;
    if (!TryU8(pane + kPaneAlpha, alpha) || alpha == 0) return;
    const std::string name = PaneName(pane);
    if (LooksLikeTextBox(pane, name)) {
        TextPane t;
        t.pane = pane;
        t.name = name;
        t.text = ReadText(pane);
        GlobalPosition(pane, t.x, t.y);
        if (!t.text.empty()) out.push_back(std::move(t));
    }
    for (std::uint32_t child : Children(pane)) {
        Collect(child, depth + 1, budget, out);
    }
}

}  // namespace

std::string PaneName(std::uint32_t pane) {
    std::string name;
    for (std::uint32_t i = 0; i < kPaneNameChars; ++i) {
        std::uint8_t c = 0;
        if (!TryU8(pane + kPaneName + i, c) || c == 0) break;
        name.push_back(static_cast<char>(c));
    }
    return name;
}

std::uint32_t Parent(std::uint32_t pane) {
    std::uint32_t parent = 0;
    return TryPointer(pane + kPaneParent, parent) ? parent : 0;
}

std::vector<std::uint32_t> Children(std::uint32_t pane) {
    std::vector<std::uint32_t> out;
    const std::uint32_t sentinel = pane + kPaneChildSentinel;
    std::uint32_t node = 0;
    if (!TryU32(sentinel, node)) return out;
    while (node != 0 && node != sentinel && out.size() < kMaxChildren) {
        if (!Memory::Contains(node, 8)) break;
        out.push_back(node - kPaneLinkNode);
        std::uint32_t next = 0;
        if (!TryU32(node, next) || next == node) break;
        node = next;
    }
    return out;
}

bool IsShown(std::uint32_t pane) {
    for (int depth = 0; pane != 0 && depth <= kMaxDepth; ++depth) {
        std::uint8_t flags = 0, alpha = 0;
        if (!TryU8(pane + kPaneFlags, flags) || (flags & kPaneFlagVisible) == 0) return false;
        if (!TryU8(pane + kPaneAlpha, alpha) || alpha == 0) return false;
        pane = Parent(pane);
    }
    return true;
}

bool GlobalPosition(std::uint32_t pane, float& x, float& y) {
    return TryFloat(pane + kPaneGlobalMtx + kMtxTransX, x) &&
           TryFloat(pane + kPaneGlobalMtx + kMtxTransY, y);
}

std::uint32_t Root(std::uint32_t pane) {
    for (int depth = 0; depth <= kMaxDepth; ++depth) {
        const std::uint32_t parent = Parent(pane);
        if (parent == 0) return pane;
        pane = parent;
    }
    return pane;
}

std::vector<TextPane> ShownTexts(std::uint32_t pane) {
    std::vector<TextPane> out;
    int budget = kMaxPanesPerWalk;
    if (pane != 0) Collect(pane, 0, budget, out);
    return out;
}

}  // namespace a11y::ui::lyt
