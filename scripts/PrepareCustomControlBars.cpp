#include <filesystem>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <regex>
#include <mutex>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <sstream>
// Reborn: Retain namespaced assets so cached W3D textures can still reopen an earlier package's files.
typedef std::vector<unsigned char> Bytes;
typedef std::map<std::string, Bytes> Files;
std::recursive_mutex packageMutex;
Files virtualFiles;
Files activeFiles;
std::map<std::string, std::string> imageNames;
std::vector<std::string> imageIniFiles;
std::set<std::string> loadedImageIniFiles;
std::string activePackage;
std::string activePrefix;
// Reborn: Prepared Custom templates and normal snapshots are parsed once, never during file lookup.

size_t retainedBytes = 0;
const size_t MAX_PACKAGE_BYTES = 128u * 1024u * 1024u;
const size_t MAX_RETAINED_BYTES = 512u * 1024u * 1024u;

//-------------------------------------------------------------------------------------------------
/** Reborn: Normalize virtual names without accepting absolute or traversal paths from an archive. */
//-------------------------------------------------------------------------------------------------
std::string key(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    for (char& c : path) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (path.empty() || path[0] == '/' || path.find(':') != std::string::npos ||
        path.find("..") != std::string::npos || path.find("//") != std::string::npos)
        throw std::runtime_error("Unsafe package path");
    return path;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Read bounded little- or big-endian fields without trusting ZIP/BIG offsets. */
//-------------------------------------------------------------------------------------------------
unsigned number(const Bytes& data, size_t pos, size_t length, bool big = false)
{
    if (pos > data.size() || length > data.size() - pos) throw std::runtime_error("Truncated archive");
    unsigned result = 0;
    for (size_t i = 0; i < length; ++i)
        result |= unsigned(data[pos + i]) << (8 * (big ? length - i - 1 : i));
    return result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Normal filesystem probes are not package errors; do not throw on ordinary absolute paths. */
//-------------------------------------------------------------------------------------------------
std::string lookupKey(const char* filename)
{
    std::string path(filename ? filename : "");
    std::replace(path.begin(), path.end(), '\\', '/');
    for (char& c : path) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return path;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Permit only the in-game screens a control bar is allowed to skin. */
//-------------------------------------------------------------------------------------------------
bool isWindow(const std::string& name)
{
    static const char* names[] = {
        "window/controlbar.wnd", "window/controlbarpopupdescription.wnd", "window/diplomacy.wnd",
        "window/generalsexppoints.wnd", "window/genpowersshortcutbarchina.wnd",
        "window/genpowersshortcutbargla.wnd", "window/genpowersshortcutbarus.wnd",
        // Reborn: Prepare the logo-less restart/surrender confirmation as well as the exit popup.
        "window/menus/messagebox.wnd", "window/menus/quitmenu.wnd", "window/menus/quitnosave.wnd", "window/menus/quitmessagebox.wnd",
        "window/menus/observerquit.wnd"
    };
    for (const char* allowed : names) if (name == allowed) return true;
    return false;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Only UI art, mapped images and the control bar scheme enter the overlay. */
//-------------------------------------------------------------------------------------------------
bool allowed(const std::string& name)
{
    if (isWindow(name) || name == "data/ini/controlbarscheme.ini") return true;
    if (name.find("data/ini/mappedimages/") == 0 && name.size() > 4 && name.substr(name.size() - 4) == ".ini") return true;
    return name.find("art/textures/") == 0 && name.size() > 4 &&
        (name.substr(name.size() - 4) == ".dds" || name.substr(name.size() - 4) == ".tga");
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Read an uncompressed BIG inside the ZIP, skipping privacy add-ons and non-UI files. */
//-------------------------------------------------------------------------------------------------
void readBig(const Bytes& data, Files& files, size_t& total)
{
    if (data.size() < 16 || (std::string(data.begin(), data.begin() + 4) != "BIGF" &&
        std::string(data.begin(), data.begin() + 4) != "BIG4")) throw std::runtime_error("Invalid BIG header");
    const unsigned count = number(data, 8, 4, true);
    const unsigned end = number(data, 12, 4, true);
    if (count > 4096 || end < 16 || end > data.size()) throw std::runtime_error("Invalid BIG directory");
    size_t pos = 16;
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned offset = number(data, pos, 4, true), size = number(data, pos + 4, 4, true);
        pos += 8;
        const size_t start = pos;
        while (pos < end && data[pos] != 0) ++pos;
        if (pos == end || offset < end || offset > data.size() || size > data.size() - offset)
            throw std::runtime_error("Invalid BIG entry");
        const std::string name = key(std::string(data.begin() + start, data.begin() + pos++));
        if (!allowed(name)) continue;
        if (files.count(name) || size > MAX_PACKAGE_BYTES - total) throw std::runtime_error("Duplicate or oversized UI entry");
        total += size;
        files[name] = Bytes(data.begin() + offset, data.begin() + offset + size);
    }
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Decode stored/deflated ZIP entries with CRC and size validation; never execute installers. */
// Reborn: Parse WND structure rather than dropping retail files over required Reborn controls.
struct CustomWndNode
{
    bool modControl = false; // Reborn: Keep gameplay controls above retail input blockers and decorative overlays.
    std::map<std::string, std::string> fields;
    std::vector<CustomWndNode> children;
};
std::vector<CustomWndNode> activeSkin, normalSkin; // Reborn: Immutable visual snapshots for live, pointer-stable changes.

//-------------------------------------------------------------------------------------------------
/** Reborn: Tokenize WND statements, preserving quoted text and multi-line draw data. */
//-------------------------------------------------------------------------------------------------
std::vector<std::string> statements(const std::string& text)
{
    // Reborn: WND files use semicolon-prefixed comment lines, including the themed mod layouts.
    std::string filtered, line;
    std::istringstream input(text);
    static const std::regex fieldStart("^[ \\t]*[A-Z][A-Z0-9_]*[ \\t]*=");
    while (std::getline(input, line))
    {
        const size_t start = line.find_first_not_of(" \t\r");
        // Reborn: A standalone semicolon terminates multi-line draw data; it is not a comment.
        if (start != std::string::npos && (line.compare(start, 2, "//") == 0 ||
            (line[start] == ';' && line.find_first_not_of(" \t\r", start + 1) != std::string::npos &&
                line.find('=') == std::string::npos))) continue;
        // Reborn: Some retail overlays omit the final semicolon after draw data; a new field still ends it.
        if (std::regex_search(line, fieldStart)) filtered += ';';
        filtered += line + '\n';
    }
    std::vector<std::string> result;
    std::string statement;
    bool quoted = false;
    for (char c : filtered)
    {
        if (c == '"') quoted = !quoted;
        if (!quoted && (c == ';' || c == '\n' || c == '\r'))
        {
            const size_t first = statement.find_first_not_of(" \t");
            const std::string value = first == std::string::npos ? "" : statement.substr(first, statement.find_last_not_of(" \t") - first + 1);
            if (c == ';' || value == "WINDOW" || value == "CHILD" || value == "END" || value == "ENDALLCHILDREN" ||
                value == "STARTLAYOUTBLOCK" || value == "ENDLAYOUTBLOCK")
            {
                if (!value.empty()) result.push_back(value);
                statement.clear();
            }
            else statement += ' ';
        }
        else statement += c;
    }
    if (quoted || statement.find_first_not_of(" \t\r\n") != std::string::npos) throw std::runtime_error("Invalid WND statements");
    return result;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Read bounded nested windows without changing callback identities. */
//-------------------------------------------------------------------------------------------------
CustomWndNode readNode(const std::vector<std::string>& tokens, size_t& pos, unsigned depth = 0)
{
    if (depth > 32 || pos >= tokens.size() || tokens[pos++] != "WINDOW") throw std::runtime_error("Invalid WND tree");
    CustomWndNode node;
    while (pos < tokens.size())
    {
        const std::string token = tokens[pos++];
        if (token == "END") return node;
        if (token == "CHILD")
        {
            node.children.push_back(readNode(tokens, pos, depth + 1));
        }
        else if (token == "ENDALLCHILDREN") continue;
        else
        {
            const size_t equal = token.find('=');
            if (equal == std::string::npos) throw std::runtime_error("Invalid WND field");
            std::string field = token.substr(0, equal);
            field.erase(field.find_last_not_of(" \t") + 1);
            // Reborn: Reject draw data the engine cannot consume instead of exposing a partial/null layout.
            if (field.find("DRAWDATA") != std::string::npos)
            {
                static const std::regex image("\\bIMAGE[ \\t]*:");
                const auto count = std::distance(std::sregex_iterator(token.begin(), token.end(), image), std::sregex_iterator());
                if (std::count(token.begin(), token.end(), '=') != 1 || count > 9)
                    throw std::runtime_error("Invalid custom WND draw data");
            }
            // Reborn: Retail skins sometimes spell a cleared ENABLED bit as DISABLED, which our parser does not accept.
            if (field == "STATUS" && token.find("DISABLED") != std::string::npos)
            {
                std::string status = std::regex_replace(token, std::regex("\\bDISABLED\\b[ \\t]*\\+?[ \\t]*"), "");
                status = std::regex_replace(status, std::regex("\\+[ \\t]*$"), "");
                if (status.find_first_not_of(" \\t", status.find('=') + 1) == std::string::npos) status = "STATUS = NULL";
                node.fields[field] = status;
            }
            else node.fields[field] = token;
        }
    }
    throw std::runtime_error("Unterminated WND tree");
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Match children by the name after the WND prefix, including Generals-themed originals. */
//-------------------------------------------------------------------------------------------------
std::string nodeName(const CustomWndNode& node)
{
    const auto it = node.fields.find("NAME");
    if (it == node.fields.end()) return "";
    const size_t first = it->second.find('"'), end = it->second.find_last_of('"');
    if (first == std::string::npos || end <= first) return "";
    const std::string name = it->second.substr(first + 1, end - first - 1);
    const size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Obtain absolute source coordinates and creation resolution for compatibility transforms. */
//-------------------------------------------------------------------------------------------------
std::vector<int> rect(const CustomWndNode& node)
{
    const auto it = node.fields.find("SCREENRECT");
    if (it == node.fields.end()) throw std::runtime_error("WND lacks screen coordinates");
    std::vector<int> values;
    static const std::regex pattern("-?[0-9]+");
    for (std::sregex_iterator i(it->second.begin(), it->second.end(), pattern), end; i != end; ++i)
        values.push_back(std::stoi(i->str()));
    if (values.size() != 6 || values[4] <= 0 || values[5] <= 0) throw std::runtime_error("Invalid WND coordinates");
    return values;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Rescale retained mod-only windows into the custom parent's coordinate space. */
//-------------------------------------------------------------------------------------------------
void moveNode(CustomWndNode& node, const std::vector<int>& oldRect, const std::vector<int>& newRect)
{
    const std::vector<int> r = rect(node);
    const double sx = double(newRect[2] - newRect[0]) / std::max(1, oldRect[2] - oldRect[0]);
    const double sy = double(newRect[3] - newRect[1]) / std::max(1, oldRect[3] - oldRect[1]);
    int values[4];
    for (int i = 0; i < 4; ++i)
        values[i] = int((r[i] * double(oldRect[4 + (i % 2)]) / r[4 + (i % 2)] - oldRect[i % 2]) *
            (i % 2 ? sy : sx) + newRect[i % 2] + 0.5);
    node.fields["SCREENRECT"] = "SCREENRECT = UPPERLEFT: " + std::to_string(values[0]) + " " + std::to_string(values[1]) +
        ", BOTTOMRIGHT: " + std::to_string(values[2]) + " " + std::to_string(values[3]) +
        ", CREATIONRESOLUTION: " + std::to_string(newRect[4]) + " " + std::to_string(newRect[5]);
    for (CustomWndNode& child : node.children) moveNode(child, oldRect, newRect);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Find a named control independently of changes in the retail parent hierarchy. */
//-------------------------------------------------------------------------------------------------
CustomWndNode* findNode(CustomWndNode& root, const std::string& name)
{
    if (!name.empty() && nodeName(root) == name) return &root;
    for (CustomWndNode& child : root.children)
        if (CustomWndNode* found = findNode(child, name)) return found;
    return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Fit the portrait and all seven upgrade cameos inside the code-drawn 4x4 custom grid. */
//-------------------------------------------------------------------------------------------------
void layoutUpgradeCameos(CustomWndNode& root)
{
    CustomWndNode* grid = findNode(root, "WinUnitSelected");
    if (!grid) return;
    const std::vector<int> r = rect(*grid);
    const int inset = r[4] >= 3840 ? 2 : 1;
    int x[5], y[5];
    for (int i = 0; i <= 4; ++i)
    {
        x[i] = r[0] + (r[2] - r[0]) * i / 4;
        y[i] = r[1] + (r[3] - r[1]) * i / 4;
    }
    // Reborn: Keep the template's original controls, callbacks and upgrade indices.
    const auto place = [&](const std::string& name, int left, int top, int right, int bottom)
    {
        CustomWndNode* node = findNode(*grid, name);
        if (!node) throw std::runtime_error("Missing upgrade grid control " + name);
        node->fields["SCREENRECT"] = "SCREENRECT = UPPERLEFT: " + std::to_string(left) + " " + std::to_string(top) +
            ", BOTTOMRIGHT: " + std::to_string(right) + " " + std::to_string(bottom) +
            ", CREATIONRESOLUTION: " + std::to_string(r[4]) + " " + std::to_string(r[5]);
    };
    place("CameoWindow", x[0] + inset, y[0] + inset, x[3] - inset, y[3] - inset);
    for (int i = 1; i <= 7; ++i)
    {
        // Reborn: Four upgrades run down the right column, then three run left across the bottom.
        const int column = i <= 4 ? 3 : 7 - i;
        const int row = i <= 4 ? i - 1 : 3;
        place("UnitUpgrade" + std::to_string(i), x[column] + inset, y[row] + inset,
            x[column + 1] - inset, y[row + 1] - inset);
    }
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Restrict custom input walls to actual panels instead of the normal bar's full-width strip. */
//-------------------------------------------------------------------------------------------------
void layoutCustomInputBlockers(CustomWndNode& root, CustomWndNode& retail)
{
    if (nodeName(root) != "ControlBarParent") return;
    const char* controls[][2] = {
        {"InputBlockMain", "ControlBarProBlockInputProduction"},
        {"InputBlockLeft", "ControlBarProBlockInputRadar"},
        {"InputBlockRight", "ControlBarProBlockInputUpgrades"}
    };
    for (const auto& control : controls)
    {
        CustomWndNode* target = findNode(root, control[0]);
        CustomWndNode* source = findNode(retail, control[1]);
        // Reborn: Also allow already-prepared custom templates as offline test fixtures.
        if (!source) source = findNode(retail, control[0]);
        if (!target || !source) throw std::runtime_error("Missing custom input panel " + std::string(control[0]));
        target->fields["SCREENRECT"] = source->fields["SCREENRECT"];
    }
    // Reborn: This no-draw parent also blocks input; contain it within the central command panel.
    CustomWndNode* center = findNode(root, "CenterBackground");
    if (center) center->fields["SCREENRECT"] = findNode(root, "InputBlockMain")->fields["SCREENRECT"];
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Apply a retail skin to a fixed Reborn template; never import retail parents or input blockers. */
//-------------------------------------------------------------------------------------------------
void skinNode(CustomWndNode& node, CustomWndNode& retail, const CustomWndNode* oldParent = nullptr, const CustomWndNode* newParent = nullptr)
{
    CustomWndNode original;
    original.fields = node.fields; // Reborn: Only parent coordinates are needed; avoid copying entire descendant trees.
    CustomWndNode* skin = findNode(retail, nodeName(node));
    if (skin)
    {
        for (const auto& field : skin->fields)
        {
            const std::string& name = field.first;
            if (name == "SCREENRECT" || name == "FONT" || name == "HEADERTEMPLATE" ||
                name == "TEXTCOLOR" || name.find("DRAWDATA") != std::string::npos)
                node.fields[name] = field.second;
        }
        // Reborn: The template owns input, status, callbacks, gadget data and the complete 27-slot hierarchy.
        auto status = node.fields.find("STATUS");
        const auto skinStatus = skin->fields.find("STATUS");
        if (status != node.fields.end() && skinStatus != skin->fields.end() &&
            skinStatus->second.find("IMAGE") != std::string::npos && status->second.find("IMAGE") == std::string::npos)
            status->second += "+IMAGE";
        // Reborn: Diplomacy's custom close button uses state colors, not textures; preserve its other status flags.
        if (nodeName(node) == "ButtonHide" && node.fields["NAME"].find("Diplomacy") != std::string::npos &&
            status != node.fields.end() && skinStatus != skin->fields.end() &&
            skinStatus->second.find("IMAGE") == std::string::npos)
        {
            const size_t imageFlag = status->second.find("+IMAGE");
            if (imageFlag != std::string::npos) status->second.erase(imageFlag, 6);
        }
    }
    else if (oldParent && newParent)
    {
        CustomWndNode moved = node;
        moved.children.clear();
        moveNode(moved, rect(*oldParent), rect(*newParent));
        node.fields["SCREENRECT"] = moved.fields["SCREENRECT"];
    }
    // Reborn: Legacy anonymous popup title strips are not part of ControlBarPro's plain confirmation panel.
    if (oldParent && nodeName(node).empty() &&
        (node.fields["NAME"].find("MessageBox.wnd:") != std::string::npos ||
         node.fields["NAME"].find("MessageBoxGen.wnd:") != std::string::npos))
    {
        auto& status = node.fields["STATUS"];
        if (status.find("HIDDEN") == std::string::npos) status += "+HIDDEN";
        if (status.find("SEE_THRU") == std::string::npos) status += "+SEE_THRU";
    }
    // Reborn: Keep the package's baked-in Zero Hour logo, hiding the mod logo retained by canonical quit templates.
    const std::string control = nodeName(node);
    const std::string& identity = node.fields["NAME"];
    if ((control == "WinLoad" && identity.find("QuitMenu") != std::string::npos) ||
        (control == "WinLogo" && identity.find("QuitNoSave") != std::string::npos) ||
        (control == "Logo" && identity.find("QuitMessageBox") != std::string::npos))
    {
        auto& status = node.fields["STATUS"];
        if (status.find("HIDDEN") == std::string::npos) status += "+HIDDEN";
        if (status.find("SEE_THRU") == std::string::npos) status += "+SEE_THRU";
    }
    // Reborn: Control Bar Pro's science close button is an arrow with no Done caption.
    if (node.fields["NAME"] == "NAME = \"GeneralsExpPoints.wnd:ButtonExit\"")
        node.fields.erase("TEXT");
    // Reborn: These retail templates are GenTool-only; use their actual Arial 10 font directly.
    auto header = node.fields.find("HEADERTEMPLATE");
    if (header != node.fields.end() && header->second.find("ControlBarProScrollListBox") != std::string::npos)
    {
        header->second = "HEADERTEMPLATE = \"[None]\"";
        node.fields["FONT"] = "FONT = NAME: \"Arial\", SIZE: 10, BOLD: 0";
    }
    for (CustomWndNode& child : node.children) skinNode(child, retail, &original, &node);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Serialize fields in parser order; WINDOWTYPE must precede gadget data and SCREENRECT. */
//-------------------------------------------------------------------------------------------------
std::string writeNode(const CustomWndNode& node)
{
    std::string result = "WINDOW\n";
    static const char* first[] = {"WINDOWTYPE", "SCREENRECT", "NAME", "STATUS", "STYLE"};
    for (const char* field : first)
    {
        const auto i = node.fields.find(field);
        if (i != node.fields.end()) result += i->second + ";\n";
    }
    for (const auto& field : node.fields)
    {
        if (std::find(std::begin(first), std::end(first), field.first) == std::end(first)) result += field.second + ";\n";
    }
    if (!node.children.empty())
    {
        // Reborn: Newly created children are frontmost; create retail blockers before functional mod controls.
        for (bool modControl : {false, true})
            for (const CustomWndNode& child : node.children)
                if (child.modControl == modControl) result += "CHILD\n" + writeNode(child);
        result += "ENDALLCHILDREN\n";
    }
    return result + "END\n";
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Extend the faction's custom shortcut tray to the mod's 11+11+5 layout without replacing command bindings. */
//-------------------------------------------------------------------------------------------------
void layoutCustomShortcutButtons(CustomWndNode& root)
{
    CustomWndNode* bar = findNode(root, "GenPowersShortcutBarParent");
    if (!bar) return;
    CustomWndNode* bottom = findNode(*bar, "ButtonParent1");
    CustomWndNode* upper = findNode(*bar, "ButtonParent2");
    CustomWndNode* firstButton = findNode(*bar, "ButtonCommand1");
    CustomWndNode* secondButton = findNode(*bar, "ButtonCommand2");
    if (!bottom || !upper || !firstButton || !secondButton)
        throw std::runtime_error("Missing shortcut tray templates");
    // Reborn: Capture templates before changing any nodes; bottom frames retain their faction-specific footer.
    const CustomWndNode frames[] = { *bottom, *upper };
    const CustomWndNode buttons[] = { *firstButton, *secondButton };
    const auto firstRect = rect(frames[0]), secondRect = rect(frames[1]);
    const int columnWidth = firstRect[2] - firstRect[0];
    // Reborn: All three faction tray textures have a transparent 20/1920 left margin; visible frames must touch.
    const int columnPitch = columnWidth - 20 * firstRect[4] / 1920;
    // Reborn: Anchor the visible footer four pixels above MinMax at Y802 (1080 design), avoiding cumulative shifts.
    const int verticalShift = 808 * firstRect[5] / 1080 - firstRect[3];
    const int rowPitch = firstRect[1] - secondRect[1];
    if (columnPitch <= 0 || rowPitch <= 0) throw std::runtime_error("Invalid shortcut tray spacing");
    const auto place = [&](CustomWndNode& target, const CustomWndNode& source, int dx, int dy)
    {
        // Reborn: Copy only visual fields, preserving IDs, gameplay callbacks, status and gadget data.
        for (const auto& field : source.fields)
            if (field.first == "FONT" || field.first == "HEADERTEMPLATE" ||
                field.first == "TEXTCOLOR" || field.first.find("DRAWDATA") != std::string::npos)
                target.fields[field.first] = field.second;
        const auto r = rect(source);
        target.fields["SCREENRECT"] = "SCREENRECT = UPPERLEFT: " + std::to_string(r[0] + dx) + " " +
            std::to_string(r[1] + dy) + ", BOTTOMRIGHT: " + std::to_string(r[2] + dx) + " " +
            std::to_string(r[3] + dy) + ", CREATIONRESOLUTION: " + std::to_string(r[4]) + " " + std::to_string(r[5]);
    };
    for (int i = 1; i <= 27; ++i)
    {
        const int column = (i - 1) / 11, row = (i - 1) % 11;
        CustomWndNode* frame = findNode(*bar, "ButtonParent" + std::to_string(i));
        CustomWndNode* button = findNode(*bar, "ButtonCommand" + std::to_string(i));
        if (!frame || !button) throw std::runtime_error("Missing mod shortcut slot");
        const int model = row == 0 ? 0 : 1;
        const int dy = verticalShift + (row == 0 ? 0 : -(row - 1) * rowPitch);
        place(*frame, frames[model], -column * columnPitch, dy);
        place(*button, buttons[model], -column * columnPitch, dy);
    }
    // Reborn: Hover/input traversal must cover every column, including tray artwork protruding left of the retail root.
    bar->fields["SCREENRECT"] = "SCREENRECT = UPPERLEFT: " + std::to_string(firstRect[0] - 2 * columnPitch) +
        " 0, BOTTOMRIGHT: " + std::to_string(firstRect[2]) + " " + std::to_string(firstRect[3] + verticalShift) +
        ", CREATIONRESOLUTION: " + std::to_string(firstRect[4]) + " " + std::to_string(firstRect[5]);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Theme custom quit/popup buttons and frames or only the diplomacy Back button, retaining package geometry and artwork. */
//-------------------------------------------------------------------------------------------------
void applyCustomGeneralsMenuTheme(CustomWndNode& root, const CustomWndNode& themedTemplate)
{
    CustomWndNode reference = themedTemplate;
    // Reborn: Diplomacy keeps every other package control unchanged; only its Back button follows the layout theme.
    const bool diplomacy = root.fields.at("NAME").find("Diplomacy") != std::string::npos;
    const auto apply = [&](auto&& self, CustomWndNode& node) -> void
    {
        CustomWndNode* themed = findNode(reference, nodeName(node));
        if (themed && node.fields["WINDOWTYPE"] == "WINDOWTYPE = PUSHBUTTON" &&
            (!diplomacy || nodeName(node) == "ButtonHide"))
        {
            if (diplomacy)
            {
                // Reborn: Keep ControlBarPro's gray Back button and white text; recolor only hover/pressed fills.
                std::string& hilite = node.fields["HILITEDRAWDATA"];
                hilite = std::regex_replace(hilite, std::regex("18 80 129"), "129 97 0");
                hilite = std::regex_replace(hilite, std::regex("29 130 207"), "255 191 0");
            }
            else for (const char* field : {"ENABLEDDRAWDATA", "DISABLEDDRAWDATA", "HILITEDRAWDATA", "TEXTCOLOR"})
            {
                auto value = themed->fields.find(field);
                if (value != themed->fields.end()) node.fields[field] = value->second;
            }
        }
        else if (themed && (nodeName(node) == "QuitMenuParent" || nodeName(node) == "MessageBoxParent"))
        {
            // Reborn: The image-backed menu keeps its original artwork; the renderer overlays only this frame color.
            for (const char* field : {"ENABLEDDRAWDATA", "DISABLEDDRAWDATA", "HILITEDRAWDATA"})
            {
                std::smatch color;
                const auto value = themed->fields.find(field);
                if (value != themed->fields.end() &&
                    std::regex_search(value->second, color, std::regex("BORDERCOLOR: [0-9]+ [0-9]+ [0-9]+ [0-9]+")))
                    node.fields[field] = std::regex_replace(node.fields[field],
                        std::regex("BORDERCOLOR: [0-9]+ [0-9]+ [0-9]+ [0-9]+"), color.str(),
                        std::regex_constants::format_first_only);
            }
        }
        for (auto& child : node.children) self(self, child);
    };
    apply(apply, root);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Convert a package screen while keeping all current mod-specific children and layout callbacks. */
//-------------------------------------------------------------------------------------------------
std::string mergeWindow(const std::string& customText, const std::string& originalText, bool generalsMenu = false)
{
    const std::vector<std::string> customTokens = statements(customText), originalTokens = statements(originalText);
    size_t cp = 0, op = 0;
    while (cp < customTokens.size() && customTokens[cp] != "WINDOW") ++cp;
    while (op < originalTokens.size() && originalTokens[op] != "WINDOW") ++op;
    std::string result;
    for (size_t i = 0; i < op; ++i)
        result += originalTokens[i] + (originalTokens[i].find('=') == std::string::npos ? "\n" : ";\n");
    while (cp < customTokens.size())
    {
        CustomWndNode custom = readNode(customTokens, cp);
        if (op >= originalTokens.size()) throw std::runtime_error("Incompatible WND roots");
        CustomWndNode original = readNode(originalTokens, op);
        const CustomWndNode themedTemplate = original; // Reborn: Preserve the Generals template before applying package visuals.
        skinNode(original, custom);
        if (generalsMenu) applyCustomGeneralsMenuTheme(original, themedTemplate);
        // Reborn: Leave the gaps between Control Bar Pro panels available to world input.
        layoutCustomInputBlockers(original, custom);
        // Reborn: Retail's five 3x3 slots must not override the mod's seven 4x4 slots.
        layoutUpgradeCameos(original);
        // Reborn: Give all 27 faction shortcuts the same custom frame and icon dimensions.
        layoutCustomShortcutButtons(original);
        result += writeNode(original);
    }
    if (op != originalTokens.size()) throw std::runtime_error("Missing mod WND roots");
    return result;
}


 //-------------------------------------------------------------------------------------------------
 /** Reborn: Read fixture templates without initializing engine subsystems. */
 //-------------------------------------------------------------------------------------------------
 std::string readText(const std::string& name)
 {
  std::ifstream in(name,std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
 }
std::set<std::string> namesOf(const std::string& text)
{
 std::set<std::string> names;
 const std::regex pattern("NAME[ \\t]*=[ \\t]*\"([^\"]+)\"");
 for(std::sregex_iterator i(text.begin(),text.end(),pattern),end;i!=end;++i)
 {
  const std::string name=(*i)[1].str();
  if(name.empty() || name.back()==':') continue;
  if(!names.insert(name).second) throw std::runtime_error("Duplicate window: "+name);
 }
 return names;
}


//-------------------------------------------------------------------------------------------------
/** Reborn: Write fixed assets at development time; the game never opens ZIP/BIG packages. */
//-------------------------------------------------------------------------------------------------
void output(const std::string& path,const Bytes& data)
{
 std::filesystem::create_directories(std::filesystem::path(path).parent_path());
 std::ofstream file(path,std::ios::binary); file.write((const char*)data.data(),data.size());
 if(!file) throw std::runtime_error("Cannot write "+path);
}
void output(const std::string& path,const std::string& text) { output(path,Bytes(text.begin(),text.end())); }
//-------------------------------------------------------------------------------------------------
/** Reborn: Cache simple visual records for pointer-stable live switches between fixed WND layouts. */
//-------------------------------------------------------------------------------------------------
void manifestNode(std::ostream& out,const CustomWndNode& n)
{
 auto f=n.fields.find("NAME"); if(f==n.fields.end()) return;
 std::smatch match;
 std::regex_search(f->second,match,std::regex("\"([^\"]+)\"")); std::string name=match[1];
 // Reborn: Caption records preserve an arrow-only custom close button and restore normal localized text.
 if (name == "GeneralsExpPoints.wnd:ButtonExit") {
  std::string textKey;
  auto textField = n.fields.find("TEXT");
  if (textField != n.fields.end()) {
   const size_t first = textField->second.find('"'), last = textField->second.find_last_of('"');
   if (first != std::string::npos && last > first) textKey = textField->second.substr(first + 1, last - first - 1);
  }
  out<<"B "<<name<<" "<<std::quoted(textKey)<<"\n";
 }
 // Reborn: Keep texture/color drawing reversible across live player and observer bar switches.
 auto status=n.fields.find("STATUS");
 if (name.find("ControlBar.wnd:")==0 && status!=n.fields.end())
  out<<"S "<<name<<" "<<(status->second.find("IMAGE")!=std::string::npos)<<" "<<(status->second.find("SEE_THRU")!=std::string::npos)<<"\n";
 auto r=rect(n);
 out<<"R "<<name; for(int v:r) out<<" "<<v; out<<"\n";
 f=n.fields.find("FONT");
 if(f!=n.fields.end() && std::regex_search(f->second,match,std::regex("NAME: \"([^\"]+)\".*SIZE: ([0-9]+).*BOLD: ([01])")))
  out<<"F "<<name<<" "<<std::quoted(match[1].str())<<" "<<match[2]<<" "<<match[3]<<"\n";
 // Reborn: WND header templates take precedence over fonts in the normal parser too.
 f=n.fields.find("HEADERTEMPLATE");
 if(f!=n.fields.end() && std::regex_search(f->second,match,std::regex("\"([^\"]+)\"")))
  out<<"H "<<name<<" "<<match[1]<<"\n";
 const std::regex colors("(?:ENABLED|ENABLEDBORDER|DISABLED|DISABLEDBORDER|HILITE|HILITEBORDER):[ \\t]+([0-9]+)[ \\t]+([0-9]+)[ \\t]+([0-9]+)[ \\t]+([0-9]+)");
 f=n.fields.find("TEXTCOLOR");
 if(f!=n.fields.end()) {
  out<<"T "<<name;
  for(std::sregex_iterator i(f->second.begin(),f->second.end(),colors),end;i!=end;++i)
   for(int j=1;j<=4;++j) out<<" "<<(*i)[j];
  out<<"\n";
 }
 const char* fields[]={"ENABLEDDRAWDATA","DISABLEDDRAWDATA","HILITEDRAWDATA"};
 const std::regex draws("IMAGE: ([^ ,]+), COLOR: ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+), BORDERCOLOR: ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)");
 for(int state=0;state<3;++state) {
  f=n.fields.find(fields[state]); if(f==n.fields.end()) continue; int slot=0;
  for(std::sregex_iterator i(f->second.begin(),f->second.end(),draws),end;i!=end;++i,++slot) {
   out<<"D "<<name<<" "<<state<<" "<<slot<<" "<<(*i)[1];
   for(int j=2;j<=9;++j) out<<" "<<(*i)[j]; out<<"\n";
  }
 }
 for(const auto& child:n.children) manifestNode(out,child);
}
void manifest(std::ostream& out,const std::string& text)
{
 auto ts=statements(text); size_t p=0; while(p<ts.size()&&ts[p]!="WINDOW")++p;
 while(p<ts.size()) manifestNode(out,readNode(ts,p));
}
//-------------------------------------------------------------------------------------------------
/** Reborn: Extract five shipped variants into namespaced art and ready-to-use Custom WNDs. */
//-------------------------------------------------------------------------------------------------
int main(int argc,char** argv)
{
 try {
 if(argc!=2) throw std::runtime_error("Usage: PrepareCustomControlBars <repository-root>");
 const std::string root=argv[1], shared=root+"/build/shared/";
 const char* res[]={"1280x720","1600x900","1920x1080","2560x1440","3840x2160"};
 const char* height[]={"720","900","1080","1440","2160"};
 std::ostringstream normal;
 // Reborn: All custom image definitions belong in the existing HandCreatedMappedImages.INI.
 std::ostringstream imageDefinitions;
 unsigned windows=0;
 for(int ri=0;ri<5;++ri) {
  Files files; size_t total=0;
  std::string folder=shared+"RebornOmegaData/CustomControlBar/ControlBarProZH_v1.2_"+res[ri]+"/";
  for(const auto& item:std::filesystem::directory_iterator(folder))
   if(item.path().extension()==".big") {
    std::string data=readText(item.path().string()); readBig(Bytes(data.begin(),data.end()),files,total);
   }
  const std::string prefix=std::string("RebornCBP_")+(ri<3?"1080_":"2160_");
  std::map<std::string,std::string> images;
  const std::regex mapped("MappedImage[ \t]+([^ \t\r\n]+)",std::regex::icase);
  for(const auto& e:files) if(e.first.find("data/ini/mappedimages/")==0) {
   std::string text(e.second.begin(),e.second.end());
   for(std::sregex_iterator i(text.begin(),text.end(),mapped),end;i!=end;++i)
    images[lookupKey((*i)[1].str().c_str())]=prefix+(*i)[1].str();
  }
  const auto rename=[&](std::string text) {
   // Reborn: Token substitution isolates custom art without replacing standard images.
   const std::regex token("[A-Za-z0-9_][A-Za-z0-9_.-]*");
   std::string result; size_t last=0;
   for(std::sregex_iterator i(text.begin(),text.end(),token),end;i!=end;++i) {
    result+=text.substr(last,i->position()-last); std::string v=i->str();
    auto found=images.find(lookupKey(v.c_str()));
    if(found!=images.end()) result+=found->second;
    // Reborn: W3D resolves .tga references to matching DDS files; namespace both extensions consistently.
    else if(files.count("art/textures/"+lookupKey(v.c_str())) ||
      (v.size()>4 && lookupKey(v.c_str()).substr(v.size()-4)==".tga" &&
       files.count("art/textures/"+lookupKey(v.c_str()).substr(0,v.size()-4)+".dds"))) result+=prefix+v;
    else result+=v;
    last=i->position()+i->length();
   } return result+text.substr(last);
  };
  std::ostringstream live;
  for(const auto& e:files) {
   if(e.first.find("art/textures/")==0) {
    if(ri==0||ri==3) output(shared+"Art/Textures/"+prefix+e.first.substr(e.first.find_last_of('/')+1),e.second);
   } else if(e.first.find("data/ini/mappedimages/")==0) {
    if(ri==0||ri==3) imageDefinitions << "\n; Reborn: " << prefix <<
      e.first.substr(e.first.find_last_of('/')+1) << "\n" << rename(std::string(e.second.begin(),e.second.end())) << "\n";
   } else if(e.first=="data/ini/controlbarscheme.ini")
    output(shared+"Data/INI/CustomControlBar/"+res[ri]+"/ControlBarScheme.ini",rename(std::string(e.second.begin(),e.second.end())));
   else if(isWindow(e.first)) {
    const std::string custom=rename(std::string(e.second.begin(),e.second.end()));
    std::string original=readText(shared+e.first);
    if(original.empty()) throw std::runtime_error("Missing original "+e.first);
    for(int gen=0;gen<2;++gen) {
     std::string base=e.first; if(gen) base.insert(base.size()-4,"gen");
     original=readText(shared+base); if(original.empty()) continue;
     // Reborn: Theme quit menus and their confirmations; diplomacy changes only its Back button.
     const bool generalsMenu = gen && (e.first == "window/menus/quitmenu.wnd" || e.first == "window/menus/quitnosave.wnd" ||
         e.first == "window/menus/quitmessagebox.wnd" || e.first == "window/menus/messagebox.wnd" || e.first == "window/diplomacy.wnd");
     const std::string converted=mergeWindow(custom,original,generalsMenu);
     if(namesOf(original)!=namesOf(converted)) throw std::runtime_error("Changed control identities "+base);
     std::string target=base.substr(7); target.insert(target.size()-4,"Custom");
     output(shared+"Window/CustomControlBar/"+res[ri]+"/"+target,converted);
     if(!gen) {
      manifest(live,converted);
      if(ri==0) manifest(normal,original);
     }
     ++windows;
    }
   }
  }
  output(shared+"Data/INI/CustomControlBar/"+res[ri]+"/Appearance.txt",live.str());
  std::cout<<res[ri]<<": fixed WND/art/scheme ready\n";
 }
 // Reborn: Replace only our managed block so repeated preparation preserves all hand-created mod art.
 const std::string centralPath=shared+"Data/INI/MappedImages/HandCreated/HandCreatedMappedImages.INI";
 std::string central=readText(centralPath);
 if(central.empty()) throw std::runtime_error("Missing HandCreatedMappedImages.INI");
 const std::string begin="; Reborn: Begin built-in Custom Control Bar mapped images";
 const std::string end="; Reborn: End built-in Custom Control Bar mapped images";
 const size_t beginPos=central.find(begin);
 if(beginPos!=std::string::npos) {
  const size_t endPos=central.find(end,beginPos);
  if(endPos==std::string::npos) throw std::runtime_error("Unterminated custom image block");
  size_t after=endPos+end.size();
  while(after<central.size() && (central[after]=='\r' || central[after]=='\n')) ++after;
  central.erase(beginPos,after-beginPos);
 }
 output(centralPath,central+"\n"+begin+"\n"+imageDefinitions.str()+"\n"+end+"\n");
 output(shared+"Data/INI/CustomControlBar/NormalAppearance.txt",normal.str());
 std::cout<<"PASS: "<<windows<<" fixed WNDs retain original control identities\n";
 return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
