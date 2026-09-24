///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparser.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectDeparser.h"
#include "ObjectDeparserDialog.h"
#include "ObjectDeparserLoadingDialog.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/DamageFX.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/LocalFileSystem.h"
#include "Common/ModuleFactory.h"
#include "Common/MultiplayerSettings.h"
#include "Common/NameKeyGenerator.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/SubsystemInterface.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"

#include "GameClient/Anim2D.h"
#include "GameClient/ControlBar.h"
#include "GameClient/FXList.h"
#include "GameClient/Image.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/GameText.h"

#include "GameLogic/Armor.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/Weapon.h"

#include "MilesAudioDevice/MilesAudioManager.h"

#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"

#include "Win32Device/Common/Win32BIGFileSystem.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "Win32Device/GameClient/Win32Mouse.h"

#ifdef REBORN_BUILD
#include "../../../Main/RebornOmegaDllLoad.h"
#endif

static SubsystemInterfaceList TheSubsystemListRecord;

template<class SUBSYSTEM>
static void initSubsystem(
	SUBSYSTEM*& sysref,
	SUBSYSTEM* sys,
	const char* path1 = nullptr,
	const char* path2 = nullptr)
{
	sysref = sys;
	TheSubsystemListRecord.initSubsystem(sys, path1, path2, nullptr);
}

CObjectDeparserApp theApp;

HWND ApplicationHWnd = nullptr;
HINSTANCE ApplicationHInstance = nullptr;

Win32Mouse* TheWin32Mouse = nullptr;

const char* gAppPrefix = "od_";
const Char* g_strFile = "data\\Generals.str";
const Char* g_csfFile = "data\\%s\\Generals.csf";

BEGIN_MESSAGE_MAP(CObjectDeparserApp, CWinApp)
END_MESSAGE_MAP()

CObjectDeparserApp::CObjectDeparserApp()
{
}

