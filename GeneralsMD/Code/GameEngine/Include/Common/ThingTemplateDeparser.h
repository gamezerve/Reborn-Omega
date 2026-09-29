///////////////////////////////////////////////////////////////////////////////////////
// FILE: ThingTemplateDeparser.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

class ThingTemplate;
struct FieldParse;

class ThingTemplateDeparser
{
public:
	static std::string deparse(const ThingTemplate* thingTemplate);

	// Reborn: Custom Object field callbacks are referenced directly by the canonical FieldParse table.
	static Bool deparseTranslatedLabel(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseDisplayColor(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseKindOf(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseIntList(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool ignoreLegacyGeometry(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseGeometryBlock(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseWeaponSets(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseArmorSets(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparsePrerequisites(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparsePerUnitSounds(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparsePerUnitFX(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseMaxSimultaneous(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseMaxSimultaneousLinkKey(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseModules(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool deparseLocomotorSets(const FieldParse& field, const void* instance, std::string& output, const char* indent);
	static Bool ignoreFinalizedParseOperation(const FieldParse& field, const void* instance, std::string& output, const char* indent);
};
