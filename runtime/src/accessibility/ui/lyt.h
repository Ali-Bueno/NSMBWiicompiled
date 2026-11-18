#ifndef NSMBW_ACCESSIBILITY_UI_LYT_H
#define NSMBW_ACCESSIBILITY_UI_LYT_H

#include <cstdint>
#include <string>
#include <vector>

// Read-only walking of nw4r::lyt pane trees in guest memory (offsets: decomp
// include/lib/nw4r/lyt/lyt_pane.h and lyt_textBox.h; the same layout Mario Kart Wii uses).
namespace a11y::ui::lyt {

struct TextPane {
    std::uint32_t pane = 0;
    std::string name;  // pane name, e.g. "T_tuzukeru_00"
    std::string text;  // UTF-8, button-icon tags dropped
    float x = 0.0f, y = 0.0f;  // global (screen) position of the pane
};

std::string PaneName(std::uint32_t pane);
std::uint32_t Parent(std::uint32_t pane);
std::vector<std::uint32_t> Children(std::uint32_t pane);

// Visible = own visible flag set and non-zero alpha on the pane and every ancestor.
bool IsShown(std::uint32_t pane);
bool GlobalPosition(std::uint32_t pane, float& x, float& y);

// Root of the tree a pane belongs to.
std::uint32_t Root(std::uint32_t pane);

// Every shown text box under `pane` (inclusive), in tree order; empty texts skipped.
std::vector<TextPane> ShownTexts(std::uint32_t pane);

}  // namespace a11y::ui::lyt

#endif  // NSMBW_ACCESSIBILITY_UI_LYT_H
