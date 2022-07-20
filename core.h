
#ifndef _BACKPACK_GEN_CORE_H_
#define _BACKPACK_GEN_CORE_H_

#include "common.h"
#include "io.h"

namespace BpkGen {
	struct AmmoConfig {
		struct AmmoInfo {
			// ammo actor name (not to be placed on maps);
			// mandatory, valid DECORATE actor name, unique
			std::string name;

			// maximum amount of ammo without backpacks;
			// mandatory, greater than 0 and
			// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown)
			int default_max_amount;

			// maximum amount of ammo with backpacks;
			// mandatory, greater than or equal to 'default_max_amount' and
			// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown)
			int max_amount;

			// index of this ammo type in BackpackInfo.ammo_info;
			// mandatory, greater than or equal to 0 and less than ammo.size(), unique
			int index_in_backpack_data;

			AmmoInfo()
				: default_max_amount(-1)
				, max_amount(-1)
				, index_in_backpack_data(-1)
			{}
		};

		// ammo names array;
		// unique elements, each element must match the existing ammo actor name
		StringArray ammo_names;

		// ammo info: AmmoInfo::name -> AmmoInfo;
		// ammo.size() == ammo_names.size(), all keys are from ammo_names
		typedef std::unordered_map<std::string, AmmoInfo> AmmoInfoMap;
		AmmoInfoMap ammo;

		struct BackpackInfo {
			// backpack actor (thing) name - to be placed on maps;
			// mandatory, valid DECORATE actor name, unique
			std::string name;

			struct BackpackAmmoInfo {
				// max amount of this ammo type backpack allows to carry;
				// optional, default is -1 - backpack does not change ammo limits,
				// in this case 'max_amount' for all ammo types must be -1 for the backpack;
				// a backpack cannot decrease ammo maximum limit:
				// 'max_amount' must be greater than or equal to AmmoConfig::AmmoInfo::default_max_amount
				// for the ammo type and must be greater than or equal to each 'max_amount' property
				// for the ammo type for backpacks with lesser marker count value
				int max_amount;

				// amount of this ammo type backpack gives;
				// must be greater than or equal to 0 and
				// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown);
				// optional, default is 0
				int give_amount;

				BackpackAmmoInfo()
					: max_amount(-1)
					, give_amount(0)
				{}
			};

			// ammo info related to the backpack for each ammo type;
			// ammo_info.size() == AmmoConfig::ammo_names.size(),
			// must contain only existing ammo types from ammo_names array
			typedef std::vector<BackpackAmmoInfo> BackpackAmmoInfoArray;
			BackpackAmmoInfoArray ammo_info;

			// amount of backpack markers in player's inventory that matches this backpack
			// in case the backpack sets max ammo amounts;
			// must be unique (for each set of ammo max amounts) in the range 1 : backpacks.size();
			// optional, default is 0 (backpack does not change ammo limits)
			int backpack_marker_count;

			// true - give backpack item on pickup of this backpack;
			// false - do not give backpack item on pickup;
			// if ammo limits are set, this must be true;
			// optional, default - true;
			bool give_backpack_item;

			BackpackInfo()
				: backpack_marker_count(0)
				, give_backpack_item(true)
			{}
		};

		// backpacks info: BackpackInfo::name -> BackpackInfo
		typedef std::unordered_map<std::string, BackpackInfo> BackpackInfoMap;
		BackpackInfoMap backpacks;

		// name of the backpack item to give, if a backpack changes ammo limits;
		// e.g. for Doom it is "Backpack";
		// mandatory, name of the existing actor
		std::string backpack_item_name;

		struct AmmoPackInfo {
			// ammo pack actor (thing) name - to be placed on maps;
			// mandatory, valid DECORATE actor name, unique
			std::string name;

			// ammo type actor name
			// mandatory, a value from AmmoConfig::ammo_names array
			std::string ammo_type;

