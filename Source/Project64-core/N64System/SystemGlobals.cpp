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
#include <string>
#ifdef _WIN32
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#endif

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

extern "C" void TriggerDeferredCheatLoadForNetplay(void)
{
    if (g_BaseSystem)
    {
        // Load cheats now that they have been synchronized
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

        // Set the cheat selection changed flag so the system knows cheats have been updated
        // This will trigger UI updates and other notifications without clearing m_Codes
        g_BaseSystem->SetCheatsSlectionChanged(true);
        
        // Apply cheats immediately to memory if MMU is ready
        // (They'll also be applied every frame automatically in the VI handler)
        if (g_MMU)
        {
            g_BaseSystem->m_Cheats.ApplyCheats(g_MMU);
        }
    }
}

extern "C" bool GetEmulatorStateHashForNetplay(char * hash_buffer, size_t buffer_size)
{
    if (!g_BaseSystem || !hash_buffer || buffer_size < 65)
    {
        return false;
    }
    
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    
    try
    {
        // Acquire crypto context
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            return false;
        }
        
        // Create SHA256 hash object
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            CryptReleaseContext(hProv, 0);
            return false;
        }
        
        // Hash RDRAM (main memory) - most critical for desync detection
        // Use g_MMU which is set to point to the active system's MMU
        if (!g_MMU)
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }
        
        uint8_t * rdram = g_MMU->Rdram();
        uint32_t rdram_size = g_MMU->RdramSize();
        if (rdram && rdram_size > 0)
        {
            CryptHashData(hHash, rdram, rdram_size, 0);
        }
        
        // Hash CPU registers - critical state
        // Use g_Reg which is set to point to the active system's registers
        if (g_Reg)
        {
            // Program Counter
            CryptHashData(hHash, (const BYTE*)&g_Reg->m_PROGRAM_COUNTER, sizeof(g_Reg->m_PROGRAM_COUNTER), 0);
            
            // General Purpose Registers (32 registers)
            CryptHashData(hHash, (const BYTE*)g_Reg->m_GPR, sizeof(g_Reg->m_GPR), 0);
            
            // Floating Point Registers (32 registers)
            CryptHashData(hHash, (const BYTE*)g_Reg->m_FPR, sizeof(g_Reg->m_FPR), 0);
            
            // CP0 Registers (33 registers)
            CryptHashData(hHash, (const BYTE*)g_Reg->m_CP0, sizeof(g_Reg->m_CP0), 0);
            
            // HI and LO registers
            CryptHashData(hHash, (const BYTE*)&g_Reg->m_HI, sizeof(g_Reg->m_HI), 0);
            CryptHashData(hHash, (const BYTE*)&g_Reg->m_LO, sizeof(g_Reg->m_LO), 0);
        }
        
        // Hash RSP memory (DMEM and IMEM) - important for RSP state
        uint8_t * dmem = g_MMU->Dmem();
        uint8_t * imem = g_MMU->Imem();
        if (dmem)
        {
            CryptHashData(hHash, dmem, 0x1000, 0); // DMEM is 4KB
        }
        if (imem)
        {
            CryptHashData(hHash, imem, 0x1000, 0); // IMEM is 4KB
        }
        
        // Get hash value (SHA256 = 32 bytes = 64 hex chars)
        DWORD hashLen = 32;
        BYTE hashBytes[32];
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }
        
        // Convert to hex string
        static const char hexChars[] = "0123456789ABCDEF";
        std::string digest;
        digest.reserve(64);
        for (DWORD i = 0; i < hashLen; i++)
        {
            digest += hexChars[(hashBytes[i] >> 4) & 0xF];
            digest += hexChars[hashBytes[i] & 0xF];
        }
        
        // Copy to output buffer
        if (digest.length() < buffer_size)
        {
            strncpy_s(hash_buffer, buffer_size, digest.c_str(), digest.length());
            hash_buffer[digest.length()] = '\0';
            
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return true;
        }
        
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
    }
    catch (...)
    {
        if (hHash) CryptDestroyHash(hHash);
        if (hProv) CryptReleaseContext(hProv, 0);
        return false;
    }
#else
    // Non-Windows: would need to implement using OpenSSL or similar
    return false;
#endif
    
    return false;
}