///////////////////////////////////////////////////////////////////////////////////////
// FILE: ParsedDefinitionCatalog.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AsciiString.h"
#include "Common/INI.h"

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

	static void capture(
		const AsciiString& declaration,
		const AsciiString& blockType,
		const AsciiString& filename,
		UnsignedInt line,
		INILoadType loadType,
		void* userData);

private:
	std::vector<ParsedDefinition> m_definitions;
};
