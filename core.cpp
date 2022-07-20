
#include "core.h"
#include "decr.h"

#include <unordered_set>
#include <algorithm>
#include <sstream>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetAmmoConfig implementation

namespace BpkGen {
	void GetAmmoConfig(const Io::ConfigData& config_data, AmmoConfig& config) {
		class ConfigDataEntryFieldNameMatchesPred {
			const std::string m_name;
		public:
			explicit ConfigDataEntryFieldNameMatchesPred(const std::string& name)
				: m_name(name)
			{}

			bool operator()(const Io::ConfigDataEntry::Field& field) const {
				return m_name == field.name;
			}

			const std::string& Name() const {
				return m_name;
			}
		};

		config = AmmoConfig();

		//
		// process BackpackItemName setting
		//

		const auto backpack_item_name_it = config_data.find("BackpackItemName");
		if(backpack_item_name_it != config_data.end()) {
			const Io::ConfigDataEntryArray& entries = backpack_item_name_it->second;
			if(    entries.size() != 1
				|| !entries[0].name.empty()
				|| entries[0].fields.size() != 1
				|| entries[0].fields[0].field_type != Io::ConfigDataEntry::Field::enType_String
				|| entries[0].fields[0].value_string.empty())
			{
				throw Exception("bad ammo config - bad or empty value of 'BackpackItemName' setting");
			}
			config.backpack_item_name = entries[0].fields[0].value_string;

		} else {
			config.backpack_item_name = "Backpack";
		}

		//
		// process ammo types data
		//

		const auto ammo_it = config_data.find("Ammo");
		if(ammo_it != config_data.end()) {
			const Io::ConfigDataEntryArray& ammo = ammo_it->second;
			const ConfigDataEntryFieldNameMatchesPred max_prop_name_pred("max");
			int ammo_index_in_backpack_data = 0;
			for(int aix=0; aix<ammo.size(); aix++) {
				if(ammo[aix].name.empty()) {
					throw Exception("bad ammo config - ammo name is empty");
				}

				if(config.ammo.find(ammo[aix].name) != config.ammo.end()) {
					throw Exception("bad ammo config - duplicated ammo definition '" + ammo[aix].name + "'");
				}

				AmmoConfig::AmmoInfo& ammo_type_cfg = config.ammo[ammo[aix].name];
				ammo_type_cfg.name = ammo[aix].name;
				const auto max_it = std::find_if(ammo[aix].fields.begin(), ammo[aix].fields.end(), max_prop_name_pred);
				if(max_it == ammo[aix].fields.end() || max_it->field_type != Io::ConfigDataEntry::Field::enType_Int || max_it->value_int <= 0) {
					throw Exception("bad ammo config - bad or missing value of 'max' property for ammo '" + ammo[aix].name + "'");
				}
				ammo_type_cfg.default_max_amount = max_it->value_int;
				// default value for 'max_amount' is the value of 'default_max_amount'
				ammo_type_cfg.max_amount = max_it->value_int;
				ammo_type_cfg.index_in_backpack_data = ammo_index_in_backpack_data++;

				config.ammo_names.push_back(ammo[aix].name);
			}
		}

		//
		// process backpacks data
		//

		const auto backpacks_it = config_data.find("Backpacks");
		if(backpacks_it != config_data.end()) {
			const Io::ConfigDataEntryArray& backpacks = backpacks_it->second;
			const ConfigDataEntryFieldNameMatchesPred maxf_prop_name_pred("maxf");
			const ConfigDataEntryFieldNameMatchesPred give_bpk_item_prop_name_pred("givebpkitem");
			StringArray backpack_with_ammo_max_amounts_names;
			for(int bix=0; bix<backpacks.size(); bix++) {
				if(backpacks[bix].name.empty()) {
					throw Exception("bad ammo config - backpack name is empty");
				}

				if(config.backpacks.find(backpacks[bix].name) != config.backpacks.end()) {
					throw Exception("bad ammo config - duplicated backpack definition '" + backpacks[bix].name + "'");
				}

				AmmoConfig::BackpackInfo& backpack_cfg = config.backpacks[backpacks[bix].name];
				backpack_cfg.name = backpacks[bix].name;

				int ammo_max_factor = 0;
				bool have_ammo_max_factor = false;
				const auto ammo_max_factor_it = std::find_if(backpacks[bix].fields.begin(), backpacks[bix].fields.end(), maxf_prop_name_pred);
				if(ammo_max_factor_it != backpacks[bix].fields.end()) {
					// ammo max factor should be greater than or equal to 1
					if(ammo_max_factor_it->field_type != Io::ConfigDataEntry::Field::enType_Int || ammo_max_factor_it->value_int < 1) {
						throw Exception("bad ammo config - bad value of 'maxf' property for backpack '" + backpacks[bix].name + "'");
					}

					have_ammo_max_factor = true;
					ammo_max_factor = ammo_max_factor_it->value_int;
				}

				bool have_ammo_max_amount_props = false;
				for(auto ammo_it = config.ammo.begin(); ammo_it != config.ammo.end(); ammo_it++) {
					// 'max_amount' for the ammo type is set according to backpack properties
					AmmoConfig::AmmoInfo& ammo_type_cfg = ammo_it->second;
					const ConfigDataEntryFieldNameMatchesPred max_prop_name_pred(ammo_type_cfg.name + "Max");
					bool have_max_prop = false;
					const auto max_prop_it = std::find_if(backpacks[bix].fields.begin(), backpacks[bix].fields.end(), max_prop_name_pred);
					if(max_prop_it != backpacks[bix].fields.end()) {
						if(max_prop_it->field_type != Io::ConfigDataEntry::Field::enType_Int || max_prop_it->value_int <= 0) {
							throw Exception("bad ammo config - bad value of '" + max_prop_name_pred.Name() + "' property for backpack '" + backpacks[bix].name + "'");
						}
						if(have_ammo_max_factor) {
							throw Exception("bad ammo config - 'maxf' property is incompatible with '" + max_prop_name_pred.Name() + "' property for backpack '" + backpacks[bix].name + "'");
						}
						if(max_prop_it->value_int < ammo_type_cfg.default_max_amount) {
							throw Exception("bad ammo config - a backpack cannot decrease ammo limits, '" + ammo_type_cfg.name + "' ammo limit is decreased by backpack '" + backpacks[bix].name + "'");
						}

						if(!have_ammo_max_amount_props && ammo_it != config.ammo.begin()) {
							throw Exception("bad ammo config - either all ammo limits or no ammo limits should be set for backpack '" + backpacks[bix].name + "'");
						}

						have_max_prop = true;
						have_ammo_max_amount_props = true;

					} else {
						// no such field - may be ok, if no other such fields are presented (they must be presented or not presented together)
						if(have_ammo_max_amount_props) {
							throw Exception("bad ammo config - either all ammo limits or no ammo limits should be set for backpack '" + backpacks[bix].name + "'");
						}
					}

					const ConfigDataEntryFieldNameMatchesPred give_prop_name_pred(ammo_type_cfg.name + "Give");
					bool have_give_prop = false;
					const auto give_prop_it = std::find_if(backpacks[bix].fields.begin(), backpacks[bix].fields.end(), give_prop_name_pred);
					if(give_prop_it != backpacks[bix].fields.end()) {
						if(give_prop_it->field_type != Io::ConfigDataEntry::Field::enType_Int || give_prop_it->value_int < 0) {
							throw Exception("bad ammo config - bad value of '" + give_prop_name_pred.Name() + "' property for backpack '" + backpacks[bix].name + "'");
						}
						have_give_prop = true;
					}

					if(have_give_prop || have_max_prop || have_ammo_max_factor) {
						if(backpack_cfg.ammo_info.empty()) {
							backpack_cfg.ammo_info.resize(config.ammo.size());
						}
				
						if(    have_ammo_max_factor
							&& ammo_type_cfg.default_max_amount > 100000000.0 / ammo_max_factor)
						{
							throw Exception("bad ammo config - too big maximum amount for ammo type '" + ammo_type_cfg.name + "' with backpack '" + backpacks[bix].name + "'");
						}

						const int ammo_backpack_max_amount =
							have_max_prop
								? max_prop_it->value_int
								: (have_ammo_max_factor
									? (ammo_max_factor * ammo_type_cfg.default_max_amount)
									: -1);
						const int backpack_cfg_ammo_index = ammo_type_cfg.index_in_backpack_data;
						if(ammo_backpack_max_amount >= 0) {
							backpack_cfg.ammo_info.at(backpack_cfg_ammo_index).max_amount = ammo_backpack_max_amount;
							if(ammo_backpack_max_amount > ammo_type_cfg.max_amount) {
								ammo_type_cfg.max_amount = ammo_backpack_max_amount;
							}

						} else {
							// no max amount in this case - backpack will never change ammo limits for the player
						}

						if(have_give_prop) {
							backpack_cfg.ammo_info.at(backpack_cfg_ammo_index).give_amount = give_prop_it->value_int;
						}
					}
				}

				if(have_ammo_max_amount_props || have_ammo_max_factor) {
					// need to add backpack markers to player's inventory;
					// ammo max amount must be set for all ammo types in this backpack;
					backpack_with_ammo_max_amounts_names.push_back(backpacks[bix].name);
				}

				const auto give_bpk_item_it = std::find_if(backpacks[bix].fields.begin(), backpacks[bix].fields.end(), give_bpk_item_prop_name_pred);
				if(give_bpk_item_it == backpacks[bix].fields.end()) {
					// ok - it is optional

				} else if(give_bpk_item_it->field_type != Io::ConfigDataEntry::Field::enType_Int) {
					throw Exception("bad ammo config - bad value of 'givebpkitem' property for backpack '" + backpacks[bix].name + "'");

				} else if(give_bpk_item_it->value_int == 0 && (have_ammo_max_amount_props || have_ammo_max_factor)) {
					throw Exception("bad ammo config - property 'givebpkitem' cannot have value 'false' if ammo limits are set for backpack '" + backpacks[bix].name + "'");

				} else {
					backpack_cfg.give_backpack_item = (give_bpk_item_it->value_int != 0);
				}
			}

			// count number of markers needed for each backpack, that may increase max ammo number;
			// more markers are given for backpacks with greater ammo limits;
			// a backpack may not decrease ammo limits
			class BackpackSorter {
				const AmmoConfig::BackpackInfoMap& m_backpacks_cfg;
				const AmmoConfig::AmmoInfoMap& m_ammo_cfg;

			public:
				BackpackSorter(const AmmoConfig::BackpackInfoMap& backpacks_cfg,
							   const AmmoConfig::AmmoInfoMap& ammo_cfg)
					: m_backpacks_cfg(backpacks_cfg)
					, m_ammo_cfg(ammo_cfg)
				{}

				bool operator() (const std::string& backpack1_name, const std::string& backpack2_name) const {
					const AmmoConfig::BackpackInfo& backpack1_info = m_backpacks_cfg.at(backpack1_name);
					const AmmoConfig::BackpackInfo& backpack2_info = m_backpacks_cfg.at(backpack2_name);
					for(auto ammo_it = m_ammo_cfg.begin(); ammo_it != m_ammo_cfg.end(); ammo_it++) {
						const int& ammo_index_in_backpack_data = ammo_it->second.index_in_backpack_data;
						const int& ammo_max_amount1 = backpack1_info.ammo_info.at(ammo_index_in_backpack_data).max_amount;
						const int& ammo_max_amount2 = backpack2_info.ammo_info.at(ammo_index_in_backpack_data).max_amount;
						if(ammo_max_amount1 != ammo_max_amount2) {
							return ammo_max_amount1 < ammo_max_amount2;
						}
						// check max amounts for the next ammo type (if have any)
					}
				
					return false;
				}
			} backpack_sorter(config.backpacks, config.ammo);
			std::sort(backpack_with_ammo_max_amounts_names.begin(), backpack_with_ammo_max_amounts_names.end(), backpack_sorter);

			// verify, that backpacks does not decrease ammo limits;
			// set marker count for all backpacks
			int backpack_marker_count = 1;
			for(int bnix=0; bnix<backpack_with_ammo_max_amounts_names.size(); bnix++) {
				AmmoConfig::BackpackInfo& cur_backpack_info = config.backpacks.at(backpack_with_ammo_max_amounts_names[bnix]);
				bool next_backpack_increases_ammo_limits = false;
				if(bnix+1 < backpack_with_ammo_max_amounts_names.size()) {
					const AmmoConfig::BackpackInfo& next_backpack_info = config.backpacks.at(backpack_with_ammo_max_amounts_names[bnix+1]);
					for(auto ammo_it = config.ammo.begin(); ammo_it != config.ammo.end(); ammo_it++) {
						const int& ammo_index_in_backpack_data = ammo_it->second.index_in_backpack_data;
						const int& cur_backpack_ammo_max_amount = cur_backpack_info.ammo_info.at(ammo_index_in_backpack_data).max_amount;
						const int& next_backpack_ammo_max_amount = next_backpack_info.ammo_info.at(ammo_index_in_backpack_data).max_amount;
						if(next_backpack_ammo_max_amount < cur_backpack_ammo_max_amount) {
							throw Exception("bad ammo config - a backpack cannot decrease ammo limits, bad backpack pair is '" + cur_backpack_info.name + "' with '" + next_backpack_info.name + "'");
						}

						if(next_backpack_ammo_max_amount > cur_backpack_ammo_max_amount) {
							next_backpack_increases_ammo_limits = true;
						}
					}
				}

				cur_backpack_info.backpack_marker_count = backpack_marker_count;
				if(next_backpack_increases_ammo_limits) {
					backpack_marker_count++;
				}
			}
		}

		//
		// process ammo packs data
		//

		const auto ammo_packs_it = config_data.find("AmmoPacks");
		if(ammo_packs_it != config_data.end()) {
			const Io::ConfigDataEntryArray& ammo_packs = ammo_packs_it->second;
			const ConfigDataEntryFieldNameMatchesPred at_prop_name_pred("at");
			const ConfigDataEntryFieldNameMatchesPred amount_prop_name_pred("amount");
			for(int apix=0; apix<ammo_packs.size(); apix++) {
				if(ammo_packs[apix].name.empty()) {
					throw Exception("bad ammo config - ammo pack name is empty");
				}

				if(config.ammo_packs.find(ammo_packs[apix].name) != config.ammo_packs.end()) {
					throw Exception("bad ammo config - duplicated ammo pack definition '" + ammo_packs[apix].name + "'");
				}

				const auto ammo_type_prop_it = std::find_if(ammo_packs[apix].fields.begin(), ammo_packs[apix].fields.end(), at_prop_name_pred);
				if(    ammo_type_prop_it == ammo_packs[apix].fields.end()
					|| ammo_type_prop_it->field_type != Io::ConfigDataEntry::Field::enType_String
					|| ammo_type_prop_it->value_string.empty())
				{
					throw Exception("bad ammo config - bad or missing value of 'at' property for ammo pack '" + ammo_packs[apix].name + "'");
				}

				const std::string& ammo_type = ammo_type_prop_it->value_string;
				if(config.ammo.find(ammo_type) == config.ammo.end()) {
					throw Exception("bad ammo config - unknown ammo type '" + ammo_type + "' set for ammo pack '" + ammo_packs[apix].name + "'");
				}

				// ammo amount to give should be greater than or equal to 1
				auto amount_prop_it = std::find_if(ammo_packs[apix].fields.begin(), ammo_packs[apix].fields.end(), amount_prop_name_pred);
				if(    amount_prop_it == ammo_packs[apix].fields.end()
					|| amount_prop_it->field_type != Io::ConfigDataEntry::Field::enType_Int
					|| amount_prop_it->value_int < 1)
				{
					throw Exception("bad ammo config - bad or missing value of 'amount' property for ammo pack '" + ammo_packs[apix].name + "'");
				}

				const int amount = amount_prop_it->value_int;

				AmmoConfig::AmmoPackInfo& ammo_pack_cfg = config.ammo_packs[ammo_packs[apix].name];
				ammo_pack_cfg.name = ammo_packs[apix].name;
				ammo_pack_cfg.ammo_type = ammo_type;
				ammo_pack_cfg.ammo_give_amount = amount;
			}
		}

		//
		// process weapons data
		//

		const auto weapons_it = config_data.find("Weapons");
		if(weapons_it != config_data.end()) {
			const Io::ConfigDataEntryArray& weapons = weapons_it->second;
			const ConfigDataEntryFieldNameMatchesPred at1_prop_name_pred("at1");
			const ConfigDataEntryFieldNameMatchesPred at_prop_name_pred("at");
			const ConfigDataEntryFieldNameMatchesPred at2_prop_name_pred("at2");
			for(int wix=0; wix<weapons.size(); wix++) {
				if(weapons[wix].name.empty()) {
					throw Exception("bad ammo config - weapon name is empty");
				}

				if(config.weapons.find(weapons[wix].name) != config.weapons.end()) {
					throw Exception("bad ammo config - duplicated weapon definition '" + weapons[wix].name + "'");
				}

				AmmoConfig::WeaponInfo& weapon_cfg = config.weapons[weapons[wix].name];
				weapon_cfg.name = weapons[wix].name;

				auto at1_prop_it = std::find_if(weapons[wix].fields.begin(), weapons[wix].fields.end(), at1_prop_name_pred);
				std::string at1_prop_name = at1_prop_name_pred.Name();
				if(at1_prop_it == weapons[wix].fields.end()) {
					at1_prop_it = std::find_if(weapons[wix].fields.begin(), weapons[wix].fields.end(), at_prop_name_pred);
					at1_prop_name = at_prop_name_pred.Name();
				}

				if(at1_prop_it == weapons[wix].fields.end()) {
					// ok - it is optional

				} else if(at1_prop_it->field_type != Io::ConfigDataEntry::Field::enType_String || at1_prop_it->value_string.empty()) {
					throw Exception("bad ammo config - bad or empty value of '" + at1_prop_name + "' property for weapon '" + weapons[wix].name + "'");

				} else if(config.ammo.find(at1_prop_it->value_string) == config.ammo.end()) {
					throw Exception("bad ammo config - unknown ammo type '" + at1_prop_it->value_string + "' for weapon '" + weapons[wix].name + "'");

				} else {
					weapon_cfg.ammo_type1 = at1_prop_it->value_string;
				}

				const auto at2_prop_it = std::find_if(weapons[wix].fields.begin(), weapons[wix].fields.end(), at2_prop_name_pred);
				if(at2_prop_it == weapons[wix].fields.end()) {
					// ok - it is optional

				} else if(at2_prop_it->field_type != Io::ConfigDataEntry::Field::enType_String || at2_prop_it->value_string.empty()) {
					throw Exception("bad ammo config - bad or empty value of '" + at2_prop_name_pred.Name() + "' property for weapon '" + weapons[wix].name + "'");

				} else if(config.ammo.find(at2_prop_it->value_string) == config.ammo.end()) {
					throw Exception("bad ammo config - unknown ammo type '" + at2_prop_it->value_string + "' for weapon '" + weapons[wix].name + "'");

				} else {
					weapon_cfg.ammo_type2 = at2_prop_it->value_string;
				}
			}
		}

		//
		// process weapon things data
		//

		const auto weapon_things_it = config_data.find("WeaponThings");
		if(weapon_things_it != config_data.end()) {
			const Io::ConfigDataEntryArray& weapon_things = weapon_things_it->second;

			const ConfigDataEntryFieldNameMatchesPred wpname_prop_name_pred("wpname");
			const ConfigDataEntryFieldNameMatchesPred ag1_prop_name_pred("ag1");
			const ConfigDataEntryFieldNameMatchesPred ag_prop_name_pred("ag");
			const ConfigDataEntryFieldNameMatchesPred ag2_prop_name_pred("ag2");

			for(int wthix=0; wthix<weapon_things.size(); wthix++) {
				if(weapon_things[wthix].name.empty()) {
					throw Exception("bad ammo config - weapon thing name is empty");
				}

				if(config.weapon_things.find(weapon_things[wthix].name) != config.weapon_things.end()) {
					throw Exception("bad ammo config - duplicated weapon thing definition '" + weapon_things[wthix].name + "'");
				}

				AmmoConfig::WeaponThingInfo& weapon_thing_cfg = config.weapon_things[weapon_things[wthix].name];
				weapon_thing_cfg.name = weapon_things[wthix].name;

				const auto weapon_name_prop_it = std::find_if(weapon_things[wthix].fields.begin(), weapon_things[wthix].fields.end(), wpname_prop_name_pred);
				if(weapon_name_prop_it == weapon_things[wthix].fields.end()) {
					throw Exception("bad ammo config - missing 'wpname' property for weapon thing " + weapon_things[wthix].name + "'");

				} else if(weapon_name_prop_it->field_type != Io::ConfigDataEntry::Field::enType_String || weapon_name_prop_it->value_string.empty()) {
					throw Exception("bad ammo config - bad or empty value of 'wpname' property for weapon thing '" + weapon_things[wthix].name + "'");

				} else if(config.weapons.find(weapon_name_prop_it->value_string) == config.weapons.end()) {
					throw Exception("bad ammo config - unknown weapon type '" + weapon_name_prop_it->value_string + "' for weapon thing '" + weapon_things[wthix].name + "'");
				}

				weapon_thing_cfg.weapon_name = weapon_name_prop_it->value_string;
				const AmmoConfig::WeaponInfo& weapon_cfg = config.weapons.at(weapon_thing_cfg.weapon_name);

				auto ag1_prop_it = std::find_if(weapon_things[wthix].fields.begin(), weapon_things[wthix].fields.end(), ag1_prop_name_pred);
				std::string ag1_prop_name = ag1_prop_name_pred.Name();
				if(ag1_prop_it == weapon_things[wthix].fields.end()) {
					ag1_prop_it = std::find_if(weapon_things[wthix].fields.begin(), weapon_things[wthix].fields.end(), ag_prop_name_pred);
					ag1_prop_name = ag_prop_name_pred.Name();
				}

				if(ag1_prop_it == weapon_things[wthix].fields.end()) {
					// ok - it is optional

				} else if(ag1_prop_it->field_type != Io::ConfigDataEntry::Field::enType_Int || ag1_prop_it->value_int < 0) {
					throw Exception("bad ammo config - bad value of '" + ag1_prop_name + "' property for weapon thing '" + weapon_things[wthix].name + "'");

				} else if(weapon_cfg.ammo_type1.empty()) {
					throw Exception("bad ammo config - primary ammo give amount is set for weapon thing '" + weapon_things[wthix].name + "' while primary ammo type of weapon '" + weapon_cfg.name + "' is not set");

				} else {
					weapon_thing_cfg.ammo_give1 = ag1_prop_it->value_int;
				}

				const auto ag2_prop_it = std::find_if(weapon_things[wthix].fields.begin(), weapon_things[wthix].fields.end(), ag2_prop_name_pred);
				if(ag2_prop_it == weapon_things[wthix].fields.end()) {
					// ok - it is optional

				} else if(ag2_prop_it->field_type != Io::ConfigDataEntry::Field::enType_Int || ag2_prop_it->value_int < 0) {
					throw Exception("bad ammo config - bad value of '" + ag2_prop_name_pred.Name() + "' property for weapon thing '" + weapon_things[wthix].name + "'");

				} else if(weapon_cfg.ammo_type2.empty()) {
					throw Exception("bad ammo config - secondary ammo give amount is set for weapon thing '" + weapon_things[wthix].name + "' while secondary ammo type of weapon '" + weapon_cfg.name + "' is not set");

				} else {
					weapon_thing_cfg.ammo_give2 = ag2_prop_it->value_int;
				}
			}
		}

		//
		// process names of utility actor classes
		//

		const auto utility_actors_it = config_data.find("UtilityActors");
		if(utility_actors_it != config_data.end()) {
			const Io::ConfigDataEntryArray& utility_actors = utility_actors_it->second;
			for(int uaix=0; uaix<utility_actors.size(); uaix++) {
				const Io::ConfigDataEntry& entry = utility_actors[uaix];

				// for now only names can be changed - require single field with no name
				if(entry.name == "BackpackMarker") {

				} else if(entry.name == "AmmoLimitsChecker") {

				} else if(entry.name == "AmmoLimitNotReachedMarker") {

				} else {
					// unknown utility actor name - ignore
					continue;
				}

				const std::string& utility_class_base_name = entry.name;
				if(    entry.fields.size() != 1
					|| !entry.fields[0].name.empty()
					|| entry.fields[0].field_type != Io::ConfigDataEntry::Field::enType_String
					|| entry.fields[0].value_string.empty())
				{
					throw Exception("bad ammo config - bad or empty name for utility class '" + utility_class_base_name + "'");
				}

				const std::string& utility_class_name = entry.fields[0].value_string;
				config.utility_actor_names[utility_class_base_name] = utility_class_name;
			}
		}

		// set default values for utility actor names
		if(config.utility_actor_names.find("BackpackMarker") == config.utility_actor_names.end()) {
			config.utility_actor_names["BackpackMarker"] = "BackpackMarker";
		}
		if(config.utility_actor_names.find("AmmoLimitsChecker") == config.utility_actor_names.end()) {
			config.utility_actor_names["AmmoLimitsChecker"] = "AmmoLimitsChecker";
		}
		if(config.utility_actor_names.find("AmmoLimitNotReachedMarker") == config.utility_actor_names.end()) {
			config.utility_actor_names["AmmoLimitNotReachedMarker"] = "AmmoLimitNotReachedMarker";
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WriteDecorateLumpToStream implementation

namespace BpkGen {
	enum EnActorDefinitionPart {
		enActorDefinitionPart_None = -1,
		enActorDefinitionPart_Properties = 0,
		enActorDefinitionPart_States,

		EnActorDefinitionPart_Size
	};

	static void WriteAmmoDefinitionToStream(const AmmoConfig& ammo_config,
											const std::string& ammo_name,
											EnActorDefinitionPart part,
											Io::OutStream& out_stream);

	static void WriteWeaponDefinitionToStream(const AmmoConfig& ammo_config,
											  const std::string& weapon_name,
											  EnActorDefinitionPart part,
											  Io::OutStream& out_stream);

	static void WriteBackpackDefinitionToStream(const AmmoConfig& ammo_config,
												const std::string& backpack_name,
												EnActorDefinitionPart part,
												Io::OutStream& out_stream);

	static void WriteAmmoPackThingDefinitionToStream(const AmmoConfig& ammo_config,
													 const std::string& ammo_pack_name,
													 EnActorDefinitionPart part,
													 Io::OutStream& out_stream);

	static void WriteWeaponThingDefinitionToStream(const AmmoConfig& ammo_config,
												   const std::string& weapon_thing_name,
												   EnActorDefinitionPart part,
												   Io::OutStream& out_stream);

	typedef std::unordered_map<std::string, int> AmmoAmountsMap;
	static void WriteAmmoGiverThingDefinitionStatesToStream(const AmmoConfig& ammo_config,
															const AmmoAmountsMap& ammo_give_amounts,
															const std::string& weapon_name,
															Io::OutStream& out_stream);

	static void WriteAmmoLimitsCheckerDefinitionToStream(const AmmoConfig& ammo_config,
														 Io::OutStream& out_stream);

	void WriteDecorateLumpToStream(const AmmoConfig& ammo_config,
								   Io::InStream& decorate_template_stream,
								   Io::OutStream& out_stream,
								   bool ignore_label_errors/* = false*/,
								   Io::OutStream* log_stream/* = NULL*/)
	{
		// declare required labels according to the ammo config;
		// some actors require more states, others require more props
		enum EnActorType {
			enActorType_Ammo = 0,
			enActorType_AmmoPack,
			enActorType_Weapon,
			enActorType_WeaponThing,
			enActorType_Backpack,

			EnActorType_Size
		};
		struct ActorDefPartsPresence {
			enum EnRequirementState {
				// do not require the part
				enRequirementState_Abscent = -1,

				// part is presented (at least once)
				enRequirementState_Fulfilled = 0,

				// require the part
				enRequirementState_Present = 1,
			};
			EnRequirementState part_reqs[EnActorDefinitionPart_Size];

			ActorDefPartsPresence() {
				std::fill(part_reqs, part_reqs + EnActorDefinitionPart_Size, enRequirementState_Abscent);
			}
		};

		// array of maps - a map for each actor type; map: actor_name -> part presence requirements
		std::unordered_map<std::string, ActorDefPartsPresence> required_actor_def_parts[EnActorType_Size];
		for(auto it = ammo_config.ammo.begin(); it != ammo_config.ammo.end(); it++) {
			ActorDefPartsPresence::EnRequirementState& requirement_state =
				required_actor_def_parts[enActorType_Ammo][it->second.name].part_reqs[enActorDefinitionPart_Properties];
			requirement_state = ActorDefPartsPresence::enRequirementState_Present;
		}
		for(auto it = ammo_config.ammo_packs.begin(); it != ammo_config.ammo_packs.end(); it++) {
			ActorDefPartsPresence::EnRequirementState& requirement_state =
				required_actor_def_parts[enActorType_AmmoPack][it->second.name].part_reqs[enActorDefinitionPart_States];
			requirement_state = ActorDefPartsPresence::enRequirementState_Present;
		}
		for(auto it = ammo_config.weapons.begin(); it != ammo_config.weapons.end(); it++) {
			ActorDefPartsPresence::EnRequirementState& requirement_state =
				required_actor_def_parts[enActorType_Weapon][it->second.name].part_reqs[enActorDefinitionPart_Properties];
			requirement_state = ActorDefPartsPresence::enRequirementState_Present;
		}
		for(auto it = ammo_config.weapon_things.begin(); it != ammo_config.weapon_things.end(); it++) {
			ActorDefPartsPresence::EnRequirementState& requirement_state =
				required_actor_def_parts[enActorType_WeaponThing][it->second.name].part_reqs[enActorDefinitionPart_States];
			requirement_state = ActorDefPartsPresence::enRequirementState_Present;
		}
		for(auto it = ammo_config.backpacks.begin(); it != ammo_config.backpacks.end(); it++) {
			ActorDefPartsPresence::EnRequirementState& requirement_state =
				required_actor_def_parts[enActorType_Backpack][it->second.name].part_reqs[enActorDefinitionPart_States];
			requirement_state = ActorDefPartsPresence::enRequirementState_Present;
		}

		// read and pipe the template processing labels on the way
		Io::InStreamWithBuffer decorate_template_stream_with_buffer(decorate_template_stream);
		bool have_ammo_limit_checker_definition = false;
		std::string bpkgen_label_content, indent;
		while(Io::PipeCharsUntilNextBpkGenLabel(decorate_template_stream_with_buffer, out_stream, bpkgen_label_content, indent)) {
			const size_t line_number = decorate_template_stream_with_buffer.GetLineNumber();
			const size_t open_parenthesis_pos = bpkgen_label_content.find("(");
			const bool have_parameters = open_parenthesis_pos < bpkgen_label_content.size();
			const std::string label_title =
				have_parameters
					? bpkgen_label_content.substr(0, open_parenthesis_pos)
					: bpkgen_label_content;

			std::string params;
			if(have_parameters) {
				const size_t first_char_pos = bpkgen_label_content.find_first_not_of(" \t\n\r", open_parenthesis_pos + 1);
				const size_t last_char_pos = bpkgen_label_content.find_last_not_of(" \t\n\r", bpkgen_label_content.size() - 2);
				params = bpkgen_label_content.substr(first_char_pos, (last_char_pos + 1) - first_char_pos);
			}

			EnActorDefinitionPart part = enActorDefinitionPart_None;
			std::string actor_name;
			if(label_title == "AmmoLimitsCheckerDefinition") {
				if(have_parameters) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "DECORATE template", line_number,
											   "must not have any parameters", !ignore_label_errors, log_stream);
					continue;
				}

				if(have_ammo_limit_checker_definition) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_Duplicated, label_title, "DECORATE template", line_number,
											   "", !ignore_label_errors, log_stream);
					// ok - process anyway
				}

				WriteAmmoLimitsCheckerDefinitionToStream(ammo_config, out_stream);
				have_ammo_limit_checker_definition = true;

			} else if(label_title == "AmmoLimitsChecker") {
				if(have_parameters) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "DECORATE template", line_number,
											   "must not have any parameters", !ignore_label_errors, log_stream);
					continue;
				}

