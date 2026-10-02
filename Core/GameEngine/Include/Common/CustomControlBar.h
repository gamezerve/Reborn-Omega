#pragma once
#include "Common/AsciiString.h"

// Reborn: Built-in control bars are independent of the Generals/Zero Hour menu theme.
Bool UseCustomControlBar();
const char* GetCustomControlBarResolution(Int index);
Int GetCustomControlBarIndex();
Bool SetCustomControlBarSelection(Bool enabled);
AsciiString GetCustomControlBarWindowName(const AsciiString& filename);
AsciiString GetCustomControlBarSchemeFile();
AsciiString GetCustomControlBarImageName(const char* name);
void ApplyCustomControlBarAppearance();
