#include "StdAfx.h"
#include "Discord.h"
#include "MarioPartyOverlays.h"
#include <time.h>
#include <string.h>

// Discord SDK headers (statically linked)
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

CDiscord::CDiscord() :
    m_StartTime(time(0)),
    m_CurrentPlayers(0),
    m_MaxPlayers(4),
    m_CurrentGameState(0),
    m_CurrentBoardId(0),
    m_CurrentGameType(0),
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

void CDiscord::SetGameState(uint8_t gameState, uint8_t boardId, uint8_t gameType,
                           uint8_t currentTurn, uint8_t totalTurns, uint8_t maxPlayers)
{
    // Check if any state changed
    if (m_CurrentGameState != gameState ||
        m_CurrentBoardId != boardId ||
        m_CurrentGameType != gameType ||
        m_CurrentTurn != currentTurn ||
        m_CurrentTotalTurns != totalTurns ||
        m_MaxPlayers != maxPlayers)
    {
        // Reset start time when state changes
        m_StartTime = time(0);
        m_NeedsUpdate = true;
    }

    m_CurrentGameState = gameState;
    m_CurrentBoardId = boardId;
    m_CurrentGameType = gameType;
    m_CurrentTurn = currentTurn;
    m_CurrentTotalTurns = totalTurns;
    m_MaxPlayers = maxPlayers;
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
    return (strstr(m_RomName.c_str(), "MarioParty3") != nullptr);
}

bool CDiscord::IsMarioParty2() const
{
    return (strstr(m_RomName.c_str(), "MarioParty3") != nullptr);
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

void CDiscord::UpdatePresenceInternal()
{
    DiscordRichPresence discordPresence = {};
    discordPresence.startTimestamp = m_StartTime;

    char details[128] = {};

    const char* overlayName = nullptr;
    if (m_IsMP1)
    {
        overlayName = GetOverlayNameMP1(m_CurrentGameState);
    }
    else if (m_IsMP2)
    {
        overlayName = GetOverlayNameMP2(m_CurrentGameState);
    }
    else if (m_IsMP3)
    {
        overlayName = GetOverlayNameMP3(m_CurrentGameState);
    }
    else
    {
        overlayName = "";
    }

    // Set large image key based on game/board
    if (m_IsMP1)
    {
        discordPresence.largeImageKey = "box-mp1";
        discordPresence.largeImageText = "Mario Party 1";
        discordPresence.details = overlayName;

        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = state;

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
        discordPresence.details = overlayName;
        
        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = state;

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
        discordPresence.details = overlayName;

        char state[128] = {};
        snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        discordPresence.state = state;

        if (m_CurrentGameState >= 0x49 && m_CurrentGameState <= 0x4E)
        {
            switch (m_CurrentGameState)
            {
                case 0x49: discordPresence.largeImageKey = "mp3-chilly"; break;
                case 0x4A: discordPresence.largeImageKey = "mp3-bloober"; break;
                case 0x4B: discordPresence.largeImageKey = "mp3-desert"; break;
                case 0x4C: discordPresence.largeImageKey = "mp3-woody"; break;
                case 0x4D: discordPresence.largeImageKey = "mp3-creepy"; break;
                case 0x4E: discordPresence.largeImageKey = "mp3-waluigi"; break;
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
        // Non mario Party
    }

    discordPresence.details = details;

    Discord_UpdatePresence(&discordPresence);
    Discord_RunCallbacks();
}
