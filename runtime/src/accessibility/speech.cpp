#include "accessibility/speech.h"

#include <mutex>
#include <utility>
#include <vector>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/localization.h"
#include "accessibility/screen_reader.h"
#include <aurora/env.hpp>

namespace a11y::speech {
namespace {

std::string g_last;
std::mutex g_postedMutex;
std::vector<std::pair<std::string, bool>> g_posted;

// Test runs set NSMBW_A11Y_SILENT=1 so automated sessions log every line without talking over
// whatever else the machine's screen reader is doing.
bool Silent() {
    static const bool silent = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_SILENT");
        return v != nullptr && *v == '1';
    }();
    return silent;
}

}  // namespace

void Say(const std::string& text, bool interrupt) {
    if (text.empty()) {
        return;
    }
    g_last = text;
    RT_LOGF(RT_TAG_A11Y, "say%s: %s\n", interrupt ? "!" : "", text.c_str());
    if (!config::Get().speech || Silent()) {
        return;
    }
    ScreenReader::Instance().Speak(text, interrupt);
}

void Post(const std::string& text, bool interrupt) {
    std::lock_guard lock(g_postedMutex);
    if (interrupt) g_posted.clear();  // only the newest context matters
    g_posted.emplace_back(text, interrupt);
}

void DrainPosted() {
    std::vector<std::pair<std::string, bool>> posted;
    {
        std::lock_guard lock(g_postedMutex);
        posted.swap(g_posted);
    }
    for (const auto& [text, interrupt] : posted) Say(text, interrupt);
}

void RepeatLast() {
    if (g_last.empty()) {
        Say(loc::Get("repeat_none"), true);
        return;
    }
    const std::string text = g_last;
    Say(text, true);
}

void Stop() {
    ScreenReader::Instance().Silence();
}

}  // namespace a11y::speech
