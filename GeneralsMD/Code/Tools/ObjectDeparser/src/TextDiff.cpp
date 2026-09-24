///////////////////////////////////////////////////////////////////////////////////////
// FILE: TextDiff.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TextDiff.h"

#include <dtl/dtl.hpp>

#include <sstream>

static std::vector<std::string> splitLines(const std::string& text)
{
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string line;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.find("; Object INI Load Completed:") == 0)
			line = "; Object INI Load Completed:";

		lines.push_back(line);
	}

	return lines;
}

TextDiffResult TextDiff::compare(
	const std::string& left,
	const std::string& right)
{
	const std::vector<std::string> leftLines = splitLines(left);
	const std::vector<std::string> rightLines = splitLines(right);

	dtl::Diff<std::string, std::vector<std::string>> diff(
		leftLines,
		rightLines);

	diff.compose();

	const auto ses = diff.getSes().getSequence();

	TextDiffResult result;

	Int leftLine = 0;
	Int rightLine = 0;

	for (const auto& entry : ses)
	{
		switch (entry.second.type)
		{
		case dtl::SES_COMMON:
			++leftLine;
			++rightLine;
			break;

		case dtl::SES_DELETE:
			result.leftChangedLines.push_back(leftLine);
			++result.removedCount;
			++leftLine;
			break;

		case dtl::SES_ADD:
			result.rightChangedLines.push_back(rightLine);
			++result.addedCount;
			++rightLine;
			break;
		}
	}

	return result;
}
