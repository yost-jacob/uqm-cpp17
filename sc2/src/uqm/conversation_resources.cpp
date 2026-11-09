#include "conversation_resources.hpp"
#include "libs/sndlib.h"
namespace uqm {
ConversationResources::ConversationResources(const LOCDATA &configuration) noexcept {
    script.data = configuration;
    // Configuration carries resource IDs, never ownership of loaded handles.
    script.data.AlienFrame = nullptr;
    script.data.AlienFont = nullptr;
    script.data.AlienColorMap = nullptr;
    script.data.AlienSong = nullptr;
    script.data.ConversationPhrases = nullptr;
}
ConversationResources::~ConversationResources() {
    if (animation) DestroyContext(animation);
    if (textCache) DestroyContext(textCache);
    if (script.data.ConversationPhrases) DestroyStringTable(ReleaseStringTable(script.data.ConversationPhrases));
    if (script.data.AlienSong) DestroyMusic(script.data.AlienSong);
    if (script.data.AlienColorMap) DestroyColorMap(ReleaseColorMap(script.data.AlienColorMap));
    if (script.data.AlienFont) DestroyFont(script.data.AlienFont);
    if (script.data.AlienFrame) DestroyDrawable(ReleaseDrawable(script.data.AlienFrame));
    if (textCacheFrame) DestroyDrawable(ReleaseDrawable(textCacheFrame));
    if (playerFont) DestroyFont(playerFont);
}
}
