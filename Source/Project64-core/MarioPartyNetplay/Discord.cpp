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

// Discord event handlers
static void handleDiscordReady(const DiscordUser* request) { (void)request; printf("\nDiscord: ready\n"); }
static void handleDiscordDisconnected(int errcode, const char* message) { printf("\nDiscord: disconnected (%d: %s)\n", errcode, message); }
static void handleDiscordError(int errcode, const char* message) { printf("\nDiscord: error (%d: %s)\n", errcode, message); }
static void handleDiscordJoin(const char* secret) { printf("\nDiscord: join (%s)\n", secret); }
static void handleDiscordSpectate(const char* secret) { printf("\nDiscord: spectate (%s)\n", secret); }

// Singleton instance
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

CDiscord::~CDiscord() { Shutdown(); }

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

void CDiscord::Shutdown() { Discord_Shutdown(); }

void CDiscord::SetPlayerCount(uint8_t currentPlayers)
{
    m_CurrentPlayers = currentPlayers;
    m_NeedsUpdate = true;
}

void CDiscord::SetGameTitle(const char* title, const char* largeImageKey)
{
    m_GameTitle = title ? title : "";
    m_LargeImageKey = largeImageKey ? largeImageKey : "";
    m_NeedsUpdate = true;
}

void CDiscord::SetRomName(const char* romName)
{
    m_RomName = romName ? romName : "";
    m_NeedsUpdate = true;
}

const char* CDiscord::GetOverlayNameMP1(uint8_t overlayId) const {
    return (overlayId < OVERLAY_NAMES_MP1_COUNT) ? OVERLAY_NAMES_MP1[overlayId] : nullptr;
}
const char* CDiscord::GetOverlayNameMP2(uint8_t overlayId) const {
    return (overlayId < OVERLAY_NAMES_MP2_COUNT) ? OVERLAY_NAMES_MP2[overlayId] : nullptr;
}
const char* CDiscord::GetOverlayNameMP3(uint8_t overlayId) const {
    return (overlayId < OVERLAY_NAMES_MP3_COUNT) ? OVERLAY_NAMES_MP3[overlayId] : nullptr;
}

bool CDiscord::IsMarioParty1() const {
    return (m_RomName.find("MarioParty") != std::string::npos &&
        m_RomName.find("MarioParty2") == std::string::npos &&
        m_RomName.find("MarioParty3") == std::string::npos);
}
bool CDiscord::IsMarioParty2() const { return (m_RomName.find("MarioParty2") != std::string::npos); }
bool CDiscord::IsMarioParty3() const { return (m_RomName.find("MarioParty3") != std::string::npos); }

void CDiscord::UpdatePresence()
{
    uint64_t currentTime = time(0);
    if (!m_NeedsUpdate && (currentTime - m_LastUpdateTime) < 2)
        return;

    if (g_Rom) {
        std::string currentRom = g_Rom->GetRomName();
        if (m_RomName != currentRom) {
            SetRomName(currentRom.c_str());
            m_GameTitle = "";
        }
    }

    m_IsMP1 = IsMarioParty1();
    m_IsMP2 = IsMarioParty2();
    m_IsMP3 = IsMarioParty3();

    if (g_MMU && g_MMU->Rdram() && g_MMU->RdramSize() > 0)
    {
        uint8_t* rdram = g_MMU->Rdram();
        uint8_t newState = 0;

        if (m_IsMP1)      newState = rdram[0x0F09F4];
        else if (m_IsMP2) newState = rdram[0x0FA63C];
        else if (m_IsMP3) newState = rdram[0x0CE200];

        const char* potentialName = nullptr;
        if (m_IsMP1)      potentialName = GetOverlayNameMP1(newState);
        else if (m_IsMP2) potentialName = GetOverlayNameMP2(newState);
        else if (m_IsMP3) potentialName = GetOverlayNameMP3(newState);

        if (potentialName != nullptr && potentialName[0] != '\0')
        {
            m_CurrentGameState = newState;
        }

        // --- TURN COUNT LOGIC (Dummy Addresses for USA Versions) ---
        // Note: These often only populate once a board is actually loaded
        if (m_IsMP1) {
            m_CurrentTurn = rdram[0x0ED5CA];
            m_CurrentTotalTurns = rdram[0x0ED5C4];
        }
        else if (m_IsMP2) {
            m_CurrentTurn = rdram[0x0F93B2];
            m_CurrentTotalTurns = rdram[0x0F93AC];
        }
        else if (m_IsMP3) {
            m_CurrentTurn = rdram[0x0CD058];
            m_CurrentTotalTurns = rdram[0x0CD059];
        }
    }

    UpdatePresenceInternal();
    m_NeedsUpdate = false;
    m_LastUpdateTime = currentTime;
}

