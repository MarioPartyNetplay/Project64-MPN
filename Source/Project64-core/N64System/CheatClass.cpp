/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/
#include "stdafx.h"
#include "CheatClass.h"
#include <string>

#include <Project64-core/Settings/SettingType/SettingsType-Cheats.h>
#include <Project64-core/Plugins/GFXPlugin.h>
#include <Project64-core/Plugins/AudioPlugin.h>
#include <Project64-core/Plugins/RSPPlugin.h>
#include <Project64-core/Plugins/ControllerPlugin.h>
#include <Project64-core/N64System/Recompiler/RecompilerClass.h>
#include <Project64-core/N64System/SystemGlobals.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <map>

CCheats::CCheats()
{
}

CCheats::~CCheats()
{
}

bool CCheats::LoadCode(int CheatNo, const char * CheatString)
{
    if (!IsValid16BitCode(CheatString))
    {
        return false;
    }

    const char * ReadPos = CheatString;
    size_t remaining_length = strlen(CheatString);

    CODES Code;
    while (ReadPos && remaining_length > 0)
    {
        GAMESHARK_CODE CodeEntry;

        CodeEntry.Command = strtoul(ReadPos, 0, 16);
        ReadPos = strchr(ReadPos, ' ');
        if (ReadPos == NULL) { break; }
        ReadPos += 1;
        remaining_length = strlen(ReadPos);

        if (remaining_length >= 4 && strncmp(ReadPos, "????", 4) == 0)
        {
            if (CheatNo < 0 || CheatNo > MaxCheats) { return false; }
            stdstr CheatExt = g_Settings->LoadStringIndex(Cheat_Extension, CheatNo);
            if (CheatExt.empty() || CheatExt.length() < 2) { return false; }
            CodeEntry.Value = CheatExt[0] == '$' ? (uint16_t)strtoul(&CheatExt.c_str()[1], 0, 16) : (uint16_t)atol(CheatExt.c_str());
        }
        else if (remaining_length >= 2 && strncmp(ReadPos, "??", 2) == 0)
        {
            if (CheatNo < 0 || CheatNo > MaxCheats) { return false; }
            stdstr CheatExt = g_Settings->LoadStringIndex(Cheat_Extension, CheatNo);
            if (CheatExt.empty()) { return false; }
            CodeEntry.Value = (uint8_t)(strtoul(ReadPos, 0, 16));
            CodeEntry.Value |= (CheatExt[0] == '$' ? (uint8_t)strtoul(&CheatExt.c_str()[1], 0, 16) : (uint8_t)atol(CheatExt.c_str())) << 16;
        }
        else if (remaining_length >= 4 && strncmp(&ReadPos[2], "??", 2) == 0)
        {
            if (CheatNo < 0 || CheatNo > MaxCheats) { return false; }
            stdstr CheatExt = g_Settings->LoadStringIndex(Cheat_Extension, CheatNo);
            if (CheatExt.empty()) { return false; }
            CodeEntry.Value = (uint16_t)(strtoul(ReadPos, 0, 16) << 16);
            CodeEntry.Value |= CheatExt[0] == '$' ? (uint8_t)strtoul(&CheatExt.c_str()[1], 0, 16) : (uint8_t)atol(CheatExt.c_str());
        }
        else
        {
            CodeEntry.Value = remaining_length >= 4 ? (uint16_t)strtoul(ReadPos, 0, 16) : 0;
        }
        Code.push_back(CodeEntry);

        ReadPos = strchr(ReadPos, ',');
        if (ReadPos == NULL)
        {
            break;
        }
        ReadPos++;
        remaining_length = strlen(ReadPos);
    }
    if (Code.size() == 0 || Code.size() > MaxGSEntries)
    {
        return false;
    }

    m_Codes.push_back(Code);
    return true;
}

