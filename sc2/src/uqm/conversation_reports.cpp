// Report policy adapted from Paul Reiche/Fred Ford's starbase script (GPL-2.0-or-later).
#include "conversation_reports.hpp"
#include "game_state.hpp"
#include "inventory.hpp"
#include "simulation_engine.hpp"
#include "sis_legacy.hpp"
#include "build.h"
#include "commglue.h"
#include "comm/starbas/strings.h"
#include "globdata.h"
#include "libs/mathlib.h"
#include <algorithm>
#include <array>
namespace uqm {
using dialogue::Json;
using dialogue::Result;
class StarbaseReports::Builder {
public:
    Builder(const StarbaseReports &owner, unsigned greeting, unsigned intro)
        : owner_(owner), sis_(detail::SISAccess::record(owner.player_.sis())), greeting_(greeting), intro_(intro) { plan_.bulletins = owner.bulletins_; }
    Plan plan_;
    const StarbaseReports &owner_;
    const SIS_STATE &sis_;
    unsigned greeting_, intro_;
    bool restoreTalking_ = false;
    COUNT alliance(COUNT race) const {
        const auto &fleets = owner_.session_.raceFleets();
        return race < fleets.size() ? fleets.at(race).allied_state : 0;
    }
    bool escortRoom(COUNT race) const {
        return race < owner_.session_.raceFleets().size() && owner_.player_.fleet().size() < MAX_BUILT_SHIPS;
    }
    COUNT escortsWorth() const {
        COUNT total = 0;
        const auto &fleet = owner_.player_.fleet();
        for (const auto id : fleet.ships()) total += shipPrice(fleet.find(id)->get().race());
        return total;
    }
    unsigned getByte(std::string_view name) const {
        const auto found = plan_.writes.find(name);
        return found == plan_.writes.end() ? owner_.player_.story().readNamed(name) : found->second;
    }
    unsigned get(std::string_view name) const {
        if (name != "STARBASE_BULLETS0" && name != "ZOQFOT_GRPOFFS0") return getByte(name);
        std::string field(name);
        unsigned result = 0;
        for (unsigned i = 0; i < 4; ++i) { field.back() = char('0' + i); result |= getByte(field) << (8 * i); }
        return result;
    }
    void put(const std::string &name, unsigned value) {
        if (name == "STARBASE_BULLETS0") {
            auto field = name;
            for (unsigned i = 0; i < 4; ++i) { field.back() = char('0' + i); plan_.writes[field] = (value >> (8 * i)) & 255; }
            return;
        }
        if (value > owner_.player_.story().maximumNamed(name)) throw std::out_of_range("report field out of range: " + name);
        plan_.writes[name] = value;
    }
    std::string phrase(int id) const { return owner_.phrases_.at(id - 1).at("id").get<std::string>(); }
    void say(int id) {
        if (id == GLOBAL_PLAYER_NAME) plan_.speech.push_back({{"insert", owner_.player_.ship().commanderName}});
        else if (id == GLOBAL_SHIP_NAME) plan_.speech.push_back({{"insert", owner_.player_.ship().shipName}});
        else if (id) {
            if (restoreTalking_) {
                plan_.speech.push_back({{"phrase", phrase(id)}, {"on_start", "starbase.commander"}});
                restoreTalking_ = false;
            } else plan_.speech.push_back(phrase(id));
        }
    }
void AnalyzeCondition() {
	BYTE i;
	BYTE num_thrusters = 0,
				num_jets = 0,
				num_guns = 0,
				num_bays = 0,
				num_batts = 0,
				num_track = 0,
				num_defense = 0;
	bool HasMinimum;

	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
	{
		if (sis_.DriveSlots[i] < EMPTY_SLOT)
			++num_thrusters;
	}
	for (i = 0; i < NUM_JET_SLOTS; ++i)
	{
		if (sis_.JetSlots[i] < EMPTY_SLOT)
			++num_jets;
	}
	for (i = 0; i < NUM_MODULE_SLOTS; ++i)
	{
		BYTE which_piece;

		switch (which_piece = sis_.ModuleSlots[i])
		{
			case STORAGE_BAY:
				++num_bays;
				break;
			case DYNAMO_UNIT:
			case SHIVA_FURNACE:
				num_batts += 1 + (which_piece - DYNAMO_UNIT);
				break;
			case GUN_WEAPON:
			case BLASTER_WEAPON:
			case CANNON_WEAPON:
				num_guns += 1 + (which_piece - GUN_WEAPON);
				break;
			case TRACKING_SYSTEM:
				++num_track;
				break;
			case ANTIMISSILE_DEFENSE:
				++num_defense;
				break;
		}
	}
	if (num_track && num_guns)
		num_guns += 2;

	HasMinimum = (num_thrusters >= 7 && num_jets >= 5
			&& sis_.CrewEnlisted >= CREW_POD_CAPACITY
			&& sis_.FuelOnBoard >= FUEL_TANK_CAPACITY
			&& num_bays >= 1 && sis_.NumLanders
			&& num_batts >= 1 && num_guns >= 2);
	say(LETS_SEE);
	if (!HasMinimum && get("CHMMR_BOMB_STATE") < 2)
	{
		say(IMPROVE_1);
		if (num_thrusters < 7)
			say(NEED_THRUSTERS_1);
		if (num_jets < 5)
			say(NEED_TURN_1);
		if (num_guns < 2)
			say(NEED_GUNS_1);
		if (sis_.CrewEnlisted < CREW_POD_CAPACITY)
			say(NEED_CREW_1);
		if (sis_.FuelOnBoard < FUEL_TANK_CAPACITY)
			say(NEED_FUEL_1);
		if (num_bays < 1)
			say(NEED_STORAGE_1);
		if (sis_.NumLanders == 0)
			say(NEED_LANDERS_2);
		if (num_batts < 1)
			say(NEED_DYNAMOS_1);

		if (sis_.ResUnits >= 3000)
			say(IMPROVE_FLAGSHIP_WITH_RU);
		else
			say(GO_GET_MINERALS);
	}
	else
	{
		BYTE num_aliens = 0;
		COUNT FleetStrength;
		bool HasMaximum;

		FleetStrength = escortsWorth();
		for (i = 0; i < NUM_AVAILABLE_RACES; ++i)
		{
			if (i != HUMAN_SHIP && alliance(i) == GOOD_GUY)
				++num_aliens;
		}

		HasMaximum = (num_thrusters == NUM_DRIVE_SLOTS
				&& num_jets == NUM_JET_SLOTS
				&& sis_.CrewEnlisted >= CREW_POD_CAPACITY * 3
				&& sis_.FuelOnBoard >= FUEL_TANK_CAPACITY * 3
				&& sis_.NumLanders >= 3
				&& num_batts >= 4 && num_guns >= 7 && num_defense >= 2);
		if (!HasMaximum && get("CHMMR_BOMB_STATE") < 2)
			say(GOT_OK_FLAGSHIP);
		else
			say(GOT_AWESOME_FLAGSHIP);

		if (get("CHMMR_BOMB_STATE") >= 2)
		{
			say(CHMMR_IMPROVED_BOMB);
			if (FleetStrength < 20000)
				say(MUST_ACQUIRE_AWESOME_FLEET);
			else
			{
				say(GOT_AWESOME_FLEET);
				if (!get("TALKING_PET_ON_SHIP"))
					say(MUST_ELIMINATE_URQUAN_GUARDS);
				else
					say(GO_DESTROY_SAMATRA);
			}
		}
		else if (num_aliens < 2)
			say(GO_ALLY_WITH_ALIENS);
		else
		{
			say(MADE_SOME_ALLIES);
			if (FleetStrength < 6000)
			{
				if (sis_.ResUnits >= 3000)
					say(BUY_COMBAT_SHIPS);
				else
					say(GET_SHIPS_BY_MINING_OR_ALLIANCE);
			}
			else
			{
				say(GOT_OK_FLEET);
				if (!HasMaximum)
				{
					say(IMPROVE_2);
					if (num_thrusters < NUM_DRIVE_SLOTS)
						say(NEED_THRUSTERS_2);
					if (num_jets < NUM_JET_SLOTS)
						say(NEED_TURN_2);
					if (num_guns < 7)
						say(NEED_GUNS_2);
					if (sis_.CrewEnlisted < CREW_POD_CAPACITY * 3)
						say(NEED_CREW_2);
					if (sis_.FuelOnBoard < FUEL_TANK_CAPACITY * 3)
						say(NEED_FUEL_2);
					if (sis_.NumLanders < 3)
						say(NEED_LANDERS_1);
					if (num_batts < 4)
						say(NEED_DYNAMOS_2);
					if (num_defense < 2)
						say(NEED_POINT);
				}
				else if (!get("AWARE_OF_SAMATRA"))
					say(GO_LEARN_ABOUT_URQUAN);
				else
				{
					say(KNOW_ABOUT_SAMATRA);
					if (!get("UTWIG_BOMB"))
						say(FIND_WAY_TO_DESTROY_SAMATRA);
					else if (get("UTWIG_BOMB_ON_SHIP"))
						say(MUST_INCREASE_BOMB_STRENGTH);
				}
			}
		}
	}
}
bool DiscussDevices(bool talking) {
    struct Device { const char *owned; unsigned condition; const char *discussed; int phrase; };
    // Keep the original device enumeration order for report speech.
    static constexpr Device devices[] = {
        {"PORTAL_SPAWNER_ON_SHIP", 0, "DISCUSSED_PORTAL_SPAWNER", ABOUT_PORTAL},
        {"TALKING_PET_ON_SHIP", 0, "DISCUSSED_TALKING_PET", ABOUT_TALKPET},
        {"UTWIG_BOMB_ON_SHIP", 0, "DISCUSSED_UTWIG_BOMB", ABOUT_BOMB},
        {"SUN_DEVICE_ON_SHIP", 0, "DISCUSSED_SUN_EFFICIENCY", ABOUT_SUN},
        {"ROSY_SPHERE_ON_SHIP", 0, "DISCUSSED_ROSY_SPHERE", ABOUT_SPHERE},
        {"AQUA_HELIX_ON_SHIP", 0, "DISCUSSED_AQUA_HELIX", ABOUT_HELIX},
        {"CLEAR_SPINDLE_ON_SHIP", 0, "DISCUSSED_CLEAR_SPINDLE", ABOUT_SPINDLE},
        {"ULTRON_CONDITION", 1, "DISCUSSED_ULTRON", ABOUT_ULTRON_0},
        {"ULTRON_CONDITION", 2, "DISCUSSED_ULTRON", ABOUT_ULTRON_1},
        {"ULTRON_CONDITION", 3, "DISCUSSED_ULTRON", ABOUT_ULTRON_2},
        {"ULTRON_CONDITION", 4, "DISCUSSED_ULTRON", ABOUT_ULTRON_3},
        {"MAIDENS_ON_SHIP", 0, "DISCUSSED_MAIDENS", ABOUT_MAIDENS},
        {"UMGAH_BROADCASTERS_ON_SHIP", 0, "DISCUSSED_UMGAH_HYPERWAVE", ABOUT_UCASTER},
        {"BURV_BROADCASTERS_ON_SHIP", 0, "DISCUSSED_BURVIX_HYPERWAVE", ABOUT_BCASTER},
        {"TAALO_PROTECTOR_ON_SHIP", 0, "DISCUSSED_TAALO_PROTECTOR", ABOUT_SHIELD},
        {"EGG_CASE0_ON_SHIP", 0, "DISCUSSED_EGG_CASING0", ABOUT_EGGCASE_0},
        {"EGG_CASE1_ON_SHIP", 0, "DISCUSSED_EGG_CASING1", ABOUT_EGGCASE_0},
        {"EGG_CASE2_ON_SHIP", 0, "DISCUSSED_EGG_CASING2", ABOUT_EGGCASE_0},
        {"SYREEN_SHUTTLE_ON_SHIP", 0, "DISCUSSED_SYREEN_SHUTTLE", ABOUT_SHUTTLE},
        {"VUX_BEAST_ON_SHIP", 0, "DISCUSSED_VUX_BEAST", ABOUT_VUXBEAST0},
        {"DESTRUCT_CODE_ON_SHIP", 0, "DISCUSSED_DESTRUCT_CODE", ABOUT_DESTRUCT},
        {"PORTAL_KEY_ON_SHIP", 0, "DISCUSSED_URQUAN_WARP", ABOUT_WARPPOD},
        {"ARTIFACT_2_ON_SHIP", 0, "DISCUSSED_ARTIFACT_2", ABOUT_ARTIFACT_2},
        {"ARTIFACT_3_ON_SHIP", 0, "DISCUSSED_ARTIFACT_3", ABOUT_ARTIFACT_3},
    };
    bool found = false;
    if (talking) say(DEVICE_HEAD);
    for (const auto &device : devices) {
        const auto value = get(device.owned);
        if (!(device.condition ? value == device.condition : value != 0) || get(device.discussed)) continue;
        if (talking && found) say(BETWEEN_DEVICES);
        if (talking) say(device.phrase);
        put(device.discussed, talking);
        if (device.phrase == ABOUT_EGGCASE_0) {
            for (auto field : {"DISCUSSED_EGG_CASING0", "DISCUSSED_EGG_CASING1", "DISCUSSED_EGG_CASING2"}) put(field, talking);
        }
        if (talking && device.phrase == ABOUT_VUXBEAST0) {
            plan_.speech.push_back({{"phrase", phrase(ABOUT_VUXBEAST1)}, {"on_start", "starbase.scientist"}});
            restoreTalking_ = true;
        }
        found = true;
    }
    if (talking) plan_.speech.push_back({{"phrase", phrase(DEVICE_TAIL)}, {"on_start", "starbase.commander"}});
    return found;
}
bool CheckTiming(COUNT month_index, COUNT day_index) {
	COUNT mi, year_index;
	BYTE days_in_month[12] =
	{
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	};

	mi = get("STARBASE_MONTH");
    if (mi < 1 || mi > 12) return false;
	year_index = START_YEAR;

	day_index += get("STARBASE_DAY");
	while (day_index > days_in_month[mi - 1])
	{
		day_index -= days_in_month[mi - 1];
		if (++mi > 12)
		{
			mi = 1;
			++year_index;
		}
	}

	month_index += mi;
	year_index += (month_index - 1) / 12;
	month_index = ((month_index - 1) % 12) + 1;

	return (year_index < owner_.clock_.date().year
			|| (year_index == owner_.clock_.date().year
			&& (month_index < owner_.clock_.date().month
			|| (month_index == owner_.clock_.date().month
			&& day_index < owner_.clock_.date().day))));
}
void CheckBulletins(bool repeat) {
    unsigned mask = repeat ? ~plan_.bulletins : get("STARBASE_BULLETS0");
    std::array<bool, 32> eligible{}, obsolete{};
    static constexpr std::pair<unsigned, COUNT> allies[] = {
        {0, SPATHI_SHIP}, {1, ZOQFOTPIK_SHIP}, {2, SUPOX_SHIP}, {3, UTWIG_SHIP},
        {4, ORZ_SHIP}, {12, CHMMR_SHIP}, {13, SHOFIXTI_SHIP}};
    for (auto [index, race] : allies) eligible[index] = alliance(race) == GOOD_GUY;
    obsolete[5] = get("ARILOU_MANNER") == 2;
    eligible[5] = !obsolete[5] && get("PORTAL_SPAWNER") && (repeat || escortRoom(ARILOU_SHIP));
    eligible[6] = get("ZOQFOT_DISTRESS") == 1;
    obsolete[7] = obsolete[8] = get("MET_MELNORME") != 0;
    eligible[7] = !obsolete[7] && CheckTiming(3, 0);
    eligible[8] = !obsolete[8] && CheckTiming(6, 0);
    obsolete[9] = get("FOUND_PLUTO_SPATHI") != 0;
    eligible[9] = !obsolete[9] && CheckTiming(0, 7);
    eligible[10] = get("SPATHI_SHIELDED_SELVES") != 0;
    obsolete[11] = get("ZOQFOT_HOME_VISITS") || get("ZOQFOT_GRPOFFS0");
    eligible[11] = !obsolete[11] && CheckTiming(0, 42);
    eligible[14] = get("PKUNK_MISSION") != 0;
    obsolete[15] = get("DESTRUCT_CODE_ON_SHIP") != 0;
    eligible[15] = !obsolete[15] && CheckTiming(7, 0);
    eligible[17] = get("YEHAT_ABSORBED_PKUNK") != 0;
    eligible[18] = get("CHMMR_BOMB_STATE") == 2;
    eligible[21] = get("ZOQFOT_DISTRESS") == 2;
    const auto sold = get("CREW_SOLD_TO_DRUUGE0") | (get("CREW_SOLD_TO_DRUUGE1") << 8);
    const auto bought = get("CREW_PURCHASED0") | (get("CREW_PURCHASED1") << 8);
    obsolete[26] = sold > 100; eligible[26] = sold && !obsolete[26];
    obsolete[27] = sold > 250; eligible[27] = sold > 100 && !obsolete[27];
    eligible[28] = bought >= CREW_EXPENSE_THRESHOLD; eligible[29] = sold > 250;
    static constexpr int introductions[] = {BEFORE_WE_GO_ON_1, BEFORE_WE_GO_ON_2, BEFORE_WE_GO_ON_3,
        BEFORE_WE_GO_ON_4, BEFORE_WE_GO_ON_5, BEFORE_WE_GO_ON_6, BEFORE_WE_GO_ON_7};
    static constexpr int lines[] = {STARBASE_BULLETIN_1,STARBASE_BULLETIN_2,STARBASE_BULLETIN_3,STARBASE_BULLETIN_4,
        STARBASE_BULLETIN_5,STARBASE_BULLETIN_6,STARBASE_BULLETIN_7,STARBASE_BULLETIN_8,STARBASE_BULLETIN_9,
        STARBASE_BULLETIN_10,STARBASE_BULLETIN_11,STARBASE_BULLETIN_12,STARBASE_BULLETIN_13,STARBASE_BULLETIN_14,
        STARBASE_BULLETIN_15,STARBASE_BULLETIN_16,0,STARBASE_BULLETIN_18,STARBASE_BULLETIN_19,0,0,STARBASE_BULLETIN_22,
        0,0,0,0,STARBASE_BULLETIN_27,STARBASE_BULLETIN_28,STARBASE_BULLETIN_29,STARBASE_BULLETIN_30,0,0};
    bool any = false;
    for (unsigned i = 0; i < 32; ++i) {
        const auto bit = 1u << i;
        if (mask & bit) continue;
        if (obsolete[i]) { mask |= bit; continue; }
        if (!eligible[i]) continue;
        if (any) say(BETWEEN_BULLETINS);
        else if (!repeat) say(introductions[intro_]);
        say(lines[i]);
        any = true;
        plan_.bulletins |= bit;
        if (i == 5 && !repeat) plan_.gift = 3;
    }
    if (!any && get("STARBASE_VISITED")) say(RETURN_HELLO);
    else if (!repeat) put("STARBASE_BULLETS0", mask);
}
void arrival() {
	if (true)
	{
		if (get("MOONBASE_ON_SHIP"))
		{
			say(STARBASE_IS_READY_A);
			if (owner_.voiced_)
				say(YOUR_FLAGSHIP_3DO1);
			else {
				say(YOUR_FLAGSHIP_PC);
				say(GLOBAL_SHIP_NAME);
			}
			say(STARBASE_IS_READY_B);
			if (owner_.voiced_)
				say(YOUR_FLAGSHIP_3DO0);
			else
				say(GLOBAL_SHIP_NAME);
			say(STARBASE_IS_READY_C);
			plan_.ru = 2500;
            put("MOONBASE_ON_SHIP", 0);
			put("STARBASE_MONTH",
					owner_.clock_.date().month);
			put("STARBASE_DAY",
					owner_.clock_.date().day);
		}
		else if (get("STARBASE_VISITED"))
		{
			CheckBulletins (FALSE);
		}
		else
		{
			RESPONSE_REF pStr0 = 0;
			RESPONSE_REF pStr1 = 0;

			static constexpr std::pair<int, int> lines[] = {{NORMAL_HELLO_A0, NORMAL_HELLO_A1}, {NORMAL_HELLO_B0, NORMAL_HELLO_B1}, {NORMAL_HELLO_C0, NORMAL_HELLO_C1}, {NORMAL_HELLO_D0, NORMAL_HELLO_D1}, {NORMAL_HELLO_E0, NORMAL_HELLO_E1}, {NORMAL_HELLO_F0, NORMAL_HELLO_F1}, {NORMAL_HELLO_G0, NORMAL_HELLO_G1}, {NORMAL_HELLO_H0, NORMAL_HELLO_H1}};
            pStr0 = lines[greeting_].first; pStr1 = lines[greeting_].second;
			say(pStr0);
			if (!owner_.voiced_)
			{
				say(SPACE);
				say(GLOBAL_PLAYER_NAME);
			}
			say(pStr1);
			CheckBulletins (FALSE);
		}

		put("STARBASE_VISITED", 1);
	}

}
void departure() {
	//            

	plan_.bulletins |= get("STARBASE_BULLETS0");
	put("STARBASE_BULLETS0", plan_.bulletins);

	                                          
	if (get("CHMMR_BOMB_STATE") >= 2)
		say(GOOD_LUCK_AGAIN);
	else
	{
		RESPONSE_REF pStr0 = 0;
		RESPONSE_REF pStr1 = 0;
		
		static constexpr std::pair<int, int> lines[] = {{NORMAL_GOODBYE_A0, NORMAL_GOODBYE_A1}, {NORMAL_GOODBYE_B0, NORMAL_GOODBYE_B1}, {NORMAL_GOODBYE_C0, NORMAL_GOODBYE_C1}, {NORMAL_GOODBYE_D0, NORMAL_GOODBYE_D1}, {NORMAL_GOODBYE_E0, NORMAL_GOODBYE_E1}, {NORMAL_GOODBYE_F0, NORMAL_GOODBYE_F1}, {NORMAL_GOODBYE_G0, NORMAL_GOODBYE_G1}, {NORMAL_GOODBYE_H0, NORMAL_GOODBYE_H1}};
            pStr0 = lines[greeting_].first; pStr1 = lines[greeting_].second;

		say(pStr0);
		if (!owner_.voiced_)
		{
			say(SPACE);
			say(GLOBAL_PLAYER_NAME);
		}
		say(pStr1);
	}
}
};
StarbaseReports::StarbaseReports(GameSession &session, RandomStream &random, Json phrases, bool voiced)
    : session_(session), random_(random), player_(session.player()), clock_(session.clock()), phrases_(std::move(phrases)), voiced_(voiced) {}
StarbaseReports::Plan StarbaseReports::build(const std::string &name, unsigned greeting, unsigned intro) const {
    Builder builder{*this, greeting, intro};
    if (name == "hayes.devices") builder.DiscussDevices(true);
    else if (name == "hayes.flagship_analysis") builder.AnalyzeCondition();
    else if (name == "hayes.arrival") builder.arrival();
    else if (name == "hayes.departure") builder.departure();
    else throw std::invalid_argument("unknown starbase report: " + name);
    return std::move(builder.plan_);
}
bool StarbaseReports::available(const std::string &name) const {
    if (name == "hayes.repeat_bulletins") return bulletins_ != 0;
    if (name == "hayes.devices") {
        Builder scan{*this, 0, 0};
        return scan.DiscussDevices(false); // Proposed flags remain in the local scan.
    }
    throw std::invalid_argument("unknown report query: " + name);
}
Result StarbaseReports::prepare(const std::string &name, const Json &parameters) {
    if (name == "hayes.commit") {
        const auto token = parameters.at("token").get<unsigned>();
        if (token != serial_) return {"stale", {}};
        if (!pending_) return {}; // Playback acknowledgements are idempotent.
        auto &plan = *pending_;
        for (const auto &[key, value] : plan.writes)
            if (value > player_.story().maximumNamed(key)) throw std::out_of_range("invalid prepared report write");
        // Validate exact payment capacity before publishing any other report effect.
        if (plan.ru) {
            const auto result = player_.inventory().transact({{}, {{"ru", plan.ru}}});
            if (result.status != InventoryStatus::Completed) return {"no_room", {}};
        }
        if (plan.gift) {
            const auto &fleets = session_.raceFleets();
            if (ARILOU_SHIP < fleets.size()) {
                const auto &definition = fleets.at(ARILOU_SHIP);
                // Existing up-to-three gift; the caller's stream supplies captain names.
                for (unsigned i = 0; i < plan.gift; ++i)
                    if (!player_.fleet().create(ARILOU_SHIP, definition, random_)) break;
            }
            // A played grant is durable even if the visit exits before farewell;
            // replaying the report must never recruit the gift a second time.

        }
        for (const auto &[key, value] : plan.writes) player_.story().setNamed(key, value);
        if (plan.gift) player_.story().setNamed("STARBASE_BULLETS0", player_.story().readNamed("STARBASE_BULLETS0") | (1u << 5));
        bulletins_ = plan.bulletins;
        pending_.reset();
        return {};
    }
    if (!parameters.empty()) throw std::invalid_argument("unexpected report arguments");
    if (name == "hayes.repeat_bulletins") return {"ok", {{"speech", bulletinSpeech_}, {"token", serial_}}};
    const auto greeting = name == "hayes.arrival" || name == "hayes.departure" ? random_() & 7 : 0;
    const auto introduction = name == "hayes.arrival" ? static_cast<BYTE>(random_()) % 7 : 0;
    auto plan = build(name, greeting, introduction);
    Result result{"ok", {{"speech", plan.speech}, {"token", serial_ + 1}, {"commit", {{"token", serial_ + 1}}}}};
    if (name == "hayes.arrival") {
        // Store only the bulletin body for replay, not grants or greeting effects.
        Builder replay{*this, 0, 0};
        replay.plan_.bulletins = plan.bulletins;
        replay.CheckBulletins(true);
        bulletinSpeech_ = std::move(replay.plan_.speech);
    }
    ++serial_;
    pending_ = std::move(plan);
    return result;
}
Result StarbaseReports::sellMinerals() {
    static constexpr const char *keys[] = {"common","corrosive","base_metal","noble","rare_earth","precious","radioactive","exotic"};
    auto &inventory = player_.inventory();
    InventoryTransaction sale;
    std::int64_t total = 0;
    for (unsigned i = 0; i < std::size(keys); ++i) {
        const auto key = "minerals." + std::string(keys[i]);
        const auto count = inventory.quantity(key);
        if (count) sale.give[key] = count;
        total += count * player_.gameData().prices.minerals[i];
    }
    if (sale.give.empty()) return {"unavailable", {}};
    if (total) sale.receive["ru"] = total;
    const std::string weight = total < 1000 ? "LIGHT" : total < 2500 ? "MEDIUM" : "HEAVY";
    const auto field = weight + "_MINERAL_LOAD";
    const auto index = std::min(6u, player_.story().readNamed(field));
    const auto prefix = weight + "_LOAD_" + char('A' + index);
    Json speech = Json::array({prefix + "0"});
    if (!voiced_) { speech.push_back("SPACE"); speech.push_back({{"insert", player_.ship().commanderName}}); }
    speech.push_back(prefix + "1");
    Result result{"ok", {{"speech", std::move(speech)}, {"amount", total}}};
    if (inventory.transact(sale).status != InventoryStatus::Completed) return {"no_room", {}};
    player_.story().setNamed(field, std::min(6u, index + 1));
    return result;
}
}
