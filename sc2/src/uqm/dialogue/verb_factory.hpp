#ifndef UQM_DIALOGUE_VERB_FACTORY_HPP
#define UQM_DIALOGUE_VERB_FACTORY_HPP
#include "state.hpp"
#include <functional>
#include <map>
#include <vector>
namespace uqm::dialogue {
using Value = std::function<Json()>;
using Condition = std::function<bool()>;
using Action = std::function<Result()>;
struct Memory {
    std::map<std::string, Json> local;
    std::map<std::string, Result> results;
};
// Borrowed State and Memory must outlive every bound callable. Read binding has
// no mutable access, including while constructing specialized query handlers.
class QueryFactory {
  public:
    QueryFactory(const State &state, const Memory &memory) : state_(state), memory_(memory) {}
    Value value(const Json &definition) const;
    Value speech(const Json &definition) const;
    Condition condition(const Json &definition) const;
    static std::vector<std::string> queryNames();

  private:
    friend class VerbFactory;
    // Explicit native escape hatches, shared with the action binder. Functions
    // defined inside this nested class are implicitly inline; no instance exists.
    // Query handlers borrow const state. Action handlers borrow mutable state and
    // return data/outcomes rather than controlling playback or graph traversal.
    struct Handlers {
        static Result legacyTurn(State &state, const std::string &script, const Json &selection) {
            return state.legacyTurn(script, selection);
        }
        static bool reportAvailable(const State &state, const std::string &report) {
            return state.reportAvailable(report);
        }
        static Result prepareReport(State &state, const std::string &report,
                                    const Json &parameters) {
            return state.prepareReport(report, parameters);
        }
        static Result prepareRescue(State &state, const std::string &merchant) {
            return state.prepareRescue(merchant);
        }
        static Result acceptRescue(State &state, const std::string &token) {
            return state.acceptRescue(token);
        }
        static Result stripShip(State &state, const std::string &policy) {
            return state.stripShip(policy);
        }
    };
    const State &state_;
    const Memory &memory_;
    using Arguments = std::vector<Value>;
    using QueryFunction = Json (*)(const State &, const Json &);
    Arguments arguments(const Json &, std::size_t, std::size_t) const;
    static Json evaluate(const Arguments &);
    static Json invokeQuery(QueryFunction, const State &, const Arguments &);
    static Json get(const State &, const Memory &, const std::string &, const std::string &,
                    const Json &);
    static Json arithmetic(const std::string &, const std::vector<Value> &);
    static bool comparison(const std::string &, const Value &, const Value &);
    static Json segment(const std::string &, const Value &);
    static bool truth(const Value &);
    static bool every(const std::vector<Condition> &);
    static bool some(const std::vector<Condition> &);
    static bool negate(const Condition &);
};
// Actions alone retain mutable access. Their argument expressions use the same
// const query binder, so evaluating arguments cannot perform state operations.
class VerbFactory {
  public:
    VerbFactory(State &state, Memory &memory)
        : state_(state), memory_(memory), queries_(state, memory) {}
    Value value(const Json &definition) const {
        return queries_.value(definition);
    }
    Value speech(const Json &definition) const {
        return queries_.speech(definition);
    }
    Condition condition(const Json &definition) const {
        return queries_.condition(definition);
    }
    Action action(const Json &definition) const;
    static std::vector<std::string> queryNames() {
        return QueryFactory::queryNames();
    }
    static std::vector<std::string> actionNames();

  private:
    State &state_;
    Memory &memory_;
    QueryFactory queries_;
    using Handlers = QueryFactory::Handlers;
    using Arguments = std::vector<Value>;
    using ActionFunction = Result (*)(State &, const Json &);
    static Result invokeAction(ActionFunction, State &, const Arguments &);
    static Result set(State &, Memory &, const std::string &, const std::string &, const Value &);
    static Result increment(State &, Memory &, const std::string &, const std::string &,
                            const Value &);
    static Result bits(State &, Memory &, const std::string &, const std::string &,
                       const std::string &, const Value &);
    static Result sequence(State &, Memory &, const std::string &, const std::string &,
                           const Json &, const std::string &, Integer);
};
} // namespace uqm::dialogue
#endif