void CDiscord::UpdatePresenceInternal()
{
    static char details[128] = {};
    static char state[128] = {};
    static char largeImageText[128] = {};

    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    discordPresence.startTimestamp = m_StartTime;

    const char* overlayName = nullptr;

    if (m_IsMP1)      overlayName = GetOverlayNameMP1(m_CurrentGameState);
    else if (m_IsMP2) overlayName = GetOverlayNameMP2(m_CurrentGameState);
    else if (m_IsMP3) overlayName = GetOverlayNameMP3(m_CurrentGameState);

    if (overlayName && overlayName[0] != '\0') {
        snprintf(details, sizeof(details), "%s", overlayName);
    }
    else if (!m_RomName.empty()) {
        snprintf(details, sizeof(details), "%s", m_RomName.c_str());
    }
    else {
        snprintf(details, sizeof(details), "Project64");
    }

    discordPresence.details = details;

    if (m_IsMP1 || m_IsMP2 || m_IsMP3) {
        // Large Image Key (Box Art)
        discordPresence.largeImageKey = m_IsMP1 ? "box-mp1" : (m_IsMP2 ? "box-mp2" : "box-mp3");

        snprintf(largeImageText, sizeof(largeImageText), "Mario Party %s", m_IsMP1 ? "1" : (m_IsMP2 ? "2" : "3"));
        discordPresence.largeImageText = largeImageText;

        // State Line (Players & Turns)
        if (m_CurrentTotalTurns > 0 && m_CurrentTotalTurns != 127) {
            snprintf(state, sizeof(state), "Players: %d/%d | Turn: %d/%d",
                m_CurrentPlayers, m_MaxPlayers, m_CurrentTurn, m_CurrentTotalTurns);
        }
        else {
            snprintf(state, sizeof(state), "Players: %d/%d", m_CurrentPlayers, m_MaxPlayers);
        }
        discordPresence.state = state;

        // Small Image Key (Board Icons)
        if (m_IsMP1 && m_CurrentGameState >= 0x36 && m_CurrentGameState <= 0x3D) {
            const char* mp1Keys[] = { "mp1-dkja", "mp1-pbc", "mp1-yti", "mp1-wbc", "mp1-ler", "mp1-mrc", "mp1-bmm", "mp1-es" };
            discordPresence.smallImageKey = mp1Keys[m_CurrentGameState - 0x36];
        }
        else if (m_IsMP2 && m_CurrentGameState >= 0x45 && m_CurrentGameState <= 0x4A) {
            const char* mp2Keys[] = { "mp2-western", "mp2-pirate", "mp2-horror", "mp2-space", "mp2-mystery", "mp2-bowser" };
            discordPresence.smallImageKey = mp2Keys[m_CurrentGameState - 0x45];
        }
        else if (m_IsMP3 && m_CurrentGameState >= 0x48 && m_CurrentGameState <= 0x4D) {
            const char* mp3Keys[] = { "mp3-chilly", "mp3-bloober", "mp3-desert", "mp3-woody", "mp3-creepy", "mp3-waluigi" };
            discordPresence.smallImageKey = mp3Keys[m_CurrentGameState - 0x48];
        }
    }
    else {
        discordPresence.largeImageKey = "project64";
        discordPresence.largeImageText = "Project64";
    }

    Discord_UpdatePresence(&discordPresence);
    Discord_RunCallbacks();
}

void CDiscord::ForceUpdatePresence() { m_NeedsUpdate = true; UpdatePresence(); }

void CDiscord::Init()
{
    if (s_DiscordInstance) return;
    s_DiscordInstance = new CDiscord();
    s_DiscordInstance->Initialize();

    if (g_Rom) {
        s_DiscordInstance->SetRomName(g_Rom->GetRomName().c_str());
    }
    else {
        s_DiscordInstance->SetGameTitle("Waiting for game...", "project64");
    }
    s_DiscordInstance->ForceUpdatePresence();
}

void CDiscord::Update(bool force)
{
    if (!s_DiscordInstance) return;
    force ? s_DiscordInstance->ForceUpdatePresence() : s_DiscordInstance->UpdatePresence();
}