void CCheats::LoadPermCheats(CPlugins * Plugins)
{
    if (g_Settings->LoadBool(Debugger_DisableGameFixes))
    {
        return;
    }
    for (int CheatNo = 0; CheatNo < MaxCheats; CheatNo++)
    {
        stdstr LineEntry;
        if (!g_Settings->LoadStringIndex(Rdb_GameCheatFix, CheatNo, LineEntry) || LineEntry.empty())
        {
            break;
        }

        stdstr CheatPlugins;
        bool LoadEntry = true;
        if (g_Settings->LoadStringIndex(Rdb_GameCheatFixPlugin, CheatNo, CheatPlugins) && !CheatPlugins.empty())
        {
            LoadEntry = false;

            strvector PluginList = CheatPlugins.Tokenize(',');
            for (size_t i = 0, n = PluginList.size(); i < n; i++)
            {
                stdstr PluginName = PluginList[i].Trim();
                if (Plugins->Gfx() != NULL && strstr(Plugins->Gfx()->PluginName(), PluginName.c_str()) != NULL)
                {
                    LoadEntry = true;
                    break;
                }
                if (Plugins->Audio() != NULL && strstr(Plugins->Audio()->PluginName(), PluginName.c_str()) != NULL)
                {
                    LoadEntry = true;
                    break;
                }
                if (Plugins->RSP() != NULL && strstr(Plugins->RSP()->PluginName(), PluginName.c_str()) != NULL)
                {
                    LoadEntry = true;
                    break;
                }
                if (Plugins->Control() != NULL && strstr(Plugins->Control()->PluginName(), PluginName.c_str()) != NULL)
                {
                    LoadEntry = true;
                    break;
                }
            }
        }

        if (LoadEntry)
        {
            LoadCode(-1, LineEntry.c_str());
        }
    }
}

void CCheats::LoadCheats(bool DisableSelected, CPlugins * Plugins)
{
    m_Codes.clear();
    LoadPermCheats(Plugins);

    for (int CheatNo = 0; CheatNo < MaxCheats; CheatNo++)
    {
        stdstr LineEntry = g_Settings->LoadStringIndex(Cheat_Entry, CheatNo);
        if (LineEntry.empty()) { break; }
        if (!g_Settings->LoadBoolIndex(Cheat_Active, CheatNo))
        {
            continue;
        }
        if (DisableSelected)
        {
            g_Settings->SaveBoolIndex(Cheat_Active, CheatNo, false);
            continue;
        }

        //Find the start and end of the name which is surrounded in ""
        int StartOfName = LineEntry.find("\"");
        if (StartOfName == -1) { continue; }
        int EndOfName = LineEntry.find("\"", StartOfName + 1);
        if (EndOfName == -1) { continue; }

        LoadCode(CheatNo, &LineEntry.c_str()[EndOfName + 2]);
    }
}

