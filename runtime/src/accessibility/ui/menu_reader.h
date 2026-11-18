#ifndef NSMBW_ACCESSIBILITY_UI_MENU_READER_H
#define NSMBW_ACCESSIBILITY_UI_MENU_READER_H

#include <cstdint>

// Reads the menu item the game's selection cursor sits on. Every menu puts its cursor on a
// button pane through dGameCom::SelectCursorSetup (0x800B44D0); our override of it reports the
// pane, and the reader speaks the text drawn on that button, plus the window's question or title
// when the cursor arrives in a new layout.
namespace a11y::ui::menu {

// Game thread, from the SelectCursorSetup override.
void OnCursorPane(std::uint32_t pane, std::uint32_t slot);

// Game thread, once per frame.
void Tick();

// Whether a menu cursor moved recently (a menu is probably on screen).
bool Active();

}  // namespace a11y::ui::menu

#endif  // NSMBW_ACCESSIBILITY_UI_MENU_READER_H
