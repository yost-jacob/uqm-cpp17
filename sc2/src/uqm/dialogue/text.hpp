#ifndef UQM_DIALOGUE_TEXT_HPP
#define UQM_DIALOGUE_TEXT_HPP
#include "state.hpp"
#include <functional>
namespace uqm::dialogue {
using PhraseText = std::function<std::string(const std::string &)>;
// Resolved segments only: phrase IDs, raw insertions, integers and composition.
// Lookup is borrowed for this call. No state writes, RNG or printf expressions.
std::string formatText(const Json &segments, const PhraseText &phrase);
} // namespace uqm::dialogue
#endif