				if(!have_ammo_limit_checker_definition) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_Misplaced, label_title, "DECORATE template", line_number,
											   "must be placed after Ammo Limits Checker definition", !ignore_label_errors, log_stream);
					// ok - process anyway
				}
				out_stream << ammo_config.utility_actor_names.at("AmmoLimitsChecker");

			} else if(label_title == "MoreProps") {
				if(!have_parameters || params.empty()) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "DECORATE template", line_number,
											   "must have actor name as a parameter", !ignore_label_errors, log_stream);
					continue;
				}

				part = enActorDefinitionPart_Properties;
				actor_name = params;

			} else if(label_title == "MoreStates") {
				if(!have_parameters || params.empty()) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "DECORATE template", line_number,
											   "must have actor name as a parameter", !ignore_label_errors, log_stream);
					continue;
				}

				part = enActorDefinitionPart_States;
				actor_name = params;

			} else {
				// unknown label
				Io::ReportBpkGenLabelError(Io::enBadLabel_Unknown, label_title, "DECORATE template", line_number,
										   "", !ignore_label_errors, log_stream);
				continue;
			}

			if(part != enActorDefinitionPart_None) {
				// write more props or states for the actor on the place of the label
				ActorDefPartsPresence::EnRequirementState* requirement_state = NULL;
				if(ammo_config.ammo.find(actor_name) != ammo_config.ammo.end()) {
					WriteAmmoDefinitionToStream(ammo_config, actor_name, part, out_stream);
					requirement_state = &required_actor_def_parts[enActorType_Ammo].at(actor_name).part_reqs[part];

				} else if(ammo_config.weapons.find(actor_name) != ammo_config.weapons.end()) {
					WriteWeaponDefinitionToStream(ammo_config, actor_name, part, out_stream);
					requirement_state = &required_actor_def_parts[enActorType_Weapon].at(actor_name).part_reqs[part];

				} else if(ammo_config.backpacks.find(actor_name) != ammo_config.backpacks.end()) {
					WriteBackpackDefinitionToStream(ammo_config, actor_name, part, out_stream);
					requirement_state = &required_actor_def_parts[enActorType_Backpack].at(actor_name).part_reqs[part];

				} else if(ammo_config.ammo_packs.find(actor_name) != ammo_config.ammo_packs.end()) {
					WriteAmmoPackThingDefinitionToStream(ammo_config, actor_name, part, out_stream);
					requirement_state = &required_actor_def_parts[enActorType_AmmoPack].at(actor_name).part_reqs[part];

				} else if(ammo_config.weapon_things.find(actor_name) != ammo_config.weapon_things.end()) {
					WriteWeaponThingDefinitionToStream(ammo_config, actor_name, part, out_stream);
					requirement_state = &required_actor_def_parts[enActorType_WeaponThing].at(actor_name).part_reqs[part];

				} else {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "DECORATE template", line_number,
											   "unknown actor name parameter '" + actor_name + "'", !ignore_label_errors, log_stream);
					continue;
				}

				if(requirement_state) {
					switch(*requirement_state) {
					case ActorDefPartsPresence::enRequirementState_Present:
						// good - found the required actor definition part label
						*requirement_state = ActorDefPartsPresence::enRequirementState_Fulfilled;
						break;

					case ActorDefPartsPresence::enRequirementState_Fulfilled:
						// a duplicate, while this code piece should be unique for the specified actor
						Io::ReportBpkGenLabelError(Io::enBadLabel_Duplicated, bpkgen_label_content, "DECORATE template", line_number,
												   "", !ignore_label_errors, log_stream);
						// leave fulfilled - report a duplicate for all other occurrences too
						break;

					case ActorDefPartsPresence::enRequirementState_Abscent:
						Io::ReportBpkGenLabelError(Io::enBadLabel_Redundant, bpkgen_label_content, "DECORATE template", line_number,
												   "this label is redundant and should be removed", !ignore_label_errors, log_stream);
						// report other occurrences as duplicates
						*requirement_state = ActorDefPartsPresence::enRequirementState_Fulfilled;
						break;

					default:
						throw Exception("internal error - bad requirement state for actor definition part presence");
					}
				}
			}
		}

		// check that all label presence requirements are fulfilled (if they were presented)
		for(size_t atix=0; atix<_countof(required_actor_def_parts); atix++) {
			for(auto it = required_actor_def_parts[atix].begin(); it != required_actor_def_parts[atix].end(); it++) {
				for(size_t pix=0; pix<_countof(it->second.part_reqs); pix++) {
					const ActorDefPartsPresence::EnRequirementState& req_state = it->second.part_reqs[pix];
					if(req_state != ActorDefPartsPresence::enRequirementState_Present) {
						continue;
					}

					std::string label;
					switch(pix) {
					case enActorDefinitionPart_Properties:
						label = "MoreProps(" + it->first + ")";
						break;

					case enActorDefinitionPart_States:
						label = "MoreStates(" + it->first + ")";
						break;

					default:
						throw Exception("internal error - unknown actor definition part while checking required labels");
					}
					Io::ReportBpkGenLabelError(Io::enBadLabel_Missing, label, "DECORATE template", 0,
											   "", !ignore_label_errors, log_stream);
				}
			}
		}
	}

	void WriteAmmoDefinitionToStream(const AmmoConfig& ammo_config,
									 const std::string& ammo_name,
									 EnActorDefinitionPart part,
									 Io::OutStream& out_stream)
	{
		const AmmoConfig::AmmoInfo& ammo_type_cfg = ammo_config.ammo.at(ammo_name);
		switch(part) {
		case enActorDefinitionPart_Properties:
			DECR_START(out_stream)
				DECR_ACTOR_PROP_NUM("Inventory.Amount", 1)
				DECR_ACTOR_PROP_NUM("Inventory.MaxAmount", ammo_type_cfg.default_max_amount)
				DECR_ACTOR_PROP_NUM("Ammo.BackpackAmount", 0)
				DECR_DISABLE_TRAILING_ENDL()
				DECR_ACTOR_PROP_NUM("Ammo.BackpackMaxAmount", ammo_type_cfg.max_amount)
			DECR_END()
			break;

		case enActorDefinitionPart_States:
			// no states here
			break;

		case enActorDefinitionPart_None:
		default:
			throw Exception("unexpected error - cannot write ammo actor '" + ammo_name + "' part to stream, unknown part type " + std::to_string(part));
		}
	}

	void WriteWeaponDefinitionToStream(const AmmoConfig& ammo_config,
									   const std::string& weapon_name,
									   EnActorDefinitionPart part,
									   Io::OutStream& out_stream)
	{
		const AmmoConfig::WeaponInfo& weapon_cfg = ammo_config.weapons.at(weapon_name);
		const bool have_ammo_type1 = !weapon_cfg.ammo_type1.empty();
		const bool have_ammo_type2 = !weapon_cfg.ammo_type2.empty();
		switch(part) {
		case enActorDefinitionPart_Properties:
			DECR_START(out_stream)
				// set ammo type, ammo itself will be given by the thing actor
				if(have_ammo_type2) {
					if(have_ammo_type1) {
						DECR_ACTOR_PROP_STR("Weapon.AmmoType1", weapon_cfg.ammo_type1)
						DECR_ACTOR_PROP_NUM("Weapon.AmmoGive1", 0)
					}
					DECR_ACTOR_PROP_STR("Weapon.AmmoType2", weapon_cfg.ammo_type2)
					DECR_DISABLE_TRAILING_ENDL()
					DECR_ACTOR_PROP_NUM("Weapon.AmmoGive2", 0)

				} else if(have_ammo_type1) {
					DECR_ACTOR_PROP_STR("Weapon.AmmoType", weapon_cfg.ammo_type1)
					DECR_DISABLE_TRAILING_ENDL()
					DECR_ACTOR_PROP_NUM("Weapon.AmmoGive", 0)

				} else {
					// nothing
				}
			DECR_END()
			break;

		case enActorDefinitionPart_States:
			// no states here
			break;

		case enActorDefinitionPart_None:
		default:
			throw Exception("unexpected error - cannot write weapon actor '" + weapon_name + "' part to stream, unknown part type " + std::to_string(part));
		}
	}

	void WriteBackpackDefinitionToStream(const AmmoConfig& ammo_config,
										 const std::string& backpack_name,
										 EnActorDefinitionPart part,
										 Io::OutStream& out_stream)
	{
		const AmmoConfig::BackpackInfo& backpack_cfg = ammo_config.backpacks.at(backpack_name);
		switch(part) {
		case enActorDefinitionPart_Properties:
			// no props here
			break;

		case enActorDefinitionPart_States:
			DECR_START(out_stream)
				DECR_ACTOR_STATE_LABEL("Pickup")
				bool have_some_states = false;
				if(backpack_cfg.give_backpack_item) {
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + ammo_config.backpack_item_name + "\", 1)")
					have_some_states = true;
				}

				bool gave_some_ammo = false;
				if(backpack_cfg.ammo_info.size() > 0) {
					for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
						const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
						const AmmoConfig::BackpackInfo::BackpackAmmoInfo& backpack_ammo_cfg = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data);
						if(backpack_ammo_cfg.give_amount > 0) {
							const std::string amount_str = std::to_string(backpack_ammo_cfg.give_amount);
							DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + ammo_cfg.name + "\", " + amount_str + ")")
							have_some_states = true;
							gave_some_ammo = true;
						}
					}
				}

				if(backpack_cfg.backpack_marker_count > 0) {
					const std::string& backpack_marker_actor_name = ammo_config.utility_actor_names.at("BackpackMarker");
					const std::string amount_str = std::to_string(backpack_cfg.backpack_marker_count);
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + backpack_marker_actor_name + "\", " + amount_str + ", 3)")
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_TakeInventory(\"" + backpack_marker_actor_name + "\")")
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + backpack_marker_actor_name + "\", " + amount_str + ")")
					DECR_ACTOR_STATE("TNT1", "A", 0, "")
					have_some_states = true;
				}

				if(!have_some_states) {
					DECR_ACTOR_STATE("TNT1", "A", 0, "")
				}

				DECR_DISABLE_TRAILING_ENDL()
				if(gave_some_ammo) {
					// received some ammo - check that current amounts does not exceed limits
					// and fix amounts if they exceed limits
					DECR_ACTOR_STATE_GOTO("Super::FixUpAmmoStart")

				} else {
					// nothing else to do
					DECR_ACTOR_STATE_ENDSEQ("Stop")
				}

			DECR_END()
			break;

		case enActorDefinitionPart_None:
		default:
			throw Exception("unexpected error - cannot write backpack actor '" + backpack_name + "' part to stream, unknown part type " + std::to_string(part));
		}
	}

	void WriteAmmoPackThingDefinitionToStream(const AmmoConfig& ammo_config,
											  const std::string& ammo_pack_name,
											  EnActorDefinitionPart part,
											  Io::OutStream& out_stream)
	{
		switch(part) {
		case enActorDefinitionPart_Properties:
			// no props here
			break;

		case enActorDefinitionPart_States:
			{
				const AmmoConfig::AmmoPackInfo& ammo_pack_cfg = ammo_config.ammo_packs.at(ammo_pack_name);

				AmmoAmountsMap ammo_give_amounts;
				if(!ammo_pack_cfg.ammo_type.empty() && ammo_pack_cfg.ammo_give_amount > 0) {
					ammo_give_amounts[ammo_pack_cfg.ammo_type] = ammo_pack_cfg.ammo_give_amount;
				}
				WriteAmmoGiverThingDefinitionStatesToStream(ammo_config, ammo_give_amounts, "", out_stream);
			}
			break;

		case enActorDefinitionPart_None:
		default:
			throw Exception("unexpected error - cannot write ammo pack thing actor '" + ammo_pack_name + "' part to stream, unknown part type " + std::to_string(part));
		}
	}

	void WriteWeaponThingDefinitionToStream(const AmmoConfig& ammo_config,
											const std::string& weapon_thing_name,
											EnActorDefinitionPart part,
											Io::OutStream& out_stream)
	{
		switch(part) {
		case enActorDefinitionPart_Properties:
			// no props here
			break;

		case enActorDefinitionPart_States:
			{
				const AmmoConfig::WeaponThingInfo& weapon_thing_cfg = ammo_config.weapon_things.at(weapon_thing_name);
				const AmmoConfig::WeaponInfo& weapon_cfg = ammo_config.weapons.at(weapon_thing_cfg.weapon_name);

				AmmoAmountsMap ammo_give_amounts;
				if(!weapon_cfg.ammo_type1.empty() && weapon_thing_cfg.ammo_give1 > 0) {
					ammo_give_amounts[weapon_cfg.ammo_type1] = weapon_thing_cfg.ammo_give1;
				}
				if(!weapon_cfg.ammo_type2.empty() && weapon_thing_cfg.ammo_give2 > 0) {
					ammo_give_amounts[weapon_cfg.ammo_type2] = weapon_thing_cfg.ammo_give2;
				}
				WriteAmmoGiverThingDefinitionStatesToStream(ammo_config, ammo_give_amounts, weapon_cfg.name, out_stream);
			}
			break;

		case enActorDefinitionPart_None:
		default:
			throw Exception("unexpected error - cannot write weapon thing actor '" + weapon_thing_name + "' part to stream, unknown part type " + std::to_string(part));
		}
	}

	void WriteAmmoGiverThingDefinitionStatesToStream(const AmmoConfig& ammo_config,
													 const AmmoAmountsMap& ammo_give_amounts,
													 const std::string& weapon_name,
													 Io::OutStream& out_stream)
	{
		DECR_START(out_stream)
			DECR_ENDL()
			DECR_ACTOR_STATE_LABEL("Pickup")
			if(weapon_name.empty()) {
				DECR_ACTOR_STATE("TNT1", "A", 0, "")
			}

			const std::string& ammo_limit_not_reached_marker_actor_name = ammo_config.utility_actor_names.at("AmmoLimitNotReachedMarker");
			bool have_ammo_checking_states = false;
			for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
				const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
				if(ammo_give_amounts.find(ammo_cfg.name) == ammo_give_amounts.end()) {
					// thing does not give this ammo type on pickup - skip its check
					continue;
				}

				const std::string check_ammo_label_name = "Super::" + ammo_cfg.name + "CheckLimitStart";
				if(!weapon_name.empty() && !have_ammo_checking_states) {
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + weapon_name + "\", 1, \"" + check_ammo_label_name + "\")")
					DECR_ACTOR_STATE_GOTO("ActualPickup")

				} else {
					DECR_ACTOR_STATE_GOTO(check_ammo_label_name)
				}
				DECR_ENDL()

				const std::string check_ammo_after_label_name = ammo_cfg.name + "CheckLimitAfter";
				DECR_ACTOR_STATE_LABEL(check_ammo_after_label_name)
				DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + ammo_limit_not_reached_marker_actor_name + "\", 1, \"ActualPickup\")")
				have_ammo_checking_states = true;
			}

			if(!have_ammo_checking_states) {
				if(!weapon_name.empty()) {
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + weapon_name + "\", 1, 2)")
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + weapon_name + "\")")
					DECR_ACTOR_STATE_ENDSEQ("Stop")
					DECR_ACTOR_STATE("TNT1", "A", 0, "")
					DECR_ACTOR_STATE_ENDSEQ("Fail")

				} else {
					// nothing to do
					DECR_ACTOR_STATE_ENDSEQ("Stop")
				}

			} else {
				DECR_ACTOR_STATE_ENDSEQ("Fail")
				DECR_ENDL()

				DECR_ACTOR_STATE_LABEL("ActualPickup")
				DECR_ACTOR_STATE("TNT1", "A", 0, "A_TakeInventory(\"" + ammo_limit_not_reached_marker_actor_name + "\")")
				if(!weapon_name.empty()) {
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + weapon_name + "\")")
				}

				for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
					const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
					const auto it = ammo_give_amounts.find(ammo_cfg.name);
					if(it == ammo_give_amounts.end()) {
						continue;
					}

					const std::string amount_str = std::to_string(it->second);
					DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + ammo_cfg.name + "\", " + amount_str + ")")
				}
				DECR_DISABLE_TRAILING_ENDL()
				DECR_ACTOR_STATE_GOTO("Super::FixUpAmmoStart")
			}
		DECR_END()
	}

	void WriteAmmoLimitsCheckerDefinitionToStream(const AmmoConfig& ammo_config, Io::OutStream& out_stream) {
		// sort backpacks that may change ammo limits by their marker count values ascending
		class BackpackSorterByMarkerCount {
			const AmmoConfig::BackpackInfoMap& m_backpacks;

		public:
			BackpackSorterByMarkerCount(const AmmoConfig::BackpackInfoMap& backpacks)
				: m_backpacks(backpacks)
			{}

			bool operator() (const std::string& backpack1_name, const std::string& backpack2_name) {
				const AmmoConfig::BackpackInfo& backpack1_cfg = m_backpacks.at(backpack1_name);
				const AmmoConfig::BackpackInfo& backpack2_cfg = m_backpacks.at(backpack2_name);
				return backpack1_cfg.backpack_marker_count < backpack2_cfg.backpack_marker_count;
			}
		} backpack_sorter(ammo_config.backpacks);

		StringArray backpack_names_sorted;
		for(auto it = ammo_config.backpacks.begin(); it != ammo_config.backpacks.end(); it++) {
			const AmmoConfig::BackpackInfo& backpack_cfg = it->second;
			if(backpack_cfg.backpack_marker_count <= 0) {
				continue;
			}

			backpack_names_sorted.push_back(backpack_cfg.name);
		}

		std::sort(backpack_names_sorted.begin(), backpack_names_sorted.end(), backpack_sorter);
		const int backpack_marker_max_amount =
			!backpack_names_sorted.empty()
				? ammo_config.backpacks.at(backpack_names_sorted[backpack_names_sorted.size()-1]).backpack_marker_count
				: 0;

		DECR_START(out_stream)
			const std::string ammo_limit_checker_actor_name = ammo_config.utility_actor_names.at("AmmoLimitsChecker");
			DECR_LINE_COMMENT_SL("")
			DECR_LINE_COMMENT_SL(ammo_limit_checker_actor_name + " - checks ammo limits are not exceeded when derived by a child")
			DECR_LINE_COMMENT_SL("also, some helper classes for " + ammo_limit_checker_actor_name)
			DECR_LINE_COMMENT_SL("")
			DECR_ENDL()

			// 1) define the backpack marker inventory item;
			const std::string backpack_marker_actor_name = ammo_config.utility_actor_names.at("BackpackMarker");
			if(backpack_marker_max_amount > 0) {
				DECR_ACTOR_START(backpack_marker_actor_name, "Inventory")
					DECR_ACTOR_PROP_NUM("Inventory.MaxAmount", backpack_marker_max_amount)
				DECR_ACTOR_END()
				DECR_ENDL()
			}

			// 2) define special marker to check that player can pickup more ammo of a certain type
			const std::string ammo_limit_not_reached_marker_actor_name = ammo_config.utility_actor_names.at("AmmoLimitNotReachedMarker");
			DECR_ACTOR_START(ammo_limit_not_reached_marker_actor_name, "Inventory")
				DECR_ACTOR_PROP_NUM("Inventory.MaxAmount", 1)
			DECR_ACTOR_END()

			// 3) define Ammo Limits Checker actor - the parent actor for all ammo giving things, not for direct use
			DECR_ACTOR_START(ammo_limit_checker_actor_name, "CustomInventory")
				DECR_ACTOR_STATES_START()
					// code for check, that ammo limits are reached;
					// if derived actor call this and player gets Ammo Limit Not Reached Marker,
					// then ammo limit is not reached, otherwise it is reached
					for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
						const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
						const std::string check_ammo_limit_start_label = ammo_cfg.name + "CheckLimitStart";
						DECR_ACTOR_STATE_LABEL(check_ammo_limit_start_label)

						// marker count -> jump offset
						std::unordered_map<int, size_t> backpack_marker_check_jump_offsets;
						// also count default ammo limit
						size_t number_of_ammo_limits = 1;
						for(int bix=0; bix<backpack_names_sorted.size(); bix++) {
							const AmmoConfig::BackpackInfo& backpack_cfg = ammo_config.backpacks.at(backpack_names_sorted[bix]);
							const int& max_amount = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
							const int& prev_max_amount =
								(bix > 0)
									? ammo_config.backpacks.at(backpack_names_sorted[bix-1]).ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount
									: ammo_cfg.default_max_amount;

							if(max_amount != prev_max_amount) {
								number_of_ammo_limits++;
								backpack_marker_check_jump_offsets[backpack_cfg.backpack_marker_count] =
									backpack_marker_check_jump_offsets.size() + number_of_ammo_limits;
							}
						}

						for(int mix=backpack_marker_max_amount; mix>0; mix--) {
							auto jump_offset_it = backpack_marker_check_jump_offsets.find(mix);
							if(jump_offset_it == backpack_marker_check_jump_offsets.end()) {
								// this ammo limit is the same as with previous marker count (or default marker count) - use next jump or goto
								continue;
							}

							const std::string marker_item_count_str = std::to_string(mix);
							const std::string offset_str = std::to_string(jump_offset_it->second);
							DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + backpack_marker_actor_name + "\", " + marker_item_count_str + ", " + offset_str + ")")
						}

						if(backpack_marker_check_jump_offsets.size() > 0) {
							DECR_ACTOR_STATE_GOTO_O(check_ammo_limit_start_label, backpack_marker_check_jump_offsets.size())
							DECR_ENDL()
							DECR_TEXT_L("    // " + check_ammo_limit_start_label + "+" + std::to_string(backpack_marker_check_jump_offsets.size()))
						}
						const std::string ammo_check_limit_after_label = ammo_cfg.name + "CheckLimitAfter";
						const size_t ammo_limits_not_reached_state_offset = backpack_marker_check_jump_offsets.size() + number_of_ammo_limits;
						const std::string max_amount_with_no_backpack_str = std::to_string(ammo_cfg.default_max_amount);
						DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + ammo_cfg.name + "\", " + max_amount_with_no_backpack_str + ", \"" + ammo_check_limit_after_label + "\")")
						DECR_ACTOR_STATE_GOTO_O(check_ammo_limit_start_label, ammo_limits_not_reached_state_offset)

						int prev_max_amount_value = ammo_cfg.default_max_amount;
						for(int mix=1, bix=0; mix<=backpack_marker_max_amount; mix++) {
							while(bix<backpack_names_sorted.size() && ammo_config.backpacks.at(backpack_names_sorted[bix]).backpack_marker_count != mix) {
								bix++;
							}

							const AmmoConfig::BackpackInfo& backpack_cfg = ammo_config.backpacks.at(backpack_names_sorted.at(bix));
							const int cur_max_amount_value = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
							if(cur_max_amount_value == prev_max_amount_value) {
								// same limit - do not duplicate states
								continue;
							}

							const std::string amount_str = std::to_string(backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount);
							DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + ammo_cfg.name + "\", " + amount_str + ", \"" + ammo_check_limit_after_label + "\")")
							DECR_ACTOR_STATE_GOTO_O(check_ammo_limit_start_label, ammo_limits_not_reached_state_offset)
							prev_max_amount_value = cur_max_amount_value;
						}
						DECR_ENDL()

						DECR_TEXT_L("    // " + check_ammo_limit_start_label + "+" + std::to_string(ammo_limits_not_reached_state_offset))
						DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + ammo_limit_not_reached_marker_actor_name + "\")")
						DECR_ACTOR_STATE("TNT1", "A", 0, "A_Jump(256, \"" + ammo_check_limit_after_label + "\")")
						DECR_ACTOR_STATE_ENDSEQ("Stop")
						DECR_ENDL()
					}

					// check and fix ammo amounts for actual backpack;
					// derived actor calls this in the end of pickup sequence
					DECR_ACTOR_STATE_LABEL("FixUpAmmoStart")
					for(int mix=backpack_marker_max_amount; mix>0; mix--) {
						const std::string marker_item_count_str = std::to_string(mix);
						const std::string label = "FixUpAmmo" + marker_item_count_str;

						const std::string jump_statement = "A_JumpIfInventory(\""
							+ backpack_marker_actor_name + "\", "
							+ marker_item_count_str + ", \""
							+ label + "\")";
						DECR_ACTOR_STATE("TNT1", "A", 0, jump_statement)
					}
					DECR_ACTOR_STATE("TNT1", "A", 0, "")
					DECR_ACTOR_STATE_ENDSEQ("Stop")

					for(int mix=1, bix=0; mix<=backpack_marker_max_amount; mix++) {
						while(bix<backpack_names_sorted.size() && ammo_config.backpacks.at(backpack_names_sorted[bix]).backpack_marker_count != mix) {
							bix++;
						}

						DECR_ENDL()
						const AmmoConfig::BackpackInfo& backpack_cfg = ammo_config.backpacks.at(backpack_names_sorted.at(bix));
						const std::string backpack_ammo_limit_check_label = "FixUpAmmo" + std::to_string(mix);
						std::unordered_set<std::string> required_ammo_checks;
						for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
							const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
							const int& ammo_backpack_max_amount = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
							if(ammo_backpack_max_amount < ammo_cfg.max_amount) {
								required_ammo_checks.insert(ammo_cfg.name);

							} else {
								// game will check this limit by itself
							}
						}

						// for each ammo type check if its amount is greater than the current maximum amount
						DECR_ACTOR_STATE_LABEL(backpack_ammo_limit_check_label)
							int ammo_jumps_processed_count = 0;
							for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
								const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);
								if(required_ammo_checks.find(ammo_cfg.name) == required_ammo_checks.end()) {
									continue;
								}

								const int& ammo_backpack_max_amount = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
								// check ammo limit
								const std::string amount_str = std::to_string(ammo_backpack_max_amount + 1);
								const std::string offset_str = std::to_string(required_ammo_checks.size() + ammo_jumps_processed_count + 1);
								DECR_ACTOR_STATE("TNT1", "A", 0, "A_JumpIfInventory(\"" + ammo_cfg.name + "\", " + amount_str + ", " + offset_str + ")")
								ammo_jumps_processed_count++;
							}
							DECR_ACTOR_STATE("TNT1", "A", 0, "")
						DECR_ACTOR_STATE_ENDSEQ("Stop")

						// set ammo amount to the current max amount and jump to check the next ammo amount
						int goto_shift = 1;
						for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
							const AmmoConfig::AmmoInfo& ammo_cfg = ammo_config.ammo.at(ammo_config.ammo_names[aix]);

							if(required_ammo_checks.find(ammo_cfg.name) == required_ammo_checks.end()) {
								// this ammo type limit is not checked with the backpack
								// (game checks limit itself)
								continue;
							}

							const int& ammo_backpack_max_amount = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
							const std::string amount_str = std::to_string(ammo_backpack_max_amount);
							DECR_ENDL()
							DECR_TEXT_L("    // fix up '" + ammo_cfg.name + "' amount in player's inventory")
							DECR_ACTOR_STATE("TNT1", "A", 0, "A_TakeInventory(\"" + ammo_cfg.name + "\")")
							DECR_ACTOR_STATE("TNT1", "A", 0, "A_GiveInventory(\"" + ammo_cfg.name + "\", " + amount_str + ")")
							DECR_ACTOR_STATE_GOTO_O(backpack_ammo_limit_check_label, goto_shift)
							goto_shift++;
						}
					}
				DECR_ACTOR_STATES_END()
				DECR_DISABLE_TRAILING_ENDL()
			DECR_ACTOR_END()
		DECR_END()
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WriteSbarinfoLumpToStream implementation

