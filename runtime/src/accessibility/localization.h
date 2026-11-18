#ifndef MKW_ACCESSIBILITY_LOCALIZATION_H
#define MKW_ACCESSIBILITY_LOCALIZATION_H

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace a11y::loc {

// Directory of the running exe — where the mod's data folders (accessibility_lang,
// accessibility_sounds) live. Shared so no other file re-derives it.
std::filesystem::path ExeDirectory();

// Loads the phrase table for the configured language (accessibility.ini, "auto" = Windows).
// Files live in accessibility_lang\<code>.ini next to the exe; a missing file or key
// falls back to the built-in English defaults, so speech can never go mute.
void Init();

// The language code in use ("es", "en"), after resolving "auto".
std::string CurrentLanguage();

// Text for a key: the language file's value, or the built-in English default.
std::string Get(const std::string& key);

// Whether a key exists at all. Lets an optional phrase — a menu option's description — be
// skipped, instead of speaking the key back, which is what Get() does when it is missing.
bool Has(const std::string& key);

// Get() plus {name} placeholder substitution, e.g. Format("lap", {{"n", "2"}, {"total", "3"}}).
std::string Format(const std::string& key,
                   std::initializer_list<std::pair<std::string_view, std::string>> args);


}  // namespace a11y::loc

#endif  // MKW_ACCESSIBILITY_LOCALIZATION_H
