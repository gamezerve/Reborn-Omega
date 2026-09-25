///////////////////////////////////////////////////////////////////////////////////////
// FILE: INIFieldDeparser.cpp 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/INIFieldDeparser.h"

#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "Common/INI.h"

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

	const char* store =
		reinterpret_cast<const char*>(instance) +
		field.offset;

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

	if (field.parse == INI::parseAsciiStringVector)
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
