#include "accessibility/localization.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "accessibility/a11y_log.h"
#include "accessibility/a11y_config.h"

namespace a11y::loc {
namespace {

struct Phrase {
    const char* key;
    const char* english;
};

// Built-in English defaults, also the key catalogue: a file key not listed here is reported.
constexpr Phrase kEnglishDefaults[] = {
#include "accessibility/phrases_en.inc"
};

// "auto" follows the Windows display language; only the languages we ship a file for are picked.
std::string LanguageCode() {
    const std::string& wanted = config::Get().language;
    if (wanted != "auto" && !wanted.empty()) return wanted;
    const LANGID lang = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(lang)) {
        case LANG_SPANISH: return "es";
        default: return "en";
    }
}

std::unordered_map<std::string, std::string> g_phrases;

std::string_view TrimView(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

// Values wanting leading/trailing spaces (the "curve_then" joiner) wrap them in quotes.
std::string_view Unquote(std::string_view value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

std::filesystem::path ExeDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, length)).parent_path();
}

namespace {

void LoadLanguageFile(const std::filesystem::path& path, const char* code) {
    std::ifstream file(path);
    if (!file.is_open()) {
        RT_LOGF(RT_TAG_A11Y, "localization: no file for '%s', using built-in English\n", code);
        return;
    }
    int applied = 0;
    std::string line;
    while (std::getline(file, line)) {
        const std::string_view trimmed = TrimView(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }
        const size_t equals = trimmed.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        const std::string key(TrimView(trimmed.substr(0, equals)));
        const std::string_view value = Unquote(TrimView(trimmed.substr(equals + 1)));
        const auto it = g_phrases.find(key);
        if (it == g_phrases.end()) {
            RT_LOGF(RT_TAG_A11Y, "localization: unknown key '%s' in %s.ini\n", key.c_str(), code);
            continue;
        }
        it->second.assign(value);
        ++applied;
    }
    RT_LOGF(RT_TAG_A11Y, "localization: '%s' loaded, %d phrases\n", code, applied);
}

}  // namespace

void Init() {
    g_phrases.clear();
    for (const Phrase& phrase : kEnglishDefaults) {
        g_phrases.emplace(phrase.key, phrase.english);
    }
    const std::string code = LanguageCode();
    if (code != "en") {
        LoadLanguageFile(ExeDirectory() / "accessibility_lang" / (code + ".ini"), code.c_str());
    }
}

std::string CurrentLanguage() {
    return LanguageCode();
}

std::string Get(const std::string& key) {
    const auto it = g_phrases.find(key);
    if (it == g_phrases.end()) {
        RT_LOGF(RT_TAG_A11Y, "localization: missing key '%s'\n", key.c_str());
        return key;  // audible and debuggable, unlike silence
    }
    return it->second;
}

bool Has(const std::string& key) {
    return g_phrases.find(key) != g_phrases.end();
}

std::string Format(const std::string& key,
                   std::initializer_list<std::pair<std::string_view, std::string>> args) {
    std::string text = Get(key);
    for (const auto& [name, value] : args) {
        const std::string token = "{" + std::string(name) + "}";
        for (size_t pos = text.find(token); pos != std::string::npos;
             pos = text.find(token, pos + value.size())) {
            text.replace(pos, token.size(), value);
        }
    }
    return text;
}

}  // namespace a11y::loc
