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

// FILE: Image.cpp ////////////////////////////////////////////////////////////////////////////////
// Created:   Colin Day, June 2001
// Desc:      High level representation of images, this is currently being
//						written so we have a way to refer to images in the windows
//						GUI, this system should be replaced with something that can
//						handle real image management or written to accommodate
//						all parts of the engine that need images.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_IMAGE_STATUS_NAMES
#include "Common/Debug.h"
#include "Common/INI.h"
#include "Common/GlobalData.h"
#include "GameClient/Image.h"
#include "Common/NameKeyGenerator.h"

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
const FieldParse Image::m_imageFieldParseTable[] =
{

	{ "Texture",				INI::parseAsciiString,							nullptr, 		offsetof( Image, m_filename ) },
	{ "TextureWidth",		INI::parseInt,											nullptr, 		offsetof( Image, m_textureSize.x ) },
	{ "TextureHeight",	INI::parseInt,											nullptr, 		offsetof( Image, m_textureSize.y ) },
	{ "Coords",					Image::parseImageCoords,						nullptr, 		offsetof( Image, m_UVCoords ) },
	{ "Status",					Image::parseImageStatus,						nullptr, 		offsetof( Image, m_status ) },

	{ nullptr,							nullptr,																nullptr, 		0 }

};

// Reborn: Inherited mapped images may layer up to eight mapped images over their inherited base.
const FieldParse Image::m_imageInheritFieldParseTable[] =
{
	{ "Texture",       INI::parseAsciiString, nullptr,   offsetof( Image, m_filename ) },
	{ "TextureWidth",  INI::parseInt,         nullptr,   offsetof( Image, m_textureSize.x ) },
	{ "TextureHeight", INI::parseInt,         nullptr,   offsetof( Image, m_textureSize.y ) },
	{ "Coords",        Image::parseImageCoords, nullptr, offsetof( Image, m_UVCoords ) },
	{ "Status",        Image::parseImageStatus, nullptr, offsetof( Image, m_status ) },
	{ "Overlay1",      Image::parseImageOverlay, (void *)0, 0 },
	{ "Overlay2",      Image::parseImageOverlay, (void *)1, 0 },
	{ "Overlay3",      Image::parseImageOverlay, (void *)2, 0 },
	{ "Overlay4",      Image::parseImageOverlay, (void *)3, 0 },
	{ "Overlay5",      Image::parseImageOverlay, (void *)4, 0 },
	{ "Overlay6",      Image::parseImageOverlay, (void *)5, 0 },
	{ "Overlay7",      Image::parseImageOverlay, (void *)6, 0 },
	{ "Overlay8",      Image::parseImageOverlay, (void *)7, 0 },
	{ "MappedImageOverlay", Image::parseBuffNerfOverlayModule, nullptr, 0 },

	{ nullptr, nullptr, nullptr, 0 }
};

// Reborn: The nested MappedImageOverlay module accepts four reference-positioned icon fields.
const FieldParse Image::m_imageBuffNerfOverlayFieldParseTable[] =
{
	{ "MappedImage1", Image::parseBuffNerfOverlay, (void *)0, 0 },
	{ "MappedImage2", Image::parseBuffNerfOverlay, (void *)1, 0 },
	{ "MappedImage3", Image::parseBuffNerfOverlay, (void *)2, 0 },
	{ "MappedImage4", Image::parseBuffNerfOverlay, (void *)3, 0 },

	{ nullptr, nullptr, nullptr, 0 }
};

// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
/** Parse an image coordinates in the form of
	*
	* COORDS = Left:AAA Top:BBB Right:CCC Bottom:DDD */
//-------------------------------------------------------------------------------------------------
void Image::parseImageCoords( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	Int left = INI::scanInt(ini->getNextSubToken("Left"));
	Int top = INI::scanInt(ini->getNextSubToken("Top"));
	Int right = INI::scanInt(ini->getNextSubToken("Right"));
	Int bottom = INI::scanInt(ini->getNextSubToken("Bottom"));

	// get the image we're storing in
	Image *theImage = (Image *)instance;

	//
	// store the UV coords based on what we've read in and the texture size
	// defined for this image
	//
	Region2D uvCoords;

	uvCoords.lo.x = (Real)left;
	uvCoords.lo.y = (Real)top;
	uvCoords.hi.x = (Real)right;
	uvCoords.hi.y = (Real)bottom;

	// adjust the coords by texture size
	const ICoord2D *textureSize = theImage->getTextureSize();
	if( textureSize->x )
	{
		uvCoords.lo.x /= (Real)textureSize->x;
		uvCoords.hi.x /= (Real)textureSize->x;
	}
	if( textureSize->y )
	{
		uvCoords.lo.y /= (Real)textureSize->y;
		uvCoords.hi.y /= (Real)textureSize->y;
	}

	// store the uv coords
	theImage->setUV( &uvCoords );

	// compute the image size based on the coords we read and store
	ICoord2D imageSize;
	imageSize.x = right - left;
	imageSize.y = bottom - top;
	theImage->setImageSize( &imageSize );

}

