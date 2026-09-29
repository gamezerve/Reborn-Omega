///////////////////////////////////////////////////////////////////////////////////////
// FILE: TextDiff.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TextDiff.h"

#include <dtl/dtl.hpp>

#include <algorithm>
#include <sstream>

//-------------------------------------------------------------------------------------------------
/** Reborn: Split editor content into stable comparison lines while ignoring the reload timestamp. */
//-------------------------------------------------------------------------------------------------
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

namespace
{
	struct ComparisonBlockContext
	{
		Int indent;
		std::string identity;
	};

	struct ComparisonLineInfo
	{
		std::string exactKey;
		std::string replacementKey;
	};

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Normalize one structural line without changing the editor text used for display. */
	//-------------------------------------------------------------------------------------------------
	std::string trimComparisonLine(const std::string& line)
	{
		const std::string::size_type first = line.find_first_not_of(" \t");
		if (first == std::string::npos)
			return std::string();
		const std::string::size_type last = line.find_last_not_of(" \t");
		return line.substr(first, last - first + 1);
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Normalize layout whitespace outside quoted values so formatting columns do not split blocks. */
	//-------------------------------------------------------------------------------------------------
	std::string canonicalizeComparisonLine(const std::string& line)
	{
		const std::string trimmed = trimComparisonLine(line);
		std::string canonical;
		canonical.reserve(trimmed.size());
		Bool quoted = FALSE;
		Bool pendingSpace = FALSE;

		for (char character : trimmed)
		{
			if (character == '"')
			{
				if (pendingSpace && !canonical.empty() && canonical.back() != '=')
					canonical += ' ';
				pendingSpace = FALSE;
				quoted = !quoted;
				canonical += character;
				continue;
			}

			if (!quoted && (character == ' ' || character == '\t'))
			{
				pendingSpace = TRUE;
				continue;
			}

			if (!quoted && character == '=')
			{
				while (!canonical.empty() && canonical.back() == ' ')
					canonical.pop_back();
				canonical += character;
				pendingSpace = FALSE;
				continue;
			}

			if (pendingSpace && !canonical.empty() && canonical.back() != '=')
				canonical += ' ';
			pendingSpace = FALSE;
			canonical += character;
		}

		return canonical;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Convert tabs and spaces into a stable relative indentation depth for block discovery. */
	//-------------------------------------------------------------------------------------------------
	Int getComparisonIndent(const std::string& line)
	{
		Int indent = 0;
		for (char character : line)
		{
			if (character == ' ')
				++indent;
			else if (character == '\t')
				indent += 4;
			else
				break;
		}
		return indent;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Build a comparison key from the complete dynamic module/block ancestry. */
	//-------------------------------------------------------------------------------------------------
	std::string makeContextualLineKey(
		const std::vector<ComparisonBlockContext>& contexts,
		const std::string& line)
	{
		std::string key;
		for (const ComparisonBlockContext& context : contexts)
		{
			key += '\x1e';
			key += context.identity;
		}
		key += '\x1f';
		key += line;
		return key;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Preserve the complete dynamic block header so repeated module types keep distinct instance tags. */
	//-------------------------------------------------------------------------------------------------
	std::string makeBlockIdentity(const std::string& header)
	{
		return header;
	}

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Pair changed leaf values only inside the same structural owner. */
	//-------------------------------------------------------------------------------------------------
	std::string makeLeafReplacementKey(
		const std::vector<ComparisonBlockContext>& contexts,
		const std::string& trimmed)
	{
		const std::string::size_type equals = trimmed.find('=');
		const std::string field = equals == std::string::npos
			? std::string("<leaf>")
			: trimComparisonLine(trimmed.substr(0, equals));
		return makeContextualLineKey(contexts, field);
	}
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Derive nested comparison paths from indentation and End structure without field-name lists. */
//-------------------------------------------------------------------------------------------------
static std::vector<ComparisonLineInfo> buildComparisonKeys(const std::vector<std::string>& lines)
{
	std::vector<ComparisonLineInfo> keys;
	keys.reserve(lines.size());
	std::vector<ComparisonBlockContext> contexts;
	Bool rootDeclarationFound = FALSE;

	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
	{
		const std::string& line = lines[lineIndex];
		const std::string trimmed = trimComparisonLine(line);
		const std::string canonical = canonicalizeComparisonLine(line);
		const Int indent = getComparisonIndent(line);
		const Bool isComment = !trimmed.empty() && trimmed.front() == ';';
		const Bool isEnd = trimmed == "End";

		if (isEnd)
		{
			while (!contexts.empty() && contexts.back().indent > indent)
				contexts.pop_back();
		}
		else if (!trimmed.empty() && !isComment)
		{
			while (!contexts.empty() && contexts.back().indent >= indent)
				contexts.pop_back();
		}

		size_t nextIndex = lineIndex + 1;
		while (nextIndex < lines.size())
		{
			const std::string nextTrimmed = trimComparisonLine(lines[nextIndex]);
			if (!nextTrimmed.empty() && nextTrimmed.front() != ';')
				break;
			++nextIndex;
		}

		Bool isBlockHeader = FALSE;
		if (!isEnd && !trimmed.empty() && !isComment && nextIndex < lines.size())
		{
			const std::string nextTrimmed = trimComparisonLine(lines[nextIndex]);
			const Int nextIndent = getComparisonIndent(lines[nextIndex]);
			const Bool hasIndentedBody = nextIndent > indent;
			const Bool hasEmptyBody = nextTrimmed == "End" && nextIndent == indent;
			const Bool rootCandidate = indent == 0 && trimmed.find('=') == std::string::npos;
			const Bool isRootDeclaration = rootCandidate && !rootDeclarationFound;
			if (isRootDeclaration)
				rootDeclarationFound = TRUE;
			isBlockHeader = (hasIndentedBody || hasEmptyBody) && !isRootDeclaration;
		}

		ComparisonLineInfo info;
		// Reborn: Compare structural content independently of tabs, indentation width and trailing spaces.
		info.exactKey = makeContextualLineKey(contexts, canonical);
		if (isEnd)
			info.replacementKey = makeContextualLineKey(contexts, "<end>");
		else if (isBlockHeader)
			info.replacementKey = makeContextualLineKey(contexts,
				makeBlockIdentity(canonical));
		else if (!rootDeclarationFound || indent != 0 || trimmed.find('=') != std::string::npos)
			info.replacementKey = makeLeafReplacementKey(contexts, trimmed);
		else
			info.replacementKey = "<root-declaration>";
		keys.push_back(info);

		if (isEnd)
		{
			if (!contexts.empty() && contexts.back().indent == indent)
				contexts.pop_back();
			continue;
		}

		if (isBlockHeader)
		{
			// Reborn: Every discovered block, including its instance tag, owns its descendants.
			ComparisonBlockContext context;
			context.indent = indent;
			context.identity = makeBlockIdentity(canonical);
			contexts.push_back(context);
		}
	}

	return keys;
}

namespace
{
	struct ChangedLine
	{
		Int line;
		std::string text;
		std::string comparisonKey;
		std::string replacementKey;
		Bool moved;
		Bool paired;

		// Reborn: Keep diff classification state separate from the immutable editor text.
		ChangedLine(Int lineIndex, const std::string& value,
			const std::string& key, const std::string& replaceKey)
			: line(lineIndex),
			text(value),
			comparisonKey(key),
			replacementKey(replaceKey),
			moved(FALSE),
			paired(FALSE)
		{
		}
	};

	struct DiffHunk
	{
		std::vector<Int> deleted;
		std::vector<Int> added;
		Int leftResumeLine;
		Int rightResumeLine;

		// Reborn: A hunk groups adjacent edits so replacements can be paired before counting additions.
		DiffHunk()
			: leftResumeLine(0),
			rightResumeLine(0)
		{
		}
	};

	//-------------------------------------------------------------------------------------------------
	/** Reborn: Append the visual padding required to align both editors after one diff hunk. */
	//-------------------------------------------------------------------------------------------------
	void appendAlignmentGap(
		std::vector<TextDiffLineGap>& gaps,
		Int resumeLine,
		Int lineCount,
		Int missingLineCount)
	{
		if (missingLineCount <= 0 || lineCount <= 0)
			return;

		const Bool afterLine = resumeLine >= lineCount;
		const Int targetLine = afterLine ? lineCount - 1 : resumeLine;
		gaps.push_back(TextDiffLineGap(targetLine, missingLineCount, afterLine));
	}
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Build an aligned, move-aware line diff without altering either editor's source text. */
//-------------------------------------------------------------------------------------------------
TextDiffResult TextDiff::compare(
	const std::string& left,
	const std::string& right)
{
	const std::vector<std::string> leftLines = splitLines(left);
	const std::vector<std::string> rightLines = splitLines(right);
	const std::vector<ComparisonLineInfo> leftInfo = buildComparisonKeys(leftLines);
	const std::vector<ComparisonLineInfo> rightInfo = buildComparisonKeys(rightLines);
	std::vector<std::string> leftKeys;
	std::vector<std::string> rightKeys;
	leftKeys.reserve(leftInfo.size());
	rightKeys.reserve(rightInfo.size());
	for (const ComparisonLineInfo& info : leftInfo)
		leftKeys.push_back(info.exactKey);
	for (const ComparisonLineInfo& info : rightInfo)
		rightKeys.push_back(info.exactKey);

	dtl::Diff<std::string, std::vector<std::string>> diff(
		leftKeys,
		rightKeys);

	diff.compose();

	const auto ses = diff.getSes().getSequence();

	Int leftLine = 0;
	Int rightLine = 0;
	std::vector<ChangedLine> deletedLines;
	std::vector<ChangedLine> addedLines;
	std::vector<DiffHunk> hunks;
	DiffHunk currentHunk;
	Bool hunkActive = FALSE;

	auto finishHunk = [&]()
	{
		if (!hunkActive)
			return;
		currentHunk.leftResumeLine = leftLine;
		currentHunk.rightResumeLine = rightLine;
		hunks.push_back(currentHunk);
		currentHunk = DiffHunk();
		hunkActive = FALSE;
	};

	for (const auto& entry : ses)
	{
		switch (entry.second.type)
		{
		case dtl::SES_COMMON:
			finishHunk();
			++leftLine;
			++rightLine;
			break;

		case dtl::SES_DELETE:
			hunkActive = TRUE;
			currentHunk.deleted.push_back(static_cast<Int>(deletedLines.size()));
			deletedLines.push_back(ChangedLine(leftLine, leftLines[leftLine],
				leftKeys[leftLine], leftInfo[leftLine].replacementKey));
			++leftLine;
			break;

		case dtl::SES_ADD:
			hunkActive = TRUE;
			currentHunk.added.push_back(static_cast<Int>(addedLines.size()));
			addedLines.push_back(ChangedLine(rightLine, rightLines[rightLine],
				rightKeys[rightLine], rightInfo[rightLine].replacementKey));
			++rightLine;
			break;
		}
	}
	finishHunk();

	TextDiffResult result;

	// Reborn: Match unchanged text across different hunks as moved lines, preserving duplicate order.
	for (ChangedLine& deleted : deletedLines)
	{
		if (deleted.text.empty())
			continue;

		for (ChangedLine& added : addedLines)
		{
			if (added.moved || deleted.comparisonKey != added.comparisonKey)
				continue;

			deleted.moved = TRUE;
			added.moved = TRUE;
			result.leftMovedLines.push_back(deleted.line);
			result.rightMovedLines.push_back(added.line);
			++result.movedCount;
			break;
		}
	}

	for (const DiffHunk& hunk : hunks)
	{
		std::vector<Int> remainingDeleted;
		std::vector<Int> remainingAdded;

		for (Int index : hunk.deleted)
		{
			if (!deletedLines[index].moved)
				remainingDeleted.push_back(index);
		}
		for (Int index : hunk.added)
		{
			if (!addedLines[index].moved)
				remainingAdded.push_back(index);
		}

		// Reborn: Parenthesize std::min so the Win32 min macro cannot rewrite the diff pairing call.
		Int pairedCount = 0;
		for (Int deletedIndex : remainingDeleted)
		{
			ChangedLine& deleted = deletedLines[deletedIndex];
			ChangedLine* matchedAdded = nullptr;
			for (Int addedIndex : remainingAdded)
			{
				ChangedLine& candidate = addedLines[addedIndex];
				if (!candidate.paired && deleted.replacementKey == candidate.replacementKey)
				{
					matchedAdded = &candidate;
					break;
				}
			}
			if (matchedAdded == nullptr)
				continue;

			ChangedLine& added = *matchedAdded;
			deleted.paired = TRUE;
			added.paired = TRUE;
			result.leftModifiedLines.push_back(deleted.line);
			result.rightModifiedLines.push_back(added.line);
			++result.modifiedCount;
			++pairedCount;
		}

		const Int unpairedDeletedCount = static_cast<Int>(remainingDeleted.size()) - pairedCount;
		const Int unpairedAddedCount = static_cast<Int>(remainingAdded.size()) - pairedCount;
		if (pairedCount == 0 && unpairedDeletedCount > 0 && unpairedAddedCount > 0)
		{
			// Reborn: Give unrelated replacement blocks independent rows instead of placing them side by side.
			const Int lastDeletedLine = deletedLines[remainingDeleted.back()].line;
			const Int firstAddedLine = addedLines[remainingAdded.front()].line;
			result.leftGaps.push_back(TextDiffLineGap(lastDeletedLine, unpairedAddedCount, TRUE));
			result.rightGaps.push_back(TextDiffLineGap(firstAddedLine, unpairedDeletedCount, FALSE));
		}
		else
		{
			// Reborn: Align ordinary replacements by only the unmatched tail after compatible fields are paired.
			appendAlignmentGap(result.leftGaps, hunk.leftResumeLine,
				static_cast<Int>(leftLines.size()), unpairedAddedCount - unpairedDeletedCount);
			appendAlignmentGap(result.rightGaps, hunk.rightResumeLine,
				static_cast<Int>(rightLines.size()), unpairedDeletedCount - unpairedAddedCount);
		}
	}

	for (const ChangedLine& deleted : deletedLines)
	{
		if (deleted.moved || deleted.paired)
			continue;
		result.leftRemovedLines.push_back(deleted.line);
		++result.removedCount;
	}

	for (const ChangedLine& added : addedLines)
	{
		if (added.moved || added.paired)
			continue;
		result.rightAddedLines.push_back(added.line);
		++result.addedCount;
	}

	return result;
}