void CCheats::LoadCheatsFromData(const char * cheat_file_content, const char * enabled_file_content, const char * game_identifier, CPlugins * Plugins)
{
    m_Codes.clear();
    LoadPermCheats(Plugins);

    if (!cheat_file_content || !game_identifier)
    {
        return;
    }

    // Debug logging
    OutputDebugStringA("LoadCheatsFromData: loading cheats for game ");
    OutputDebugStringA(game_identifier);

    // Parse enabled file to get active cheat status
    std::map<int, bool> active_cheats;
    if (enabled_file_content)
    {
        std::istringstream enabled_stream(enabled_file_content);
        std::string line;
        std::string current_section;
        
        while (std::getline(enabled_stream, line))
        {
            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos)
            {
                continue;
            }
            size_t end = line.find_last_not_of(" \t");
            if (end == std::string::npos || end < start)
            {
                continue; // Malformed line
            }
            line = line.substr(start, end - start + 1);
            
            // Check for section header
            if (!line.empty() && line[0] == '[' && line.back() == ']')
            {
                current_section = line.substr(1, line.length() - 2);
                continue;
            }
            
            // Only process entries for the game identifier section
            if (current_section != game_identifier)
            {
                continue;
            }
            
            // Parse key=value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos || eq_pos == 0 || eq_pos >= line.length() - 1)
            {
                continue;
            }

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Check if it's a CheatN key
            if (key.length() > 5 && key.substr(0, 5) == "Cheat")
            {
                int cheat_no = atoi(key.substr(5).c_str());
                active_cheats[cheat_no] = (value == "1" || value == "true" || value == "True");
            }
        }
    }

    // Parse cheat file content
    std::istringstream cheat_stream(cheat_file_content);
    std::string line;
    std::string current_section;
    std::map<int, std::string> cheat_entries;
    
    while (std::getline(cheat_stream, line))
    {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
        {
            continue;
        }
        size_t end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);
        
        // Check for section header
        if (!line.empty() && line[0] == '[' && line.back() == ']')
        {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Only process entries for the game identifier section
        if (current_section != game_identifier)
        {
            continue;
        }
        
        // Parse key=value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos)
        {
            continue;
        }
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Check if it's a CheatN key
        if (key.length() > 5 && key.substr(0, 5) == "Cheat")
        {
            int cheat_no = atoi(key.substr(5).c_str());
            cheat_entries[cheat_no] = value;
        }
    }

    // Load active cheats
    // If enabled_file_content is empty or no active cheats found, load all cheats (fallback)
    bool load_all_cheats = (!enabled_file_content || (enabled_file_content[0] == '\0') || active_cheats.empty());

    for (const auto& entry : cheat_entries)
    {
        int cheat_no = entry.first;

        // Check if cheat is active
        bool is_active = false;
        if (!load_all_cheats)
        {
            auto active_it = active_cheats.find(cheat_no);
            if (active_it != active_cheats.end())
            {
                is_active = active_it->second;
            }
        }
        else
        {
            // Fallback: load all cheats if enabled file is empty or parsing failed
            is_active = true;
        }

        if (!is_active)
        {
            continue;
        }

        // Parse the cheat entry: format is "Name" code1,code2,code3,...
        std::string cheat_entry = entry.second;

        // Find the start and end of the name which is surrounded in ""
        size_t start_of_name = cheat_entry.find("\"");
        if (start_of_name == std::string::npos || start_of_name >= cheat_entry.length() - 2) { continue; }
        size_t end_of_name = cheat_entry.find("\"", start_of_name + 1);
        if (end_of_name == std::string::npos || end_of_name <= start_of_name || end_of_name >= cheat_entry.length()) { continue; }

        // Get the code part (after the closing quote and space)
        size_t code_start = end_of_name + 1;
        if (code_start >= cheat_entry.length()) { continue; }

        while (code_start < cheat_entry.length() && (cheat_entry[code_start] == ' ' || cheat_entry[code_start] == '\t'))
        {
            code_start++;
        }

        if (code_start >= cheat_entry.length()) { continue; }
        std::string cheat_code = cheat_entry.substr(code_start);

        // Load the cheat code directly (use -1 for CheatNo since we're not using extensions)
        if (LoadCode(-1, cheat_code.c_str()))
        {
            // Cheat loaded successfully
        }
        else
        {
            // Cheat failed to load (invalid code format or requires extension)
            // This is OK - some cheats require extensions which we don't support for p2-4
        }
    }

    // Debug logging
    OutputDebugStringA("LoadCheatsFromData: finished loading cheats");
}

/********************************************************************************************
ConvertXP64Address

Purpose: Decode encoded XP64 address to physical address
Parameters:
Returns:
Author: Witten

********************************************************************************************/
uint32_t ConvertXP64Address(uint32_t Address)
{
    uint32_t tmpAddress;

    tmpAddress = (Address ^ 0x68000000) & 0xFF000000;
    tmpAddress += ((Address + 0x002B0000) ^ 0x00810000) & 0x00FF0000;
    tmpAddress += ((Address + 0x00002B00) ^ 0x00008200) & 0x0000FF00;
    tmpAddress += ((Address + 0x0000002B) ^ 0x00000083) & 0x000000FF;
    return tmpAddress;
}

/********************************************************************************************
ConvertXP64Value

Purpose: Decode encoded XP64 value
Parameters:
Returns:
Author: Witten

********************************************************************************************/
uint16_t ConvertXP64Value(uint16_t Value)
{
    uint16_t  tmpValue;

    tmpValue = ((Value + 0x2B00) ^ 0x8400) & 0xFF00;
    tmpValue += ((Value + 0x002B) ^ 0x0085) & 0x00FF;
    return tmpValue;
}

// Helper function to invalidate recompiler cache after writing to memory
static void InvalidateRecompilerCache(uint32_t VAddr)
{
    // Always invalidate recompiler cache when cheats write to memory
    // This ensures the dynarec sees the updated values
    if (g_Recompiler)
    {
        // ClearRecompCode_Virt handles virtual addresses directly
        // Invalidate a 4KB page (0x1000 bytes) aligned to page boundary
        // This ensures any recompiled code that reads from this address will be recompiled
        // Use Remove_ProtectedMem reason to indicate this is a cheat/modification
        try {
            g_Recompiler->ClearRecompCode_Virt(VAddr & ~0xFFF, 0x1000, CRecompiler::Remove_ProtectedMem);
        } catch (...) {
            // Ignore errors - recompiler might not be active or address might be invalid
        }
    }
}

