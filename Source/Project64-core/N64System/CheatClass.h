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
#pragma once
#include "N64RomClass.h"
#include <Project64-core/N64System/Mips/MemoryVirtualMem.h>
#include <Project64-core/Plugins/PluginClass.h>
#include <Common/CriticalSection.h>

class CCheats
{
public:
    CCheats();
    ~CCheats(void);

    enum
    {
        MaxCheats = 50000,
        MaxGSEntries = 20480,
    };

    void ApplyCheats(CMipsMemoryVM * MMU);
    void ApplyGSButton(CMipsMemoryVM * MMU);
    void LoadCheats(bool DisableSelected, CPlugins * Plugins);
    void LoadCheatsFromData(const char * cheat_file_content, const char * enabled_file_content, const char * game_identifier, CPlugins * Plugins);

    // Thread-safe netplay cheat functions
    void ApplyCheatsForNetplay(CMipsMemoryVM * MMU);
    void LoadCheatsFromDataForNetplay(const char * cheat_file_content, const char * enabled_file_content, const char * game_identifier, CPlugins * Plugins);

    static bool IsValid16BitCode(const char * CheatString);

private:
    struct GAMESHARK_CODE
    {
        uint32_t Command;
        uint16_t Value;
    };

    typedef std::vector<GAMESHARK_CODE> CODES;
    typedef std::vector<CODES>          CODES_ARRAY;

    void LoadPermCheats(CPlugins * Plugins);

    CODES_ARRAY     m_Codes;
    CODES_ARRAY     m_NetplayCodes;  // Separate array for thread-safe netplay cheat loading
    CriticalSection m_CriticalSection; // Protects access to cheat arrays and global resources

    bool LoadCode(int32_t CheatNo, const char * CheatString);
    bool LoadCodeIntoArray(CODES_ARRAY& codes_array, int32_t CheatNo, const char * CheatString);
    int32_t ApplyCheatEntry(CMipsMemoryVM * MMU, const CODES & CodeEntry, int32_t CurrentEntry, bool Execute);
};
