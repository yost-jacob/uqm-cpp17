#include "live_definition.hpp"
#include <set>
namespace uqm::dialogue {
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