void CCheats::ApplyCheats(CMipsMemoryVM * MMU)
{
    for (size_t CurrentCheat = 0; CurrentCheat < m_Codes.size(); CurrentCheat++)
    {
        const CODES & CodeEntry = m_Codes[CurrentCheat];
        for (size_t CurrentEntry = 0; CurrentEntry < CodeEntry.size();)
        {
            CurrentEntry += ApplyCheatEntry(MMU, CodeEntry, CurrentEntry, true);
        }
    }
}

void CCheats::ApplyGSButton(CMipsMemoryVM * MMU)
{
    uint32_t Address;
    for (size_t CurrentCheat = 0; CurrentCheat < m_Codes.size(); CurrentCheat++)
    {
        const CODES & CodeEntry = m_Codes[CurrentCheat];
        for (size_t CurrentEntry = 0; CurrentEntry < CodeEntry.size(); CurrentEntry++)
        {
            const GAMESHARK_CODE & Code = CodeEntry[CurrentEntry];
            switch (Code.Command & 0xFF000000) {
            case 0x88000000:
                Address = 0x80000000 | (Code.Command & 0xFFFFFF);
                MMU->SB_VAddr(Address, (uint8_t)Code.Value);
                InvalidateRecompilerCache(Address);
                break;
            case 0x89000000:
                Address = 0x80000000 | (Code.Command & 0xFFFFFF);
                MMU->SH_VAddr(Address, Code.Value);
                InvalidateRecompilerCache(Address);
                break;
                // Xplorer64
            case 0xA8000000:
                Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
                MMU->SB_VAddr(Address, (uint8_t)ConvertXP64Value(Code.Value));
                InvalidateRecompilerCache(Address);
                break;
            case 0xA9000000:
                Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
                MMU->SH_VAddr(Address, ConvertXP64Value(Code.Value));
                InvalidateRecompilerCache(Address);
                break;
            }
        }
    }
}

bool CCheats::IsValid16BitCode(const char * CheatString)
{
    const char * ReadPos = CheatString;
    bool GSButtonCheat = false, FirstEntry = true;

    while (ReadPos)
    {
        GAMESHARK_CODE CodeEntry;

        CodeEntry.Command = strtoul(ReadPos, 0, 16);
        ReadPos = strchr(ReadPos, ' ');
        if (ReadPos == NULL) { break; }
        ReadPos += 1;

        //validate Code Entry
        switch (CodeEntry.Command & 0xFF000000) {
        case 0x50000000:
        case 0x80000000:
        case 0xA0000000:
        case 0xD0000000:
        case 0xD2000000:
        case 0xC8000000:
        case 0xE8000000:
        case 0x10000000: // Xplorer64
            break;
        case 0x81000000:
        case 0xA1000000:
        case 0xD1000000:
        case 0xD3000000:
            if (((CodeEntry.Command & 0xFFFFFF) & 1) == 1)
            {
                return false;
            }
            break;
        case 0x88000000:
        case 0xA8000000:
            if (FirstEntry)     { GSButtonCheat = true; }
            if (!GSButtonCheat) { return false; }
            break;
        case 0x89000000:
            if (FirstEntry)     { GSButtonCheat = true; }
            if (!GSButtonCheat) { return false; }
            if (((CodeEntry.Command & 0xFFFFFF) & 1) == 1)
            {
                return false;
            }
            break;
        case 0xA9000000:
            if (FirstEntry)     { GSButtonCheat = true; }
            if (!GSButtonCheat) { return false; }
            if (((ConvertXP64Address(CodeEntry.Command) & 0xFFFFFF) & 1) == 1)
            {
                return false;
            }
            break;
        case 0x11000000: // Xplorer64
        case 0xE9000000:
        case 0xC9000000:
            if (((ConvertXP64Address(CodeEntry.Command) & 0xFFFFFF) & 1) == 1)
            {
                return false;
            }
            break;
        default:
            return false;
        }

        FirstEntry = false;

        ReadPos = strchr(ReadPos, ',');
        if (ReadPos == NULL)
        {
            continue;
        }
        ReadPos++;
    }
    return true;
}