namespace BpkGen {
	static void WriteSbarinfoAmmoLimitDrawStatementsToStream(const AmmoConfig& ammo_config,
															 const std::string& data,
															 const std::string& cur_indent,
															 Io::OutStream& out_stream,
															 size_t cur_line_number,
															 bool ignore_label_errors,
															 Io::OutStream* log_stream);

	void WriteSbarinfoLumpToStream(const AmmoConfig& ammo_config,
								   Io::InStream& sbarinfo_template_stream,
								   Io::OutStream& out_stream,
								   bool ignore_label_errors/* = false*/,
								   Io::OutStream* log_stream/* = NULL*/)
	{
		Io::InStreamWithBuffer sbarinfo_template_stream_with_buffer(sbarinfo_template_stream);
		std::string bpkgen_label_content, indent;
		while(Io::PipeCharsUntilNextBpkGenLabel(sbarinfo_template_stream_with_buffer, out_stream, bpkgen_label_content, indent)) {
			const size_t line_number = sbarinfo_template_stream_with_buffer.GetLineNumber();
			const size_t open_parenthesis_pos = bpkgen_label_content.find("(");
			const bool have_parameters = open_parenthesis_pos < bpkgen_label_content.size();
			const std::string label_title =
				have_parameters
					? bpkgen_label_content.substr(0, open_parenthesis_pos)
					: bpkgen_label_content;

			if(label_title == "DrawAmmoMaxAmounts") {
				if(!have_parameters) {
					Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "SBARINFO template", line_number,
											   "must have drawing code as a parameter", !ignore_label_errors, log_stream);
					continue;
				}

				const size_t pos_start = label_title.size() + 1;
				const size_t pos_first_not_space = bpkgen_label_content.find_first_not_of(" \t\r\n", pos_start);

				const size_t pos_end = bpkgen_label_content.size()-2;
				const size_t pos_last_not_space = bpkgen_label_content.find_last_not_of(" \t\r\n", pos_end);

				size_t pos_data_start = 0;
				size_t len_data = 0;
				if(pos_first_not_space < bpkgen_label_content.size()-1) {
					// have some data (not an all-whitespace case)
					// skip whitespaces until the beginning of the line with the data
					// (leave the same indentation for the data)
					size_t off_data_start = 0;
					while(true) {
						const size_t new_pos_el = bpkgen_label_content.find_first_of("\r\n", pos_start + off_data_start);
						if(new_pos_el > pos_first_not_space) {
							break;
						}
						off_data_start = (new_pos_el + 1) - pos_start;
					}

					pos_data_start = pos_start + off_data_start;
					len_data = pos_last_not_space + 1 - pos_data_start;
				}

				const std::string data = bpkgen_label_content.substr(pos_data_start, len_data);
				if(!data.empty()) {
					WriteSbarinfoAmmoLimitDrawStatementsToStream(ammo_config, data, indent, out_stream,
																 line_number, ignore_label_errors, log_stream);
				}

			} else {
				Io::ReportBpkGenLabelError(Io::enBadLabel_Unknown, label_title, "SBARINFO template", line_number,
										   "", !ignore_label_errors, log_stream);
				continue;
			}
		}
	}

	void WriteSbarinfoAmmoLimitDrawStatementsToStream(const AmmoConfig& ammo_config,
													  const std::string& data,
													  const std::string& cur_indent,
													  Io::OutStream& out_stream,
													  size_t cur_line_number,
													  bool ignore_label_errors,
													  Io::OutStream* log_stream)
	{
		std::unordered_map<int, std::string> map_backpack_marker_value_to_backpack;
		const AmmoConfig::BackpackInfoMap& backpacks = ammo_config.backpacks;
		const AmmoConfig::AmmoInfoMap& ammo = ammo_config.ammo;
		int max_backpack_marker_count = 0;
		for(auto it=backpacks.begin(); it != backpacks.end(); it++) {
			const AmmoConfig::BackpackInfo& backpack_cfg = it->second;
			if(backpack_cfg.backpack_marker_count <= 0) {
				continue;
			}

			// backpack modifies ammo limits
			const auto another_backpack_it = map_backpack_marker_value_to_backpack.find(backpack_cfg.backpack_marker_count);
			if(another_backpack_it == map_backpack_marker_value_to_backpack.end()) {
				map_backpack_marker_value_to_backpack[backpack_cfg.backpack_marker_count] = backpack_cfg.name;

			} else {
				// check that ammo limits are the same for both backpacks
				for(int aix=0; aix<ammo_config.ammo_names.size(); aix++) {
					const AmmoConfig::AmmoInfo& ammo_cfg = ammo.at(ammo_config.ammo_names[aix]);
					const int& max_amount1 = backpack_cfg.ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
					const int& max_amount2 = backpacks.at(another_backpack_it->second).ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount;
					if(max_amount1 != max_amount2) {
						throw Exception("internal error - bad backpack marker counts (two backpack with different ammo limits have the same count)");
					}
				}
			}

			if(backpack_cfg.backpack_marker_count > max_backpack_marker_count) {
				max_backpack_marker_count = backpack_cfg.backpack_marker_count;
			}
		}

		const std::string backpack_marker_actor_name = ammo_config.utility_actor_names.at("BackpackMarker");
		// do not duplicate label error messages
		bool first_processing = true;
		for(int mix = max_backpack_marker_count; mix >= 0; mix--) {
			if(mix < max_backpack_marker_count) {
				first_processing = false;
			}

			auto backpack_name_it = map_backpack_marker_value_to_backpack.end();
			if(mix > 0) {
				backpack_name_it = map_backpack_marker_value_to_backpack.find(mix);
				if(backpack_name_it == map_backpack_marker_value_to_backpack.end()) {
					throw Exception("internal error - bad backpack marker counts (missing backpack for a marker count)");
				}
			}

			std::string closing_seq;
			if(mix > 0) {
				// jump into the block, if marker amount in the inventory is not less than 'mix'
				out_stream << "InInventory " << backpack_marker_actor_name << ", " << mix << " {";
				closing_seq += "}";
			}

			if(mix < max_backpack_marker_count) {
				if(mix > 0) {
					out_stream << " ";
				}

				// jump into the block, if marker amount in the inventory is less than 'mix+1'
				out_stream << "InInventory not " << backpack_marker_actor_name << ", " << (mix+1) << " {";
				closing_seq += "}";
			}

			out_stream << "\n";

			// write ammo limits drawing code
			std::stringstream data_stream(data, std::ios_base::in);
			Io::InStreamWithBuffer data_stream_with_buffer(data_stream);
			std::string bpkgen_label_content, indent;
			while(Io::PipeCharsUntilNextBpkGenLabel(data_stream_with_buffer, out_stream, bpkgen_label_content, indent)) {
				const size_t open_parenthesis_pos = bpkgen_label_content.find("(");
				const bool have_parameters = open_parenthesis_pos < bpkgen_label_content.size();
				const std::string label_title =
					have_parameters
						? bpkgen_label_content.substr(0, open_parenthesis_pos)
						: bpkgen_label_content;

				std::string params;
				if(have_parameters) {
					// have label params
					const size_t first_char_pos = bpkgen_label_content.find_first_not_of(" \t\n\r", open_parenthesis_pos + 1);
					const size_t last_char_pos = bpkgen_label_content.find_last_not_of(" \t\n\r", bpkgen_label_content.size() - 2);
					params = bpkgen_label_content.substr(first_char_pos, (last_char_pos + 1) - first_char_pos);
				}

				if(label_title == "AmmoMaxAmount") {
					if(!have_parameters || params.empty()) {
						if(first_processing) {
							Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "SBARINFO template", cur_line_number,
													   "must have ammo actor name as a parameter (in ammo limits drawing code)", !ignore_label_errors, log_stream);
						}

						continue;
					}

					// put actual ammo limit
					const std::string ammo_name = params;
					const auto ammo_cfg_it = ammo_config.ammo.find(ammo_name);
					if(ammo_cfg_it == ammo_config.ammo.end()) {
						if(first_processing) {
							Io::ReportBpkGenLabelError(Io::enBadLabel_BadParams, label_title, "SBARINFO template", cur_line_number,
													   "unknown ammo actor name '" + ammo_name + "' as a parameter (in ammo limits drawing code)", !ignore_label_errors, log_stream);
						}

						continue;
					}

					const AmmoConfig::AmmoInfo& ammo_cfg = ammo_cfg_it->second;
					const int& ammo_max_amount = 
						(mix > 0)
							? backpacks.at(backpack_name_it->second).ammo_info.at(ammo_cfg.index_in_backpack_data).max_amount
							: ammo_cfg.default_max_amount;

					out_stream << ammo_max_amount;

				} else {
					if(first_processing) {
						Io::ReportBpkGenLabelError(Io::enBadLabel_Unknown, label_title, "SBARINFO template", cur_line_number,
												   "presented in ammo limits drawing code", !ignore_label_errors, log_stream);
					}

					continue;
				}
			}

			out_stream << "\n" << cur_indent << closing_seq;
			if(mix > 0) {
				out_stream << "\n\n" << cur_indent;
			}
		}
	}
}
