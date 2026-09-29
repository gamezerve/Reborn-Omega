///////////////////////////////////////////////////////////////////////////////////////
// FILE: ParsedDefinitionCatalog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParsedDefinitionCatalog.h"

#include <algorithm>
#include <cctype>
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Build locale-independent lowercase keys for case-insensitive catalog indexes. */
//-------------------------------------------------------------------------------------------------
static std::string normalizeCatalogKey(const AsciiString& value)
{
	std::string key(value.str());
	std::transform(
		key.begin(),
		key.end(),
		key.begin(),
		[](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
	return key;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Combine normalized family and name into one unambiguous direct-lookup key. */
//-------------------------------------------------------------------------------------------------
static std::string makeDefinitionIdentityKey(
	const AsciiString& family,
	const AsciiString& name)
{
	return normalizeCatalogKey(family) + "\x1f" + normalizeCatalogKey(name);
}

void ParsedDefinitionCatalog::clear()
{
	m_definitions.clear();
	// Reborn: A reload invalidates both direct lookup indexes together with their backing records.
	m_identityIndex.clear();
	m_nameIndex.clear();
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
		const std::string identityKey = makeDefinitionIdentityKey(family, name);
		const auto existing = m_identityIndex.find(identityKey);
		if (existing != m_identityIndex.end())
		{
			// Reborn: Reload overrides update the indexed record without a full catalog scan.
			ParsedDefinition& definition = m_definitions[existing->second];
			definition.declaration = normalizedDeclaration;
			definition.blockType = blockType;
			definition.name = name;
			definition.filename = filename;
			definition.line = line;
			definition.loadType = loadType;
			return;
		}
	}

	ParsedDefinition definition;
	definition.declaration = normalizedDeclaration;
	definition.blockType = blockType;
	definition.name = name;
	definition.filename = filename;
	definition.line = line;
	definition.loadType = loadType;

	const size_t index = m_definitions.size();
	m_definitions.push_back(definition);
	if (!name.isEmpty())
	{
		// Reborn: Store stable vector indices so reallocations cannot invalidate the lookup tables.
		m_identityIndex[makeDefinitionIdentityKey(family, name)] = index;
		m_nameIndex[normalizeCatalogKey(name)].push_back(index);
	}
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

	const auto match = m_identityIndex.find(makeDefinitionIdentityKey(family, name));
	return match == m_identityIndex.end()
		? nullptr
		: &m_definitions[match->second];
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

	const auto candidates = m_nameIndex.find(normalizeCatalogKey(name));
	if (candidates == m_nameIndex.end())
		return nullptr;

	for (size_t index : candidates->second)
	{
		const ParsedDefinition& definition = m_definitions[index];

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
