#include "text.hpp"
#include <limits>
namespace uqm::dialogue {
std::string formatText(const Json &value, const PhraseText &phrase) {
    if (value.is_string())
        return phrase(value.get<std::string>());
    if (value.is_array()) {
        std::string result;
        for (const auto &part : value)
            result += formatText(part, phrase);
        return result;
    }
    if (!value.is_object() || value.size() != 1)
        throw std::invalid_argument("invalid resolved dialogue text segment");
    if (value.contains("compose"))
        return formatText(value.at("compose"), phrase);
    if (value.contains("phrase"))
        return phrase(value.at("phrase").get<std::string>());
    if (value.contains("insert"))
        return value.at("insert").get<std::string>();
    if (value.contains("number")) {
        const auto &number = value.at("number");
        if (!number.is_number_integer() ||
            (number.is_number_unsigned() &&
             number.get<std::uint64_t>() >
                 static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())))
            throw std::invalid_argument("invalid resolved dialogue number");
        return std::to_string(number.get<Integer>());
    }
    throw std::invalid_argument("unknown resolved dialogue text segment");
}
} // namespace uqm::dialogue
