///////////////////////////////////////////////////////////////////////////////////////
// FILE: TextDiff.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

struct TextDiffResult
{
	std::vector<Int> leftChangedLines;
	std::vector<Int> rightChangedLines;
	Int addedCount;
	Int removedCount;

	TextDiffResult()
		: addedCount(0),
		removedCount(0)
	{
	}
};

class TextDiff
{
public:
	static TextDiffResult compare(
		const std::string& left,
		const std::string& right);
};
