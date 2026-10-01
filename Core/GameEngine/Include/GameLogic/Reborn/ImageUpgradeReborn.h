// FILE: ImageUpgradeReborn.h ///////////////////////////////////////////////////////////////////////////


#pragma once


#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameClient/Image.h"

class Xfer;

class ImageUpgradeRebornModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;

	AsciiString m_newSelectPortraitImageName;
	AsciiString m_newButtonImageName;
	AsciiString m_newCommandButtonImageName;

	const Image* m_newSelectPortraitImage;
	const Image* m_newButtonImage;
	const Image* m_newCommandButtonImage;

	const Image* getResolvedNewSelectPortraitImage() const;
	const Image* getResolvedNewButtonImage() const;
	const Image* getResolvedNewCommandButtonImage() const;

	ImageUpgradeRebornModuleData()
	{
		m_newSelectPortraitImage = nullptr;
		m_newButtonImage = nullptr;
		m_newCommandButtonImage = nullptr;
	}

	static void buildFieldParse(MultiIniFieldParse& p);
	void resolveNames();
};

class ImageUpgradeReborn : public UpdateModule,
	public UpgradeMux
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ImageUpgradeReborn, "ImageUpgradeReborn")
		MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ImageUpgradeReborn, ImageUpgradeRebornModuleData)

public:
	ImageUpgradeReborn(Thing* thing, const ModuleData* moduleData);

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }

	virtual UpgradeModuleInterface* getUpgrade() override { return this; }
	virtual UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }

	const Image* getNewSelectPortraitImage() const;
	const Image* getNewButtonImage() const;
	Bool isImageUpgradeActive() const { return isAlreadyUpgraded(); }

protected:
	virtual void upgradeImplementation() override {}
	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override;
	virtual void performUpgradeFX() override;
	virtual void processUpgradeRemoval() override;
	virtual Bool requiresAllActivationUpgrades() const override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};
