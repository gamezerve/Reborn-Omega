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

// FILE: W3DFileSystem.cpp ////////////////////////////////////////////////////////////////////////
//
// W3D implementation of a file factory.  This replaces the W3D file factory,
// and uses GDI assets, so that
// W3D files and targa files are loaded using the GDI file interface.
// Note - this only servers up read only files.
//
// Author: John Ahlquist, Sept 2001
//				 Colin Day, November 2001
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////


// for now we maintain old legacy files
// #define MAINTAIN_LEGACY_FILES

#include "Common/ArchiveFile.h"
#include "Common/Debug.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/Registry.h"
#include "Common/RebornOmegaPreferences.h" // Reborn: Restore the infantry texture source preference before loading models.
#include "Common/UserPreferences.h"
#include "Common/RebornLog.h"
#include "W3DDevice/GameClient/W3DFileSystem.h"

#include <atomic> // Reborn: Publish the selected source safely to background texture loaders.

#include <io.h>

// Reborn: Keep the texture selection atomic because W3D also opens files on its loading thread.
static std::atomic<bool> s_legacyInfantryTextures(false);
const char* const W3DFileSystem::infantryTextureNames[INFANTRY_TEXTURE_COUNT] = {
	"zhca_airanger.tga", "zhca_nithunter.tga", "zhca_uirguard.tga",
	"zhca_uirtunfan.tga", "zhca_uiworker.tga", "zhca_niconscrpt.tga", "zhca_aimsslguy.tga",
	// Reborn: China Hacker, GLA Stinger Soldier and GLA Terrorist also have Reborn texture replacements.
	"zhca_nihacker.tga", "zhca_uirstnger.tga", "zhca_uiter.tga"
};

