///////////////////////////////////////////////////////////////////////////////////////
// FILE: ThingTemplateDeparser.cpp 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/INIFieldDeparser.h"
#include "Common/ThingTemplateDeparser.h"

#include "Common/INI.h"
#include "Common/ThingTemplate.h"

#include <cstdio>
#include <vector>

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
