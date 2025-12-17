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
#include "SystemGlobals.h"
#include "N64Class.h"
#include "Mips/SystemEvents.h"
#include "Settings/SettingType/SettingsType-Cheats.h"

CN64System    * g_System = NULL;
CN64System    * g_BaseSystem = NULL;
CN64System    * g_SyncSystem = NULL;
CRecompiler   * g_Recompiler = NULL;
CMipsMemoryVM * g_MMU = NULL; //Memory of the n64
CTLB          * g_TLB = NULL; //TLB Unit
CRegisters    * g_Reg = NULL; //Current Register Set attacted to the g_MMU
CNotification * g_Notify = NULL;
CPlugins      * g_Plugins = NULL;
CN64Rom       * g_Rom = NULL;      //The current rom that this system is executing.. it can only execute one file at the time
CN64Rom       * g_DDRom = NULL;    //64DD IPL ROM
CN64Disk      * g_Disk = NULL;     //64DD DISK
CAudio        * g_Audio = NULL;
CSystemTimer  * g_SystemTimer = NULL;
CTransVaddr   * g_TransVaddr = NULL;
CSystemEvents * g_SystemEvents = NULL;
uint32_t      * g_TLBLoadAddress = NULL;
uint32_t      * g_TLBStoreAddress = NULL;
CDebugger     * g_Debugger = NULL;
uint8_t      ** g_RecompPos = NULL;
CMempak       * g_Mempak = NULL;

int * g_NextTimer;

extern "C" bool SaveStateToFileForNetplay(const char * FilePath)
{
    if (g_BaseSystem)
    {
        return g_BaseSystem->SaveStateToFile(FilePath);
    }
    return false;
}

extern "C" void CloseCheatFileForNetplay(void)
{
    // Close the cheat INI file handle to allow external writes
    CSettingTypeCheats::CloseCheatFile();
}

extern "C" void TriggerCheatReloadForNetplay(void)
{
    if (g_BaseSystem)
    {
        // Force the cheat INI file to reload from disk
        // This clears the cache so LoadCheats reads the updated file
        CSettingTypeCheats::ReloadCheatFile();
        
        // Set the flag for the CPU loop to reload
        g_BaseSystem->SetCheatsSlectionChanged(true);
        // Also reload immediately so GUI and frame 0 injection work
        g_BaseSystem->m_Cheats.LoadCheats(false, g_BaseSystem->GetPlugins());
    }
}

extern "C" void TriggerForceCheatReloadForNetplay(void)
{
    if (g_BaseSystem)
    {
        // Force reload: completely clear all caches and force full file re-scan
        CSettingTypeCheats::ForceReloadCheatFile();
        
        // Clear cheat list and reload from freshly scanned file
        g_BaseSystem->SetCheatsSlectionChanged(true);
        g_BaseSystem->m_Cheats.LoadCheats(false, g_BaseSystem->GetPlugins());
    }
}

extern "C" void TriggerSoftResetForNetplay(void)
{
    if (g_BaseSystem)
    {
        // Queue a soft reset event to reload the game with new cheats
        g_BaseSystem->ExternalEvent(SysEvent_ResetCPU_Soft);
    }
}

extern "C" void ApplyCheatsDirectlyForNetplay(const char * cheat_file_content, const char * enabled_file_content, const char * game_identifier)
{
    if (g_BaseSystem && cheat_file_content && game_identifier)
    {
        // Load cheats directly from data without reading .ini files
        // This loads cheats into m_Codes, which will be applied every frame automatically
        g_BaseSystem->m_Cheats.LoadCheatsFromData(cheat_file_content, enabled_file_content, game_identifier, g_BaseSystem->GetPlugins());
        
        // Clear the cheat selection changed flag so LoadCheats won't be called
        // (which would clear m_Codes and try to reload from .ini files)
        g_BaseSystem->SetCheatsSlectionChanged(false);
        
        // Apply cheats immediately to memory if MMU is ready
        // (They'll also be applied every frame automatically in the VI handler)
        if (g_MMU)
        {
            g_BaseSystem->m_Cheats.ApplyCheats(g_MMU);
        }
    }
}