int CCheats::ApplyCheatEntry(CMipsMemoryVM * MMU, const CODES & CodeEntry, int CurrentEntry, bool Execute)
{
    if (CurrentEntry < 0 || CurrentEntry >= (int)CodeEntry.size() || CodeEntry.empty())
    {
        return 0;
    }
    const GAMESHARK_CODE & Code = CodeEntry[CurrentEntry];
    uint32_t Address;
    uint16_t  wMemory;
    uint8_t  bMemory;

    switch (Code.Command & 0xFF000000)
    {
        // Gameshark / AR
    case 0x50000000:
    {
        if ((CurrentEntry + 1) >= (int)CodeEntry.size())
        {
            return 1;
        }

        if (CurrentEntry + 1 >= (int)CodeEntry.size()) // Double-check for safety
        {
            return 1;
        }

        const GAMESHARK_CODE & NextCodeEntry = CodeEntry[CurrentEntry + 1];
        int numrepeats = (Code.Command & 0x0000FF00) >> 8;
        int offset = Code.Command & 0x000000FF;
        int incr = Code.Value;
        int i;

        switch (NextCodeEntry.Command & 0xFF000000) {
        case 0x10000000: // Xplorer64
        case 0x80000000:
            Address = 0x80000000 | (NextCodeEntry.Command & 0xFFFFFF);
            wMemory = NextCodeEntry.Value;
            for (i = 0; i < numrepeats; i++)
            {
                MMU->SB_VAddr(Address, (uint8_t)wMemory);
                InvalidateRecompilerCache(Address);
                Address += offset;
                wMemory += (uint16_t)incr;
            }
            return 2;
        case 0x11000000: // Xplorer64
        case 0x81000000:
            Address = 0x80000000 | (NextCodeEntry.Command & 0xFFFFFF);
            wMemory = NextCodeEntry.Value;
            for (i = 0; i < numrepeats; i++)
            {
                MMU->SH_VAddr(Address, wMemory);
                InvalidateRecompilerCache(Address);
                Address += offset;
                wMemory += (uint16_t)incr;
            }
            return 2;
        default: return 1;
        }
    }
    break;
    case 0x80000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SB_VAddr(Address, (uint8_t)Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0x81000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SH_VAddr(Address, Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xA0000000:
        Address = 0xA0000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SB_VAddr(Address, (uint8_t)Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xA1000000:
        Address = 0xA0000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SH_VAddr(Address, Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xD0000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        MMU->LB_VAddr(Address, bMemory);
        if (bMemory != Code.Value) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xD1000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        MMU->LH_VAddr(Address, wMemory);
        if (wMemory != Code.Value) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xD2000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        MMU->LB_VAddr(Address, bMemory);
        if (bMemory == Code.Value) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xD3000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        MMU->LH_VAddr(Address, wMemory);
        if (wMemory == Code.Value) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;

        // Xplorer64 (Author: Witten)
    case 0x30000000:
    case 0x82000000:
    case 0x84000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SB_VAddr(Address, (uint8_t)Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0x31000000:
    case 0x83000000:
    case 0x85000000:
        Address = 0x80000000 | (Code.Command & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SH_VAddr(Address, Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xE8000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SB_VAddr(Address, (uint8_t)ConvertXP64Value(Code.Value));
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xE9000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SH_VAddr(Address, ConvertXP64Value(Code.Value));
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xC8000000:
        Address = 0xA0000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SB_VAddr(Address, (uint8_t)Code.Value);
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xC9000000:
        Address = 0xA0000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        if (Execute) 
        { 
            MMU->SH_VAddr(Address, ConvertXP64Value(Code.Value));
            InvalidateRecompilerCache(Address);
        }
        break;
    case 0xB8000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        MMU->LB_VAddr(Address, bMemory);
        if (bMemory != ConvertXP64Value(Code.Value)) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xB9000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        MMU->LH_VAddr(Address, wMemory);
        if (wMemory != ConvertXP64Value(Code.Value)) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xBA000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        MMU->LB_VAddr(Address, bMemory);
        if (bMemory == ConvertXP64Value(Code.Value)) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0xBB000000:
        Address = 0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF);
        MMU->LH_VAddr(Address, wMemory);
        if (wMemory == ConvertXP64Value(Code.Value)) { Execute = false; }
        return ApplyCheatEntry(MMU, CodeEntry, CurrentEntry + 1, Execute) + 1;
    case 0: return MaxGSEntries; break;
    }
    return 1;
}