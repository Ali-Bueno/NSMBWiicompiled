// Accessibility: the game's text language follows the accessibility setting.
//
// The managed NAND has no SYSCONF, so the SDK's SCGetLanguage (0x801DD460; its only caller is
// dGameCom::GetLanguageHBM, 0x800B4630, which picks the /EU/<Language>/ message and layout
// folders) always answered English. A screen-reader user needs menus in the same language the
// mod speaks, so the answer comes from accessibility.ini's game_language
// (runtime/src/accessibility/a11y_config.cpp). Values are the SDK's SC_LANG_* (decomp
// include/lib/revolution/SC/scapi.h): the PAL disc carries EN, DE, FR, ES and IT.
#include "nsmbw_va.h"  // NSMBW_VA / NSMBW_FUNC: guest addresses are PAL rev 1, mapped per version
#include <cstdint>
#include "hle_stubs.h"

extern "C" uint32_t NsmbwA11yGameLanguage();  // runtime/src/accessibility/accessibility.cpp

extern "C" uint32_t NsmbwA11ySCGetLanguage_801DD460()
{
    return NsmbwA11yGameLanguage();
}
PPC_NATIVE_OVERRIDE(801DD460, NsmbwA11ySCGetLanguage_801DD460, uint32_t, (), ());