//-------------------------------------------------------------------------------------------------
/** Reborn: Identify only the listed logical infantry textures, including the loader's DDS alternatives. */
//-------------------------------------------------------------------------------------------------
Int W3DFileSystem::getInfantryTextureIndex(const char* filename)
{
	if (!filename) return -1;
	const char* basename = filename;
	for (const char* p = filename; *p; ++p)
		if (*p == '/' || *p == '\\') basename = p + 1;
	const char* extension = strrchr(basename, '.');
	if (!extension || (_stricmp(extension, ".tga") != 0 && _stricmp(extension, ".dds") != 0)) return -1;
	for (Int i = 0; i < INFANTRY_TEXTURE_COUNT; ++i)
	{
		const size_t length = strlen(infantryTextureNames[i]) - 4;
		if (size_t(extension - basename) == length && _strnicmp(basename, infantryTextureNames[i], length) == 0) return i;
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Choose original texture archives explicitly, never the mod PAK or a loose replacement. */
//-------------------------------------------------------------------------------------------------
ArchiveFile* W3DFileSystem::findLegacyInfantryArchive(const char* filename)
{
	const Int index = getInfantryTextureIndex(filename);
	if (index < 0 || !TheArchiveFileSystem) return nullptr;
	// Reborn: Legacy assets are TGA; suppress mod DDS overrides so the loader falls back to the original TGA.
	const char* extension = strrchr(filename, '.');
	if (_stricmp(extension, ".tga") != 0) return nullptr;
	ArchivedDirectoryInfo* directory = TheArchiveFileSystem->friend_getArchivedDirectoryInfo(TGA_DIR_PATH);
	if (!directory) return nullptr;
	AsciiString name(infantryTextureNames[index]);
	const auto entries = directory->m_files.equal_range(name);
	const char* const archives[] = { "TexturesZH.big", "Textures.big" };
	for (const char* wanted : archives)
	{
		for (auto entry = entries.first; entry != entries.second; ++entry)
		{
			ArchiveFile* archive = entry->second;
			const AsciiString path = archive->getName();
			const char* basename = path.str();
			for (const char* p = path.str(); *p; ++p)
				if (*p == '/' || *p == '\\') basename = p + 1;
			if (_stricmp(basename, wanted) == 0) return archive;
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Read the selected infantry source from both rendering and loading threads. */
//-------------------------------------------------------------------------------------------------
Bool W3DFileSystem::getLegacyInfantryTextures()
{
	return s_legacyInfantryTextures.load();
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Publish the infantry source only after pending texture loads have been drained. */
//-------------------------------------------------------------------------------------------------
void W3DFileSystem::setLegacyInfantryTextures(Bool enabled)
{
	s_legacyInfantryTextures.store(enabled != FALSE);
}

// DEFINES ////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Game file access.  At present this allows us to access test assets, assets from
	* legacy GDI assets, and the current flat directory access for textures, models etc */
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
typedef enum
{
	FILE_TYPE_COMPLETELY_UNKNOWN = 0,	// MBL 08.15.2002 - compile error with FILE_TYPE_UNKNOWN, is constant
	FILE_TYPE_W3D,
	FILE_TYPE_TGA,
	FILE_TYPE_DDS,
} GameFileType;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameFileClass::GameFileClass( char const *filename )
{

	m_theFile = nullptr;
	m_fileExists = FALSE;
	m_filePath[0] = 0;
	m_filename[0] = 0;

	if( filename )
		Set_Name( filename );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameFileClass::GameFileClass()
{

	m_fileExists = FALSE;
	m_theFile = nullptr;
	m_filePath[ 0 ] = 0;
	m_filename[ 0 ] = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameFileClass::~GameFileClass()
{

	Close();

}

//-------------------------------------------------------------------------------------------------
/** Gets the file name */
//-------------------------------------------------------------------------------------------------
char const * GameFileClass::File_Name() const
{

	return m_filename;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
inline static Bool isImageFileType( GameFileType fileType )
{
	return (fileType == FILE_TYPE_TGA || fileType == FILE_TYPE_DDS);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static GameFileType getFileType( char const *filename )
{
	if (char const *extension = strrchr( filename, '.' ))
	{
		// test the extension to recognize a few key file types
		if( stricmp( extension, ".w3d" ) == 0 )
			return FILE_TYPE_W3D;
		else if( stricmp( extension, ".tga" ) == 0 )
			return FILE_TYPE_TGA;
		else if( stricmp( extension, ".dds" ) == 0 )
			return FILE_TYPE_DDS;
	}

	return FILE_TYPE_COMPLETELY_UNKNOWN;  // MBL FILE_TYPE_UNKNOWN change due to compile error
}

//-------------------------------------------------------------------------------------------------
/**
	Sets the file name, and finds the GDI asset if present.


	Well, that is the worst comment ever for the most important function there is.
	Everything comes through this.  This builds the directory and tests for the file
	in several different places.

	First we look in Language subfolders so that our Perforce	build can handle files that have
	been localized but were in Generals.

	Then we do the normal TheFileSystem lookup.  In there it does LocalFile (Art/Textures) then it does
	big files (which internally are also Art/Textures).

	Finally we try UserData.
*/
//-------------------------------------------------------------------------------------------------
char const * GameFileClass::Set_Name( char const *filename )
{

	if( Is_Open() )
		Close();

	// save the filename
	strlcpy( m_filename, filename, _MAX_PATH );

	// Reborn: Resolve only opted-in infantry textures directly from their original archive; all other lookup rules remain unchanged.
	m_legacyTextureArchive = nullptr;
	m_useLegacyTextureSource = FALSE;
	if (W3DFileSystem::getLegacyInfantryTextures() && W3DFileSystem::getInfantryTextureIndex(filename) >= 0)
	{
		m_useLegacyTextureSource = TRUE;
		m_legacyTextureArchive = W3DFileSystem::findLegacyInfantryArchive(filename);
		m_fileExists = m_legacyTextureArchive != nullptr;
		if (m_fileExists)
			snprintf(m_filePath, ARRAY_SIZE(m_filePath), "%s%s", TGA_DIR_PATH,
				W3DFileSystem::infantryTextureNames[W3DFileSystem::getInfantryTextureIndex(filename)]);
		return m_filename;
	}

	GameFileType fileType = getFileType(filename);

	// We need to be able to grab w3d's from a localization dir, since Germany hates exploding people units.
	if( fileType == FILE_TYPE_W3D )
	{
		static const char *localizedPathFormat = "Data/%s/Art/W3D/";
		sprintf(m_filePath,localizedPathFormat, GetRegistryLanguage().str());
		strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

	}
	// We need to be able to grab images from a localization dir, because Art has a fetish for baked-in text.  Munkee.
	else if( isImageFileType(fileType) )
	{
		static const char *localizedPathFormat = "Data/%s/Art/Textures/";
		sprintf(m_filePath,localizedPathFormat, GetRegistryLanguage().str());
		strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

	}

	// see if the file exists
	m_fileExists = TheFileSystem->doesFileExist( m_filePath );



	// Now try the main lookup of hitting local files and big files
	if( m_fileExists == FALSE )
	{
		// all .w3d files are in W3D_DIR_PATH, all .tga files are in TGA_DIR_PATH
		if( fileType == FILE_TYPE_W3D )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(W3D_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, W3D_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}
		else if( isImageFileType(fileType) )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(TGA_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, TGA_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}
		else
			strlcpy(m_filePath, filename, ARRAY_SIZE(m_filePath));

		// see if the file exists
		m_fileExists = TheFileSystem->doesFileExist( m_filePath );
	}



	// maintain legacy compatibility directories for now
	#ifdef MAINTAIN_LEGACY_FILES
	if( m_fileExists == FALSE )
	{

		if( fileType == FILE_TYPE_W3D )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(LEGACY_W3D_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, LEGACY_W3D_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}
		else if( isImageFileType(fileType) )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(LEGACY_TGA_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, LEGACY_TGA_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}

		// see if the file exists
		m_fileExists = TheFileSystem->doesFileExist( m_filePath );

	}
	#endif



	// if file is still not found, try the test art folders
	#ifdef LOAD_TEST_ASSETS
	if( m_fileExists == FALSE )
	{

		if( fileType == FILE_TYPE_W3D )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(TEST_W3D_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, TEST_W3D_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}
		else if( isImageFileType(fileType) )
		{

			static_assert(ARRAY_SIZE(m_filePath) >= ARRAY_SIZE(TEST_TGA_DIR_PATH), "Incorrect array size");
			strcpy( m_filePath, TEST_TGA_DIR_PATH );
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}

		// see if the file exists
		m_fileExists = TheFileSystem->doesFileExist( m_filePath );

	}
	#endif

	// We allow the user to load their own images for various assets (like the control bar)
	if( m_fileExists == FALSE  && TheGlobalData)
	{
		if( fileType == FILE_TYPE_W3D )
		{
			sprintf(m_filePath,USER_W3D_DIR_PATH, TheGlobalData->getPath_UserData().str());
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}
		else if( isImageFileType(fileType) )
		{
			sprintf(m_filePath,USER_TGA_DIR_PATH, TheGlobalData->getPath_UserData().str());
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}

		// see if the file exists
		m_fileExists = TheFileSystem->doesFileExist( m_filePath );

	}


	// We need to be able to temporarily copy over the map preview for whichever directory it came from
	if( m_fileExists == FALSE  && TheGlobalData)
	{
		if( fileType == FILE_TYPE_TGA ) // just TGA, since we don't do dds previews
		{
			sprintf(m_filePath,MAP_PREVIEW_DIR_PATH, TheGlobalData->getPath_UserData().str());
			strlcat(m_filePath, filename, ARRAY_SIZE(m_filePath));

		}

		// see if the file exists
		m_fileExists = TheFileSystem->doesFileExist( m_filePath );

	}

	return m_filename;

}

//-------------------------------------------------------------------------------------------------
/** If we found a gdi asset, the file is available. */
//-------------------------------------------------------------------------------------------------
bool GameFileClass::Is_Available( int forced )
{

	// not maintaining any GDF compatibility, all files should be where the m_filePath says
	return m_fileExists;

}

//-------------------------------------------------------------------------------------------------
/** Is the file open. */
//-------------------------------------------------------------------------------------------------
bool GameFileClass::Is_Open() const
{
	return m_theFile != nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Open the named file. */
//-------------------------------------------------------------------------------------------------
int  GameFileClass::Open(char const *filename, int rights)
{
	Set_Name(filename);
	if (Is_Available(false)) {
		return(Open(rights));
	}
	return(false);
}

//-------------------------------------------------------------------------------------------------
/** Open the file using the current file name. */
//-------------------------------------------------------------------------------------------------
int  GameFileClass::Open(int rights)
{
	if( rights != READ )
	{
		return(false);
	}

	// Reborn: Open the archive selected at name resolution instead of reapplying mod/loose-file priority.
	m_theFile = m_useLegacyTextureSource
		? (m_legacyTextureArchive ? m_legacyTextureArchive->openFile(m_filePath, File::READ | File::BINARY) : nullptr)
		: TheFileSystem->openFile(m_filePath, File::READ | File::BINARY);

	return (m_theFile != nullptr);
}

//-------------------------------------------------------------------------------------------------
/** Read. */
//-------------------------------------------------------------------------------------------------
int GameFileClass::Read(void *buffer, int len)
{
	if (m_theFile) {
		return m_theFile->read(buffer, len);
	}
	return(0);
}

//-------------------------------------------------------------------------------------------------
/** Seek. */
//-------------------------------------------------------------------------------------------------
int GameFileClass::Seek(int pos, int dir)
{
	File::seekMode mode = File::CURRENT;
	switch (dir) {
		default:
		case SEEK_CUR: mode = File::CURRENT; break;
		case SEEK_SET: mode = File::START; break;
		case SEEK_END: mode = File::END; break;
	}
	if (m_theFile) {
		return m_theFile->seek(pos, mode);
	}
	return 0xFFFFFFFF;
}

//-------------------------------------------------------------------------------------------------
/** Size. */
//-------------------------------------------------------------------------------------------------
int GameFileClass::Size()
{
	if (m_theFile) {
		return m_theFile->size();
	}
	return 0xFFFFFFFF;
}

//-------------------------------------------------------------------------------------------------
/** Write. */
//-------------------------------------------------------------------------------------------------
int GameFileClass::Write(void const *buffer, Int len)
{
#ifdef RTS_DEBUG
#endif
	return(0);
}

//-------------------------------------------------------------------------------------------------
/** Close. */
//-------------------------------------------------------------------------------------------------
void GameFileClass::Close()
{
	if (m_theFile) {
		m_theFile->close();
		m_theFile = nullptr;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// W3DFileSystem Class ////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
W3DFileSystem *TheW3DFileSystem = nullptr;

//-------------------------------------------------------------------------------------------------
/** Constructor.  Creating an instance of this class overrides the default
W3D file factory.  */
//-------------------------------------------------------------------------------------------------
W3DFileSystem::W3DFileSystem()
{
	_TheFileFactory = this; // override the w3d file factory.

	// Reborn: Modern textures remain the default for missing or invalid preference values.
	UserPreferences preferences;
	LoadRebornOmegaPreferences(preferences);
	AsciiString requested = preferences["UseLegacyInfantryTextures"];
	requested.toLower();
	Bool legacy = requested == "yes";
	if (legacy)
	{
		for (const char* name : infantryTextureNames)
			if (!findLegacyInfantryArchive(name))
			{
				REBORN_LOG("Legacy infantry textures unavailable: original archive entry %s missing; using modern textures", name);
				// Reborn: Mirror this texture-source diagnostic to the debugger log.
				DEBUG_LOG(("Legacy infantry textures unavailable: original archive entry %s missing; using modern textures" "\n", name));
				legacy = FALSE;
				break;
			}
	}
	setLegacyInfantryTextures(legacy);

#if RTS_ZEROHOUR && PRIORITIZE_TEXTURES_BY_SIZE
	reprioritizeTexturesBySize();
#endif
}

//-------------------------------------------------------------------------------------------------
/** Destructor.  This removes the W3D file factory, so shouldn't be done until
after W3D is shutdown.  */
//-------------------------------------------------------------------------------------------------
W3DFileSystem::~W3DFileSystem()
{
	_TheFileFactory = nullptr; // remove the w3d file factory.
}

//-------------------------------------------------------------------------------------------------
/** Gets a file with the specified filename. */
//-------------------------------------------------------------------------------------------------
FileClass * W3DFileSystem::Get_File( char const *filename )
{
	return NEW GameFileClass( filename );	// poolify
}

//-------------------------------------------------------------------------------------------------
/** Releases a file returned by Get_File. */
//-------------------------------------------------------------------------------------------------
void W3DFileSystem::Return_File( FileClass *file )
{
	delete file;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DFileSystem::reprioritizeTexturesBySize()
{
	ArchivedDirectoryInfo* dirInfo = TheArchiveFileSystem->friend_getArchivedDirectoryInfo(TGA_DIR_PATH);
	if (dirInfo != nullptr)
	{
		reprioritizeTexturesBySize(*dirInfo);
	}
}

//-------------------------------------------------------------------------------------------------
// TheSuperHackers @info This function moves the largest texture of its name to the front of the
// directory info. The algorithm only prioritizes the first item in the multimap, because this is
// what we currently need:
// Before: A(256kb) B(128kb) C(512kb)
// After:  C(512kb) B(128kb) A(256kb)
// 
// Catered to specific game archives only. This ensures that user created archives are not included
// for the re-prioritization of textures.
//-------------------------------------------------------------------------------------------------
void W3DFileSystem::reprioritizeTexturesBySize(ArchivedDirectoryInfo& dirInfo)
{
	const char* const superiorArchive = "Textures.big";
	const char* const inferiorArchive = "TexturesZH.big";

	ArchivedFileLocationMap::iterator it0;
	ArchivedFileLocationMap::iterator it1 = dirInfo.m_files.begin();
	ArchivedFileLocationMap::iterator end = dirInfo.m_files.end();

	if (it1 != end)
	{
		it0 = it1;
		++it1;
	}

	for (; it1 != end; ++it1)
	{
		const AsciiString& file0 = it0->first;
		const AsciiString& file1 = it1->first;

		if (file0 == file1)
		{
			GameFileType type = getFileType(file0.str());
			if (isImageFileType(type))
			{
				ArchiveFile* archive0 = it0->second;
				ArchiveFile* archive1 = it1->second;
				FileInfo info0;
				FileInfo info1;
				AsciiString filepath(dirInfo.m_path);
				filepath.concat(file0);

				if (archive0->getFileInfo(filepath, &info0) && archive1->getFileInfo(filepath, &info1))
				{
					if (info0.size() < info1.size() &&
						archive0->getName().endsWithNoCase(inferiorArchive) &&
						archive1->getName().endsWithNoCase(superiorArchive))
					{
						std::swap(it0->second, it1->second);

#if ENABLE_FILESYSTEM_LOGGING
						DEBUG_LOG(("W3DFileSystem::reprioritizeTexturesBySize - prioritize %s(%ukb) from %s over %s(%ukb) from %s",
							file1.str(), UnsignedInt(info1.size() / 1024), archive1->getName().str(),
							file0.str(), UnsignedInt(info0.size() / 1024), archive0->getName().str()));
#endif
					}
				}
			}
		}
		else
		{
			it0 = it1;
		}
	}
}
