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

// FILE: Geometry.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, Aug 2002
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_GEOMETRY_NAMES

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "Common/Geometry.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

////=============================================================================
///*static*/ void GeometryInfo::parseGeometryType( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
//{
//	((GeometryInfo*)store)->m_type = (GeometryType)INI::scanIndexList(ini->getNextToken(), GeometryNames);
//	((GeometryInfo*)store)->calcBoundingStuff();
//}
//
////=============================================================================
///*static*/ void GeometryInfo::parseGeometryIsSmall( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
//{
//	((GeometryInfo*)store)->m_isSmall = INI::scanBool(ini->getNextToken());
//	((GeometryInfo*)store)->calcBoundingStuff();
//}
//
////=============================================================================
///*static*/ void GeometryInfo::parseGeometryHeight( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
//{
//	((GeometryInfo*)store)->m_height = INI::scanReal(ini->getNextToken());
//	((GeometryInfo*)store)->calcBoundingStuff();
//}
//
////=============================================================================
///*static*/ void GeometryInfo::parseGeometryMajorRadius( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
//{
//	((GeometryInfo*)store)->m_majorRadius = INI::scanReal(ini->getNextToken());
//	((GeometryInfo*)store)->calcBoundingStuff();
//}
//
////=============================================================================
///*static*/ void GeometryInfo::parseGeometryMinorRadius( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
//{
//	((GeometryInfo*)store)->m_minorRadius = INI::scanReal(ini->getNextToken());
//	((GeometryInfo*)store)->calcBoundingStuff();
//}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryType(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	if (geometry->m_shapes.empty())
		geometry->m_shapes.push_back(Shape());

	geometry->m_shapes[0].m_type =
		(GeometryType)INI::scanIndexList(ini->getNextToken(), GeometryNames);

	geometry->calcBoundingStuff();
}

