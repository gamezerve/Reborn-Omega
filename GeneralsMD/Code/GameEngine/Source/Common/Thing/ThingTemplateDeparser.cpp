///////////////////////////////////////////////////////////////////////////////////////
// FILE: ThingTemplateDeparser.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "Common/ThingTemplateDeparser.h"

#include "Common/ThingTemplate.h"

AsciiString ThingTemplateDeparser::deparse(const ThingTemplate* thingTemplate)
{
	if (!thingTemplate)
		return AsciiString::TheEmptyString;

	const ThingTemplate* finalTemplate =
		static_cast<const ThingTemplate*>(thingTemplate->getFinalOverride());

	AsciiString output;

	output.concat("Object ");
	output.concat(finalTemplate->getName());
	output.concat("\r\n");
	output.concat("\r\n");
	output.concat("End\r\n");

	return output;
}
