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
#include "SettingsType-CheatsEnabled.h"
#include "SettingsType-Cheats.h"
#include <Common/path.h>
#include <algorithm>
#include <cctype>

CIniFile * CSettingTypeCheatsEnabled::m_EnabledIniFile = NULL;
stdstr   * CSettingTypeCheatsEnabled::m_SectionIdent = NULL;

CSettingTypeCheatsEnabled::CSettingTypeCheatsEnabled(const char * PostFix, bool DefaultValue) :
    m_PostFix(PostFix),
    m_DefaultValue(DefaultValue)
{
}

CSettingTypeCheatsEnabled::~CSettingTypeCheatsEnabled(void)
{
}

stdstr CSettingTypeCheatsEnabled::GetGameSpecificCheatEnabledFilePath(void)
{
    stdstr romName = g_Settings->LoadStringVal(Game_GameName);
    stdstr cheatFileName;
    
    if (!romName.empty())
    {
        stdstr sanitized = CSettingTypeCheats::SanitizeRomNameForFilename(romName);
        if (!sanitized.empty() && sanitized != "Unknown")
        {
            cheatFileName = sanitized + ".cht";
        }
    }
    
    // If we still don't have a valid filename, try game identifier
    if (cheatFileName.empty())
    {
        stdstr gameId = g_Settings->LoadStringVal(Game_IniKey);
        if (!gameId.empty())
        {
            // Sanitize gameId as well to ensure it's safe for filenames
            stdstr sanitizedGameId = CSettingTypeCheats::SanitizeRomNameForFilename(gameId);
            if (!sanitizedGameId.empty() && sanitizedGameId != "Unknown")
            {
                cheatFileName = sanitizedGameId + ".cht";
            }
        }
    }
    
    // Final fallback if both are empty or invalid
    if (cheatFileName.empty() || cheatFileName == ".cht")
    {
        cheatFileName = "Unknown.cht";
    }
    
    // Get base directory and construct full path in User/Cheats/ subdirectory
    CPath BaseDir(g_Settings->LoadStringVal(Cmd_BaseDirectory).c_str(), "");
    CPath CheatFile(BaseDir);
    CheatFile.AppendDirectory("User");
    CheatFile.AppendDirectory("Cheats");
    CheatFile.SetNameExtension(cheatFileName.c_str());
    
    // Ensure the Cheats directory exists
    if (!CheatFile.DirectoryExists())
    {
        CheatFile.DirectoryCreate();
    }
    
    return (const std::string &)CheatFile;
}

void CSettingTypeCheatsEnabled::Initialize(void)
{
    WriteTrace(TraceAppInit, TraceDebug, "Start");
    
    // Use game-specific cheat enabled file path
    stdstr enabledFilePath = GetGameSpecificCheatEnabledFilePath();
    m_EnabledIniFile = new CIniFile(enabledFilePath.c_str());
    m_EnabledIniFile->SetAutoFlush(false);
    
    g_Settings->RegisterChangeCB(Game_IniKey, NULL, GameChanged);
    g_Settings->RegisterChangeCB(Game_GameName, NULL, GameChanged);
    m_SectionIdent = new stdstr(g_Settings->LoadStringVal(Game_IniKey));
    GameChanged(NULL);
    WriteTrace(TraceAppInit, TraceDebug, "Done");
}

void CSettingTypeCheatsEnabled::CleanUp(void)
{
    g_Settings->UnregisterChangeCB(Game_GameName, NULL, GameChanged);
    g_Settings->UnregisterChangeCB(Game_IniKey, NULL, GameChanged);
    
    if (m_EnabledIniFile)
    {
        m_EnabledIniFile->SetAutoFlush(true);
        delete m_EnabledIniFile;
        m_EnabledIniFile = NULL;
    }
    if (m_SectionIdent)
    {
        delete m_SectionIdent;
        m_SectionIdent = NULL;
    }
}

void CSettingTypeCheatsEnabled::FlushChanges(void)
{
    if (m_EnabledIniFile)
    {
        m_EnabledIniFile->FlushChanges();
    }
}

