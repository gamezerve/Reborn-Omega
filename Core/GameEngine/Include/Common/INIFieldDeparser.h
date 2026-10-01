///////////////////////////////////////////////////////////////////////////////////////
// FILE: INIFieldDeparser.h 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AsciiString.h"

#include <string>

struct FieldParse;

class INIFieldDeparser
{
public:
	static Bool deparseField(
		const FieldParse& field,
		const void* instance,
		std::string& output,
		const char* indent = "  ");

	static void appendField(
		std::string& output,
		const char* token,
		const std::string& value,
		const char* indent = "  ");

	static std::string formatReal(Real value);
	static std::string formatAsciiString(const AsciiString& value);
};
