// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <unordered_map>
#include <vector>

#ifdef WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <unistd.h>
	#include <stdio.h>
#endif

#include <yaml-cpp/yaml.h>

#include "../common/cbasetypes.hpp"
#include "../common/core.hpp"
#include "../common/malloc.hpp"
#include "../common/mmo.hpp"
#include "../common/nullpo.hpp"
#include "../common/showmsg.hpp"
#include "../common/strlib.hpp"
#include "../common/utilities.hpp"
#include "../common/utils.hpp"
#ifdef WIN32
#include "../common/winapi.hpp"
#endif

using namespace rathena;

#ifndef WIN32
int getch( void ){
    struct termios oldattr, newattr;
    int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return ch;
}
#endif

static std::unordered_map<std::string, int> um_jobnames {
	{ "Novice", JOB_NOVICE },
	{ "Swordman", JOB_SWORDMAN },
	{ "Mage", JOB_MAGE },
	{ "Archer", JOB_ARCHER },
	{ "Acolyte", JOB_ACOLYTE },
	{ "Merchant", JOB_MERCHANT },
	{ "Thief", JOB_THIEF },
	{ "Knight", JOB_KNIGHT },
	{ "Priest", JOB_PRIEST },
	{ "Wizard", JOB_WIZARD },
	{ "Blacksmith", JOB_BLACKSMITH },
	{ "Hunter", JOB_HUNTER },
	{ "Assassin", JOB_ASSASSIN },
	{ "Crusader", JOB_CRUSADER },
	{ "Monk", JOB_MONK },
	{ "Sage", JOB_SAGE },
	{ "Rogue", JOB_ROGUE },
	{ "Alchemist", JOB_ALCHEMIST },
	{ "Bard", JOB_BARD },
	{ "Dancer", JOB_DANCER },
	{ "Super_Novice", JOB_SUPER_NOVICE },
	{ "Gunslinger", JOB_GUNSLINGER },
	{ "Ninja", JOB_NINJA },
	{ "Taekwon", JOB_TAEKWON, },
	{ "Star_Gladiator", JOB_STAR_GLADIATOR },
	{ "Soul_Linker", JOB_SOUL_LINKER },
//	{ "Gangsi", JOB_GANGSI },
//	{ "Death_Knight", JOB_DEATH_KNIGHT },
//	{ "Dark_Collector", JOB_DARK_COLLECTOR },
	{ "Kagerou", JOB_KAGEROU },
	{ "Oboro", JOB_OBORO },
	{ "Rebellion", JOB_REBELLION },
	{ "Summoner", JOB_SUMMONER },
};

static std::unordered_map<std::string, equip_pos> um_equipnames{
	{ "Head_Low", EQP_HEAD_LOW },
	{ "Head_Mid", EQP_HEAD_MID },
	{ "Head_Top", EQP_HEAD_TOP },
	{ "Right_Hand", EQP_HAND_R },
	{ "Left_Hand", EQP_HAND_L },
	{ "Armor", EQP_ARMOR },
	{ "Shoes", EQP_SHOES },
	{ "Garment", EQP_GARMENT },
	{ "Right_Accessory", EQP_ACC_R },
	{ "Left_Accessory", EQP_ACC_L },
	{ "Costume_Head_Top", EQP_COSTUME_HEAD_TOP },
	{ "Costume_Head_Mid", EQP_COSTUME_HEAD_MID },
	{ "Costume_Head_Low", EQP_COSTUME_HEAD_LOW },
	{ "Costume_Garment", EQP_COSTUME_GARMENT },
	{ "Ammo", EQP_AMMO },
	{ "Shadow_Armor", EQP_SHADOW_ARMOR },
	{ "Shadow_Weapon", EQP_SHADOW_WEAPON },
	{ "Shadow_Shield", EQP_SHADOW_SHIELD },
	{ "Shadow_Shoes", EQP_SHADOW_SHOES },
	{ "Shadow_Right_Accessory", EQP_SHADOW_ACC_R },
	{ "Shadow_Left_Accessory", EQP_SHADOW_ACC_L },
};

