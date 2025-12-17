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
#include "SettingsType-Cheats.h"
#include <Common/path.h>
#include <algorithm>
#include <cctype>
#include <cstring>

CIniFile * CSettingTypeCheats::m_CheatIniFile = NULL;
stdstr   * CSettingTypeCheats::m_SectionIdent = NULL;

CSettingTypeCheats::CSettingTypeCheats(const char * PostFix ) :
    m_PostFix(PostFix)
{
}

CSettingTypeCheats::~CSettingTypeCheats ( void )
{
}

stdstr CSettingTypeCheats::SanitizeRomNameForFilename(const stdstr& romName)
{
    stdstr sanitized = romName;
    
    // Remove or replace invalid filename characters
    const char* invalid_chars = "<>:\"/\\|?*";
    for (size_t i = 0; i < sanitized.length(); i++)
    {
        if (strchr(invalid_chars, sanitized[i]) != NULL)
        {
            sanitized[i] = '_'; // Replace invalid chars with underscore
        }
    }
    
    // Remove leading/trailing spaces and dots
    while (!sanitized.empty() && (sanitized[0] == ' ' || sanitized[0] == '.'))
    {
        sanitized.erase(0, 1);
    }
    while (!sanitized.empty() && (sanitized[sanitized.length() - 1] == ' ' || sanitized[sanitized.length() - 1] == '.'))
    {
        sanitized.erase(sanitized.length() - 1, 1);
    }
    
    // If empty after sanitization, use a default
    if (sanitized.empty())
    {
        sanitized = "Unknown";
    }
    
    // Limit length to avoid filesystem issues
    if (sanitized.length() > 200)
    {
        sanitized = sanitized.substr(0, 200);
    }
    
    return sanitized;
}

stdstr CSettingTypeCheats::GetGameSpecificCheatFilePath(const stdstr& extension)
{
    stdstr romName = g_Settings->LoadStringVal(Game_GameName);
    stdstr cheatFileName;
    
    if (!romName.empty())
    {
        stdstr sanitized = SanitizeRomNameForFilename(romName);
        if (!sanitized.empty() && sanitized != "Unknown")
        {
            cheatFileName = sanitized + extension;
        }
    }
    
    // If we still don't have a valid filename, try game identifier
    if (cheatFileName.empty())
    {
        stdstr gameId = g_Settings->LoadStringVal(Game_IniKey);
        if (!gameId.empty())
        {
            // Sanitize gameId as well to ensure it's safe for filenames
            stdstr sanitizedGameId = SanitizeRomNameForFilename(gameId);
            if (!sanitizedGameId.empty() && sanitizedGameId != "Unknown")
            {
                cheatFileName = sanitizedGameId + extension;
            }
        }
    }
    
    // Final fallback if both are empty or invalid
    if (cheatFileName.empty() || cheatFileName == extension)
    {
        cheatFileName = "Unknown" + extension;
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

void CSettingTypeCheats::Initialize ( void )
{
    WriteTrace(TraceAppInit, TraceDebug, "Start");
    
    // Use game-specific cheat file path
    stdstr cheatFilePath = GetGameSpecificCheatFilePath(".cht");
    m_CheatIniFile = new CIniFile(cheatFilePath.c_str());
    m_CheatIniFile->SetAutoFlush(false);
    
    g_Settings->RegisterChangeCB(Game_IniKey, NULL, GameChanged);
    g_Settings->RegisterChangeCB(Game_GameName, NULL, GameChanged);
    m_SectionIdent = new stdstr(g_Settings->LoadStringVal(Game_IniKey));
    GameChanged(NULL);
    WriteTrace(TraceAppInit, TraceDebug, "Done");
}

void CSettingTypeCheats::CleanUp   ( void )
{
    g_Settings->UnregisterChangeCB(Game_GameName, NULL, GameChanged);
    g_Settings->UnregisterChangeCB(Game_IniKey, NULL, GameChanged);
    
    if (m_CheatIniFile)
    {
        m_CheatIniFile->SetAutoFlush(true);
        delete m_CheatIniFile;
        m_CheatIniFile = NULL;
    }
    if (m_SectionIdent)
    {
        delete m_SectionIdent;
        m_SectionIdent = NULL;
    }
}

void CSettingTypeCheats::FlushChanges( void )
{
    if (m_CheatIniFile)
    {
        m_CheatIniFile->FlushChanges();
    }
}

void CSettingTypeCheats::GameChanged ( void * /*Data */ )
{
    if (m_CheatIniFile == NULL) return;
    
    // Update section identifier
    *m_SectionIdent = g_Settings->LoadStringVal(Game_IniKey);
    
    // Get new game-specific cheat file path
    stdstr newCheatFilePath = GetGameSpecificCheatFilePath(".cht");
    stdstr currentCheatFilePath = m_CheatIniFile->GetFileName();
    
    // Normalize paths for comparison (convert to lowercase and normalize separators)
    stdstr currentLower = currentCheatFilePath;
    stdstr newLower = newCheatFilePath;
    std::transform(currentLower.begin(), currentLower.end(), currentLower.begin(), ::tolower);
    std::transform(newLower.begin(), newLower.end(), newLower.begin(), ::tolower);
    
    // Replace backslashes with forward slashes for comparison
    std::replace(currentLower.begin(), currentLower.end(), '\\', '/');
    std::replace(newLower.begin(), newLower.end(), '\\', '/');
    
    // If the file path changed, close old file and open new one
    if (currentLower != newLower)
    {
        WriteTrace(TraceAppInit, TraceDebug, "Switching cheat file from %s to %s", currentCheatFilePath.c_str(), newCheatFilePath.c_str());
        
        // Flush and close old file
        m_CheatIniFile->SetAutoFlush(true);
        m_CheatIniFile->FlushChanges();
        delete m_CheatIniFile;
        
        // Open new game-specific file
        m_CheatIniFile = new CIniFile(newCheatFilePath.c_str());
        m_CheatIniFile->SetAutoFlush(false);
    }
}

bool CSettingTypeCheats::IsSettingSet(void) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
    return false;
}

bool CSettingTypeCheats::Load ( int /*Index*/, bool & /*Value*/ ) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
    return false;
}