void CSettingTypeCheatsEnabled::GameChanged(void * /*Data */)
{
    if (m_EnabledIniFile == NULL) return;
    
    // Update section identifier
    *m_SectionIdent = g_Settings->LoadStringVal(Game_IniKey);
    
    // Get new game-specific cheat enabled file path
    stdstr newEnabledFilePath = GetGameSpecificCheatEnabledFilePath();
    stdstr currentEnabledFilePath = m_EnabledIniFile->GetFileName();
    
    // Normalize paths for comparison (convert to lowercase and normalize separators)
    stdstr currentLower = currentEnabledFilePath;
    stdstr newLower = newEnabledFilePath;
    std::transform(currentLower.begin(), currentLower.end(), currentLower.begin(), ::tolower);
    std::transform(newLower.begin(), newLower.end(), newLower.begin(), ::tolower);
    
    // Replace backslashes with forward slashes for comparison
    std::replace(currentLower.begin(), currentLower.end(), '\\', '/');
    std::replace(newLower.begin(), newLower.end(), '\\', '/');
    
    // If the file path changed, close old file and open new one
    if (currentLower != newLower)
    {
        WriteTrace(TraceAppInit, TraceDebug, "Switching cheat enabled file from %s to %s", currentEnabledFilePath.c_str(), newEnabledFilePath.c_str());
        
        // Flush and close old file
        m_EnabledIniFile->SetAutoFlush(true);
        m_EnabledIniFile->FlushChanges();
        delete m_EnabledIniFile;
        
        // Open new game-specific file
        m_EnabledIniFile = new CIniFile(newEnabledFilePath.c_str());
        m_EnabledIniFile->SetAutoFlush(false);
    }
}

bool CSettingTypeCheatsEnabled::IsSettingSet(void) const
{
    if (m_EnabledIniFile == NULL)
    {
        return false;
    }
    uint32_t Value;
    stdstr_f Key("Cheat%d_S%s", 0, m_PostFix);
    return m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), 0, Value);
}

bool CSettingTypeCheatsEnabled::Load(int Index, bool & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        Value = m_DefaultValue;
        return false;
    }
    uint32_t IntValue;
    stdstr_f Key("Cheat%d_S%s", Index, m_PostFix);
    bool Found = m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), m_DefaultValue ? 1 : 0, IntValue);
    Value = (IntValue != 0);
    return Found;
}

bool CSettingTypeCheatsEnabled::Load(int Index, uint32_t & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        Value = m_DefaultValue ? 1 : 0;
        return false;
    }
    // Use CheatN_ENABLED format in .cht file instead of separate .cht_enabled file
    stdstr_f Key("Cheat%d_K%s", Index, m_PostFix);
    return m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), m_DefaultValue ? 1 : 0, Value);
}

bool CSettingTypeCheatsEnabled::Load(int Index, stdstr & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        return false;
    }
    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    return m_EnabledIniFile->GetString(m_SectionIdent->c_str(), Key.c_str(), "", Value);
}

//return the default values
void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, bool & Value) const
{
    Value = m_DefaultValue;
}

void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, uint32_t & Value) const
{
    Value = m_DefaultValue ? 1 : 0;
}

void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, stdstr & /*Value*/) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

//Update the settings
void CSettingTypeCheatsEnabled::Save(int Index, bool Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d_S%s", Index, m_PostFix);
    m_EnabledIniFile->SaveNumber(m_SectionIdent->c_str(), Key.c_str(), Value ? 1 : 0);
}

void CSettingTypeCheatsEnabled::Save(int Index, uint32_t Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d_S%s", Index, m_PostFix);
    m_EnabledIniFile->SaveNumber(m_SectionIdent->c_str(), Key.c_str(), Value);
}

void CSettingTypeCheatsEnabled::Save(int Index, const stdstr & Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), Value.c_str());
}

void CSettingTypeCheatsEnabled::Save(int Index, const char * Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), Value);
}

void CSettingTypeCheatsEnabled::Delete(int Index)
{
    stdstr_f Key("Cheat%d_S%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), NULL);
}
