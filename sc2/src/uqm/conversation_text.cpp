#include "conversation_text.hpp"
#include "libs/strings/strintrn.h"
namespace uqm {
std::size_t applyConversationText(const dialogue::Json &definition, const std::string &resource,
                                  STRING phrases) {
    dialogue::validateLiveDefinition(definition, resource, GetStringTableCount(phrases));
    for (const auto &entry : definition.at("phrases")) {
        const auto *name =
            GetStringName(SetAbsStringTableIndex(phrases, entry.at("index").get<COUNT>()));
        if (!name || entry.at("id") != name)
            throw std::invalid_argument("phrase ID/index does not match native resource table");
    }
    std::size_t edits = 0;
    for (const auto &entry : definition.at("phrases")) {
        const auto &text = entry.at("text").get_ref<const std::string &>();
        // Do not overwrite translations or original whitespace with an unchanged
        // English snapshot. Only user edits replace the native text bytes.
        if (dialogue::textFingerprint(text) !=
            entry.at("original_text_hash").get<std::uint64_t>()) {
            SetStringTableEntry(ReleaseStringTable(phrases), entry.at("index").get<COUNT>(),
                                text.c_str(), text.size() + 1);
            ++edits;
        }
    }
    return edits;
}
} // namespace uqm
