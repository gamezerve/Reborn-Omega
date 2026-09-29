///////////////////////////////////////////////////////////////////////////////////////
// FILE: ParsedDefinitionCatalog.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AsciiString.h"
#include "Common/INI.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ParsedDefinition
{
	AsciiString declaration;
	AsciiString blockType;
	AsciiString name;
	AsciiString filename;
	UnsignedInt line;
	INILoadType loadType;
};

class ParsedDefinitionCatalog
{
public:
	void clear();

	void add(
		const AsciiString& declaration,
		const AsciiString& blockType,
		const AsciiString& filename,
		UnsignedInt line,
		INILoadType loadType);

	const std::vector<ParsedDefinition>& getDefinitions() const;

	// Reborn: Resolve a definition by both its registered name and normalized type family.
	const ParsedDefinition* findDefinition(
		const AsciiString& name,
		const AsciiString& blockType) const;

	// Reborn: Resolve text references only when name/type context identifies one safe target.
	const ParsedDefinition* resolveReference(
		const AsciiString& name,
		const AsciiString& typeHint) const;

	static void capture(
		const AsciiString& declaration,
		const AsciiString& blockType,
		const AsciiString& filename,
		UnsignedInt line,
		INILoadType loadType,
		void* userData);

private:
	std::vector<ParsedDefinition> m_definitions;
	// Reborn: Resolve link candidates by indexed lowercase identity instead of scanning every definition.
	std::unordered_map<std::string, size_t> m_identityIndex;
	std::unordered_map<std::string, std::vector<size_t>> m_nameIndex;
};
