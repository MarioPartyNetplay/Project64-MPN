#include "stdafx.h"

#include "Discord.h"
#include "MarioPartyOverlays.h"
#include <time.h>
#include <string.h>
#include <cctype>
#include <stdio.h>
#include <stdlib.h>

#include <Project64-core/N64System/Mips/MemoryVirtualMem.h>
#include <Project64-core/N64System/SystemGlobals.h>
#include <Project64-core/N64System/N64RomClass.h>
#include <3rdParty/discord-rpc/include/discord_rpc.h>

// Constants
static const char* const DISCORD_APP_ID = "888655408623943731";
static const int UPDATE_THROTTLE_SECONDS = 1;

// Discord event handlers
static void handleDiscordReady(const DiscordUser* request);
static void handleDiscordDisconnected(int errcode, const char* message);
static void handleDiscordError(int errcode, const char* message);
static void handleDiscordJoin(const char* secret);
static void handleDiscordSpectate(const char* secret);

static void handleDiscordReady(const DiscordUser* request)
{
    (void)request;
    printf("\nDiscord: ready\n");
}

static void handleDiscordDisconnected(int errcode, const char* message)
{
    printf("\nDiscord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message)
{
    printf("\nDiscord: error (%d: %s)\n", errcode, message);
}

static void handleDiscordJoin(const char* secret)
{
    printf("\nDiscord: join (%s)\n", secret);
}

static void handleDiscordSpectate(const char* secret)
{
    printf("\nDiscord: spectate (%s)\n", secret);
}

// singleton instance used by static Init/Update
static CDiscord* s_DiscordInstance = nullptr;

CDiscord::CDiscord() :
    m_StartTime(time(0)),
    m_CurrentPlayers(0),
    m_MaxPlayers(4),
    m_CurrentGameState(0),
    m_CurrentTurn(0),
    m_CurrentTotalTurns(0),
    m_NeedsUpdate(true),
    m_LastUpdateTime(0),
    m_IsMP1(false),
    m_IsMP2(false),
    m_IsMP3(false)
{
}

CDiscord::~CDiscord()
{
    Shutdown();
}

void CDiscord::Initialize()
{
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = handleDiscordReady;
    handlers.disconnected = handleDiscordDisconnected;
    handlers.errored = handleDiscordError;
    handlers.joinGame = handleDiscordJoin;
    handlers.spectateGame = handleDiscordSpectate;

    Discord_Initialize(DISCORD_APP_ID, &handlers, 1, nullptr);
}

void CDiscord::Shutdown()
{
    Discord_Shutdown();
}

void CDiscord::SetPlayerCount(uint8_t currentPlayers)
{
    if (m_CurrentPlayers != currentPlayers)
    {
        m_CurrentPlayers = currentPlayers;
        m_NeedsUpdate = true;
    }
}

void CDiscord::SetGameTitle(const char* title, const char* largeImageKey)
{
    if (m_GameTitle != title || m_LargeImageKey != largeImageKey)
    {
        m_GameTitle = title ? title : "";
        m_LargeImageKey = largeImageKey ? largeImageKey : "";
        m_NeedsUpdate = true;
    }
}

void CDiscord::SetRomName(const char* romName)
{
    if (m_RomName != romName)
    {
        m_RomName = romName ? romName : "";
        m_NeedsUpdate = true;
    }
}

const char* CDiscord::GetOverlayNameMP1(uint8_t overlayId) const
{
    if (overlayId < OVERLAY_NAMES_MP1_COUNT)
    {
        const char* name = OVERLAY_NAMES_MP1[overlayId];
        return (name && name[0] != '\0') ? name : nullptr;
    }
    return nullptr;
}

const char* CDiscord::GetOverlayNameMP2(uint8_t overlayId) const
{
    if (overlayId < OVERLAY_NAMES_MP2_COUNT)
    {
        const char* name = OVERLAY_NAMES_MP2[overlayId];
        return (name && name[0] != '\0') ? name : nullptr;
    }
    return nullptr;
}

const char* CDiscord::GetOverlayNameMP3(uint8_t overlayId) const
{
    if (overlayId < OVERLAY_NAMES_MP3_COUNT)
    {
        const char* name = OVERLAY_NAMES_MP3[overlayId];
        return (name && name[0] != '\0') ? name : nullptr;
    }
    return nullptr;
}

bool CDiscord::IsMarioParty1() const
{
    const std::string& name = m_RomName;
    const char* token = "MarioParty";
    size_t pos = name.find(token);
    if (pos == std::string::npos) return false;

    size_t end = pos + strlen(token);
    if (end == name.size()) return true; // exact match at end

    // allow trailing spaces but reject numeric suffixes like "MarioParty2" / "MarioParty3"
    if (std::isdigit(static_cast<unsigned char>(name[end]))) return false;

    for (size_t i = end; i < name.size(); ++i)
    {
        if (name[i] != ' ' && name[i] != '\0') return false;
    }
    return true;
}

bool CDiscord::IsMarioParty2() const
{
    return (strstr(m_RomName.c_str(), "MarioParty2") != nullptr);
}

bool CDiscord::IsMarioParty3() const
{
    return (strstr(m_RomName.c_str(), "MarioParty3") != nullptr);
}

void CDiscord::UpdatePresence()
{
    uint64_t currentTime = time(0);

    // Update at most once per second to avoid rate limiting
    if (!m_NeedsUpdate && (currentTime - m_LastUpdateTime) < UPDATE_THROTTLE_SECONDS)
        return;

    m_IsMP1 = IsMarioParty1();
    m_IsMP2 = IsMarioParty2();
    m_IsMP3 = IsMarioParty3();

    UpdatePresenceInternal();
    m_NeedsUpdate = false;
    m_LastUpdateTime = currentTime;
}

void CDiscord::ForceUpdatePresence()
{
    m_NeedsUpdate = true;
    UpdatePresence();
}

void CDiscord::UpdatePresenceInternal()
{
    DiscordRichPresence discordPresence = {};
    discordPresence.startTimestamp = m_StartTime;

    char details[128] = {};

    // Replace the existing overlayName selection with bounds-checked access to RDRAM
    const char* overlayName = nullptr;

    // Addresses used by the original implementation
    const uint32_t ADDR_MP1 = 0x0F09F4;
    const uint32_t ADDR_MP2 = 0x0FA63C;
    const uint32_t ADDR_MP3 = 0x0CE200;

    // Guard against using g_MMU or RDRAM before they're initialized to avoid crashes at boot
    if (g_MMU && g_MMU->Rdram() && g_MMU->RdramSize() > 0)
    {
        if (m_IsMP1 && g_MMU->RdramSize() > ADDR_MP1)
        {
            overlayName = GetOverlayNameMP1(g_MMU->Rdram()[ADDR_MP1]);
        }
        else if (m_IsMP2 && g_MMU->RdramSize() > ADDR_MP2)
        {
            overlayName = GetOverlayNameMP2(g_MMU->Rdram()[ADDR_MP2]);
        }
        else if (m_IsMP3 && g_MMU->RdramSize() > ADDR_MP3)
        {
            overlayName = GetOverlayNameMP3(g_MMU->Rdram()[ADDR_MP3]);
        }
        else
        {
            overlayName = nullptr;
        }
    }
    else
    {
        // Memory subsystem not ready yet (booting) — don't touch RDRAM
        overlayName = nullptr;
    }

    // Build details: prefer overlayName, then custom title, then ROM name
    if (overlayName && overlayName[0] != '\0')
    {
        snprintf(details, sizeof(details), "%s", overlayName);
    }
    else if (!m_GameTitle.empty())
    {
        snprintf(details, sizeof(details), "%s", m_GameTitle.c_str());
    }
    else if (!m_RomName.empty())
    {
        snprintf(details, sizeof(details), "%s", m_RomName.c_str());
    }
    else
    {
        snprintf(details, sizeof(details), "%s", "Not in-game");
    }

    // Set large/small image keys and state based on identified game
    if (m_IsMP1)
    {
        discordPresence.largeImageKey = "box-mp1";
        discordPresence.largeImageText = "Mario Party 1";

        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = _strdup(state);

        if (m_CurrentGameState >= 0x36 && m_CurrentGameState <= 0x3D)
        {
            switch (m_CurrentGameState)
            {
            case 0x36: discordPresence.smallImageKey = "mp1-dkja"; break;
            case 0x37: discordPresence.smallImageKey = "mp1-pbc"; break;
            case 0x38: discordPresence.smallImageKey = "mp1-yti"; break;
            case 0x39: discordPresence.smallImageKey = "mp1-wbc"; break;
            case 0x3A: discordPresence.smallImageKey = "mp1-ler"; break;
            case 0x3B: discordPresence.smallImageKey = "mp1-mrc"; break;
            case 0x3C: discordPresence.smallImageKey = "mp1-bmm"; break;
            case 0x3D: discordPresence.smallImageKey = "mp1-es"; break;
            default: discordPresence.smallImageKey = ""; break;
            }
        }
        else
        {
            discordPresence.smallImageKey = "";
        }
    }
    else if (m_IsMP2)
    {
        discordPresence.largeImageKey = "box-mp2";
        discordPresence.largeImageText = "Mario Party 2";

        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = _strdup(state);

        if (m_CurrentGameState >= 0x45 && m_CurrentGameState <= 0x4A)
        {
            switch (m_CurrentGameState)
            {
            case 0x45: discordPresence.smallImageKey = "mp2-western"; break;
            case 0x46: discordPresence.smallImageKey = "mp2-pirate"; break;
            case 0x47: discordPresence.smallImageKey = "mp2-horror"; break;
            case 0x48: discordPresence.smallImageKey = "mp2-space"; break;
            case 0x49: discordPresence.smallImageKey = "mp2-mystery"; break;
            case 0x4A: discordPresence.smallImageKey = "mp2-bowser"; break;
            default: discordPresence.smallImageKey = ""; break;
            }
        }
        else
        {
            discordPresence.smallImageKey = "";
        }
    }
    else if (m_IsMP3)
    {
        discordPresence.largeImageKey = "box-mp3";
        discordPresence.largeImageText = "Mario Party 3";

        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = _strdup(state);

        if (m_CurrentGameState >= 0x49 && m_CurrentGameState <= 0x4E)
        {
            switch (m_CurrentGameState)
            {
            case 0x49: discordPresence.smallImageKey = "mp3-chilly"; break;
            case 0x4A: discordPresence.smallImageKey = "mp3-bloober"; break;
            case 0x4B: discordPresence.smallImageKey = "mp3-desert"; break;
            case 0x4C: discordPresence.smallImageKey = "mp3-woody"; break;
            case 0x4D: discordPresence.smallImageKey = "mp3-creepy"; break;
            case 0x4E: discordPresence.smallImageKey = "mp3-waluigi"; break;
            default: discordPresence.smallImageKey = ""; break;
            }
        }
        else
        {
            discordPresence.smallImageKey = "";
        }
    }
    else
    {
        // Non-Mario Party -> use configured large image key if provided, otherwise fallback to project icon
        if (!m_LargeImageKey.empty())
        {
            discordPresence.largeImageKey = m_LargeImageKey.c_str();
            discordPresence.largeImageText = m_GameTitle.empty() ? "Project64" : m_GameTitle.c_str();
        }
        else
        {
            discordPresence.largeImageKey = "project64";
            discordPresence.largeImageText = "Project64";
        }
        discordPresence.smallImageKey = "";
        // If not in-game ensure state is Not in-game
        if (discordPresence.state == nullptr)
        {
            // state will be set to details earlier; keep state empty
        }
    }

    // Finally set details (the string we filled above)
    discordPresence.details = details;

    // party sizes, secrets etc can be set here if needed
    Discord_UpdatePresence(&discordPresence);
    Discord_RunCallbacks();
}

// Static helper implementations to make initialization easy from other modules
void CDiscord::Init()
{
    if (!s_DiscordInstance)
    {
        s_DiscordInstance = new CDiscord();
        s_DiscordInstance->Initialize();

        // If a ROM is already loaded, populate the Discord object so IsMarioParty* works
        if (g_Rom)
        {
            s_DiscordInstance->SetRomName(g_Rom->GetRomName().c_str());
        }

        // Ensure presence shows Project64 icon and "Not in-game" at startup
        s_DiscordInstance->SetGameTitle("Not in-game", "project64");
        s_DiscordInstance->SetPlayerCount(0);

        // Force an initial update
        s_DiscordInstance->ForceUpdatePresence();
    }
}

void CDiscord::Update(bool force /*= true*/)
{
    if (!s_DiscordInstance) return;
    if (force)
    {
        s_DiscordInstance->ForceUpdatePresence();
    }
    else
    {
        s_DiscordInstance->UpdatePresence();
    }
}