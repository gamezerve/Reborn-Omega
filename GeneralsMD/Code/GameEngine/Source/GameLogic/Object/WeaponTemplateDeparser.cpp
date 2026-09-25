/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: WeaponTemplateDeparser.cpp /////////////////////////////////////////////////////////////////////////////////
// Author: Gamezerve, September 2026
// Desc:   Weapon Template Deparser
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/GameCommon.h"
#include "Common/INI.h"
#include "Common/INIFieldDeparser.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponTemplateDeparser.h"

#include <cmath>
#include <cstring>
#include <string>

static Int framesToMilliseconds(Int frames)
{
	return static_cast<Int>(
		std::floor(
			static_cast<double>(frames) *
			MSEC_PER_LOGICFRAME_REAL));
}

std::string WeaponTemplateDeparser::deparse(
	const WeaponTemplate* weapon)
{
	if (!weapon)
		return std::string();

	std::string output;

	output += "; Partial Weapon deparse: special FX, OCL and bonus fields are not yet serialized.\r\n\r\n";
	output += "Weapon ";
	output += weapon->getName().str();
	output += "\r\n\r\n";

	const FieldParse* fields =
		weapon->getFieldParse();

	for (const FieldParse* field = fields;
		field && field->token;
		++field)
	{
		if (strcmp(field->token, "DamageType") == 0)
		{
			const char* name =
				DamageTypeFlags::getNameFromSingleBit(
					static_cast<Int>(
						weapon->getDamageType()));

			if (name)
			{
				INIFieldDeparser::appendField(
					output,
					field->token,
					name);
			}

			continue;
		}

		if (strcmp(field->token, "DamageStatusType") == 0)
		{
			const char* name =
				ObjectStatusMaskType::getNameFromSingleBit(
					static_cast<Int>(
						weapon->getDamageStatusType()));

			if (name)
			{
				INIFieldDeparser::appendField(
					output,
					field->token,
					name);
			}

			continue;
		}

		if (strcmp(field->token, "ProjectileObject") == 0)
		{
			const ThingTemplate* projectile =
				weapon->getProjectileTemplate();

			const std::string name =
				projectile
				? projectile->getName().str()
				: "None";

			INIFieldDeparser::appendField(
				output,
				field->token,
				name);

			continue;
		}

		if (strcmp(field->token, "FireSound") == 0)
		{
			const AsciiString name =
				weapon->getFireSound().getEventName();

			INIFieldDeparser::appendField(
				output,
				field->token,
				name.isEmpty()
				? "NoSound"
				: name.str());

			continue;
		}

		if (strcmp(field->token, "DelayBetweenShots") == 0)
		{
			const Int minimum =
				framesToMilliseconds(
					weapon->getMinDelayBetweenShotsFrames());

			const Int maximum =
				framesToMilliseconds(
					weapon->getMaxDelayBetweenShotsFrames());

			std::string value;

			if (minimum == maximum)
			{
				value = std::to_string(minimum);
			}
			else
			{
				value =
					"Min:" +
					std::to_string(minimum) +
					" Max:" +
					std::to_string(maximum);
			}

			INIFieldDeparser::appendField(
				output,
				field->token,
				value);

			continue;
		}

		if (strcmp(field->token, "HistoricBonusWeapon") == 0)
		{
			const WeaponTemplate* historic =
				weapon->getHistoricBonusWeapon();

			INIFieldDeparser::appendField(
				output,
				field->token,
				historic
				? historic->getName().str()
				: "None");

			continue;
		}

		if (strcmp(field->token, "ScatterTarget") == 0)
		{
			const std::vector<Coord2D>& targets =
				weapon->getScatterTargetsVector();

			for (const Coord2D& target : targets)
			{
				const std::string value =
					"X:" +
					INIFieldDeparser::formatReal(target.x) +
					" Y:" +
					INIFieldDeparser::formatReal(target.y);

				INIFieldDeparser::appendField(
					output,
					field->token,
					value);
			}

			continue;
		}

		INIFieldDeparser::deparseField(
			*field,
			weapon,
			output);
	}

	output += "\r\nEnd\r\n";

	return output;
}