BOOL CObjectDeparserApp::InitInstance()
{
	CWinApp::InitInstance();

	AfxEnableControlContainer();
	AfxInitRichEdit2();

	CObjectDeparserLoadingDialog loadingDialog;

	if (!loadingDialog.Create(IDD_OBJECT_DEPARSER_LOADING))
		return FALSE;

	loadingDialog.ShowWindow(SW_SHOW);
	loadingDialog.UpdateWindow();

	loadingDialog.setProgress(
		2,
		"Validating Reborn Omega runtime...");

#ifdef REBORN_BUILD
	if (!validateRebornOmegaRuntime())
	{
		loadingDialog.DestroyWindow();
		return FALSE;
	}
#endif

	ApplicationHWnd = GetDesktopWindow();
	ApplicationHInstance = AfxGetInstanceHandle();

	loadingDialog.setProgress(
		5,
		"Initializing memory system...");

	initMemoryManager();

	m_definitionCatalog.clear();

	INI::setBlockParsedProc(
		&ParsedDefinitionCatalog::capture,
		&m_definitionCatalog);

	loadingDialog.setProgress(
		8,
		"Initializing file system...");

	TheNameKeyGenerator = new NameKeyGenerator;
	TheNameKeyGenerator->init();

	TheFileSystem = new FileSystem;

	initSubsystem(
		TheLocalFileSystem,
		static_cast<LocalFileSystem*>(new Win32LocalFileSystem));

	initSubsystem(
		TheArchiveFileSystem,
		static_cast<ArchiveFileSystem*>(new Win32BIGFileSystem));

	loadingDialog.setProgress(
		12,
		"Loading data archives...");

	TheArchiveFileSystem->loadMods();

	INI ini;

	loadingDialog.setProgress(
		16,
		"Loading game data...");

	initSubsystem(
		TheWritableGlobalData,
		TheWritableGlobalData,
		"Data\\INI\\Default\\GameData",
		"Data\\INI\\GameData");

	initSubsystem(
		TheGameText,
		CreateGameTextInterface());

	initSubsystem(
		TheMappedImageCollection,
		new ImageCollection());

	TheMappedImageCollection->load(512);

	loadingDialog.setProgress(
		22,
		"Loading sciences and multiplayer data...");

	initSubsystem(
		TheScienceStore,
		new ScienceStore(),
		"Data\\INI\\Default\\Science",
		"Data\\INI\\Science");

	initSubsystem(
		TheMultiplayerSettings,
		new MultiplayerSettings(),
		"Data\\INI\\Default\\Multiplayer",
		"Data\\INI\\Multiplayer");

	loadingDialog.setProgress(
		28,
		"Initializing audio and modules...");

	initSubsystem(
		TheAudio,
		static_cast<AudioManager*>(new MilesAudioManager()));

	initSubsystem(
		TheModuleFactory,
		static_cast<ModuleFactory*>(new W3DModuleFactory()));

	initSubsystem(
		TheSidesList,
		new SidesList());

	initSubsystem(
		TheRankInfoStore,
		new RankInfoStore(),
		nullptr,
		"Data\\INI\\Rank");

	loadingDialog.setProgress(
		34,
		"Loading player templates...");

	initSubsystem(
		ThePlayerTemplateStore,
		new PlayerTemplateStore(),
		"Data\\INI\\Default\\PlayerTemplate",
		"Data\\INI\\PlayerTemplate");

	initSubsystem(
		TheSpecialPowerStore,
		new SpecialPowerStore(),
		"Data\\INI\\Default\\SpecialPower",
		"Data\\INI\\SpecialPower");

	loadingDialog.setProgress(
		34,
		"Loading player templates...");

	initSubsystem(
		TheParticleSystemManager,
		static_cast<ParticleSystemManager*>(new W3DParticleSystemManager()));

	initSubsystem(
		TheFXListStore,
		new FXListStore(),
		"Data\\INI\\Default\\FXList",
		"Data\\INI\\FXList");

	loadingDialog.setProgress(
		50,
		"Loading weapons...");

	initSubsystem(
		TheWeaponStore,
		new WeaponStore(),
		nullptr,
		"Data\\INI\\Weapon");

	loadingDialog.setProgress(
		58,
		"Loading object creation lists...");

	initSubsystem(
		TheObjectCreationListStore,
		new ObjectCreationListStore(),
		"Data\\INI\\Default\\ObjectCreationList",
		"Data\\INI\\ObjectCreationList");

	loadingDialog.setProgress(
		64,
		"Loading locomotors...");

	initSubsystem(
		TheLocomotorStore,
		new LocomotorStore(),
		nullptr,
		"Data\\INI\\Locomotor");

	loadingDialog.setProgress(
		70,
		"Loading damage and armor data...");

	initSubsystem(
		TheDamageFXStore,
		new DamageFXStore(),
		nullptr,
		"Data\\INI\\DamageFX");

	initSubsystem(
		TheArmorStore,
		new ArmorStore(),
		nullptr,
		"Data\\INI\\Armor");

	loadingDialog.setProgress(
		76,
		"Loading objects...");

	initSubsystem(
		TheThingFactory,
		new ThingFactory(),
		"Data\\INI\\Default\\Object",
		"Data\\INI\\Object");

	initSubsystem(
		TheCrateSystem,
		new CrateSystem(),
		"Data\\INI\\Default\\Crate",
		"Data\\INI\\Crate");

	loadingDialog.setProgress(
		88,
		"Loading upgrades...");

	initSubsystem(
		TheUpgradeCenter,
		new UpgradeCenter,
		"Data\\INI\\Default\\Upgrade",
		"Data\\INI\\Upgrade");

	initSubsystem(
		TheControlBar,
		new ControlBar());

	initSubsystem(
		TheAnim2DCollection,
		new Anim2DCollection);

	loadingDialog.setProgress(
		94,
		"Resolving INI references...");

	TheSubsystemListRecord.postProcessLoadAll();

	updateObjectIniLoadTimestamp();

	loadingDialog.setProgress(
		100,
		"Loading complete.");

	loadingDialog.DestroyWindow();

	CObjectDeparserDialog dialog;
	m_pMainWnd = &dialog;

	dialog.DoModal();

	return FALSE;
}

Bool CObjectDeparserApp::reloadObjectDatabase(
	ObjectReloadProgressProc progressProc,
	void* userData)
{
	if (!TheThingFactory)
		return FALSE;

	try
	{
		TheThingFactory->reloadFromINI(
			"Data\\INI\\Default\\Object",
			"Data\\INI\\Object",
			progressProc,
			userData);

		updateObjectIniLoadTimestamp();

		return TRUE;
	}
	catch (...)
	{
		if (progressProc)
			progressProc(
				0,
				"Reload failed.",
				userData);

		return FALSE;
	}
}

void CObjectDeparserApp::updateObjectIniLoadTimestamp()
{
	m_lastObjectIniLoadTime =
		CTime::GetCurrentTime().Format("%Y-%m-%d %H:%M:%S");
}

int CObjectDeparserApp::ExitInstance()
{

	INI::setBlockParsedProc(nullptr, nullptr);

	TheSubsystemListRecord.shutdownAll();

	delete TheFileSystem;
	TheFileSystem = nullptr;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = nullptr;

	shutdownMemoryManager();

	return CWinApp::ExitInstance();
}
