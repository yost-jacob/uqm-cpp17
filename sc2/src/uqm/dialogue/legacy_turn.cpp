#include "legacy_turn.hpp"
#include <set>
namespace uqm::dialogue {
LegacyTurnState::LegacyTurnState(std::string script, Callback callback)
    : script_(std::move(script)), callback_(std::move(callback)) {
    if (script_.empty() || !callback_)
        throw std::invalid_argument("legacy conversation handler requires script and callback");
}
Result LegacyTurnState::legacyTurn(const std::string &script, const Json &selection) {
    if (script != script_)
        throw std::invalid_argument("unknown conversation handler script: " + script);
    if (selection.is_null()) {
        if (started_)
            throw std::logic_error("conversation intro handler invoked twice");
        started_ = true;
        return callback_(std::nullopt);
    }
    if (!started_ || !selection.is_number_integer() || selection < 0 || selection >= 8)
        throw std::invalid_argument("invalid native conversation response slot");
    return callback_(selection.get<std::size_t>());
}
std::uint64_t textFingerprint(std::string_view text) noexcept {
    std::uint64_t value = 14695981039346656037ULL;
    for (const unsigned char byte : text)
        value = (value ^ byte) * 1099511628211ULL;
    return value;
}
void validateLiveDefinition(const Json &definition, const std::string &resource,
                            std::size_t phraseCount) {
    if (definition.at("resource") != resource || !definition.at("id").is_string() ||
        definition.at("id").get<std::string>().empty())
        throw std::invalid_argument("conversation resource or script identity mismatch");
    const auto &phrases = definition.at("phrases");
    if (!phrases.is_array() || phrases.size() != phraseCount)
        throw std::invalid_argument("conversation phrase count does not match native table");
    std::set<std::string> names;
    for (std::size_t i = 0; i < phrases.size(); ++i) {
        const auto &entry = phrases[i];
        if (!entry.at("index").is_number_integer() || entry.at("index") != i ||
            !entry.at("id").is_string() || !entry.at("text").is_string())
            throw std::invalid_argument("invalid indexed conversation phrase");
        if (!entry.at("original_text_hash").is_number_unsigned())
            throw std::invalid_argument("invalid original phrase text fingerprint");
        const auto id = entry.at("id").get<std::string>();
        const auto &text = entry.at("text").get_ref<const std::string &>();
        if (id.empty() || !names.insert(id).second || text.find('\0') != std::string::npos)
            throw std::invalid_argument("duplicate phrase ID or embedded NUL in text");
    }
}
} // namespace uqm::dialogue
