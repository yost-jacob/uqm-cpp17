#include "verb_factory.hpp"
#include <algorithm>
#include <limits>
#include <utility>
namespace uqm::design::conversation {
namespace {
enum class QueryVerb {
    FleetContains,
    FleetSlots,
    FleetCount,
    FleetValue,
    GalaxyRelationship,
    GalaxyAvailable,
    InventoryHas,
    InventoryQuantity,
    ShipFuel,
    ShipFuelCapacity,
    CurrencyBalance,
    EquipmentCount,
    SalvageValue,
    Capability,
    TechnologyOwned,
    MarketAvailable,
    DateReached,
    ReportAvailable,
    EncounterRead,
    Distance,
    FreeFuel
};
enum class ActionVerb {
    FleetGive,
    FleetTake,
    FleetSetCrew,
    InventoryTransfer,
    ShipAdjust,
    ShipConfigure,
    CurrencyAdjust,
    Purchase,
    Sell,
    PrepareRescue,
    AcceptRescue,
    StripShip,
    GrantTechnology,
    SetRelationship,
    RevealRace,
    ScheduleEvent,
    RememberDate,
    PrepareReport,
    SetOutcome,
    RandomDraw
};
struct Spec {
    const char *name;
    std::size_t minimum, maximum;
};
const std::vector<Spec> queries = {
    {"fleet.contains", 1, 1},     {"fleet.slots", 1, 1},         {"fleet.count", 1, 1},
    {"fleet.value", 0, 0},        {"galaxy.relationship", 1, 1}, {"galaxy.available", 1, 1},
    {"inventory.has", 1, 1},      {"inventory.quantity", 1, 1},  {"ship.fuel", 0, 0},
    {"ship.fuel_capacity", 0, 0}, {"currency.balance", 1, 1},    {"ship.equipment_count", 1, 1},
    {"ship.salvage_value", 0, 0}, {"ship.capability", 1, 1},     {"technology.owned", 1, 1},
    {"market.available", 2, 2},   {"calendar.reached", 4, 4},    {"report.available", 1, 1},
    {"encounter.read", 1, 1},     {"location.distance", 1, 1},   {"ship.free_fuel", 0, 0}};
const std::vector<Spec> actions = {{"fleet.give", 1, 3},
                                   {"fleet.take", 1, 2},
                                   {"fleet.set_crew", 3, 3},
                                   {"inventory.transfer", 1, 2},
                                   {"ship.adjust", 2, 2},
                                   {"ship.configure", 2, 2},
                                   {"currency.adjust", 2, 2},
                                   {"trade.purchase", 3, 3},
                                   {"trade.sell", 3, 3},
                                   {"rescue.prepare", 1, 1},
                                   {"rescue.accept", 1, 1},
                                   {"ship.strip", 1, 1},
                                   {"technology.grant", 1, 1},
                                   {"galaxy.set_relationship", 2, 2},
                                   {"galaxy.reveal", 1, 1},
                                   {"calendar.schedule", 4, 4},
                                   {"calendar.remember", 1, 1},
                                   {"report.prepare", 2, 2},
                                   {"encounter.set_outcome", 1, 1},
                                   {"random.draw", 1, 1}};
std::size_t find(const std::vector<Spec> &specs, const std::string &name) {
    for (std::size_t i = 0; i < specs.size(); ++i)
        if (name == specs[i].name)
            return i;
    throw std::invalid_argument("unknown verb: " + name);
}
std::string text(const Json &value) {
    if (!value.is_string())
        throw std::invalid_argument("expected string argument");
    return value.get<std::string>();
}
Integer number(const Json &value) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() &&
         value.get<std::uint64_t>() >
             static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())))
        throw std::invalid_argument("expected signed 64-bit integer");
    return value.get<Integer>();
}
Integer positive(const Json &value) {
    const auto n = number(value);
    if (n <= 0)
        throw std::invalid_argument("expected positive quantity");
    return n;
}
bool boolean(const Json &value) {
    if (!value.is_boolean())
        throw std::invalid_argument("expected boolean argument");
    return value.get<bool>();
}
Integer add(Integer a, Integer b) {
    if ((b > 0 && a > std::numeric_limits<Integer>::max() - b) ||
        (b < 0 && a < std::numeric_limits<Integer>::min() - b))
        throw std::overflow_error("integer addition overflow");
    return a + b;
}
Integer subtract(Integer a, Integer b) {
    if ((b > 0 && a < std::numeric_limits<Integer>::min() + b) ||
        (b < 0 && a > std::numeric_limits<Integer>::max() + b))
        throw std::overflow_error("integer subtraction overflow");
    return a - b;
}
Integer multiply(Integer a, Integer b) {
    if (a && b &&
        ((a > 0 && b > 0 && a > std::numeric_limits<Integer>::max() / b) ||
         (a > 0 && b < 0 && b < std::numeric_limits<Integer>::min() / a) ||
         (a < 0 && b > 0 && a < std::numeric_limits<Integer>::min() / b) ||
         (a < 0 && b < 0 && a < std::numeric_limits<Integer>::max() / b)))
        throw std::overflow_error("integer multiplication overflow");
    return a * b;
}
void scope(const std::string &value, bool writing = false) {
    static const std::vector<std::string> scopes = {"story",     "player", "galaxy", "session",
                                                    "encounter", "config", "local",  "result"};
    if (std::find(scopes.begin(), scopes.end(), value) == scopes.end() ||
        (writing && (value == "result" || value == "config")))
        throw std::invalid_argument("invalid state scope: " + value);
}
void shape(const Json &value, std::initializer_list<const char *> keys) {
    if (!value.is_object())
        throw std::invalid_argument("expected instruction object");
    for (const auto &item : value.items()) {
        bool found = false;
        for (const auto *key : keys)
            if (item.key() == key)
                found = true;
        if (!found)
            throw std::invalid_argument("unexpected instruction field: " + item.key());
    }
}
} // namespace
std::vector<std::string> QueryFactory::queryNames() {
    std::vector<std::string> result;
    for (const auto &spec : queries)
        result.emplace_back(spec.name);
    return result;
}
std::vector<std::string> VerbFactory::actionNames() {
    std::vector<std::string> result = {"state.set", "state.add", "state.bits", "sequence.next"};
    for (const auto &spec : actions)
        result.emplace_back(spec.name);
    return result;
}
QueryFactory::Arguments QueryFactory::arguments(const Json &definition, std::size_t min,
                                                std::size_t max) const {
    if (!definition.is_array() || definition.size() < min || definition.size() > max)
        throw std::invalid_argument("wrong argument count");
    Arguments result;
    for (const auto &arg : definition)
        result.push_back(value(arg));
    return result;
}
Json QueryFactory::evaluate(const Arguments &args) {
    Json result = Json::array();
    for (const auto &arg : args)
        result.push_back(std::invoke(arg));
    return result;
}
Json QueryFactory::invokeQuery(QueryFunction fn, const State &state, const Arguments &args) {
    return std::invoke(fn, state, QueryFactory::evaluate(args));
}
Result VerbFactory::invokeAction(ActionFunction fn, State &state, const Arguments &args) {
    return std::invoke(fn, state, QueryFactory::evaluate(args));
}
Json QueryFactory::get(const State &state, const Memory &memory, const std::string &domain,
                       const std::string &key, const Json &fallback) {
    Json found;
    if (domain == "local") {
        const auto item = memory.local.find(key);
        if (item != memory.local.end())
            found = item->second;
    } else if (domain == "result") {
        const auto dot = key.find('.');
        if (dot == std::string::npos)
            throw std::invalid_argument("result reference needs name.field");
        const auto item = memory.results.find(key.substr(0, dot));
        if (item != memory.results.end()) {
            const auto field = key.substr(dot + 1);
            if (field == "code")
                found = item->second.code;
            else if (item->second.values.contains(field))
                found = item->second.values.at(field);
        }
    } else
        found = state.read(domain, key);
    if (!found.is_null())
        return found;
    if (!fallback.empty())
        return fallback.at(0);
    throw std::out_of_range("missing state value: " + domain + "." + key);
}
Result VerbFactory::set(State &state, Memory &memory, const std::string &domain,
                        const std::string &key, const Value &value) {
    auto evaluated = std::invoke(value);
    if (domain == "local") {
        memory.local[key] = std::move(evaluated);
        return {};
    }
    return state.set(domain, key, evaluated);
}
Result VerbFactory::increment(State &state, Memory &memory, const std::string &domain,
                              const std::string &key, const Value &value) {
    const auto updated =
        add(number(QueryFactory::get(state, memory, domain, key, Json::array({0}))),
            number(std::invoke(value)));
    return set(state, memory, domain, key, std::bind([](Integer n) -> Json { return n; }, updated));
}
Result VerbFactory::bits(State &state, Memory &memory, const std::string &domain,
                         const std::string &key, const std::string &operation, const Value &value) {
    auto previous = number(QueryFactory::get(state, memory, domain, key, Json::array({0})));
    auto mask = number(std::invoke(value));
    if (operation == "set")
        previous |= mask;
    else if (operation == "clear")
        previous &= ~mask;
    else
        throw std::invalid_argument("bit operation must be set or clear");
    return set(state, memory, domain, key,
               std::bind([](Integer n) -> Json { return n; }, previous));
}
Result VerbFactory::sequence(State &state, Memory &memory, const std::string &domain,
                             const std::string &key, const Json &items, const std::string &policy,
                             Integer base) {
    const auto previous =
        number(QueryFactory::get(state, memory, domain, key, Json::array({base})));
    if (previous < base)
        throw std::out_of_range("sequence cursor below base");
    const auto relative = subtract(previous, base);
    const auto count = static_cast<Integer>(items.size());
    const auto index = policy == "cycle" ? relative % count : std::min(relative, count - 1);
    const auto next = policy == "cycle" ? (index + 1) % count : std::min(index + 1, count - 1);
    const auto result = set(state, memory, domain, key,
                            std::bind([](Integer n) -> Json { return n; }, add(next, base)));
    if (result.code != "ok")
        return result;
    return {"ok", {{"speech", items.at(static_cast<std::size_t>(index))}, {"index", index}}};
}
Json QueryFactory::arithmetic(const std::string &op, const std::vector<Value> &values) {
    const auto a = number(std::invoke(values.at(0))), b = number(std::invoke(values.at(1)));
    if (op == "add")
        return add(a, b);
    if (op == "subtract")
        return subtract(a, b);
    if (op == "multiply")
        return multiply(a, b);
    if (op == "min")
        return std::min(a, b);
    if (op == "max")
        return std::max(a, b);
    if (op == "bit_and")
        return a & b;
    if (b == 0 || (a == std::numeric_limits<Integer>::min() && b == -1))
        throw std::overflow_error("invalid integer division");
    if (op == "divide")
        return a / b; // Truncates toward zero; unit conversions belong to state operations.
    return a % b;
}
bool QueryFactory::comparison(const std::string &op, const Value &left, const Value &right) {
    const auto a = std::invoke(left), b = std::invoke(right);
    if (a.type() != b.type() && !(a.is_number_integer() && b.is_number_integer()))
        throw std::invalid_argument("comparison operand types differ");
    if (!(a.is_boolean() || a.is_string() || a.is_number_integer()))
        throw std::invalid_argument("comparison requires scalar operands");
    if (a.is_number_integer()) {
        number(a);
        number(b);
    }
    if (op == "eq")
        return a == b;
    if (op == "ne")
        return a != b;
    const auto x = number(a), y = number(b);
    if (op == "gt")
        return x > y;
    if (op == "ge")
        return x >= y;
    if (op == "lt")
        return x < y;
    return x <= y;
}
bool QueryFactory::truth(const Value &value) {
    return boolean(std::invoke(value));
}
bool QueryFactory::every(const std::vector<Condition> &conditions) {
    for (const auto &c : conditions)
        if (!std::invoke(c))
            return false;
    return true;
}
bool QueryFactory::some(const std::vector<Condition> &conditions) {
    for (const auto &c : conditions)
        if (std::invoke(c))
            return true;
    return false;
}
bool QueryFactory::negate(const Condition &condition) {
    return !std::invoke(condition);
}
Value QueryFactory::value(const Json &definition) const {
    if (definition.is_number_integer())
        number(definition);
    if (definition.is_number_float())
        throw std::invalid_argument("floating-point expressions are not supported");
    if (!definition.is_object())
        return std::bind([](const Json &v) { return v; }, definition);
    if (definition.contains("literal")) {
        shape(definition, {"literal"});
        return std::bind([](const Json &v) { return v; }, definition.at("literal"));
    }
    if (definition.contains("get")) {
        shape(definition, {"get"});
        const auto &a = definition.at("get");
        if (!a.is_array() || a.size() < 2 || a.size() > 3)
            throw std::invalid_argument("get needs scope,key[,default]");
        auto domain = text(a.at(0)), key = text(a.at(1));
        scope(domain);
        if (key.empty())
            throw std::invalid_argument("empty state key");
        return std::bind(&QueryFactory::get, std::cref(state_), std::cref(memory_), domain, key,
                         a.size() == 3 ? Json::array({a.at(2)}) : Json::array());
    }
    if (definition.contains("query")) {
        shape(definition, {"query", "args"});
        auto index = find(queries, text(definition.at("query")));
        auto args = arguments(definition.value("args", Json::array()), queries[index].minimum,
                              queries[index].maximum);
        QueryFunction fn = nullptr;
        switch (static_cast<QueryVerb>(index)) {
        case QueryVerb::FleetContains:
            fn = [](const State &s, const Json &a) -> Json { return s.hasShip(text(a[0])); };
            break;
        case QueryVerb::FleetSlots:
            fn = [](const State &s, const Json &a) -> Json { return s.availableSlots(text(a[0])); };
            break;
        case QueryVerb::FleetCount:
            fn = [](const State &s, const Json &a) -> Json { return s.shipCount(text(a[0])); };
            break;
        case QueryVerb::FleetValue:
            fn = [](const State &s, const Json &) -> Json { return s.fleetValue(); };
            break;
        case QueryVerb::GalaxyRelationship:
            fn = [](const State &s, const Json &a) -> Json { return s.relationship(text(a[0])); };
            break;
        case QueryVerb::GalaxyAvailable:
            fn = [](const State &s, const Json &a) -> Json { return s.raceAvailable(text(a[0])); };
            break;
        case QueryVerb::InventoryHas:
            fn = [](const State &s, const Json &a) -> Json { return s.hasItem(text(a[0])); };
            break;
        case QueryVerb::InventoryQuantity:
            fn = [](const State &s, const Json &a) -> Json { return s.quantity(text(a[0])); };
            break;
        case QueryVerb::ShipFuel:
            fn = [](const State &s, const Json &) -> Json { return s.fuel(); };
            break;
        case QueryVerb::ShipFuelCapacity:
            fn = [](const State &s, const Json &) -> Json { return s.fuelCapacity(); };
            break;
        case QueryVerb::CurrencyBalance:
            fn = [](const State &s, const Json &a) -> Json { return s.balance(text(a[0])); };
            break;
        case QueryVerb::EquipmentCount:
            fn = [](const State &s, const Json &a) -> Json { return s.equipmentCount(text(a[0])); };
            break;
        case QueryVerb::SalvageValue:
            fn = [](const State &s, const Json &) -> Json { return s.salvageValue(); };
            break;
        case QueryVerb::Capability:
            fn = [](const State &s, const Json &a) -> Json { return s.capability(text(a[0])); };
            break;
        case QueryVerb::TechnologyOwned:
            fn = [](const State &s, const Json &a) -> Json { return s.hasTechnology(text(a[0])); };
            break;
        case QueryVerb::MarketAvailable:
            fn = [](const State &s, const Json &a) -> Json {
                return s.marketAvailable(text(a[0]), text(a[1]));
            };
            break;
        case QueryVerb::DateReached:
            fn = [](const State &s, const Json &a) -> Json {
                return s.dateReached(text(a[0]), number(a[1]), number(a[2]), boolean(a[3]));
            };
            break;
        case QueryVerb::ReportAvailable:
            fn = [](const State &s, const Json &a) -> Json {
                return s.reportAvailable(text(a[0]));
            };
            break;
        case QueryVerb::EncounterRead:
            fn = [](const State &s, const Json &a) -> Json { return s.encounter(text(a[0])); };
            break;
        case QueryVerb::Distance:
            fn = [](const State &s, const Json &a) -> Json { return s.distanceTo(text(a[0])); };
            break;
        case QueryVerb::FreeFuel:
            fn = [](const State &s, const Json &) -> Json {
                return std::max<Integer>(0, subtract(s.fuelCapacity(), s.fuel()));
            };
            break;
        default:
            throw std::logic_error("query registration missing binder");
        }
        return std::bind(&QueryFactory::invokeQuery, fn, std::cref(state_), std::move(args));
    }
    if (definition.size() == 1) {
        const auto op = definition.begin().key();
        static const std::vector<std::string> operations = {
            "add", "subtract", "multiply", "divide", "mod", "min", "max", "bit_and"};
        if (std::find(operations.begin(), operations.end(), op) != operations.end())
            return std::bind(&QueryFactory::arithmetic, op,
                             arguments(definition.begin().value(), 2, 2));
    }
    throw std::invalid_argument("unknown value expression: " + definition.dump());
}
Json QueryFactory::segment(const std::string &kind, const Value &value) {
    auto resolved = std::invoke(value);
    if (kind == "number")
        number(resolved);
    else if (kind == "insert" || kind == "phrase")
        text(resolved);
    else if (!resolved.is_array())
        throw std::invalid_argument("compose needs text segments");
    return Json{{kind, std::move(resolved)}};
}
Value QueryFactory::speech(const Json &definition) const {
    if (definition.is_array()) {
        Arguments segments;
        for (const auto &item : definition)
            segments.push_back(speech(item));
        return std::bind(&QueryFactory::evaluate, std::move(segments));
    }
    if (definition.is_object()) {
        for (const auto *kind : {"insert", "number", "phrase", "compose"}) {
            if (definition.contains(kind)) {
                shape(definition, {kind});
                auto contents = kind == std::string("compose") ? speech(definition.at(kind))
                                                               : value(definition.at(kind));
                return std::bind(&QueryFactory::segment, std::string(kind), std::move(contents));
            }
        }
    }
    // Literal phrase IDs or a getter for an already prepared native utterance list.
    return value(definition);
}
Condition QueryFactory::condition(const Json &definition) const {
    if (definition.is_boolean())
        return std::bind([](bool v) { return v; }, definition.get<bool>());
    if (!definition.is_object() || definition.empty())
        throw std::invalid_argument("invalid condition");
    if (definition.contains("all") || definition.contains("any")) {
        const auto key = definition.contains("all") ? "all" : "any";
        shape(definition, {key});
        const auto &items = definition.at(key);
        if (!items.is_array())
            throw std::invalid_argument("all/any need an array");
        std::vector<Condition> conditions;
        for (const auto &item : items)
            conditions.push_back(condition(item));
        return std::bind(key == std::string("all") ? &QueryFactory::every : &QueryFactory::some,
                         std::move(conditions));
    }
    if (definition.contains("not")) {
        shape(definition, {"not"});
        return std::bind(&QueryFactory::negate, condition(definition.at("not")));
    }
    if (definition.size() == 1) {
        const auto op = definition.begin().key();
        static const std::vector<std::string> ops = {"eq", "ne", "gt", "ge", "lt", "le"};
        if (std::find(ops.begin(), ops.end(), op) != ops.end()) {
            const auto &a = definition.begin().value();
            if (!a.is_array() || a.size() != 2)
                throw std::invalid_argument("comparison needs two operands");
            return std::bind(&QueryFactory::comparison, op, value(a[0]), value(a[1]));
        }
    }
    return std::bind(&QueryFactory::truth, value(definition));
}
Action VerbFactory::action(const Json &definition) const {
    shape(definition, {"verb", "args"});
    const auto name = text(definition.at("verb"));
    const auto a = definition.value("args", Json::array());
    if (name == "state.set" || name == "state.add" || name == "state.bits" ||
        name == "sequence.next") {
        const auto expected = (name == "state.bits" || name == "sequence.next") ? 4u : 3u;
        if (!a.is_array() || (a.size() != expected && !(name == "sequence.next" && a.size() == 5)))
            throw std::invalid_argument("wrong state-operation arguments");
        auto domain = text(a[0]), key = text(a[1]);
        scope(domain, true);
        if (key.empty())
            throw std::invalid_argument("empty state key");
        if (name == "state.set")
            return std::bind(&VerbFactory::set, std::ref(state_), std::ref(memory_), domain, key,
                             value(a[2]));
        if (name == "state.add")
            return std::bind(&VerbFactory::increment, std::ref(state_), std::ref(memory_), domain,
                             key, value(a[2]));
        if (name == "state.bits") {
            auto op = text(a[2]);
            if (op != "set" && op != "clear")
                throw std::invalid_argument("invalid bit operation");
            return std::bind(&VerbFactory::bits, std::ref(state_), std::ref(memory_), domain, key,
                             op, value(a[3]));
        }
        if (!a[2].is_array() || a[2].empty())
            throw std::invalid_argument("sequence needs items");
        auto policy = text(a[3]);
        if (policy != "cycle" && policy != "repeat_last")
            throw std::invalid_argument("unknown sequence policy");
        return std::bind(&VerbFactory::sequence, std::ref(state_), std::ref(memory_), domain, key,
                         a[2], policy, a.size() == 5 ? number(a[4]) : 0);
    }
    const auto index = find(actions, name);
    auto args = queries_.arguments(a, actions[index].minimum, actions[index].maximum);
    if (name == "fleet.give" && a.size() == 2 && !a[1].is_object() && !a[1].is_number_integer())
        throw std::invalid_argument("two-argument fleet.give needs a count");
    ActionFunction fn = nullptr;
    switch (static_cast<ActionVerb>(index)) {
    case ActionVerb::FleetGive:
        // Select the overload while binding; generate defaults only on invocation.
        if (a.size() == 1)
            fn = [](State &s, const Json &v) { return s.createShip(text(v[0])); };
        else if (a.size() == 2)
            fn = [](State &s, const Json &v) { return s.giveShips(text(v[0]), positive(v[1])); };
        else
            fn = [](State &s, const Json &v) {
                if (number(v[2]) < 0)
                    throw std::invalid_argument("negative crew");
                return s.createShip(text(v[0]), text(v[1]), number(v[2]));
            };
        break;
    case ActionVerb::FleetTake:
        if (a.size() == 1)
            fn = [](State &s, const Json &v) { return s.removeShips(text(v[0])); };
        else
            fn = [](State &s, const Json &v) {
                auto count = number(v[1]);
                if (count < 0)
                    throw std::invalid_argument("negative ship removal count");
                return s.removeShips(text(v[0]), count);
            };
        break;
    case ActionVerb::FleetSetCrew:
        fn = [](State &s, const Json &a) {
            return s.setShipCrew(text(a[0]), number(a[1]), text(a[2]));
        };
        break;
    case ActionVerb::InventoryTransfer:
        if (a.size() == 1)
            fn = [](State &s, const Json &v) { return s.transferItem(text(v[0])); };
        else
            fn = [](State &s, const Json &v) { return s.transferItem(text(v[0]), number(v[1])); };
        break;
    case ActionVerb::ShipAdjust:
        fn = [](State &s, const Json &a) { return s.adjustShip(text(a[0]), number(a[1])); };
        break;
    case ActionVerb::ShipConfigure:
        fn = [](State &s, const Json &a) { return s.configureShip(text(a[0]), a[1]); };
        break;
    case ActionVerb::CurrencyAdjust:
        fn = [](State &s, const Json &a) { return s.adjustCurrency(text(a[0]), number(a[1])); };
        break;
    case ActionVerb::Purchase:
        fn = [](State &s, const Json &a) { return s.purchase(text(a[0]), text(a[1]), a[2]); };
        break;
    case ActionVerb::Sell:
        fn = [](State &s, const Json &a) { return s.sell(text(a[0]), text(a[1]), a[2]); };
        break;
    case ActionVerb::PrepareRescue:
        fn = [](State &s, const Json &a) { return s.prepareRescue(text(a[0])); };
        break;
    case ActionVerb::AcceptRescue:
        fn = [](State &s, const Json &a) { return s.acceptRescue(text(a[0])); };
        break;
    case ActionVerb::StripShip:
        fn = [](State &s, const Json &a) { return s.stripShip(text(a[0])); };
        break;
    case ActionVerb::GrantTechnology:
        fn = [](State &s, const Json &a) { return s.grantTechnology(text(a[0])); };
        break;
    case ActionVerb::SetRelationship:
        fn = [](State &s, const Json &a) { return s.setRelationship(text(a[0]), text(a[1])); };
        break;
    case ActionVerb::RevealRace:
        fn = [](State &s, const Json &a) { return s.revealRace(text(a[0])); };
        break;
    case ActionVerb::ScheduleEvent:
        fn = [](State &s, const Json &a) {
            return s.scheduleEvent(text(a[0]), number(a[1]), number(a[2]), number(a[3]));
        };
        break;
    case ActionVerb::RememberDate:
        fn = [](State &s, const Json &a) { return s.rememberDate(text(a[0])); };
        break;
    case ActionVerb::PrepareReport:
        fn = [](State &s, const Json &a) { return s.prepareReport(text(a[0]), a[1]); };
        break;
    case ActionVerb::SetOutcome:
        fn = [](State &s, const Json &a) { return s.setOutcome(text(a[0])); };
        break;
    case ActionVerb::RandomDraw:
        fn = [](State &s, const Json &a) {
            return Result{"ok", {{"value", s.randomBelow(positive(a[0]))}}};
        };
        break;
    default:
        throw std::logic_error("action registration missing binder");
    }
    return std::bind(&VerbFactory::invokeAction, fn, std::ref(state_), std::move(args));
}
} // namespace uqm::design::conversation
