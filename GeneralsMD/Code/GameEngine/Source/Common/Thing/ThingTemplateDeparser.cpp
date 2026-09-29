///////////////////////////////////////////////////////////////////////////////////////
// FILE: ThingTemplateDeparser.cpp 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/INIFieldDeparser.h"
#include "Common/ThingTemplateDeparser.h"

#include "Common/INI.h"
#include "Common/ThingTemplate.h"
#include "Common/DamageFX.h"
#include "GameClient/FXList.h"
#include "GameLogic/Armor.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Weapon.h"

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
	//-------------------------------------------------------------------------------------------------
	/** Reborn: Return the INI spelling for a stored geometry type. */
	//-------------------------------------------------------------------------------------------------
	std::string formatGeometryType(GeometryType type)
	{
		const Int index = static_cast<Int>(type);
		const char* const* names =
			GeometryInfo::getGeometryTypeNames();

		return index >= GEOMETRY_FIRST && index < GEOMETRY_NUM_TYPES
			? names[index - GEOMETRY_FIRST]
			: std::to_string(index);
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Format a KindOf mask for nested model fields such as WeaponSet preferences. */
	//-------------------------------------------------------------------------------------------------
	std::string formatKindOfMask(const KindOfMaskType& mask)
	{
		const char* const* names =
			KindOfMaskType::getBitNames();
		std::string value;

		for (Int kind = KINDOF_FIRST;
			kind < KINDOF_COUNT;
			++kind)
		{
			if (!mask.test(kind))
				continue;

			if (!value.empty())
				value += " ";

			value += names[kind - KINDOF_FIRST];
		}

		return value.empty() ? "NONE" : value;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Format the conditions which select a final effective WeaponSet. */
	//-------------------------------------------------------------------------------------------------
	std::string formatWeaponSetConditions(
		const WeaponSetFlags& flags)
	{
		const char* const* names =
			WeaponSetFlags::getBitNames();
		std::string value;

		for (Int condition = 0;
			condition < WEAPONSET_COUNT;
			++condition)
		{
			if (!flags.test(condition))
				continue;

			if (!value.empty())
				value += " ";

			value += names[condition];
		}

		return value.empty() ? "NONE" : value;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Format the command sources stored for one WeaponSet slot. */
	//-------------------------------------------------------------------------------------------------
	std::string formatCommandSources(UnsignedInt mask)
	{
		const char* const* names =
			WeaponTemplateSet::getCommandSourceMaskNames();
		std::string value;

		for (UnsignedInt bit = 0;
			names[bit] != nullptr;
			++bit)
		{
			if ((mask & (1u << bit)) == 0)
				continue;

			if (!value.empty())
				value += " ";

			value += names[bit];
		}

		return value.empty() ? "NONE" : value;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit every final effective WeaponSet and its per-slot selection metadata. */
	//-------------------------------------------------------------------------------------------------
	void appendWeaponSets(
		const WeaponTemplateSetVector& weaponSets,
		std::string& output)
	{
		const char* const* slotNames =
			WeaponTemplateSet::getWeaponSlotTypeNames();

		for (WeaponTemplateSetVector::const_iterator setIt =
				weaponSets.begin();
			setIt != weaponSets.end();
			++setIt)
		{
			output += "  WeaponSet\r\n";
			INIFieldDeparser::appendField(
				output,
				"Conditions",
				formatWeaponSetConditions(
					setIt->getNthConditionsYes(0)),
				"    ");

			for (Int slot = 0;
				slot < WEAPONSLOT_COUNT;
				++slot)
			{
				const WeaponSlotType slotType =
					static_cast<WeaponSlotType>(slot);
				const WeaponTemplate* weapon =
					setIt->getNth(slotType);

				if (!weapon)
					continue;

				INIFieldDeparser::appendField(
					output,
					"Weapon",
					std::string(slotNames[slot]) + " " +
					weapon->getName().str(),
					"    ");
				INIFieldDeparser::appendField(
					output,
					"AutoChooseSources",
					std::string(slotNames[slot]) + " " +
					formatCommandSources(
						setIt->getNthCommandSourceMask(slotType)),
					"    ");
				INIFieldDeparser::appendField(
					output,
					"PreferredAgainst",
					std::string(slotNames[slot]) + " " +
					formatKindOfMask(
						setIt->getNthPreferredAgainstMask(slotType)),
					"    ");
			}

			INIFieldDeparser::appendField(
				output,
				"ShareWeaponReloadTime",
				setIt->isSharedReloadTime() ? "Yes" : "No",
				"    ");
			INIFieldDeparser::appendField(
				output,
				"WeaponLockSharedAcrossSets",
				setIt->isWeaponLockSharedAcrossSets() ? "Yes" : "No",
				"    ");
			output += "  End\r\n";
		}
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit the final prerequisite alternatives and science requirements. */
	//-------------------------------------------------------------------------------------------------
	void appendPrerequisites(
		const ThingTemplate& thingTemplate,
		std::string& output)
	{
		output += "  Prerequisites\r\n";

		for (Int prereqIndex = 0;
			prereqIndex < thingTemplate.getPrereqCount();
			++prereqIndex)
		{
			const ProductionPrerequisite* prereq =
				thingTemplate.getNthPrereq(prereqIndex);

			if (!prereq)
				continue;

			std::string objectNames;
			for (Int unitIndex = 0;
				unitIndex < prereq->getNumUnitPrereqs();
				++unitIndex)
			{
				const ThingTemplate* unit =
					prereq->getUnitPrereq(unitIndex);

				if (!unit)
					continue;

				if (!objectNames.empty())
					objectNames += " ";

				objectNames += unit->getName().str();
			}

			if (!objectNames.empty())
			{
				INIFieldDeparser::appendField(
					output,
					"Object",
					objectNames,
					"    ");
			}

			for (Int scienceIndex = 0;
				scienceIndex < prereq->getNumSciencePrereqs();
				++scienceIndex)
			{
				const ScienceType science =
					prereq->getSciencePrereq(scienceIndex);

				if (science == SCIENCE_INVALID || !TheScienceStore)
					continue;

				INIFieldDeparser::appendField(
					output,
					"Science",
					TheScienceStore->getInternalNameForScience(science).str(),
					"    ");
			}
		}

		output += "  End\r\n";
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Format the conditions which select a final effective ArmorSet. */
	//-------------------------------------------------------------------------------------------------
	std::string formatArmorSetConditions(
		const ArmorSetFlags& flags)
	{
		const char* const* names =
			ArmorSetFlags::getBitNames();
		std::string value;

		for (Int condition = 0;
			condition < ARMORSET_COUNT;
			++condition)
		{
			if (!flags.test(condition))
				continue;

			if (!value.empty())
				value += " ";

			value += names[condition];
		}

		return value.empty() ? "NONE" : value;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit every final effective ArmorSet using registered pointer-to-name lookups. */
	//-------------------------------------------------------------------------------------------------
	void appendArmorSets(
		const ArmorTemplateSetVector& armorSets,
		std::string& output)
	{
		for (ArmorTemplateSetVector::const_iterator setIt =
				armorSets.begin();
			setIt != armorSets.end();
			++setIt)
		{
			AsciiString armorName;
			AsciiString damageFXName;

			if (TheArmorStore)
			{
				armorName = TheArmorStore->getNameForArmorTemplate(
					setIt->getArmorTemplate());
			}

			if (TheDamageFXStore)
			{
				damageFXName = TheDamageFXStore->getNameForDamageFX(
					setIt->getDamageFX());
			}

			output += "  ArmorSet\r\n";
			INIFieldDeparser::appendField(
				output,
				"Conditions",
				formatArmorSetConditions(
					setIt->getNthConditionsYes(0)),
				"    ");
			INIFieldDeparser::appendField(
				output,
				"Armor",
				armorName.isEmpty() ? "None" : armorName.str(),
				"    ");
			INIFieldDeparser::appendField(
				output,
				"DamageFX",
				damageFXName.isEmpty() ? "None" : damageFXName.str(),
				"    ");
			output += "  End\r\n";
		}
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit arbitrary per-unit sound aliases and their registered audio events. */
	//-------------------------------------------------------------------------------------------------
	void appendPerUnitSounds(
		const PerUnitSoundMap& sounds,
		std::string& output)
	{
		output += "  UnitSpecificSounds\r\n";

		for (PerUnitSoundMap::const_iterator it = sounds.begin();
			it != sounds.end();
			++it)
		{
			INIFieldDeparser::appendField(
				output,
				it->first.str(),
				it->second.getEventName().isEmpty()
				? "NoSound"
				: it->second.getEventName().str(),
				"    ");
		}

		output += "  End\r\n";
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit arbitrary per-unit FX aliases using the FX store's reverse-name lookup. */
	//-------------------------------------------------------------------------------------------------
	void appendPerUnitFX(
		const PerUnitFXMap& effects,
		std::string& output)
	{
		output += "  UnitSpecificFX\r\n";

		for (PerUnitFXMap::const_iterator it = effects.begin();
			it != effects.end();
			++it)
		{
			const AsciiString effectName = TheFXListStore
				? TheFXListStore->getNameForList(it->second)
				: AsciiString();

			INIFieldDeparser::appendField(
				output,
				it->first.str(),
				effectName.isEmpty() ? "None" : effectName.str(),
				"    ");
		}

		output += "  End\r\n";
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Emit the final multi-shape geometry without duplicating legacy geometry fields. */
	//-------------------------------------------------------------------------------------------------
	void appendGeometryBlock(
		const GeometryInfo& geometry,
		std::string& output)
	{
		output += "  GeometryBlock\r\n";
		INIFieldDeparser::appendField(
			output,
			"IsSmall",
			geometry.getIsSmall() ? "Yes" : "No",
			"    ");

		for (Int index = 0;
			index < geometry.getShapeCount();
			++index)
		{
			const GeometryInfo::Shape& shape =
				geometry.getShape(index);

			output += "    Shape\r\n";
			INIFieldDeparser::appendField(
				output,
				"Type",
				formatGeometryType(shape.m_type),
				"      ");
			INIFieldDeparser::appendField(
				output,
				"MajorRadius",
				INIFieldDeparser::formatReal(shape.m_majorRadius),
				"      ");
			INIFieldDeparser::appendField(
				output,
				"MinorRadius",
				INIFieldDeparser::formatReal(shape.m_minorRadius),
				"      ");
			INIFieldDeparser::appendField(
				output,
				"Height",
				INIFieldDeparser::formatReal(shape.m_height),
				"      ");
			INIFieldDeparser::appendField(
				output,
				"Offset",
				"X:" + INIFieldDeparser::formatReal(shape.m_offset.x) +
				" Y:" + INIFieldDeparser::formatReal(shape.m_offset.y) +
				" Z:" + INIFieldDeparser::formatReal(shape.m_offset.z),
				"      ");
			output += "    End\r\n";
		}

		output += "  End\r\n";
	}

}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse a translated field through the original label offset stored by its parser row. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseTranslatedLabel(const FieldParse& field, const void* instance,
	std::string& output, const char* indent)
{
	if (!field.userData)
		return false;

	const AsciiString& label = *reinterpret_cast<const AsciiString*>(
		reinterpret_cast<const char*>(instance) + reinterpret_cast<std::uintptr_t>(field.userData));
	if (!label.isEmpty())
		INIFieldDeparser::appendField(output, field.token, label.str(), indent);
	return true;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse DisplayColor through the callback stored on its canonical FieldParse row. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseDisplayColor(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const UnsignedInt color = static_cast<UnsignedInt>(
		*reinterpret_cast<const Color*>(reinterpret_cast<const char*>(instance) + field.offset));
	INIFieldDeparser::appendField(output, field.token,
		"R:" + std::to_string((color >> 16) & 0xff) +
		" G:" + std::to_string((color >> 8) & 0xff) +
		" B:" + std::to_string(color & 0xff) +
		" A:" + std::to_string((color >> 24) & 0xff), indent);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse KindOf using the stored mask and its canonical BitFlags name table. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseKindOf(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const KindOfMaskType& kindOf = *reinterpret_cast<const KindOfMaskType*>(
		reinterpret_cast<const char*>(instance) + field.offset);
	INIFieldDeparser::appendField(output, field.token, formatKindOfMask(kindOf), indent);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse any fixed-size integer list from its canonical FieldParse offset and count. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseIntList(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const Int* values = reinterpret_cast<const Int*>(reinterpret_cast<const char*>(instance) + field.offset);
	const Int count = static_cast<Int>(reinterpret_cast<std::uintptr_t>(field.userData));
	std::string value;
	for (Int index = 0; index < count; ++index)
	{
		if (!value.empty()) value += " ";
		value += std::to_string(values[index]);
	}
	INIFieldDeparser::appendField(output, field.token, value, indent);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Suppress legacy scalar geometry rows because GeometryBlock emits the same canonical state. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::ignoreLegacyGeometry(const FieldParse&, const void*, std::string&, const char*)
{
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse final geometry through the callback on the canonical GeometryBlock row. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseGeometryBlock(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendGeometryBlock(static_cast<const ThingTemplate*>(instance)->getTemplateGeometryInfo(), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse final WeaponSet blocks from the canonical Object field record. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseWeaponSets(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendWeaponSets(static_cast<const ThingTemplate*>(instance)->getWeaponTemplateSets(), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse final ArmorSet blocks from the canonical Object field record. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseArmorSets(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendArmorSets(static_cast<const ThingTemplate*>(instance)->getArmorTemplateSets(), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse final prerequisites from the canonical Object field record. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparsePrerequisites(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendPrerequisites(*static_cast<const ThingTemplate*>(instance), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse per-unit sound aliases from the canonical Object field record. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparsePerUnitSounds(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendPerUnitSounds(static_cast<const ThingTemplate*>(instance)->getPerUnitSoundsForDeparser(), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse per-unit FX aliases from the canonical Object field record. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparsePerUnitFX(const FieldParse&, const void* instance, std::string& output, const char*)
{
	appendPerUnitFX(static_cast<const ThingTemplate*>(instance)->getPerUnitFXForDeparser(), output);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse the special MaxSimultaneous keyword or its stored numeric value. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseMaxSimultaneous(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const ThingTemplate& thingTemplate = *static_cast<const ThingTemplate*>(instance);
	INIFieldDeparser::appendField(output, field.token,
		thingTemplate.isMaxSimultaneousDeterminedBySuperweaponRestriction()
		? "DeterminedBySuperweaponRestriction"
		: std::to_string(thingTemplate.getMaxSimultaneousOfType()), indent);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse a registered MaxSimultaneous link key without inventing a duplicate name table. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseMaxSimultaneousLinkKey(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const NameKeyType linkKey = static_cast<const ThingTemplate*>(instance)->getMaxSimultaneousLinkKey();
	if (linkKey != NAMEKEY_INVALID)
		INIFieldDeparser::appendField(output, field.token, KEYNAME(linkKey).str(), indent);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse registered modules by walking their canonical ModuleFactory FieldParse chains. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseModules(const FieldParse& field, const void* instance, std::string& output, const char* indent)
{
	const Int fieldType = static_cast<Int>(reinterpret_cast<std::uintptr_t>(field.userData));
	const Bool bodyField = fieldType == 999;
	const ModuleType moduleType = bodyField ? MODULETYPE_BEHAVIOR : static_cast<ModuleType>(fieldType);
	const ModuleInfo& modules = *reinterpret_cast<const ModuleInfo*>(
		reinterpret_cast<const char*>(instance) + field.offset);

	for (Int moduleIndex = 0; moduleIndex < modules.getCount(); ++moduleIndex)
	{
		const Bool bodyModule = (modules.getNthInterfaceMask(moduleIndex) & MODULEINTERFACE_BODY) != 0;
		if (moduleType == MODULETYPE_BEHAVIOR && bodyModule != bodyField)
			continue;

		const AsciiString moduleName = modules.getNthName(moduleIndex);
		const AsciiString moduleTag = modules.getNthTag(moduleIndex);
		const ModuleData* moduleData = modules.getNthData(moduleIndex);
		if (!moduleData || !TheModuleFactory)
			continue;

		output += indent;
		output += field.token;
		output += " = ";
		output += moduleName.str();
		output += " ";
		output += moduleTag.str();
		output += "\r\n";

		MultiIniFieldParse fieldChain;
		if (TheModuleFactory->buildModuleFieldParse(moduleName, moduleType, fieldChain))
		{
			std::string nestedIndent = indent;
			nestedIndent += "  ";

			for (Int tableIndex = 0; tableIndex < fieldChain.getCount(); ++tableIndex)
			{
				const FieldParse* moduleFields = fieldChain.getNthFieldParse(tableIndex);
				const char* tableInstance = reinterpret_cast<const char*>(moduleData) +
					fieldChain.getNthExtraOffset(tableIndex);

				for (const FieldParse* moduleField = moduleFields;
					moduleField && moduleField->token;
					++moduleField)
				{
					if (!INIFieldDeparser::deparseField(*moduleField, tableInstance, output, nestedIndent.c_str()))
					{
						output += nestedIndent;
						output += "; Unsupported field: ";
						output += moduleField->token;
						output += "\r\n";
					}
				}
			}
		}

		output += indent;
		output += "End\r\n";
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse final Locomotor assignments with the set names and template names owned by the game. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::deparseLocomotorSets(const FieldParse& field, const void* instance,
	std::string& output, const char* indent)
{
	const AIUpdateModuleData* aiData = static_cast<const ThingTemplate*>(instance)->friend_getAIModuleInfo();
	if (!aiData)
		return TRUE;

	const char* const* setNames = AIUpdateModuleData::getLocomotorSetNames();
	for (Int setIndex = 0; setIndex < LOCOMOTORSET_COUNT; ++setIndex)
	{
		const LocomotorTemplateVector* locomotors = aiData->findLocomotorTemplateVector(
			static_cast<LocomotorSetType>(setIndex));
		if (!locomotors || locomotors->empty())
			continue;

		std::string value = setNames[setIndex];
		for (LocomotorTemplateVector::const_iterator it = locomotors->begin(); it != locomotors->end(); ++it)
		{
			if (!*it)
				continue;
			value += " ";
			value += (*it)->getName().str();
		}

		INIFieldDeparser::appendField(output, field.token, value, indent);
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Ignore parse-time mutation directives after their final effective module state is emitted. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplateDeparser::ignoreFinalizedParseOperation(const FieldParse&, const void*, std::string&, const char*)
{
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Deparse the final effective Object definition, including Object-specific model fields. */
//-------------------------------------------------------------------------------------------------
std::string ThingTemplateDeparser::deparse(
	const ThingTemplate* thingTemplate)
{
	if (!thingTemplate)
		return std::string();

	const ThingTemplate* finalTemplate =
		static_cast<const ThingTemplate*>(
			thingTemplate->getFinalOverride());

	std::string output;

	output += "Object ";
	output += finalTemplate->getName().str();
	output += "\r\n\r\n";

	const FieldParse* fields =
		finalTemplate->getFieldParse();

	std::string unsupportedFields;

	for (const FieldParse* field = fields;
		field && field->token;
		++field)
	{
		if (!INIFieldDeparser::deparseField(
			*field,
			finalTemplate,
			output))
		{
			unsupportedFields += "; Unsupported field: ";
			unsupportedFields += field->token;
			unsupportedFields += "\r\n";
		}
	}

	if (!unsupportedFields.empty())
	{
		output += "\r\n";
		output += unsupportedFields;
	}

	output += "\r\nEnd\r\n";

	return output;
}
