///////////////////////////////////////////////////////////////////////////////////////
// FILE: ParsedDefinitionCatalog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParsedDefinitionCatalog.h"

#include <algorithm>
#include <sstream>
#include <string>

static AsciiString getDefinitionName(const AsciiString& declaration)
{
	std::istringstream stream(declaration.str());

	std::string type;
	std::string name;

	stream >> type;
	stream >> name;

	return name.c_str();
}

static AsciiString getDefinitionFamily(const AsciiString& blockType)
{
	if (blockType.compareNoCase("Object") == 0 ||
		blockType.compareNoCase("ObjectInherit") == 0 ||
		blockType.compareNoCase("ObjectReskin") == 0)
	{
		return "Object";
	}

	return blockType;
}

void ParsedDefinitionCatalog::clear()
{
	m_definitions.clear();
}

void ParsedDefinitionCatalog::add(
	const AsciiString& declaration,
	const AsciiString& blockType,
	const AsciiString& filename,
	UnsignedInt line,
	INILoadType loadType)
{
	AsciiString normalizedDeclaration = declaration;
	normalizedDeclaration.trim();

	const AsciiString name =
		getDefinitionName(normalizedDeclaration);

	const AsciiString family =
		getDefinitionFamily(blockType);

	if (!name.isEmpty())
	{
		for (ParsedDefinition& definition : m_definitions)
		{
			if (getDefinitionFamily(definition.blockType).compareNoCase(family) == 0 &&
				definition.name.compareNoCase(name) == 0)
			{
				definition.declaration = normalizedDeclaration;
				definition.blockType = blockType;
				definition.name = name;
				definition.filename = filename;
				definition.line = line;
				definition.loadType = loadType;
				return;
			}
		}
	}

	ParsedDefinition definition;
	definition.declaration = normalizedDeclaration;
	definition.blockType = blockType;
	definition.name = name;
	definition.filename = filename;
	definition.line = line;
	definition.loadType = loadType;

	m_definitions.push_back(definition);
}

const std::vector<ParsedDefinition>& ParsedDefinitionCatalog::getDefinitions() const
{
	return m_definitions;
}

// Reborn: Match a saved reference identity without retaining catalog element pointers.
const ParsedDefinition* ParsedDefinitionCatalog::findDefinition(
	const AsciiString& name,
	const AsciiString& blockType) const
{
	const AsciiString family =
		getDefinitionFamily(blockType);

	for (const ParsedDefinition& definition : m_definitions)
	{
		if (definition.name.compareNoCase(name) == 0 &&
			getDefinitionFamily(definition.blockType).compareNoCase(family) == 0)
		{
			return &definition;
		}
	}

	return nullptr;
}

// Reborn: Prefer an exact type-family hint and reject ambiguous same-name references.
const ParsedDefinition* ParsedDefinitionCatalog::resolveReference(
	const AsciiString& name,
	const AsciiString& typeHint) const
{
	const ParsedDefinition* uniqueMatch = nullptr;
	const ParsedDefinition* typedMatch = nullptr;
	Int matchCount = 0;
	Int typedMatchCount = 0;

	const AsciiString hintedFamily =
		getDefinitionFamily(typeHint);

	for (const ParsedDefinition& definition : m_definitions)
	{
		if (definition.name.compareNoCase(name) != 0)
			continue;

		uniqueMatch = &definition;
		++matchCount;

		if (!typeHint.isEmpty() &&
			(getDefinitionFamily(definition.blockType).compareNoCase(hintedFamily) == 0 ||
				definition.blockType.compareNoCase(typeHint) == 0))
		{
			typedMatch = &definition;
			++typedMatchCount;
		}
	}

	if (typedMatchCount == 1)
		return typedMatch;

	if (typedMatchCount > 1)
		return nullptr;

	return matchCount == 1
		? uniqueMatch
		: nullptr;
}

void ParsedDefinitionCatalog::capture(
	const AsciiString& declaration,
	const AsciiString& blockType,
	const AsciiString& filename,
	UnsignedInt line,
	INILoadType loadType,
	void* userData)
{
	ParsedDefinitionCatalog* catalog =
		static_cast<ParsedDefinitionCatalog*>(userData);

	if (!catalog)
		return;

	catalog->add(
		declaration,
		blockType,
		filename,
		line,
		loadType);
}