// Forward declaration of conversion functions
static bool item_db_yaml2sql(const std::string &file, const std::string &table);

bool fileExists( const std::string& path );
bool askConfirmation( const char* fmt, ... );

YAML::Node inNode;
std::ofstream out;

void copyFileIfExists( std::ofstream& file,const std::string& name ){
	std::string path = "doc/yaml/sql/" + name + ".sql";

	if( fileExists( path ) ){
		std::ifstream source( path, std::ios::binary );

		std::istreambuf_iterator<char> begin_source(source);
		std::istreambuf_iterator<char> end_source;
		std::ostreambuf_iterator<char> begin_dest( file );
		copy( begin_source, end_source, begin_dest );

		source.close();
	}
}

void prepareHeader(std::ofstream &file, const std::string& name) {
	copyFileIfExists( file, name );

	file << "\n";
}

template<typename Func>
bool process( const std::string& type, uint32 version, const std::vector<std::string>& paths, const std::string& name, Func lambda ){
	for( const std::string& path : paths ){
		const std::string name_ext = name + ".yml";
		const std::string from = path + "/" + name_ext;
		std::string rename = "";

		if (name.find("import") == std::string::npos) {
#ifdef RENEWAL
			rename = "item_db_re";
#else
			rename = "item_db";
#endif
		} else {
#ifdef RENEWAL
			rename = "item_db2_re";
#else
			rename = "item_db2";
#endif
		}

		const std::string to = path + "/" + (rename.size() > 0 ? rename : name) + ".sql";

		if( fileExists( from ) ){
			inNode.reset();
			inNode = YAML::LoadFile(from);

			if (!inNode["Body"].IsDefined())
				continue;

			if( !askConfirmation( "Found the file \"%s\", which requires migration to sql.\nDo you want to convert it now? (Y/N)\n", from.c_str() ) ){
				continue;
			}

			if (fileExists(to)) {
				if (!askConfirmation("The file \"%s\" already exists.\nDo you want to replace it? (Y/N)\n", to.c_str())) {
					continue;
				}
			}

			out.open(to);

			if (!out.is_open()) {
				ShowError("Can not open file \"%s\" for writing.\n", to.c_str());
				return false;
			}

			prepareHeader(out, (rename.size() > 0 ? rename : name));

			if( !lambda( path, name_ext, (rename.size() > 0 ? rename : name) ) ){
				out.close();
				return false;
			}

			out.close();
		}
	}

	return true;
}

int do_init( int argc, char** argv ){
	const std::string path_db = std::string( db_path );
	const std::string path_db_mode = path_db + "/" + DBPATH;
	const std::string path_db_import = path_db + "/" + DBIMPORT + "/";

	std::vector<std::string> root_paths = {
		path_db,
		path_db_mode,
		path_db_import
	};

	if (!process("ITEM_DB", 1, root_paths, "item_db", [](const std::string& path, const std::string& name_ext, const std::string &table) -> bool {
		return item_db_yaml2sql(path + name_ext, table);
	})) {
		return 0;
	}

	// TODO: add implementations ;-)

	return 0;
}

void do_final(void){
}

bool fileExists( const std::string& path ){
	std::ifstream in;

	in.open( path );

	if( in.is_open() ){
		in.close();

		return true;
	}else{
		return false;
	}
}

bool askConfirmation( const char* fmt, ... ){
	va_list ap;

	va_start( ap, fmt );

	_vShowMessage( MSG_NONE, fmt, ap );

	va_end( ap );

	char c = getch();

	if( c == 'Y' || c == 'y' ){
		return true;
	}else{
		return false;
	}
}

