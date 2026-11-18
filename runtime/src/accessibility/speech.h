#ifndef NSMBW_ACCESSIBILITY_SPEECH_H
#define NSMBW_ACCESSIBILITY_SPEECH_H

#include <string>

// Everything the mod says goes through here: it logs the line, remembers it for F9 and honours
// the speech setting before handing it to the ScreenReader sink.
namespace a11y::speech {

// `interrupt` cuts the current utterance: use it for a context change (new menu, new screen).
void Say(const std::string& text, bool interrupt);

// Any thread (the host-drawn options menu): queued and spoken on the next game frame.
void Post(const std::string& text, bool interrupt);
// Game thread: speaks whatever Post() queued.
void DrainPosted();

void RepeatLast();
void Stop();

}  // namespace a11y::speech

#endif  // NSMBW_ACCESSIBILITY_SPEECH_H