			// amount of ammo this pack gives to player;
			// mandatory, must be greater than 0 and
			// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown)
			int ammo_give_amount;

			AmmoPackInfo()
				: ammo_give_amount(-1)
			{}
		};

		// ammo packs: AmmoPackInfo::name -> AmmoPackInfo
		typedef std::unordered_map<std::string, AmmoPackInfo> AmmoPackInfoMap;
		AmmoPackInfoMap ammo_packs;

		struct WeaponInfo {
			// weapon actor name (not to be placed on maps);
			// mandatory, valid DECORATE actor name, unique
			std::string name;

			// primary ammo type weapon uses;
			// optional, default is missing;
			// if set - must be valid value from AmmoConfig::ammo_names array
			std::string ammo_type1;

			// secondary ammo type weapon uses;
			// optional, default is missing,
			// if set - must be valid value from AmmoConfig::ammo_names array
			std::string ammo_type2;
		};

		// weapons info: WeaponInfo::name -> WeaponInfo
		typedef std::unordered_map<std::string, WeaponInfo> WeaponInfoMap;
		WeaponInfoMap weapons;

		struct WeaponThingInfo {
			// weapon thing name - actor with this name is to be placed on maps;
			// mandatory, valid DECORATE actor name, unique
			std::string name;

			// actual weapon to give on pickup;
			// mandatory, valid key from AmmoConfig::weapons
			std::string weapon_name;

			// the amount of primary ammo weapon gives on pickup;
			// optional, default is 0;
			// ignored, if weapons[weapon_name].ammo_type1 is missing;
			// must be greater than or equal to 0 and
			// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown)
			int ammo_give1;

			// the amount of secondary ammo weapon gives on pickup;
			// optional, default is 0;
			// ignored, if weapons[weapon_name].ammo_type2 is missing;
			// must be greater than or equal to 0 and
			// less than the maximum value DECORATE allows for Inventory.MaxAmount (unknown)
			int ammo_give2;

			WeaponThingInfo()
				: ammo_give1(0)
				, ammo_give2(0)
			{}
		};

		// weapon things map: WeaponThingInfo::name -> WeaponThingInfo
		typedef std::unordered_map<std::string, WeaponThingInfo> WeaponThingInfoMap;
		WeaponThingInfoMap weapon_things;

		// names of utility actors created by backpack-gen;
		// map: internal identifier -> valid DECORATE actor name, unique
		StringToStringMap utility_actor_names;
	};

	// extracts ammo config from an input data;
	// throws if no valid config can be extracted
	void GetAmmoConfig(const Io::ConfigData& config_data, AmmoConfig& config);

	// writes DECORATE lump to the output stream according to the given
	// ammo config and DECORATE template (DOES NOT validate the template);
	// throws in case of:
	//   - bad ammo config;
	//   - bad label in DECORATE template is found (may be disabled by the flag);
	//   - I/O errors
	// if 'ignore_label_errors' is true, then all label errors are put into 'log_stream'
	void WriteDecorateLumpToStream(const AmmoConfig& ammo_config,
								   Io::InStream& decorate_template_stream,
								   Io::OutStream& out_stream,
								   bool ignore_label_errors = false,
								   Io::OutStream* log_stream = NULL);

	// writes SBARINFO lump to the output stream according to the given
	// ammo config and SBARINFO template (DOES NOT validate the template);
	// throws in case of:
	//   - bad ammo config;
	//   - bad label in SBARINFO template is found (may be disabled by the flag);
	//   - I/O errors
	// if 'ignore_label_errors' is true, then all label errors are put into 'log_stream'
	void WriteSbarinfoLumpToStream(const AmmoConfig& ammo_config,
								   Io::InStream& sbarinfo_template_stream,
								   Io::OutStream& out_stream,
								   bool ignore_label_errors = false,
								   Io::OutStream* log_stream = NULL);
}

#endif // _BACKPACK_GEN_CORE_H_
