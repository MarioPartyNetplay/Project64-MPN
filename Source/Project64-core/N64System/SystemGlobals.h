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

class CSettings;
extern CSettings     * g_Settings;

class CN64System;
extern CN64System    * g_System;
extern CN64System    * g_BaseSystem;
extern CN64System    * g_SyncSystem;

class CRecompiler;
extern CRecompiler   * g_Recompiler;

class CMipsMemoryVM;
extern CMipsMemoryVM * g_MMU; //Memory of the n64

class CTLB;
extern CTLB          * g_TLB; //TLB Unit

class CRegisters;
extern CRegisters    * g_Reg; //Current Register Set attached to the g_MMU

class CPlugins;
extern CPlugins      * g_Plugins;

class CN64Rom;
extern CN64Rom       * g_Rom;      //The current rom that this system is executing.. it can only execute one file at the time
extern CN64Rom       * g_DDRom;    //64DD IPL ROM

class CN64Disk;
extern CN64Disk      * g_Disk;     //64DD DISK

class CAudio;
extern CAudio        * g_Audio;

class CSystemTimer;
extern CSystemTimer  * g_SystemTimer;

__interface CTransVaddr;
extern CTransVaddr   * g_TransVaddr;

class CSystemEvents;
extern CSystemEvents * g_SystemEvents;

extern int32_t       * g_NextTimer;
extern uint32_t      * g_TLBLoadAddress;
extern uint32_t      * g_TLBStoreAddress;

__interface CDebugger;
extern CDebugger     * g_Debugger;

extern uint8_t      ** g_RecompPos;

// Function to save state to a specific file path (for netplay desync detection)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) bool SaveStateToFileForNetplay(const char * FilePath);

// Function to close cheat file handle (for netplay cheat syncing)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void CloseCheatFileForNetplay(void);

// Function to trigger cheat reload (for netplay cheat syncing)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void TriggerCheatReloadForNetplay(void);

// Function to trigger force cheat reload - completely clears cache and forces full file re-scan
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void TriggerForceCheatReloadForNetplay(void);

// Function to trigger deferred cheat loading after synchronization (for netplay)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void TriggerDeferredCheatLoadForNetplay(void);

// Function to trigger soft reset (for netplay cheat syncing)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void TriggerSoftResetForNetplay(void);

// Function to apply cheats directly to memory from cheat data (for p2-4 netplay clients)
// Export from main executable so plugin can use GetProcAddress
extern "C" __declspec(dllexport) void ApplyCheatsDirectlyForNetplay(const char * cheat_file_content, const char * enabled_file_content, const char * game_identifier);

// Function to get emulator state hash for desync detection (for netplay)
// Export from main executable so plugin can use GetProcAddress
// Returns SHA256 hash of critical emulator state (RDRAM, CPU registers, RSP memory)
// Caller must provide a buffer of at least 65 bytes (64 hex chars + null terminator)
extern "C" __declspec(dllexport) bool GetEmulatorStateHashForNetplay(char * hash_buffer, size_t buffer_size);

class CMempak;
extern CMempak       * g_Mempak;