//=============================================================================
/** Reborn: Return the same geometry name table used by the INI parser. */
//=============================================================================
/*static*/ const char* const* GeometryInfo::getGeometryTypeNames()
{
	return GeometryNames;
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryIsSmall(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	geometry->m_isSmall = INI::scanBool(ini->getNextToken());
	geometry->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryHeight(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	if (geometry->m_shapes.empty())
		geometry->m_shapes.push_back(Shape());

	geometry->m_shapes[0].m_height = INI::scanReal(ini->getNextToken());
	geometry->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryMajorRadius(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	if (geometry->m_shapes.empty())
		geometry->m_shapes.push_back(Shape());

	geometry->m_shapes[0].m_majorRadius = INI::scanReal(ini->getNextToken());
	geometry->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryMinorRadius(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	if (geometry->m_shapes.empty())
		geometry->m_shapes.push_back(Shape());

	geometry->m_shapes[0].m_minorRadius = INI::scanReal(ini->getNextToken());
	geometry->calcBoundingStuff();
}

//=============================================================================
static void parseGeometryShapeType(
	INI* ini,
	void* /*instance*/,
	void* store,
	const void* /*userData*/)
{
	GeometryType* type = static_cast<GeometryType*>(store);

	*type =
		(GeometryType)INI::scanIndexList(
			ini->getNextToken(),
			GeometryNames);
}

//=============================================================================
/*static*/ void GeometryInfo::parseShape(
	INI* ini,
	void* /*instance*/,
	void* store,
	const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	Shape shape;

	static const FieldParse shapeFieldParse[] =
	{
		{ "Type",        parseGeometryShapeType, nullptr, offsetof(GeometryInfo::Shape, m_type) },
		{ "MajorRadius", INI::parseReal,         nullptr, offsetof(GeometryInfo::Shape, m_majorRadius) },
		{ "MinorRadius", INI::parseReal,         nullptr, offsetof(GeometryInfo::Shape, m_minorRadius) },
		{ "Height",      INI::parseReal,         nullptr, offsetof(GeometryInfo::Shape, m_height) },
		{ "Offset",      INI::parseCoord3D,      nullptr, offsetof(GeometryInfo::Shape, m_offset) },

		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(&shape, shapeFieldParse);

	geometry->m_shapes.push_back(shape);
	geometry->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometry(
	INI* ini,
	void* /*instance*/,
	void* store,
	const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(store);

	geometry->m_shapes.clear();

	static const FieldParse geometryFieldParse[] =
	{
		{ "IsSmall", GeometryInfo::parseGeometryIsSmall, nullptr, 0 },
		{ "Shape",   GeometryInfo::parseShape,           nullptr, 0 },

		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(geometry, geometryFieldParse);

	geometry->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryIsSmallNested(
	INI* ini,
	void* instance,
	void* /*store*/,
	const void* /*userData*/)
{
	GeometryInfo* geometry = static_cast<GeometryInfo*>(instance);

	geometry->m_isSmall = INI::scanBool(ini->getNextToken());
	geometry->calcBoundingStuff();
}

static const FieldParse geometryFieldParse[] =
{
	{ "IsSmall", GeometryInfo::parseGeometryIsSmallNested, nullptr, 0 },
	{ "Shape",   GeometryInfo::parseShape,                 nullptr, 0 },

	{ nullptr, nullptr, nullptr, 0 }
};

////-----------------------------------------------------------------------------
//void GeometryInfo::set(GeometryType type, Bool isSmall, Real height, Real majorRadius, Real minorRadius)
//{
//	m_type = type;
//	m_isSmall = isSmall;
//
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//			m_majorRadius = majorRadius;
//			m_minorRadius = majorRadius;
//			m_height = majorRadius;
//			break;
//
//		case GEOMETRY_CYLINDER:
//			m_majorRadius = majorRadius;
//			m_minorRadius = majorRadius;
//			m_height = height;
//			break;
//
//		case GEOMETRY_BOX:
//			m_majorRadius = majorRadius;
//			m_minorRadius = minorRadius;
//			m_height = height;
//			break;
//	};
//
//	calcBoundingStuff();
//}

//-----------------------------------------------------------------------------
void GeometryInfo::set(GeometryType type, Bool isSmall, Real height, Real majorRadius, Real minorRadius)
{
	m_isSmall = isSmall;

	m_shapes.clear();
	m_shapes.push_back(Shape());

	Shape& shape = m_shapes[0];
	shape.m_type = type;
	shape.m_offset.zero();

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
		shape.m_majorRadius = majorRadius;
		shape.m_minorRadius = majorRadius;
		shape.m_height = majorRadius;
		break;

	case GEOMETRY_CYLINDER:
		shape.m_majorRadius = majorRadius;
		shape.m_minorRadius = majorRadius;
		shape.m_height = height;
		break;

	case GEOMETRY_BOX:
		shape.m_majorRadius = majorRadius;
		shape.m_minorRadius = minorRadius;
		shape.m_height = height;
		break;
	}

	calcBoundingStuff();
}

//-----------------------------------------------------------------------------
static Real calcDotProduct(const Coord3D& a, const Coord3D& b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

//-----------------------------------------------------------------------------
static Real calcDistSquared(const Coord3D& a, const Coord3D& b)
{
	return sqr(a.x - b.x) + sqr(a.y - b.y) + sqr(a.z - b.z);
}

//-----------------------------------------------------------------------------
static Real calcPointToLineDistSquared(const Coord3D& pt, const Coord3D& lineStart, const Coord3D& lineEnd)
{
	Coord3D line, lineToPt, closest;

	line.x = lineEnd.x - lineStart.x;
	line.y = lineEnd.y - lineStart.y;
	line.z = lineEnd.z - lineStart.z;

	lineToPt.x = pt.x - lineStart.x;
	lineToPt.y = pt.y - lineStart.y;
	lineToPt.z = pt.z - lineStart.z;

	Real dot = calcDotProduct(lineToPt, line);
	if (dot <= 0.0f)
	{
		// angle is obtuse
		return calcDistSquared(pt, lineStart);
	}

	Real lineLenSqr = calcDistSquared(lineStart, lineEnd);
	DEBUG_ASSERTCRASH(lineLenSqr==calcDotProduct(line,line),("hmm"));
	if (lineLenSqr <= dot)
	{
		return calcDistSquared(pt, lineEnd);
	}

  Real tmp = dot / lineLenSqr;

	closest.x = lineStart.x + tmp * line.x;
	closest.y = lineStart.y + tmp * line.y;
	closest.z = lineStart.z + tmp * line.z;

  return calcDistSquared(pt, closest);
}

//=============================================================================
Bool GeometryInfo::isIntersectedByLineSegment(const Coord3D& loc, const Coord3D& from, const Coord3D& to) const
{
	DEBUG_CRASH(("this call does not work properly for nonspheres yet. use with caution."));

	/// @todo srj -- treats everything as a sphere for now. fix.
	Real distSquared = calcPointToLineDistSquared(loc, from, to);
	return distSquared <= sqr(getBoundingSphereRadius());
}

//=============================================================================
// given an object with this geom, located at 'pos', and another obj with the given
// pos & geom, calc the min and max pitches from this to that.
void GeometryInfo::calcPitches(const Coord3D& thisPos, const GeometryInfo& that, const Coord3D& thatPos,
	Real& minPitch, Real& maxPitch) const
{
	Coord3D thisCenter;
	getCenterPosition(thisPos, thisCenter);

	Real dxy = sqrt(sqr(thatPos.x - thisCenter.x) + sqr(thatPos.y - thisCenter.y));

	Real dz;

	/** @todo srj -- this could be better, by calcing it for all the corners, not just top-center
		and bottom-center... oh well */
	dz = (thatPos.z + that.getMaxHeightAbovePosition()) - thisCenter.z;
	maxPitch = atan2(dz, dxy);

	dz = (thatPos.z - that.getMaxHeightBelowPosition()) - thisCenter.z;
	minPitch = atan2(dz, dxy);
}

////=============================================================================
//// given an object with this geom, SET how far above the object's canonical position its max z should extend.
//void GeometryInfo::setMaxHeightAbovePosition(Real z)
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//			m_majorRadius = z;
//			break;
//
//		case GEOMETRY_BOX:
//		case GEOMETRY_CYLINDER:
//			m_height = z;
//			break;
//	};
//
//	calcBoundingStuff();
//}
//=============================================================================
// given an object with this geom, SET how far above the object's canonical position its max z should extend.
void GeometryInfo::setMaxHeightAbovePosition(Real z)
{
	if (m_shapes.empty())
		return;

	Shape& shape = m_shapes[0];

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
		shape.m_majorRadius = z;
		shape.m_minorRadius = z;
		shape.m_height = z;
		break;

	case GEOMETRY_BOX:
	case GEOMETRY_CYLINDER:
		shape.m_height = z;
		break;
	}

	calcBoundingStuff();
}

////=============================================================================
//// given an object with this geom, how far above the object's canonical position does its max z extend?
//Real GeometryInfo::getMaxHeightAbovePosition() const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//			return m_majorRadius;
//
//		case GEOMETRY_BOX:
//		case GEOMETRY_CYLINDER:
//			return m_height;
//	};
//
//	return 0.0f;
//}
//=============================================================================
// given an object with this geom, how far above the object's canonical position does its max z extend?
Real GeometryInfo::getMaxHeightAbovePosition() const
{
	Real maxHeight = 0.0f;

	for (ShapeVector::const_iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		Real shapeTop = it->m_offset.z;

		switch (it->m_type)
		{
		case GEOMETRY_SPHERE:
			shapeTop += it->m_majorRadius;
			break;

		case GEOMETRY_BOX:
		case GEOMETRY_CYLINDER:
			shapeTop += it->m_height;
			break;
		}

		if (shapeTop > maxHeight)
			maxHeight = shapeTop;
	}

	return maxHeight;
}

////=============================================================================
//// given an object with this geom, how far below the object's canonical position does its max z extend?
//Real GeometryInfo::getMaxHeightBelowPosition() const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//			return m_majorRadius;
//
//		case GEOMETRY_BOX:
//		case GEOMETRY_CYLINDER:
//			return 0.0f;
//	};
//
//	return 0.0f;
//}
//=============================================================================
// given an object with this geom, how far below the object's canonical position does its max z extend?
Real GeometryInfo::getMaxHeightBelowPosition() const
{
	Real maxBelow = 0.0f;

	for (ShapeVector::const_iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		Real shapeBottom = 0.0f;

		switch (it->m_type)
		{
			case GEOMETRY_SPHERE:
				shapeBottom = it->m_offset.z - it->m_majorRadius;
				break;

			case GEOMETRY_BOX:
			case GEOMETRY_CYLINDER:
				shapeBottom = it->m_offset.z;
				break;
		}

		if (shapeBottom < 0.0f)
		{
			Real below = -shapeBottom;

			if (below > maxBelow)
				maxBelow = below;
		}
	}

	return maxBelow;
}

////=============================================================================
//// given an object with this geom, located at 'pos', where is the "center" of the geometry?
//Real GeometryInfo::getZDeltaToCenterPosition() const
//{
//	return (m_type == GEOMETRY_SPHERE) ? 0.0f : (m_height * 0.5f);
//}
//=============================================================================
// given an object with this geom, located at 'pos', where is the "center" of the geometry?
Real GeometryInfo::getZDeltaToCenterPosition() const
{
	if (m_shapes.empty())
		return 0.0f;

	const Shape& shape = m_shapes[0];

	return shape.m_offset.z +
		((shape.m_type == GEOMETRY_SPHERE) ? 0.0f : (shape.m_height * 0.5f));
}

//=============================================================================
// given an object with this geom, located at 'pos', where is the "center" of the geometry?
void GeometryInfo::getCenterPosition(const Coord3D& pos, Coord3D& center) const
{
	center = pos;
	center.z += getZDeltaToCenterPosition();
}

////=============================================================================
//void GeometryInfo::expandFootprint(Real radius)
//{
//	m_majorRadius += radius;
//	m_minorRadius += radius;
//	calcBoundingStuff();
//}
//=============================================================================
void GeometryInfo::expandFootprint(Real radius)
{
	for (ShapeVector::iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		it->m_majorRadius += radius;
		it->m_minorRadius += radius;
	}

	calcBoundingStuff();
}

//=============================================================================
//void GeometryInfo::get2DBounds(const Coord3D& geomCenter, Real angle, Region2D& bounds) const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//			bounds.lo.x = geomCenter.x - m_majorRadius;
//			bounds.lo.y = geomCenter.y - m_majorRadius;
//			bounds.hi.x = geomCenter.x + m_majorRadius;
//			bounds.hi.y = geomCenter.y + m_majorRadius;
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			Real c = (Real)cos(angle);
//			Real s = (Real)sin(angle);
//			Real exc = m_majorRadius*c;
//			Real eyc = m_minorRadius*c;
//			Real exs = m_majorRadius*s;
//			Real eys = m_minorRadius*s;
//			Real x,y;
//			x = geomCenter.x - exc - eys;
//			y = geomCenter.y + eyc - exs;
//			bounds.lo.x = x;
//			bounds.lo.y = y;
//			bounds.hi.x = x;
//			bounds.hi.y = y;
//
//			x = geomCenter.x + exc - eys;
//			y = geomCenter.y + eyc + exs;
//			if (bounds.lo.x > x) bounds.lo.x = x;
//			if (bounds.lo.y > y) bounds.lo.y = y;
//			if (bounds.hi.x < x) bounds.hi.x = x;
//			if (bounds.hi.y < y) bounds.hi.y = y;
//
//			x = geomCenter.x + exc + eys;
//			y = geomCenter.y - eyc + exs;
//			if (bounds.lo.x > x) bounds.lo.x = x;
//			if (bounds.lo.y > y) bounds.lo.y = y;
//			if (bounds.hi.x < x) bounds.hi.x = x;
//			if (bounds.hi.y < y) bounds.hi.y = y;
//
//			x = geomCenter.x - exc + eys;
//			y = geomCenter.y - eyc - exs;
// 			if (bounds.lo.x > x) bounds.lo.x = x;
//			if (bounds.lo.y > y) bounds.lo.y = y;
//			if (bounds.hi.x < x) bounds.hi.x = x;
//			if (bounds.hi.y < y) bounds.hi.y = y;
//
//			break;
//		}
//	}
//}
//=============================================================================
void GeometryInfo::get2DBounds(const Coord3D& geomCenter, Real angle, Region2D& bounds) const
{
	if (m_shapes.empty())
	{
		bounds.lo.x = geomCenter.x;
		bounds.lo.y = geomCenter.y;
		bounds.hi.x = geomCenter.x;
		bounds.hi.y = geomCenter.y;
		return;
	}

	Bool initialized = FALSE;

	Real c = (Real)cos(angle);
	Real s = (Real)sin(angle);

	for (ShapeVector::const_iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		const Shape& shape = *it;

		Coord3D shapeCenter = geomCenter;

		shapeCenter.x += shape.m_offset.x * c - shape.m_offset.y * s;
		shapeCenter.y += shape.m_offset.x * s + shape.m_offset.y * c;

		Region2D shapeBounds;

		switch (shape.m_type)
		{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			shapeBounds.lo.x = shapeCenter.x - shape.m_majorRadius;
			shapeBounds.lo.y = shapeCenter.y - shape.m_majorRadius;
			shapeBounds.hi.x = shapeCenter.x + shape.m_majorRadius;
			shapeBounds.hi.y = shapeCenter.y + shape.m_majorRadius;
			break;
		}

		case GEOMETRY_BOX:
		{
			Real exc = shape.m_majorRadius * c;
			Real eyc = shape.m_minorRadius * c;
			Real exs = shape.m_majorRadius * s;
			Real eys = shape.m_minorRadius * s;

			Real x;
			Real y;

			x = shapeCenter.x - exc - eys;
			y = shapeCenter.y + eyc - exs;

			shapeBounds.lo.x = x;
			shapeBounds.lo.y = y;
			shapeBounds.hi.x = x;
			shapeBounds.hi.y = y;

			x = shapeCenter.x + exc - eys;
			y = shapeCenter.y + eyc + exs;

			if (shapeBounds.lo.x > x) shapeBounds.lo.x = x;
			if (shapeBounds.lo.y > y) shapeBounds.lo.y = y;
			if (shapeBounds.hi.x < x) shapeBounds.hi.x = x;
			if (shapeBounds.hi.y < y) shapeBounds.hi.y = y;

			x = shapeCenter.x + exc + eys;
			y = shapeCenter.y - eyc + exs;

			if (shapeBounds.lo.x > x) shapeBounds.lo.x = x;
			if (shapeBounds.lo.y > y) shapeBounds.lo.y = y;
			if (shapeBounds.hi.x < x) shapeBounds.hi.x = x;
			if (shapeBounds.hi.y < y) shapeBounds.hi.y = y;

			x = shapeCenter.x - exc + eys;
			y = shapeCenter.y - eyc - exs;

			if (shapeBounds.lo.x > x) shapeBounds.lo.x = x;
			if (shapeBounds.lo.y > y) shapeBounds.lo.y = y;
			if (shapeBounds.hi.x < x) shapeBounds.hi.x = x;
			if (shapeBounds.hi.y < y) shapeBounds.hi.y = y;

			break;
		}
		}

		if (!initialized)
		{
			bounds = shapeBounds;
			initialized = TRUE;
		}
		else
		{
			if (shapeBounds.lo.x < bounds.lo.x) bounds.lo.x = shapeBounds.lo.x;
			if (shapeBounds.lo.y < bounds.lo.y) bounds.lo.y = shapeBounds.lo.y;

			if (shapeBounds.hi.x > bounds.hi.x) bounds.hi.x = shapeBounds.hi.x;
			if (shapeBounds.hi.y > bounds.hi.y) bounds.hi.y = shapeBounds.hi.y;
		}
	}
}

////=============================================================================
//void GeometryInfo::clipPointToFootprint(const Coord3D& geomCenter, Coord3D& ptToClip) const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//			Real dx = ptToClip.x - geomCenter.x;
//			Real dy = ptToClip.y - geomCenter.y;
//			Real radius = sqrt(sqr(dx) + sqr(dy));
//			if (radius > m_majorRadius)
//			{
//				Real ratio = m_majorRadius / radius;
//				ptToClip.x = geomCenter.x + dx * ratio;
//				ptToClip.y = geomCenter.y + dy * ratio;
//			}
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			ptToClip.x = clamp(geomCenter.x - m_majorRadius, ptToClip.x, geomCenter.x + m_majorRadius);
//			ptToClip.y = clamp(geomCenter.y - m_minorRadius, ptToClip.y, geomCenter.y + m_minorRadius);
//			break;
//		}
//	};
//}
//=============================================================================
void GeometryInfo::clipPointToFootprint(const Coord3D& geomCenter, Coord3D& ptToClip) const
{
	if (m_shapes.empty())
		return;

	const Shape& shape = m_shapes[0];

	Coord3D shapeCenter = geomCenter;
	shapeCenter.x += shape.m_offset.x;
	shapeCenter.y += shape.m_offset.y;

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
	case GEOMETRY_CYLINDER:
	{
		Real dx = ptToClip.x - shapeCenter.x;
		Real dy = ptToClip.y - shapeCenter.y;
		Real radius = sqrt(sqr(dx) + sqr(dy));

		if (radius > shape.m_majorRadius)
		{
			Real ratio = shape.m_majorRadius / radius;

			ptToClip.x = shapeCenter.x + dx * ratio;
			ptToClip.y = shapeCenter.y + dy * ratio;
		}

		break;
	}

	case GEOMETRY_BOX:
	{
		ptToClip.x =
			clamp(
				shapeCenter.x - shape.m_majorRadius,
				ptToClip.x,
				shapeCenter.x + shape.m_majorRadius);

		ptToClip.y =
			clamp(
				shapeCenter.y - shape.m_minorRadius,
				ptToClip.y,
				shapeCenter.y + shape.m_minorRadius);

		break;
	}
	}
}

//=============================================================================
inline Bool isWithin(Real a, Real b, Real c) { return a<=b && b<=c; }

////=============================================================================
//Bool GeometryInfo::isPointInFootprint(const Coord3D& geomCenter, const Coord3D& pt) const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//			Real dx = pt.x - geomCenter.x;
//			Real dy = pt.y - geomCenter.y;
//			Real radius = sqrt(sqr(dx) + sqr(dy));
//			return (radius <= m_majorRadius);
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			return isWithin(geomCenter.x - m_majorRadius, pt.x, geomCenter.x + m_majorRadius) &&
//							isWithin(geomCenter.y - m_minorRadius, pt.y, geomCenter.y + m_minorRadius);
//		}
//	};
//	return false;
//}
//=============================================================================
Bool GeometryInfo::isPointInFootprint(const Coord3D& geomCenter, const Coord3D& pt) const
{
	for (ShapeVector::const_iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		const Shape& shape = *it;

		Coord3D shapeCenter = geomCenter;
		shapeCenter.x += shape.m_offset.x;
		shapeCenter.y += shape.m_offset.y;

		switch (shape.m_type)
		{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			Real dx = pt.x - shapeCenter.x;
			Real dy = pt.y - shapeCenter.y;
			Real radius = sqrt(sqr(dx) + sqr(dy));

			if (radius <= shape.m_majorRadius)
				return TRUE;

			break;
		}

		case GEOMETRY_BOX:
		{
			if (isWithin(
				shapeCenter.x - shape.m_majorRadius,
				pt.x,
				shapeCenter.x + shape.m_majorRadius) &&
				isWithin(
					shapeCenter.y - shape.m_minorRadius,
					pt.y,
					shapeCenter.y + shape.m_minorRadius))
			{
				return TRUE;
			}

			break;
		}
		}
	}

	return FALSE;
}

////=============================================================================
//void GeometryInfo::makeRandomOffsetWithinFootprint(Coord3D& pt, const RandomValueClass& random) const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//#if 1
//			// this is a better technique than the more obvious radius-and-angle
//			// one, below, because the latter tends to clump more towards the center.
//			Real maxDistSqr = sqr(m_majorRadius);
//			Real distSqr;
//			do
//			{
//				pt.x = RandomValueReal(random, -m_majorRadius, m_majorRadius);
//				pt.y = RandomValueReal(random, -m_majorRadius, m_majorRadius);
//				pt.z = 0.0f;
//				distSqr = sqr(pt.x) + sqr(pt.y);
//			} while (distSqr > maxDistSqr);
//#else
//			Real radius = RandomValueReal(random, 0.0f, m_boundingCircleRadius);
//			Real angle = RandomValueReal(random, -PI, PI);
//			pt.x = radius * Cos(angle);
//			pt.y = radius * Sin(angle);
//			pt.z = 0.0f;
//#endif
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			pt.x = RandomValueReal(random, -m_majorRadius, m_majorRadius);
//			pt.y = RandomValueReal(random, -m_minorRadius, m_minorRadius);
//			pt.z = 0.0f;
//			break;
//		}
//	};
//}
//=============================================================================
void GeometryInfo::makeRandomOffsetWithinFootprint(Coord3D& pt, const RandomValueClass& random) const
{
	if (m_shapes.empty())
	{
		pt.zero();
		return;
	}

	// Legacy behavior: random point is generated from the first shape.
	const Shape& shape = m_shapes[0];

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
	case GEOMETRY_CYLINDER:
	{
#if 1
		// this is a better technique than the more obvious radius-and-angle
		// one, below, because the latter tends to clump more towards the center.
		Real maxDistSqr = sqr(shape.m_majorRadius);
		Real distSqr;

		do
		{
			pt.x = RandomValueReal(random, -shape.m_majorRadius, shape.m_majorRadius);
			pt.y = RandomValueReal(random, -shape.m_majorRadius, shape.m_majorRadius);
			pt.z = 0.0f;

			distSqr = sqr(pt.x) + sqr(pt.y);
		} while (distSqr > maxDistSqr);
#else
		Real radius = RandomValueReal(random, 0.0f, shape.m_majorRadius);
		Real angle = RandomValueReal(random, -PI, PI);

		pt.x = radius * Cos(angle);
		pt.y = radius * Sin(angle);
		pt.z = 0.0f;
#endif
		break;
	}

	case GEOMETRY_BOX:
	{
		pt.x = RandomValueReal(random, -shape.m_majorRadius, shape.m_majorRadius);
		pt.y = RandomValueReal(random, -shape.m_minorRadius, shape.m_minorRadius);
		pt.z = 0.0f;
		break;
	}
	}

	pt.x += shape.m_offset.x;
	pt.y += shape.m_offset.y;
	pt.z += shape.m_offset.z;
}

////=============================================================================
//void GeometryInfo::makeRandomOffsetOnPerimeter(Coord3D& pt) const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//			DEBUG_CRASH( ("GeometryInfo::makeRandomOffsetOnPerimeter() not implemented for SPHERE or CYLINDER extents. Using position.") );
//
//			//Kris: Did not have time nor need to support non-box extents. I added this feature for script placement
//			//      of boobytraps.
//			pt.x = 0.0f;
//			pt.y = 0.0f;
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
//			{
//				//Pick random point on x axis.
//				pt.x = GameLogicRandomValueReal(-m_majorRadius, m_majorRadius);
//
//				//Min or max the y axis value
//				if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
//					pt.y = -m_minorRadius;
//				else
//					pt.y = m_minorRadius;
//			}
//			else
//			{
//				//Pick random point on y axis.
//				pt.y = GameLogicRandomValueReal(-m_minorRadius, m_minorRadius);
//
//				//Min or max the x axis value
//				if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
//					pt.x = -m_majorRadius;
//				else
//					pt.x = m_majorRadius;
//			}
//			pt.z = 0.0f;
//			break;
//		}
//	};
//}
//=============================================================================
void GeometryInfo::makeRandomOffsetOnPerimeter(Coord3D& pt) const
{
	if (m_shapes.empty())
	{
		pt.zero();
		return;
	}

	// Legacy behavior: perimeter point is generated from the first shape.
	const Shape& shape = m_shapes[0];

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
	case GEOMETRY_CYLINDER:
	{
		DEBUG_CRASH(("GeometryInfo::makeRandomOffsetOnPerimeter() not implemented for SPHERE or CYLINDER extents. Using position."));

		//Kris: Did not have time nor need to support non-box extents. I added this feature for script placement
		//      of boobytraps.
		pt.x = shape.m_offset.x;
		pt.y = shape.m_offset.y;
		pt.z = shape.m_offset.z;
		break;
	}

	case GEOMETRY_BOX:
	{
		if (GameLogicRandomValueReal(0.0f, 1.0f) < 0.5f)
		{
			//Pick random point on x axis.
			pt.x = GameLogicRandomValueReal(-shape.m_majorRadius, shape.m_majorRadius);

			//Min or max the y axis value
			if (GameLogicRandomValueReal(0.0f, 1.0f) < 0.5f)
				pt.y = -shape.m_minorRadius;
			else
				pt.y = shape.m_minorRadius;
		}
		else
		{
			//Pick random point on y axis.
			pt.y = GameLogicRandomValueReal(-shape.m_minorRadius, shape.m_minorRadius);

			//Min or max the x axis value
			if (GameLogicRandomValueReal(0.0f, 1.0f) < 0.5f)
				pt.x = -shape.m_majorRadius;
			else
				pt.x = shape.m_majorRadius;
		}

		pt.x += shape.m_offset.x;
		pt.y += shape.m_offset.y;
		pt.z = shape.m_offset.z;

		break;
	}
	}
}

////=============================================================================
//Real GeometryInfo::getFootprintArea() const
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		case GEOMETRY_CYLINDER:
//		{
//			return PI * sqr(m_boundingCircleRadius);
//		}
//
//		case GEOMETRY_BOX:
//		{
//			return 4.0f * m_majorRadius * m_minorRadius;
//		}
//	};
//
//	DEBUG_CRASH(("should never get here"));
//	return 0.0f;
//}
//=============================================================================
Real GeometryInfo::getFootprintArea() const
{
	if (m_shapes.empty())
		return 0.0f;

	// Legacy behavior: use the first shape only.
	const Shape& shape = m_shapes[0];

	switch (shape.m_type)
	{
	case GEOMETRY_SPHERE:
	case GEOMETRY_CYLINDER:
	{
		return PI * sqr(shape.m_majorRadius);
	}

	case GEOMETRY_BOX:
	{
		return 4.0f * shape.m_majorRadius * shape.m_minorRadius;
	}
	}

	DEBUG_CRASH(("should never get here"));
	return 0.0f;
}

////=============================================================================
//void GeometryInfo::calcBoundingStuff()
//{
//	switch(m_type)
//	{
//		case GEOMETRY_SPHERE:
//		{
//			m_boundingSphereRadius = m_majorRadius;
//			m_boundingCircleRadius = m_majorRadius;
//			break;
//		}
//		case GEOMETRY_CYLINDER:
//		{
//			m_boundingCircleRadius = m_majorRadius;
//
//			m_boundingSphereRadius = m_height*0.5;
//			if (m_boundingSphereRadius < m_majorRadius)
//				m_boundingSphereRadius = m_majorRadius;
//			break;
//		}
//
//		case GEOMETRY_BOX:
//		{
//			m_boundingCircleRadius = sqrt(sqr(m_majorRadius) + sqr(m_minorRadius));
//			m_boundingSphereRadius = sqrt(sqr(m_majorRadius) + sqr(m_minorRadius) + sqr(m_height*0.5));
//			break;
//		}
//	};
//}
//=============================================================================
void GeometryInfo::calcBoundingStuff()
{
	m_boundingCircleRadius = 0.0f;
	m_boundingSphereRadius = 0.0f;

	for (ShapeVector::const_iterator it = m_shapes.begin(); it != m_shapes.end(); ++it)
	{
		const Shape& shape = *it;

		Real shapeCircleRadius = 0.0f;
		Real shapeSphereRadius = 0.0f;
		Real shapeCenterZ = shape.m_offset.z;

		switch (shape.m_type)
		{
		case GEOMETRY_SPHERE:
		{
			shapeCircleRadius = shape.m_majorRadius;
			shapeSphereRadius = shape.m_majorRadius;
			break;
		}

		case GEOMETRY_CYLINDER:
		{
			shapeCircleRadius = shape.m_majorRadius;

			Real halfHeight = shape.m_height * 0.5f;
			shapeCenterZ += halfHeight;

			shapeSphereRadius = halfHeight;

			if (shapeSphereRadius < shape.m_majorRadius)
				shapeSphereRadius = shape.m_majorRadius;

			break;
		}

		case GEOMETRY_BOX:
		{
			shapeCircleRadius =
				sqrt(
					sqr(shape.m_majorRadius) +
					sqr(shape.m_minorRadius));

			Real halfHeight = shape.m_height * 0.5f;
			shapeCenterZ += halfHeight;

			shapeSphereRadius =
				sqrt(
					sqr(shape.m_majorRadius) +
					sqr(shape.m_minorRadius) +
					sqr(halfHeight));

			break;
		}
		}

		Real offsetXY =
			sqrt(
				sqr(shape.m_offset.x) +
				sqr(shape.m_offset.y));

		Real totalCircleRadius =
			offsetXY + shapeCircleRadius;

		if (totalCircleRadius > m_boundingCircleRadius)
			m_boundingCircleRadius = totalCircleRadius;

		Real offset3D =
			sqrt(
				sqr(shape.m_offset.x) +
				sqr(shape.m_offset.y) +
				sqr(shapeCenterZ));

		Real totalSphereRadius =
			offset3D + shapeSphereRadius;

		if (totalSphereRadius > m_boundingSphereRadius)
			m_boundingSphereRadius = totalSphereRadius;
	}
}

//#if defined(RTS_DEBUG)
////=============================================================================
//void GeometryInfo::tweakExtents(ExtentModType extentModType, Real extentModAmount)
//{
//	switch(extentModType)
//	{
//		case EXTENTMOD_HEIGHT:
//			m_height += extentModAmount;
//			break;
//		case EXTENTMOD_MAJOR:
//			m_majorRadius += extentModAmount;
//			break;
//		case EXTENTMOD_MINOR:
//			m_minorRadius += extentModAmount;
//			break;
//		case EXTENTMOD_TYPE:
//			m_type = (GeometryType)((m_type + ((extentModType == EXTENTMOD_TYPE)?1:0)) % GEOMETRY_NUM_TYPES);
//			break;
//	}
//	m_isSmall = false;
//	calcBoundingStuff();
//}
//#endif
#if defined(RTS_DEBUG)
//=============================================================================
void GeometryInfo::tweakExtents(ExtentModType extentModType, Real extentModAmount)
{
	if (m_shapes.empty())
		m_shapes.push_back(Shape());

	Shape& shape = m_shapes[0];

	switch (extentModType)
	{
	case EXTENTMOD_HEIGHT:
		shape.m_height += extentModAmount;
		break;

	case EXTENTMOD_MAJOR:
		shape.m_majorRadius += extentModAmount;
		break;

	case EXTENTMOD_MINOR:
		shape.m_minorRadius += extentModAmount;
		break;

	case EXTENTMOD_TYPE:
		shape.m_type =
			(GeometryType)((shape.m_type + 1) % GEOMETRY_NUM_TYPES);
		break;
	}

	m_isSmall = false;
	calcBoundingStuff();
}
#endif

//#if defined(RTS_DEBUG)
////=============================================================================
//AsciiString GeometryInfo::getDescriptiveString() const
//{
//	AsciiString msg;
//	msg.format("%d/%d(%g %g %g)", (Int)m_type, (Int)m_isSmall, m_majorRadius, m_minorRadius, m_height);
//	return msg;
//}
//#endif
#if defined(RTS_DEBUG)
//=============================================================================
AsciiString GeometryInfo::getDescriptiveString() const
{
	AsciiString msg;

	if (m_shapes.empty())
	{
		msg.format("No Geometry");
		return msg;
	}

	const Shape& shape = m_shapes[0];

	msg.format(
		"%d/%d(%g %g %g) Shapes=%d",
		(Int)shape.m_type,
		(Int)m_isSmall,
		shape.m_majorRadius,
		shape.m_minorRadius,
		shape.m_height,
		(Int)m_shapes.size());

	return msg;
}
#endif

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GeometryInfo::crc( Xfer *xfer )
{

}

//// ------------------------------------------------------------------------------------------------
///** Xfer method
//	* Version Info:
//	* 1: Initial version */
//// ------------------------------------------------------------------------------------------------
//void GeometryInfo::xfer( Xfer *xfer )
//{
//
//	// version
//	XferVersion currentVersion = 1;
//	XferVersion version = currentVersion;
//	xfer->xferVersion( &version, currentVersion );
//
//	// type
//	xfer->xferUser( &m_type, sizeof( GeometryType ) );
//
//	// is small
//	xfer->xferBool( &m_isSmall );
//
//	// height
//	xfer->xferReal( &m_height );
//
//	// major radius
//	xfer->xferReal( &m_majorRadius );
//
//	// minor radius
//	xfer->xferReal( &m_minorRadius );
//
//	// bouncing circle radius
//	xfer->xferReal( &m_boundingCircleRadius );
//
//	// bounding sphere radius
//	xfer->xferReal( &m_boundingSphereRadius );
//
//}
// ------------------------------------------------------------------------------------------------
/** Xfer method
 * Version Info:
 * 1: Initial version
 * 2: Added support for multiple geometry shapes
 */
 // ------------------------------------------------------------------------------------------------
void GeometryInfo::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);

	if (version == 1)
	{
		GeometryType type;
		Real height;
		Real majorRadius;
		Real minorRadius;

		xfer->xferUser(&type, sizeof(GeometryType));
		xfer->xferBool(&m_isSmall);
		xfer->xferReal(&height);
		xfer->xferReal(&majorRadius);
		xfer->xferReal(&minorRadius);

		// These values were serialized in version 1.
		xfer->xferReal(&m_boundingCircleRadius);
		xfer->xferReal(&m_boundingSphereRadius);

		if (xfer->getXferMode() == XFER_LOAD)
		{
			set(type, m_isSmall, height, majorRadius, minorRadius);
		}

		return;
	}

	xfer->xferBool(&m_isSmall);

	Int shapeCount = static_cast<Int>(m_shapes.size());
	xfer->xferInt(&shapeCount);

	if (xfer->getXferMode() == XFER_LOAD)
	{
		m_shapes.clear();
		m_shapes.resize(shapeCount);
	}

	for (Int i = 0; i < shapeCount; ++i)
	{
		Shape& shape = m_shapes[i];

		xfer->xferUser(&shape.m_type, sizeof(GeometryType));
		xfer->xferReal(&shape.m_height);
		xfer->xferReal(&shape.m_majorRadius);
		xfer->xferReal(&shape.m_minorRadius);
		xfer->xferCoord3D(&shape.m_offset);
	}

	if (xfer->getXferMode() == XFER_LOAD)
	{
		calcBoundingStuff();
	}
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GeometryInfo::loadPostProcess()
{

}
