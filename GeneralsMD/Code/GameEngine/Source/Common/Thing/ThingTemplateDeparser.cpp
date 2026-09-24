///////////////////////////////////////////////////////////////////////////////////////
// FILE: ThingTemplateDeparser.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/ThingTemplateDeparser.h"

#include "Common/INI.h"
#include "Common/ThingTemplate.h"

#include <cstdio>
#include <vector>

static void appendField(
	std::string& output,
	const char* token,
	const std::string& value)
{
	output += "  ";
	output += token;
	output += " = ";
	output += value;
	output += "\r\n";
}

static std::string formatReal(Real value)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
	return buffer;
}

static std::string formatAsciiString(const AsciiString& value)
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

static std::string formatIndex(
	Byte value,
	const void* userData)
{
	const char* const* names =
		static_cast<const char* const*>(userData);

	if (!names)
		return std::to_string(static_cast<unsigned int>(value));

	for (Int i = 0; names[i] != nullptr; ++i)
	{
		if (i == value)
			return names[i];
	}

	return std::to_string(static_cast<unsigned int>(value));
}

static std::string formatBitString8(
	Byte value,
	const void* userData)
{
	const char* const* names =
		static_cast<const char* const*>(userData);

	if (!names || value == 0)
		return "None";

	std::string result;

	for (Int i = 0; i < 8 && names[i] != nullptr; ++i)
	{
		if ((value & (1 << i)) == 0)
			continue;

		if (!result.empty())
			result += " ";

		result += names[i];
	}

	if (result.empty())
		return "None";

	return result;
}

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

	const FieldParse* fields = finalTemplate->getFieldParse();

	for (const FieldParse* field = fields;
		field && field->token;
		++field)
	{
		const char* store =
			reinterpret_cast<const char*>(finalTemplate) +
			field->offset;

		if (field->parse == INI::parseUnsignedByte)
		{
			appendField(
				output,
				field->token,
				std::to_string(
					static_cast<unsigned int>(
						*reinterpret_cast<const Byte*>(store))));
		}
		else if (field->parse == INI::parseShort)
		{
			appendField(
				output,
				field->token,
				std::to_string(
					*reinterpret_cast<const Short*>(store)));
		}
		else if (field->parse == INI::parseUnsignedShort)
		{
			appendField(
				output,
				field->token,
				std::to_string(
					*reinterpret_cast<const UnsignedShort*>(store)));
		}
		else if (field->parse == INI::parseInt)
		{
			appendField(
				output,
				field->token,
				std::to_string(
					*reinterpret_cast<const Int*>(store)));
		}
		else if (field->parse == INI::parseUnsignedInt)
		{
			appendField(
				output,
				field->token,
				std::to_string(
					*reinterpret_cast<const UnsignedInt*>(store)));
		}
		else if (
			field->parse == INI::parseReal ||
			field->parse == INI::parsePositiveNonZeroReal)
		{
			appendField(
				output,
				field->token,
				formatReal(
					*reinterpret_cast<const Real*>(store)));
		}
		else if (field->parse == INI::parseBool)
		{
			appendField(
				output,
				field->token,
				*reinterpret_cast<const Bool*>(store)
				? "Yes"
				: "No");
		}
		else if (
			field->parse == INI::parseAsciiString ||
			field->parse == INI::parseQuotedAsciiString)
		{
			appendField(
				output,
				field->token,
				formatAsciiString(
					*reinterpret_cast<const AsciiString*>(store)));
		}
		else if (field->parse == INI::parseAsciiStringVector)
		{
			const std::vector<AsciiString>& values =
				*reinterpret_cast<const std::vector<AsciiString>*>(store);

			std::string value;

			for (const AsciiString& item : values)
			{
				if (!value.empty())
					value += " ";

				value += formatAsciiString(item);
			}

			appendField(output, field->token, value);
		}
		else if (field->parse == INI::parseByteSizedIndexList)
		{
			appendField(
				output,
				field->token,
				formatIndex(
					*reinterpret_cast<const Byte*>(store),
					field->userData));
		}
		else if (field->parse == INI::parseBitString8)
		{
			appendField(
				output,
				field->token,
				formatBitString8(
					*reinterpret_cast<const Byte*>(store),
					field->userData));
		}
		else if (field->parse == INI::parseAngleReal)
		{
			const Real radians =
				*reinterpret_cast<const Real*>(store);

			const Real degrees =
				radians * 180.0f / PI;

			appendField(
				output,
				field->token,
				formatReal(degrees));
		}
	}

	output += "\r\nEnd\r\n";

	return output;
}