std::string name2Upper(std::string name) {
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
	name[0] = toupper(name[0]);

	for (size_t i = 0; i < name.size(); i++) {
		if (name[i - 1] == '_' || (name[i - 2] == '1' && name[i - 1] == 'h') || (name[i - 2] == '2' && name[i - 1] == 'h'))
			name[i] = toupper(name[i]);
	}

	return name;
}

const std::string WHITESPACE = " \n\r\t\f\v";

std::string ltrim(const std::string &s) {
	size_t start = s.find_first_not_of(WHITESPACE);

	return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s) {
	size_t end = s.find_last_not_of(WHITESPACE);

	return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string string_trim(const std::string &s) {
	return rtrim(ltrim(s));
}

std::string string_escape(const std::string &s) {
	size_t n = s.length();
	std::string escaped;

	escaped.reserve(n * 2);

	for (size_t i = 0; i < n; ++i) {
		if (s[i] == '\\' || s[i] == '\'')
			escaped += '\\';
		escaped += s[i];
	}

	return escaped;
}

/**
 * Append the entry value to the string if the Node exists
 * @param node: Node with entry
 * @param value: String to store node value to
 * @param string: If value is a string or not
 * @param concat: If Node is a concatenated type
 */
static bool appendEntry(const YAML::Node &node, std::string &value, bool string = false, bool concat = false) {
	if (node.IsDefined()) {
		if (string) {
			std::string node_str = "", concat_str = "";

			if (concat) {
				for (const auto &subNode : node) {
					std::string name = subNode.first.as<std::string>();

					if (subNode.IsDefined() && subNode[name].as<bool>() == true) {
						concat_str.append(name);
						concat_str.append(":true|");
					} else {
						concat_str.append(name);
						concat_str.append(":false|");
					}
				}

				concat_str.pop_back(); // Remove last '|'
			} else
				node_str = string_escape(node.as<std::string>());

			value.append("'");
			value.append(string_trim(concat ? concat_str : node_str));
			value.append("',");
		} else {
			value.append(string_trim(node.as<std::string>()));
			value.append(",");
		}

		return true;
	}

	return false;
}

// Implementation of the conversion functions

// Copied and adjusted from itemdb.cpp
static bool item_db_yaml2sql(const std::string &file, const std::string &table) {
	size_t entries = 0;

	for (const YAML::Node &input : inNode["Body"]) {
		std::string column = "", value = "";

		if (appendEntry(input["Id"], value))
			column += "`id`,";
		if (appendEntry(input["AegisName"], value, true))
			column += "`name_aegis`,";
		if (appendEntry(input["Name"], value, true))
			column += "`name_english`,";
		if (appendEntry(input["Type"], value, true))
			column += "`type`,";
		if (appendEntry(input["SubType"], value, true))
			column += "`subtype`,";
		if (appendEntry(input["Buy"], value))
			column += "`price_buy`,";
		if (appendEntry(input["Sell"], value))
			column += "`price_sell`,";
		if (appendEntry(input["Weight"], value))
			column += "`weight`,";
		if (appendEntry(input["Attack"], value))
			column += "`attack`,";
#ifdef RENEWAL
		if (appendEntry(input["MagicAttack"], value))
			column += "`magic_attack`,";
#endif
		if (appendEntry(input["Defense"], value))
			column += "`defense`,";
		if (appendEntry(input["Range"], value))
			column += "`range`,";
		if (appendEntry(input["Slots"], value))
			column += "`slots`,";
		if (appendEntry(input["Job"], value, true, true))
			column += "`equip_jobs`,";
		if (appendEntry(input["Class"], value, true, true))
			column += "`equip_upper`,";
		if (appendEntry(input["Gender"], value, true))
			column += "`equip_genders`,";
		if (appendEntry(input["Location"], value, true, true))
			column += "`equip_locations`,";
		if (appendEntry(input["WeaponLevel"], value))
			column += "`weapon_level`,";
		if (appendEntry(input["EquipLevelMin"], value))
			column += "`equip_level_min`,";
		if (appendEntry(input["EquipLevelMax"], value))
			column += "`equip_level_max`,";
		if (appendEntry(input["Refineable"], value))
			column += "`refineable`,";
		if (appendEntry(input["View"], value))
			column += "`view`,";

		const YAML::Node &flags = input["Flags"];

		if (flags) {
			if (appendEntry(flags["BuyingStore"], value))
				column += "`flag_buyingstore`,";
			if (appendEntry(flags["DeadBranch"], value))
				column += "`flag_deadbranch`,";
			if (appendEntry(flags["Container"], value))
				column += "`flag_container`,";
			if (appendEntry(flags["Guid"], value))
				column += "`flag_guid`,";
			if (appendEntry(flags["BindOnEquip"], value))
				column += "`flag_bindonequip`,";
			if (appendEntry(flags["DropAnnounce"], value))
				column += "`flag_dropannounce`,";
			if (appendEntry(flags["NoConsume"], value))
				column += "`flag_noconsume`,";
			if (appendEntry(flags["DropEffect"], value, true))
				column += "`flag_dropeffect`,";
		}

		const YAML::Node &delay = input["Delay"];

		if (delay) {
			if (appendEntry(delay["Duration"], value))
				column += "`delay_duration`,";
			if (appendEntry(delay["Status"], value, true))
				column += "`delay_status`,";
		}

		const YAML::Node &stack = input["Stack"];

		if (stack) {
			if (appendEntry(stack["Amount"], value))
				column += "`stack_amount`,";
			if (appendEntry(stack["Inventory"], value))
				column += "`stack_inventory`,";
			if (appendEntry(stack["Cart"], value))
				column += "`stack_cart`,";
			if (appendEntry(stack["Storage"], value))
				column += "`stack_storage`,";
			if (appendEntry(stack["GuildStorage"], value))
				column += "`stack_guildstorage`,";
		}

		const YAML::Node &nouse = input["NoUse"];

		if (nouse) {
			if (appendEntry(nouse["Override"], value))
				column += "`nouse_override`,";
			if (appendEntry(nouse["Sitting"], value))
				column += "`nouse_sitting`,";
		}

		const YAML::Node &trade = input["Trade"];

		if (trade) {
			if (appendEntry(trade["Override"], value))
				column += "`trade_override`,";
			if (appendEntry(trade["NoDrop"], value))
				column += "`trade_nodrop`,";
			if (appendEntry(trade["NoTrade"], value))
				column += "`trade_notrade`,";
			if (appendEntry(trade["TradePartner"], value))
				column += "`trade_tradepartner`,";
			if (appendEntry(trade["NoSell"], value))
				column += "`trade_nosell`,";
			if (appendEntry(trade["NoCart"], value))
				column += "`trade_nocart`,";
			if (appendEntry(trade["NoStorage"], value))
				column += "`trade_nostorage`,";
			if (appendEntry(trade["NoGuildStorage"], value))
				column += "`trade_noguildstorage`,";
			if (appendEntry(trade["NoMail"], value))
				column += "`trade_nomail`,";
			if (appendEntry(trade["NoAuction"], value))
				column += "`trade_noauction`,";
		}

		if (appendEntry(input["Script"], value, true))
			column += "`script`,";
		if (appendEntry(input["EquipScript"], value, true))
			column += "`equip_script`,";
		if (appendEntry(input["UnEquipScript"], value, true))
			column += "`unequip_script`,";

		column.pop_back(); // Remove last ','
		value.pop_back(); // Remove last ','

		out << "REPLACE INTO `" + table + "` (" + column + ") VALUES (" + value + ");\n";
		entries++;
	}

	ShowStatus("Done reading '" CL_WHITE "%d" CL_RESET "' items in '" CL_WHITE "%s" CL_RESET "'.\n", entries, file.c_str());

	return true;
}