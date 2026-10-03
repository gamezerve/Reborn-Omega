#include "PreRTS.h"
#include "Common/CustomControlBar.h"
#include "Common/FileSystem.h"
#include "Common/File.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Display.h"
#include "GameClient/ControlBar.h" // Reborn: Observer skins follow observer mode, not the watched faction.
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/GameText.h" // Reborn: Restore localized science close text for the normal bar.
#include "GameClient/GadgetPushButton.h" // Reborn: Custom science close buttons use only their arrow art.
#include "GameClient/GadgetStaticText.h" // Reborn: Observer row captions retain localized text without forced line breaks.
#include "GameClient/Image.h"
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>

// Reborn: No directory scan, package mounting or WND rewriting occurs in the game.
static Bool s_customControlBarEnabled = FALSE;
static const char* s_resolutions[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" };

//-------------------------------------------------------------------------------------------------
/** Reborn: Apply only the enable toggle; art resolution follows the current display. */
//-------------------------------------------------------------------------------------------------
Bool SetCustomControlBarSelection(Bool enabled)
{
    const Bool changed = enabled != s_customControlBarEnabled;
    s_customControlBarEnabled = enabled;
    return changed;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Keep the selected custom bar independent from UseGeneralsLayout. */
//-------------------------------------------------------------------------------------------------
Bool UseCustomControlBar() { return s_customControlBarEnabled; }

//-------------------------------------------------------------------------------------------------
/** Reborn: Expose stable preference values rather than user-provided filenames. */
//-------------------------------------------------------------------------------------------------
const char* GetCustomControlBarResolution(Int index)
{
    return index >= 0 && index < 5 ? s_resolutions[index] : "";
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Choose 1080p art through Full HD and 2160p art above either Full HD dimension. */
//-------------------------------------------------------------------------------------------------
Int GetCustomControlBarIndex()
{
    // Reborn: Startup may precede display creation; never cache that temporary fallback.
    return TheDisplay && (TheDisplay->getWidth() > 1920 || TheDisplay->getHeight() > 1080) ? 4 : 2;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve supported screens to ready-made Custom WND files, preserving canonical window IDs. */
//-------------------------------------------------------------------------------------------------
AsciiString GetCustomControlBarWindowName(const AsciiString& filename)
{
    if (!UseCustomControlBar()) return filename;
    std::string name(filename.str());
    std::replace(name.begin(), name.end(), '\\', '/');
    if (name.compare(0, 7, "Window/") == 0 || name.compare(0, 7, "window/") == 0)
        name.erase(0, 7);
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)tolower(c); });
    // Reborn: Replay and defeated-player observers share a prepared skin without replacing runtime controls.
    if (lower == "controlbar.wnd" && TheControlBar && TheControlBar->isObserverControlBarOn())
        return AsciiString("Window\\CustomControlBar\\Observer\\ControlBarObserverCustom.wnd");
    // Reborn: Screens outside this list still follow their normal or Generals theme.
    static const char* supported[] = {
        "controlbar.wnd", "controlbarpopupdescription.wnd", "diplomacy.wnd", "diplomacygen.wnd",
        "generalsexppoints.wnd", "genpowersshortcutbarchina.wnd", "genpowersshortcutbargla.wnd",
        "genpowersshortcutbarus.wnd", "menus/quitmenu.wnd", "menus/quitmenugen.wnd",
        "menus/quitnosave.wnd", "menus/quitnosavegen.wnd", "menus/quitmessagebox.wnd",
        "menus/quitmessageboxgen.wnd", "menus/observerquit.wnd", "menus/observerquitgen.wnd"
    };
    for (const char* candidate : supported)
        if (lower == candidate)
        {
            name.insert(name.size() - 4, "Custom");
            AsciiString result;
            result.format("Window\\CustomControlBar\\%s\\%s", s_resolutions[GetCustomControlBarIndex()], name.c_str());
            std::string path(result.str());
            std::replace(path.begin(), path.end(), '/', '\\');
            result.set(path.c_str());
            return result;
        }
    return filename;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Load the selected fixed scheme after the normal schemes, without replacing base resources. */
//-------------------------------------------------------------------------------------------------
AsciiString GetCustomControlBarSchemeFile()
{
    AsciiString result;
    if (UseCustomControlBar() && TheControlBar && TheControlBar->isObserverControlBarOn())
        result.set("Data\\INI\\CustomControlBar\\ObserverScheme.ini");
    else if (UseCustomControlBar())
        result.format("Data\\INI\\CustomControlBar\\%s\\ControlBarScheme.ini", s_resolutions[GetCustomControlBarIndex()]);
    return result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Both art resolutions coexist with standard images in the normal image collection. */
//-------------------------------------------------------------------------------------------------
AsciiString GetCustomControlBarImageName(const char* name)
{
    AsciiString result;
    result.format("RebornCBP_%s_%s", GetCustomControlBarIndex() < 3 ? "1080" : "2160", name);
    return result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Read RGBA values emitted by the offline WND preparation tool. */
//-------------------------------------------------------------------------------------------------
static Color readAppearanceColor(std::istringstream& input)
{
    Int r = 0, g = 0, b = 0, a = 0;
    input >> r >> g >> b >> a;
    return GameMakeColor(r, g, b, a);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Apply fixed visual records to existing windows; never destroy command bindings or input owners. */
//-------------------------------------------------------------------------------------------------
void ApplyCustomControlBarAppearance()
{
    if (!TheWindowManager || !TheDisplay || !TheFileSystem) return;
    AsciiString path;
    if (UseCustomControlBar() && TheControlBar && TheControlBar->isObserverControlBarOn())
        path.set("Data\\INI\\CustomControlBar\\ObserverAppearance.txt");
    else if (UseCustomControlBar())
        path.format("Data\\INI\\CustomControlBar\\%s\\Appearance.txt", s_resolutions[GetCustomControlBarIndex()]);
    else
        path.set("Data\\INI\\CustomControlBar\\NormalAppearance.txt");
    File* file = TheFileSystem->openFile(path.str(), File::READ);
    if (!file) return;
    const Int length = file->size();
    if (length <= 0 || length > 4 * 1024 * 1024) { file->close(); return; }
    std::string text(length, '\0');
    const Int read = file->read(&text[0], length);
    file->close();
    if (read != length) return;
    std::istringstream rows(text);
    std::string line;
    while (std::getline(rows, line))
    {
        std::istringstream input(line);
        char kind;
        std::string name;
        input >> kind >> name;
        if (!input) continue;
        // Reborn: Quit and diplomacy layouts are refreshed by their normal layout lifecycle.
        if (name.find("ControlBar.wnd:") != 0 && name.find("GeneralsExpPoints.wnd:") != 0 &&
            name.find("GenPowersShortcutBar") != 0 && name.find("ControlBarPopupDescription.wnd:") != 0)
            continue;
        GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name.c_str()));
        if (!window) continue;
        if (kind == 'R')
        {
            Int left, top, right, bottom, width, height;
            input >> left >> top >> right >> bottom >> width >> height;
            if (!input || width <= 0 || height <= 0) continue;
            const Real sx = (Real)TheDisplay->getWidth() / width;
            const Real sy = (Real)TheDisplay->getHeight() / height;
            Int px = 0, py = 0;
            if (window->winGetParent()) window->winGetParent()->winGetScreenPosition(&px, &py);
            window->winSetPosition((Int)(left * sx) - px, (Int)(top * sy) - py);
            window->winSetSize((Int)((right - left) * sx), (Int)((bottom - top) * sy));
        }
        else if (kind == 'F')
        {
            std::string font;
            Int size, bold;
            input >> std::quoted(font) >> size >> bold;
            if (input) window->winSetFont(TheWindowManager->winFindFont(font.c_str(), size, bold != 0));
        }
        else if (kind == 'H')
        {
            std::string header;
            input >> header;
            if (TheHeaderTemplateManager && header != "[None]")
            {
                GameFont* font = TheHeaderTemplateManager->getFontFromTemplate(header.c_str());
                if (font) window->winSetFont(font);
            }
        }
        else if (kind == 'T')
        {
            Color color[6];
            for (Int i = 0; i < 6; ++i) color[i] = readAppearanceColor(input);
            if (!input) continue;
            window->winSetEnabledTextColors(color[0], color[1]);
            window->winSetDisabledTextColors(color[2], color[3]);
            window->winSetHiliteTextColors(color[4], color[5]);
        }
        else if (kind == 'B' && name == "GeneralsExpPoints.wnd:ButtonExit")
        {
            // Reborn: Toggle the science close caption without hiding or disabling its clickable arrow.
            std::string key;
            input >> std::quoted(key);
            if (!input) continue;
            if (key.empty()) GadgetButtonSetText(window, L"");
            else if (TheGameText) GadgetButtonSetText(window, TheGameText->fetch(key.c_str()));
        }
        else if (kind == 'S')
        {
            // Reborn: Restore drawing flags only; input bindings, hidden/enabled state and queue contents stay intact.
            Int image, seeThrough;
            input >> image >> seeThrough;
            if (!input) continue;
            if (image) window->winSetStatus(WIN_STATUS_IMAGE);
            else window->winClearStatus(WIN_STATUS_IMAGE);
            if (seeThrough) window->winSetStatus(WIN_STATUS_SEE_THRU);
            else window->winClearStatus(WIN_STATUS_SEE_THRU);
        }
        else if (kind == 'D')
        {
            Int state, slot;
            std::string imageName;
            input >> state >> slot >> imageName;
            const Color color = readAppearanceColor(input);
            const Color border = readAppearanceColor(input);
            if (!input || slot < 0 || slot >= 9 || state < 0 || state > 2) continue;
            // Reborn: Scheme-generated images must remain owned by ControlBarScheme::init.
            if (imageName.find("HardCoded") == 0) continue;
            const Image* image = imageName == "NoImage" || !TheMappedImageCollection ? nullptr :
                TheMappedImageCollection->findImageByName(imageName.c_str());
            if (state == 0) { window->winSetEnabledImage(slot, image); window->winSetEnabledColor(slot, color); window->winSetEnabledBorderColor(slot, border); }
            if (state == 1) { window->winSetDisabledImage(slot, image); window->winSetDisabledColor(slot, color); window->winSetDisabledBorderColor(slot, border); }
            if (state == 2) { window->winSetHiliteImage(slot, image); window->winSetHiliteColor(slot, color); window->winSetHiliteBorderColor(slot, border); }
        }
    }
    // Reborn: Flatten localized two-line captions only for compact observer rows; restore originals on normal bars.
    if (TheGameText)
    {
        const char* windows[] = { "ControlBar.wnd:StaticTextObsUnitsKilled", "ControlBar.wnd:StaticTextObsUnitsLost" };
        const char* keys[] = { "GUI:UnitsKilled", "GUI:UnitsLost" };
        for (Int i = 0; i < 2; ++i)
        {
            GameWindow* label = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(windows[i]));
            if (!label) continue;
            const UnicodeString localized = TheGameText->fetch(keys[i]);
            std::wstring caption(localized.str());
            if (UseCustomControlBar() && TheControlBar && TheControlBar->isObserverControlBarOn())
            {
                std::replace(caption.begin(), caption.end(), L'\n', L' ');
                std::replace(caption.begin(), caption.end(), L'\r', L' ');
            }
            GadgetStaticTextSetText(label, caption.c_str());
        }
    }
}