//-------------------------------------------------------------------------------------------------
/** Parse the image status line */
//-------------------------------------------------------------------------------------------------
void Image::parseImageStatus( INI* ini, void *instance, void *store, const void* /*userData*/)
{
	// use existing INI parsing for the bit strings
	INI::parseBitString32(ini, instance, store, imageStatusNames);

	//
	// if we are rotated 90 degrees clockwise we need to swap our width and height as
	// they were computed from the page location rect, which was for the rotated image
	// (see ImagePacker tool for more details)
	//
	UnsignedInt *theStatusBits = (UnsignedInt *)store;
	if( BitIsSet( *theStatusBits, IMAGE_STATUS_ROTATED_90_CLOCKWISE ) )
	{
		Image *theImage = (Image *)instance;
		ICoord2D imageSize;

		imageSize.x = theImage->getImageHeight();  // note it's height not width
		imageSize.y = theImage->getImageWidth();   // note it's width not height
		theImage->setImageSize( &imageSize );

	}

}

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
ImageCollection *TheMappedImageCollection = nullptr;  ///< mapped images

// PUBLIC FUNCTIONS////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Image::Image()
{

	m_name.clear();
	m_filename.clear();
	m_textureSize.x = 0;
	m_textureSize.y = 0;
	m_UVCoords.lo.x = 0.0f;
	m_UVCoords.lo.y = 0.0f;
	m_UVCoords.hi.x = 1.0f;
	m_UVCoords.hi.y = 1.0f;
	m_imageSize.x = 0;
	m_imageSize.y = 0;
	m_rawTextureData = nullptr;
	m_status = IMAGE_STATUS_NONE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Image::~Image()
{

}

//-------------------------------------------------------------------------------------------------
/** Reborn: Parse an overlay name without requiring the referenced mapped image to load first. */
//-------------------------------------------------------------------------------------------------
void Image::parseImageOverlay( INI *ini, void *instance, void *store, const void *userData )
{
	Image *image = static_cast<Image *>( instance );
	const Int overlayIndex = (Int)userData;
	const AsciiString overlayName = ini->getNextToken();

	if( image->m_overlayNames.size() <= static_cast<size_t>( overlayIndex ) )
		image->m_overlayNames.resize( overlayIndex + 1 );

	image->m_overlayNames[overlayIndex] = overlayName;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Parse a MappedImageOverlay module nested directly inside MappedImageInherit. */
//-------------------------------------------------------------------------------------------------
void Image::parseBuffNerfOverlayModule( INI *ini, void *instance, void *store, const void *userData )
{
	Image *image = static_cast<Image *>( instance );
	ini->initFromINI( image, image->getBuffNerfOverlayFieldParse() );
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Parse a buff/nerf icon field while allowing its mapped image to be declared later. */
//-------------------------------------------------------------------------------------------------
void Image::parseBuffNerfOverlay( INI *ini, void *instance, void *store, const void *userData )
{
	Image *image = static_cast<Image *>( instance );
	const Int overlayIndex = (Int)userData;
	const AsciiString overlayName = ini->getNextToken();

	if( image->m_buffNerfOverlayNames.size() <= static_cast<size_t>( overlayIndex ) )
		image->m_buffNerfOverlayNames.resize( overlayIndex + 1 );

	image->m_buffNerfOverlayNames[overlayIndex] = overlayName;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Return the icon center measured from the supplied 150x150 reference PNG layouts. */
//-------------------------------------------------------------------------------------------------
Bool Image::getBuffNerfOverlayCenter( Int index, ICoord2D *center ) const
{
	if( center == nullptr || index < 0 || index >= getBuffNerfOverlayCount() )
		return FALSE;

	static const ICoord2D centerForOne = { 121, 112 };
	static const ICoord2D centersForTwo[] =
	{
		{ 121, 95 },
		{ 121, 129 }
	};
	static const ICoord2D centersForThree[] =
	{
		{ 112, 94 },
		{ 94, 131 },
		{ 131, 131 }
	};
	static const ICoord2D centersForFour[] =
	{
		{ 94, 94 },
		{ 131, 94 },
		{ 94, 131 },
		{ 131, 131 }
	};

	if( getBuffNerfOverlayCount() == 1 )
	{
		*center = centerForOne;
		return TRUE;
	}

	if( getBuffNerfOverlayCount() == 2 )
	{
		*center = centersForTwo[index];
		return TRUE;
	}

	if( getBuffNerfOverlayCount() == 3 )
	{
		*center = centersForThree[index];
		return TRUE;
	}

	*center = centersForFour[index];
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Copy mapped-image definition data without changing the child's name or sharing runtime texture data. */
//-------------------------------------------------------------------------------------------------
void Image::copyFrom( const Image *image )
{
	DEBUG_ASSERTCRASH( image != nullptr, ("Image::copyFrom received a null parent image") );

	if( !image )
		return;

	m_filename = image->m_filename;
	m_textureSize = image->m_textureSize;
	m_UVCoords = image->m_UVCoords;
	m_imageSize = image->m_imageSize;
	m_rawTextureData = nullptr;
	m_status = image->m_status & ~IMAGE_STATUS_RAW_TEXTURE;
	m_overlayNames = image->m_overlayNames;
	m_overlays.clear();
	m_buffNerfOverlayNames = image->m_buffNerfOverlayNames;
	m_buffNerfOverlays.clear();
}

//-------------------------------------------------------------------------------------------------
/** Set a status bit into the existing status, return the previous status
	* bit collection from before the set */
//-------------------------------------------------------------------------------------------------
UnsignedInt Image::setStatus( UnsignedInt bit )
{
	UnsignedInt prevStatus = m_status;

	BitSet( m_status, bit );
	return prevStatus;

}

//-------------------------------------------------------------------------------------------------
/** Clear a status bit from the existing status, return the previous
	* status bit collection from before the clear */
//-------------------------------------------------------------------------------------------------
UnsignedInt Image::clearStatus( UnsignedInt bit )
{
	UnsignedInt prevStatus = m_status;

	BitClear( m_status, bit );
	return prevStatus;

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ImageCollection::ImageCollection()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ImageCollection::~ImageCollection()
{
  for (ImageMap::iterator i=m_imageMap.begin();i!=m_imageMap.end();++i)
    deleteInstance(i->second);
}

//-------------------------------------------------------------------------------------------------
/** adds the given image to the collection, transfers ownership to this object */
//-------------------------------------------------------------------------------------------------
void ImageCollection::addImage( Image *image )
{
  m_imageMap[TheNameKeyGenerator->nameToLowercaseKey(image->getName())]=image;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Queue a mapped image until its inherited parent becomes available. */
//-------------------------------------------------------------------------------------------------
void ImageCollection::addPendingInheritance( const PendingMappedImageInheritance& pending )
{
	m_pendingInheritances.push_back( pending );
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Find a deferred mapped-image inheritance by its case-insensitive mapped-image name. */
//-------------------------------------------------------------------------------------------------
PendingMappedImageInheritance *ImageCollection::findPendingInheritance( const AsciiString& name )
{
	for( std::vector<PendingMappedImageInheritance>::iterator it = m_pendingInheritances.begin();
		 it != m_pendingInheritances.end(); ++it )
	{
		if( it->m_name.compareNoCase( name ) == 0 )
			return &(*it);
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Report whether a mapped image exists only as an unresolved inherited placeholder. */
//-------------------------------------------------------------------------------------------------
Bool ImageCollection::hasPendingInheritance( const AsciiString& name ) const
{
	for( std::vector<PendingMappedImageInheritance>::const_iterator it = m_pendingInheritances.begin();
		 it != m_pendingInheritances.end(); ++it )
	{
		if( !it->m_resolved && it->m_name.compareNoCase( name ) == 0 )
			return TRUE;
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve one deferred mapped image, resolving inherited parent chains first. */
//-------------------------------------------------------------------------------------------------
Bool ImageCollection::resolvePendingInheritance(
	PendingMappedImageInheritance& pending,
	std::vector<AsciiString>& resolving )
{
	if( pending.m_resolved )
		return TRUE;

	for( std::vector<AsciiString>::const_iterator it = resolving.begin(); it != resolving.end(); ++it )
	{
		if( it->compareNoCase( pending.m_name ) == 0 )
		{
			DEBUG_CRASH(( "Circular MappedImage inheritance detected while resolving '%s'.", pending.m_name.str() ));
			throw INI_INVALID_DATA;
		}
	}

	resolving.push_back( pending.m_name );

	PendingMappedImageInheritance *pendingParent = findPendingInheritance( pending.m_parentName );
	if( pendingParent && !pendingParent->m_resolved )
		resolvePendingInheritance( *pendingParent, resolving );

	const Image *parentImage = findImageByName( pending.m_parentName );
	if( !parentImage || (pendingParent && !pendingParent->m_resolved) )
	{
		DEBUG_CRASH(( "Unable to resolve inherited MappedImage '%s': parent '%s' does not exist.", pending.m_name.str(), pending.m_parentName.str() ));
		REBORN_LOG(
			"INI_INVALID_DATA: Unable to resolve deferred MappedImage '%s'. Parent '%s' does not exist. OriginalINIFile='%s'.",
			pending.m_name.str(),
			pending.m_parentName.str(),
			pending.m_sourceFilename.str() );
		throw INI_INVALID_DATA;
	}

	pending.m_image->copyFrom( parentImage );

	INI replayIni;
	replayIni.initFromCapturedBlock(
		pending.m_image,
		pending.m_image->getInheritFieldParse(),
		pending.m_blockText,
		static_cast<INILoadType>( pending.m_loadType ),
		pending.m_sourceFilename );

	pending.m_resolved = TRUE;
	resolving.pop_back();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve overlay names and reject missing images before any image can be rendered. */
//-------------------------------------------------------------------------------------------------
void ImageCollection::resolveOverlays()
{
	for( ImageMap::iterator imageIt = m_imageMap.begin(); imageIt != m_imageMap.end(); ++imageIt )
	{
		Image *image = imageIt->second;
		image->m_overlays.clear();
		image->m_overlays.resize( image->m_overlayNames.size(), nullptr );

		for( size_t index = 0; index < image->m_overlayNames.size(); ++index )
		{
			const AsciiString& overlayName = image->m_overlayNames[index];
			if( overlayName.isEmpty() || overlayName.compareNoCase( "None" ) == 0 )
				continue;

			const Image *overlay = findImageByName( overlayName );
			if( !overlay )
			{
				DEBUG_CRASH(( "MappedImage '%s' references missing Overlay%d image '%s'.", image->getName().str(), static_cast<Int>( index ) + 1, overlayName.str() ));
				REBORN_LOG(
					"INI_INVALID_DATA: MappedImage '%s' references missing Overlay%d image '%s'.",
					image->getName().str(),
					static_cast<Int>( index ) + 1,
					overlayName.str() );
				throw INI_INVALID_DATA;
			}

			image->m_overlays[index] = overlay;
		}

		image->m_buffNerfOverlays.clear();
		image->m_buffNerfOverlays.resize( image->m_buffNerfOverlayNames.size(), nullptr );
		for( size_t index = 0; index < image->m_buffNerfOverlayNames.size(); ++index )
		{
			const AsciiString& overlayName = image->m_buffNerfOverlayNames[index];
			if( overlayName.isEmpty() || overlayName.compareNoCase( "None" ) == 0 )
			{
				DEBUG_CRASH(( "MappedImageOverlay '%s' must define MappedImage fields consecutively starting at MappedImage1.", image->getName().str() ));
				throw INI_INVALID_DATA;
			}

			const Image *overlay = findImageByName( overlayName );
			if( !overlay )
			{
				DEBUG_CRASH(( "MappedImageOverlay '%s' references missing MappedImage%d '%s'.", image->getName().str(), static_cast<Int>( index ) + 1, overlayName.str() ));
				REBORN_LOG(
					"INI_INVALID_DATA: MappedImageOverlay '%s' references missing MappedImage%d '%s'.",
					image->getName().str(),
					static_cast<Int>( index ) + 1,
					overlayName.str() );
				throw INI_INVALID_DATA;
			}

			image->m_buffNerfOverlays[index] = overlay;
		}
	}

	for( ImageMap::const_iterator imageIt = m_imageMap.begin(); imageIt != m_imageMap.end(); ++imageIt )
	{
		std::vector<const Image *> resolving;
		validateOverlayChain( imageIt->second, resolving );
	}
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Validate overlay nesting so recursive overlays cannot recurse forever while drawing. */
//-------------------------------------------------------------------------------------------------
Bool ImageCollection::validateOverlayChain( const Image *image, std::vector<const Image *>& resolving ) const
{
	for( std::vector<const Image *>::const_iterator it = resolving.begin(); it != resolving.end(); ++it )
	{
		if( *it == image )
		{
			DEBUG_CRASH(( "Circular MappedImage overlay detected at '%s'.", image->getName().str() ));
			REBORN_LOG( "INI_INVALID_DATA: Circular MappedImage overlay detected at '%s'.", image->getName().str() );
			throw INI_INVALID_DATA;
		}
	}

	resolving.push_back( image );
	for( Int index = 0; index < image->getOverlayCount(); ++index )
	{
		const Image *overlay = image->getOverlay( index );
		if( overlay )
			validateOverlayChain( overlay, resolving );
	}
	for( Int index = 0; index < image->getBuffNerfOverlayCount(); ++index )
	{
		const Image *overlay = image->getBuffNerfOverlay( index );
		if( overlay )
			validateOverlayChain( overlay, resolving );
	}
	resolving.pop_back();

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve every mapped image whose inherited parent was declared later in the load. */
//-------------------------------------------------------------------------------------------------
void ImageCollection::resolvePendingInheritances()
{
	for( std::vector<PendingMappedImageInheritance>::iterator it = m_pendingInheritances.begin();
		 it != m_pendingInheritances.end(); ++it )
	{
		if( !it->m_resolved )
		{
			std::vector<AsciiString> resolving;
			resolvePendingInheritance( *it, resolving );
		}
	}

	m_pendingInheritances.clear();
}

//-------------------------------------------------------------------------------------------------
const Image *ImageCollection::findImage( NameKeyType namekey ) const
{
	ImageMap::const_iterator i = m_imageMap.find(namekey);
	return i == m_imageMap.end() ? nullptr : i->second;
}

//-------------------------------------------------------------------------------------------------
/** Find an image given the image name */
//-------------------------------------------------------------------------------------------------
const Image *ImageCollection::findImageByName( const AsciiString& name ) const
{
	return findImage(TheNameKeyGenerator->nameToLowercaseKey(name));
}

//-------------------------------------------------------------------------------------------------
/** Find an image given the image name */
//-------------------------------------------------------------------------------------------------
const Image *ImageCollection::findImageByName( const char* name ) const
{
	return findImage(TheNameKeyGenerator->nameToLowercaseKey(name));
}

//-------------------------------------------------------------------------------------------------
/** Load this image collection with all the images specified in the INI files
	* for the proper texture size directory */
//-------------------------------------------------------------------------------------------------
void ImageCollection::load( Int textureSize )
{
	char buffer[ _MAX_PATH ];
	INI ini;
	// first load in the user created mapped image files if we have them.
	WIN32_FIND_DATA findData;
	AsciiString userDataPath;
	if(TheGlobalData)
	{
		userDataPath.format("%sINI\\MappedImages\\*.ini",TheGlobalData->getPath_UserData().str());
		if(FindFirstFile(userDataPath.str(), &findData) !=INVALID_HANDLE_VALUE)
		{
			userDataPath.format("%sINI\\MappedImages",TheGlobalData->getPath_UserData().str());
			ini.loadDirectory(userDataPath, INI_LOAD_OVERWRITE, nullptr );
		}
	}

	// construct path to the mapped images folder of the correct texture size
	sprintf( buffer, "Data\\INI\\MappedImages\\TextureSize_%d", textureSize );

	// load all the ine files in that directory

	ini.loadDirectory( AsciiString( buffer ), INI_LOAD_OVERWRITE, nullptr );

	ini.loadDirectory("Data\\INI\\MappedImages\\HandCreated", INI_LOAD_OVERWRITE, nullptr );

	// Reborn: All mapped-image sources are now loaded, so late parents can safely be resolved.
	resolvePendingInheritances();
	resolveOverlays(); // Reborn: Bind ordered overlays only after every possible mapped image exists.


}
