///////////////////////////////////////////////////////////////////////////////////////
// FILE: TextDiff.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

// Reborn: Describe a visual gap without inserting placeholder text into either editor.
struct TextDiffLineGap
{
	Int line;
	Int count;
	Bool afterLine;

	TextDiffLineGap(Int lineIndex, Int lineCount, Bool after)
		: line(lineIndex),
		count(lineCount),
		afterLine(after)
	{
	}
};

struct TextDiffResult
{
	std::vector<Int> leftRemovedLines;
	std::vector<Int> rightAddedLines;
	std::vector<Int> leftModifiedLines;
	std::vector<Int> rightModifiedLines;
	std::vector<Int> leftMovedLines;
	std::vector<Int> rightMovedLines;
	std::vector<TextDiffLineGap> leftGaps;
	std::vector<TextDiffLineGap> rightGaps;
	Int addedCount;
	Int removedCount;
	Int modifiedCount;
	Int movedCount;

	TextDiffResult()
		: addedCount(0),
		removedCount(0),
		modifiedCount(0),
		movedCount(0)
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
