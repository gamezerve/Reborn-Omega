///////////////////////////////////////////////////////////////////////////////////////
// FILE: INIFieldDeparser.cpp 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/INIFieldDeparser.h"

#include "Common/AudioEventRTS.h"
#include "Common/DamageFX.h"
#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "Common/INI.h"
#include "Common/KindOf.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/FXList.h"
#include "GameClient/Image.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Armor.h"
#include "GameLogic/ArmorSet.h"
#include "GameLogic/Damage.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSetFlags.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static std::string formatIndex(
	Int value,
	const void* userData)
{
	const char* const* names =
		static_cast<const char* const*>(userData);

	if (!names)
		return std::to_string(value);

	if (value >= 0)
	{
		for (Int i = 0; names[i] != nullptr; ++i)
		{
			if (i == value)
				return names[i];
		}
	}

	return std::to_string(value);
}

static std::string formatLookup(
	Int value,
	const void* userData)
{
	const LookupListRec* list =
		static_cast<const LookupListRec*>(userData);

	if (!list)
		return std::to_string(value);

	for (const LookupListRec* entry = list;
		entry->name != nullptr;
		++entry)
	{
		if (entry->value == value)
			return entry->name;
	}

	return std::to_string(value);
}

static std::string formatBitString(
	UnsignedInt value,
	const void* userData,
	Int maxBits)
{
	const char* const* names =
		static_cast<const char* const*>(userData);

	if (!names || value == 0)
		return "NONE";

	std::string result;

	for (Int i = 0;
		i < maxBits && names[i] != nullptr;
		++i)
	{
		if ((value & (1u << i)) == 0)
			continue;

		if (!result.empty())
			result += " ";

		result += names[i];
	}

	return result.empty()
		? "NONE"
		: result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Format any game BitFlags specialization through its own canonical bit-name table. */
//-------------------------------------------------------------------------------------------------
template<typename Flags>
static std::string formatRegisteredFlags(const Flags& flags, Int count)
{
	const char* const* names = Flags::getBitNames();
	std::string result;
	for (Int bit = 0; bit < count && names[bit] != nullptr; ++bit)
	{
		if (!flags.test(bit))
			continue;
		if (!result.empty())
			result += " ";
		result += names[bit];
	}
	return result.empty() ? "NONE" : result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Preserve every bit of wide game BitFlags while emitting normalized additive syntax. */
//-------------------------------------------------------------------------------------------------
template<typename Flags>
static std::string formatAdditiveRegisteredFlags(const Flags& flags, Int count)
{
	const char* const* names = Flags::getBitNames();
	std::string result = "NONE";
	for (Int bit = 0; bit < count && names[bit] != nullptr; ++bit)
	{
		if (!flags.test(bit))
			continue;
		result += " +";
		result += names[bit];
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Emit a normalized NONE-plus-flags expression through a parser-owned name table. */
//-------------------------------------------------------------------------------------------------
static std::string formatAdditiveFlags(UnsignedInt flags, const char* const* names, Int count)
{
	std::string result = "NONE";
	for (Int bit = 0; bit < count && names[bit] != nullptr; ++bit)
	{
		if ((flags & (1u << bit)) == 0)
			continue;
		result += " +";
		result += names[bit];
	}
	return result;
}

static UnsignedInt durationFramesToMilliseconds(
	UnsignedInt frames)
{
	if (frames == 0)
		return 0;

	return static_cast<UnsignedInt>(
		std::floor(
			static_cast<Real>(frames) *
			MSEC_PER_LOGICFRAME_REAL));
}

void INIFieldDeparser::appendField(
	std::string& output,
	const char* token,
	const std::string& value,
	const char* indent)
{
	output += indent;
	output += token;
	output += " = ";
	output += value;
	output += "\r\n";
}

std::string INIFieldDeparser::formatReal(Real value)
{
	char buffer[64];

	snprintf(
		buffer,
		sizeof(buffer),
		"%.9g",
		static_cast<double>(value));

	return buffer;
}

std::string INIFieldDeparser::formatAsciiString(
	const AsciiString& value)
{
	const char* text = value.str();

	if (!text || !*text)
		return "\"\"";

	Bool needsQuotes = FALSE;

	for (const char* p = text; *p; ++p)
	{
		if (*p == ' ' || *p == '\t')
		{
			needsQuotes = TRUE;
			break;
		}
	}

	if (!needsQuotes)
		return text;

	std::string result = "\"";
	result += text;
	result += "\"";

	return result;
}

Bool INIFieldDeparser::deparseField(
	const FieldParse& field,
	const void* instance,
	std::string& output,
	const char* indent)
{
	if (!field.token ||
		!field.parse ||
		!instance)
	{
		return FALSE;
	}

	// Reborn: Prefer the custom deparser stored beside the field's canonical parse-table record.
	if (field.deparse)
		return field.deparse(field, instance, output, indent);

	const char* store =
		reinterpret_cast<const char*>(instance) +
		field.offset;

	// Reborn: Preserve dynamic and embedded audio event names used by Object model fields.
	if (field.parse == INI::parseDynamicAudioEventRTS)
	{
		const RefCountPtr<DynamicAudioEventRTS>& event =
			*reinterpret_cast<
			const RefCountPtr<DynamicAudioEventRTS>*>(store);

		appendField(
			output,
			field.token,
			event
			? formatAsciiString(event->getEventName())
			: "NoSound",
			indent);

		return TRUE;
	}

	// Reborn: Preserve non-dynamic audio events handled by the same generic field table walker.
	if (field.parse == INI::parseAudioEventRTS)
	{
		const AudioEventRTS& event =
			*reinterpret_cast<const AudioEventRTS*>(store);

		appendField(
			output,
			field.token,
			event.getEventName().isEmpty()
			? "NoSound"
			: formatAsciiString(event.getEventName()),
			indent);

		return TRUE;
	}

	// Reborn: Resolve registered definition pointers through the names retained by their game-owned objects and stores.
	if (field.parse == INI::parseThingTemplate)
	{
		const ThingTemplate* value = *reinterpret_cast<const ThingTemplate* const*>(store);
		appendField(output, field.token, value ? value->getName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseArmorTemplate)
	{
		const ArmorTemplate* value = *reinterpret_cast<const ArmorTemplate* const*>(store);
		const AsciiString name = TheArmorStore ? TheArmorStore->getNameForArmorTemplate(value) : AsciiString();
		appendField(output, field.token, name.isEmpty() ? "None" : name.str(), indent);
		return TRUE;
	}

	if (field.parse == INI::parseDamageFX)
	{
		const DamageFX* value = *reinterpret_cast<const DamageFX* const*>(store);
		const AsciiString name = TheDamageFXStore ? TheDamageFXStore->getNameForDamageFX(value) : AsciiString();
		appendField(output, field.token, name.isEmpty() ? "None" : name.str(), indent);
		return TRUE;
	}

	if (field.parse == INI::parseWeaponTemplate)
	{
		const WeaponTemplate* value = *reinterpret_cast<const WeaponTemplate* const*>(store);
		appendField(output, field.token, value ? value->getName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseFXList)
	{
		const FXList* value = *reinterpret_cast<const FXList* const*>(store);
		const AsciiString name = TheFXListStore ? TheFXListStore->getNameForList(value) : AsciiString();
		appendField(output, field.token, name.isEmpty() ? "None" : name.str(), indent);
		return TRUE;
	}

	if (field.parse == INI::parseParticleSystemTemplate)
	{
		const ParticleSystemTemplate* value = *reinterpret_cast<const ParticleSystemTemplate* const*>(store);
		appendField(output, field.token, value ? value->getName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseObjectCreationList)
	{
		const ObjectCreationList* value = *reinterpret_cast<const ObjectCreationList* const*>(store);
		const AsciiString name = TheObjectCreationListStore ? TheObjectCreationListStore->getNameForList(value) : AsciiString();
		appendField(output, field.token, name.isEmpty() ? "None" : name.str(), indent);
		return TRUE;
	}

	if (field.parse == INI::parseUpgradeTemplate)
	{
		const UpgradeTemplate* value = *reinterpret_cast<const UpgradeTemplate* const*>(store);
		appendField(output, field.token, value ? value->getUpgradeName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseSpecialPowerTemplate)
	{
		const SpecialPowerTemplate* value = *reinterpret_cast<const SpecialPowerTemplate* const*>(store);
		appendField(output, field.token, value ? value->getName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseMappedImage)
	{
		const Image* value = *reinterpret_cast<const Image* const*>(store);
		appendField(output, field.token, value ? value->getName().str() : "None", indent);
		return TRUE;
	}

	if (field.parse == INI::parseScience)
	{
		const ScienceType value = *reinterpret_cast<const ScienceType*>(store);
		const AsciiString name = TheScienceStore ? TheScienceStore->getInternalNameForScience(value) : AsciiString();
		appendField(output, field.token, name.isEmpty() ? "None" : name.str(), indent);
		return TRUE;
	}

	if (field.parse == INI::parseUnsignedByte)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				static_cast<UnsignedInt>(
					*reinterpret_cast<const Byte*>(store))),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseShort)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				*reinterpret_cast<const Short*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseUnsignedShort)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				*reinterpret_cast<const UnsignedShort*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseInt)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				*reinterpret_cast<const Int*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseUnsignedInt)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				*reinterpret_cast<const UnsignedInt*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseReal ||
		field.parse == INI::parsePositiveNonZeroReal)
	{
		appendField(
			output,
			field.token,
			formatReal(
				*reinterpret_cast<const Real*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseBool)
	{
		appendField(
			output,
			field.token,
			*reinterpret_cast<const Bool*>(store)
			? "Yes"
			: "No",
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseAsciiString ||
		field.parse == INI::parseQuotedAsciiString)
	{
		appendField(
			output,
			field.token,
			formatAsciiString(
				*reinterpret_cast<const AsciiString*>(store)),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseAsciiStringVector ||
		field.parse == INI::parseAsciiStringVectorAppend ||
		field.parse == INI::parseSoundsList)
	{
		const std::vector<AsciiString>& values =
			*reinterpret_cast<
			const std::vector<AsciiString>*>(store);

		std::string value;

		for (const AsciiString& item : values)
		{
			if (!value.empty())
				value += " ";

			value += formatAsciiString(item);
		}

		appendField(
			output,
			field.token,
			value,
			indent);

		return TRUE;
	}

	// Reborn: Format parser-owned color and random-value structures directly from their canonical storage types.
	if (field.parse == INI::parseRGBColor)
	{
		const RGBColor& value = *reinterpret_cast<const RGBColor*>(store);
		appendField(output, field.token,
			"R:" + std::to_string(static_cast<Int>(value.red * 255.0f)) +
			" G:" + std::to_string(static_cast<Int>(value.green * 255.0f)) +
			" B:" + std::to_string(static_cast<Int>(value.blue * 255.0f)), indent);
		return TRUE;
	}

	if (field.parse == INI::parseRGBAColorInt)
	{
		const RGBAColorInt& value = *reinterpret_cast<const RGBAColorInt*>(store);
		appendField(output, field.token,
			"R:" + std::to_string(value.red) + " G:" + std::to_string(value.green) +
			" B:" + std::to_string(value.blue) + " A:" + std::to_string(value.alpha), indent);
		return TRUE;
	}

	if (field.parse == INI::parseColorInt)
	{
		const UnsignedInt value = static_cast<UnsignedInt>(*reinterpret_cast<const Color*>(store));
		appendField(output, field.token,
			"R:" + std::to_string((value >> 16) & 0xff) +
			" G:" + std::to_string((value >> 8) & 0xff) +
			" B:" + std::to_string(value & 0xff) +
			" A:" + std::to_string((value >> 24) & 0xff), indent);
		return TRUE;
	}

	if (field.parse == INI::parseGameClientRandomVariable)
	{
		const GameClientRandomVariable& value = *reinterpret_cast<const GameClientRandomVariable*>(store);
		const Int distribution = static_cast<Int>(value.getDistributionType());
		appendField(output, field.token,
			formatReal(value.getMinimumValue()) + " " + formatReal(value.getMaximumValue()) + " " +
			formatIndex(distribution, GameClientRandomVariable::DistributionTypeNames), indent);
		return TRUE;
	}

	if (field.parse == INI::parseByteSizedIndexList)
	{
		appendField(
			output,
			field.token,
			formatIndex(
				static_cast<Int>(
					*reinterpret_cast<const Byte*>(store)),
				field.userData),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseIndexList)
	{
		appendField(
			output,
			field.token,
			formatIndex(
				*reinterpret_cast<const Int*>(store),
				field.userData),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseLookupList)
	{
		appendField(
			output,
			field.token,
			formatLookup(
				*reinterpret_cast<const Int*>(store),
				field.userData),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseBitString8)
	{
		appendField(
			output,
			field.token,
			formatBitString(
				static_cast<UnsignedInt>(
					*reinterpret_cast<const Byte*>(store)),
				field.userData,
				8),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseBitString32)
	{
		appendField(
			output,
			field.token,
			formatBitString(
				*reinterpret_cast<const UnsignedInt*>(store),
				field.userData,
				32),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseBitInInt32)
	{
		const UnsignedInt value =
			*reinterpret_cast<const UnsignedInt*>(store);

		const UnsignedInt mask =
			static_cast<UnsignedInt>(
				reinterpret_cast<std::uintptr_t>(
					field.userData));

		appendField(
			output,
			field.token,
			(value & mask)
			? "Yes"
			: "No",
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseAngleReal)
	{
		const Real radians =
			*reinterpret_cast<const Real*>(store);

		const Real degrees =
			radians * 180.0f / PI;

		appendField(
			output,
			field.token,
			formatReal(degrees),
			indent);

		return TRUE;
	}

	// Reborn: Match registered flag parser functions and read names from the same BitFlags types.
	if (field.parse == KindOfMaskType::parseFromINI)
	{
		appendField(output, field.token,
			formatRegisteredFlags(*reinterpret_cast<const KindOfMaskType*>(store), KINDOF_COUNT), indent);
		return TRUE;
	}

	if (field.parse == ObjectStatusMaskType::parseFromINI)
	{
		appendField(output, field.token,
			formatRegisteredFlags(*reinterpret_cast<const ObjectStatusMaskType*>(store), OBJECT_STATUS_COUNT), indent);
		return TRUE;
	}

	if (field.parse == ModelConditionFlags::parseFromINI)
	{
		appendField(output, field.token,
			formatRegisteredFlags(*reinterpret_cast<const ModelConditionFlags*>(store), MODELCONDITION_COUNT), indent);
		return TRUE;
	}

	if (field.parse == WeaponSetFlags::parseFromINI)
	{
		appendField(output, field.token,
			formatRegisteredFlags(*reinterpret_cast<const WeaponSetFlags*>(store), WEAPONSET_COUNT), indent);
		return TRUE;
	}

	if (field.parse == ArmorSetFlags::parseFromINI)
	{
		appendField(output, field.token,
			formatRegisteredFlags(*reinterpret_cast<const ArmorSetFlags*>(store), ARMORSET_COUNT), indent);
		return TRUE;
	}

	if (field.parse == INI::parseDamageTypeFlags)
	{
		const DamageTypeFlags& flags = *reinterpret_cast<const DamageTypeFlags*>(store);
		appendField(output, field.token,
			formatAdditiveRegisteredFlags(flags, DAMAGE_NUM_TYPES), indent);
		return TRUE;
	}

	if (field.parse == INI::parseDeathTypeFlags)
	{
		appendField(output, field.token,
			formatAdditiveFlags(*reinterpret_cast<const DeathTypeFlags*>(store), INI::getDeathTypeNames(), DEATH_NUM_TYPES), indent);
		return TRUE;
	}

	if (field.parse == INI::parseVeterancyLevelFlags)
	{
		appendField(output, field.token,
			formatAdditiveFlags(*reinterpret_cast<const VeterancyLevelFlags*>(store), TheVeterancyNames, LEVEL_COUNT), indent);
		return TRUE;
	}

	if (field.parse == INI::parseAngularVelocityReal)
	{
		const Real radiansPerFrame = *reinterpret_cast<const Real*>(store);
		appendField(output, field.token,
			formatReal(radiansPerFrame / (SECONDS_PER_LOGICFRAME_REAL * (PI / 180.0f))), indent);
		return TRUE;
	}

	if (field.parse == INI::parseDurationReal)
	{
		const Real frames =
			*reinterpret_cast<const Real*>(store);

		appendField(
			output,
			field.token,
			formatReal(
				frames *
				MSEC_PER_LOGICFRAME_REAL),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseDurationUnsignedInt)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				durationFramesToMilliseconds(
					*reinterpret_cast<const UnsignedInt*>(
						store))),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseDurationUnsignedShort)
	{
		appendField(
			output,
			field.token,
			std::to_string(
				durationFramesToMilliseconds(
					static_cast<UnsignedInt>(
						*reinterpret_cast<
						const UnsignedShort*>(store)))),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseVelocityReal)
	{
		const Real value =
			*reinterpret_cast<const Real*>(store);

		appendField(
			output,
			field.token,
			formatReal(
				value /
				SECONDS_PER_LOGICFRAME_REAL),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseAccelerationReal)
	{
		const Real value =
			*reinterpret_cast<const Real*>(store);

		const Real factor =
			SECONDS_PER_LOGICFRAME_REAL *
			SECONDS_PER_LOGICFRAME_REAL;

		appendField(
			output,
			field.token,
			formatReal(value / factor),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parsePercentToReal)
	{
		const Real value =
			*reinterpret_cast<const Real*>(store);

		appendField(
			output,
			field.token,
			formatReal(value * 100.0f) + "%",
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseCoord2D)
	{
		const Coord2D& value =
			*reinterpret_cast<const Coord2D*>(store);

		appendField(
			output,
			field.token,
			"X:" +
			formatReal(value.x) +
			" Y:" +
			formatReal(value.y),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseCoord3D)
	{
		const Coord3D& value =
			*reinterpret_cast<const Coord3D*>(store);

		appendField(
			output,
			field.token,
			"X:" +
			formatReal(value.x) +
			" Y:" +
			formatReal(value.y) +
			" Z:" +
			formatReal(value.z),
			indent);

		return TRUE;
	}

	if (field.parse == INI::parseICoord2D)
	{
		const ICoord2D& value =
			*reinterpret_cast<const ICoord2D*>(store);

		appendField(
			output,
			field.token,
			"X:" +
			std::to_string(value.x) +
			" Y:" +
			std::to_string(value.y),
			indent);

		return TRUE;
	}

	return FALSE;
}
