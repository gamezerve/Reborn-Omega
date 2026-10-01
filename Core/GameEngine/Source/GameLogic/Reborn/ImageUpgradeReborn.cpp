// FILE: ImageUpgradeReborn.cpp ///////////////////////////////////////////////////////////////////////////


#include "PreRTS.h"
#include "Common/Xfer.h"
#include "GameLogic/Reborn/ImageUpgradeReborn.h"

void ImageUpgradeRebornModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse dataFieldParse[] =
	{
		{ "NewSelectPortrait", INI::parseAsciiString, nullptr, offsetof(ImageUpgradeRebornModuleData, m_newSelectPortraitImageName) },
		{ "NewButtonImage", INI::parseAsciiString, nullptr, offsetof(ImageUpgradeRebornModuleData, m_newButtonImageName) },
		{ "NewCommandButtonImage", INI::parseAsciiString, nullptr, offsetof(ImageUpgradeRebornModuleData, m_newCommandButtonImageName) },
		{ 0, 0, 0, 0 }
	};

	UpdateModuleData::buildFieldParse(p);
	p.add(dataFieldParse);
	p.add(UpgradeMuxData::getFieldParse(), offsetof(ImageUpgradeRebornModuleData, m_upgradeMuxData));
}



ImageUpgradeReborn::ImageUpgradeReborn(Thing* thing, const ModuleData* moduleData)
	: UpdateModule(thing, moduleData)
{
	ImageUpgradeRebornModuleData* d = const_cast<ImageUpgradeRebornModuleData*>(getImageUpgradeRebornModuleData());

	if (d->m_newSelectPortraitImage == nullptr && d->m_newSelectPortraitImageName.isNotEmpty())
	{
		d->m_newSelectPortraitImage = TheMappedImageCollection->findImageByName(d->m_newSelectPortraitImageName);

		DEBUG_ASSERTCRASH(
			d->m_newSelectPortraitImage,
			("ImageUpgradeReborn is looking for SelectPortrait %s but can't find it.",
				d->m_newSelectPortraitImageName.str())
		);
	}

	if (d->m_newButtonImage == nullptr && d->m_newButtonImageName.isNotEmpty())
	{
		d->m_newButtonImage = TheMappedImageCollection->findImageByName(d->m_newButtonImageName);

		DEBUG_ASSERTCRASH(
			d->m_newButtonImage,
			("ImageUpgradeReborn is looking for ButtonImage %s but can't find it.",
				d->m_newButtonImageName.str())
		);
	}
}

const Image* ImageUpgradeReborn::getNewSelectPortraitImage() const
{
	return getImageUpgradeRebornModuleData()->getResolvedNewSelectPortraitImage();
}

const Image* ImageUpgradeReborn::getNewButtonImage() const
{
	return getImageUpgradeRebornModuleData()->getResolvedNewButtonImage();
}

void ImageUpgradeReborn::getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const
{
	getImageUpgradeRebornModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
}

void ImageUpgradeReborn::performUpgradeFX()
{
	getImageUpgradeRebornModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
}

void ImageUpgradeReborn::processUpgradeRemoval()
{
	getImageUpgradeRebornModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
}

Bool ImageUpgradeReborn::requiresAllActivationUpgrades() const
{
	return getImageUpgradeRebornModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
}



ImageUpgradeReborn::~ImageUpgradeReborn()
{
}

void ImageUpgradeReborn::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	upgradeMuxCRC(xfer);
}

void ImageUpgradeReborn::xfer(Xfer* xfer)
{
	UpdateModule::xfer(xfer);
	upgradeMuxXfer(xfer);
}

void ImageUpgradeReborn::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	upgradeMuxLoadPostProcess();
}

const Image* ImageUpgradeRebornModuleData::getResolvedNewSelectPortraitImage() const
{
	ImageUpgradeRebornModuleData* d = const_cast<ImageUpgradeRebornModuleData*>(this);

	if (d->m_newSelectPortraitImage == nullptr && d->m_newSelectPortraitImageName.isNotEmpty())
	{
		d->m_newSelectPortraitImage = TheMappedImageCollection->findImageByName(d->m_newSelectPortraitImageName);

		DEBUG_ASSERTCRASH(
			d->m_newSelectPortraitImage,
			("ImageUpgradeReborn is looking for SelectPortrait %s but can't find it.",
				d->m_newSelectPortraitImageName.str())
		);
	}

	return d->m_newSelectPortraitImage;
}

const Image* ImageUpgradeRebornModuleData::getResolvedNewButtonImage() const
{
	ImageUpgradeRebornModuleData* d = const_cast<ImageUpgradeRebornModuleData*>(this);

	if (d->m_newButtonImage == nullptr && d->m_newButtonImageName.isNotEmpty())
	{
		d->m_newButtonImage = TheMappedImageCollection->findImageByName(d->m_newButtonImageName);

		DEBUG_ASSERTCRASH(
			d->m_newButtonImage,
			("ImageUpgradeReborn is looking for ButtonImage %s but can't find it.",
				d->m_newButtonImageName.str())
		);
	}

	return d->m_newButtonImage;
}


const Image* ImageUpgradeRebornModuleData::getResolvedNewCommandButtonImage() const
{
	ImageUpgradeRebornModuleData* d = const_cast<ImageUpgradeRebornModuleData*>(this);

	if (d->m_newCommandButtonImage == nullptr && d->m_newCommandButtonImageName.isNotEmpty())
	{
		d->m_newCommandButtonImage = TheMappedImageCollection->findImageByName(d->m_newCommandButtonImageName);

		DEBUG_ASSERTCRASH(
			d->m_newCommandButtonImage,
			("ImageUpgradeReborn is looking for CommandButtonImage %s but can't find it.",
				d->m_newCommandButtonImageName.str())
		);
	}

	return d->m_newCommandButtonImage;
}