bool CSettingTypeCheats::Load ( int /*Index*/, uint32_t & /*Value*/ ) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
    return false;
}

bool CSettingTypeCheats::Load ( int Index,  stdstr & Value ) const
{
    if (m_CheatIniFile == NULL)
    {
        return false;
    }
    stdstr_f Key("Cheat%d%s",Index,m_PostFix);
    return m_CheatIniFile->GetString(m_SectionIdent->c_str(),Key.c_str(),"",Value);
}

//return the default values
void CSettingTypeCheats::LoadDefault ( int /*Index*/, bool & /*Value*/ ) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CSettingTypeCheats::LoadDefault ( int /*Index*/, uint32_t & /*Value*/ ) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CSettingTypeCheats::LoadDefault ( int /*Index*/, stdstr & /*Value*/ ) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

//Update the settings
void CSettingTypeCheats::Save ( int /*Index*/, bool /*Value*/ )
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CSettingTypeCheats::Save ( int /*Index*/, uint32_t /*Value*/ )
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CSettingTypeCheats::Save ( int Index, const stdstr & Value )
{
    if (m_CheatIniFile == NULL) {  return;  }

    stdstr_f Key("Cheat%d%s",Index,m_PostFix);
    m_CheatIniFile->SaveString(m_SectionIdent->c_str(),Key.c_str(),Value.c_str());
}

void CSettingTypeCheats::Save ( int Index, const char * Value )
{
    if (m_CheatIniFile == NULL) {  return;  }

    stdstr_f Key("Cheat%d%s",Index,m_PostFix);
    m_CheatIniFile->SaveString(m_SectionIdent->c_str(),Key.c_str(),Value);
}

void CSettingTypeCheats::Delete ( int Index )
{
    stdstr_f Key("Cheat%d%s",Index,m_PostFix);
    m_CheatIniFile->SaveString(m_SectionIdent->c_str(),Key.c_str(),NULL);
}

void CSettingTypeCheats::CloseCheatFile ( void )
{
    if (m_CheatIniFile == NULL) { return; }
    
    // Close the file handle to allow external writes
    m_CheatIniFile->CloseFile();
}

void CSettingTypeCheats::ReloadCheatFile ( void )
{
    if (m_CheatIniFile == NULL) { return; }
    
    // Force reload by closing and reopening the file, clearing the cache
    m_CheatIniFile->ReloadFile();
}

void CSettingTypeCheats::ForceReloadCheatFile ( void )
{
    if (m_CheatIniFile == NULL) { return; }
    
    // Aggressive reload: clear all caches and force complete file re-scan
    m_CheatIniFile->ForceReloadFile();
}
