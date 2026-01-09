
#include <stdint.h>
#include <string>

class CDiscord
{
public:
    CDiscord();
    ~CDiscord();

    void Initialize();
    void Shutdown();
    void UpdatePresence();
    void ForceUpdatePresence();
    void SetPlayerCount(uint8_t currentPlayers);
    void SetGameTitle(const char* title, const char* largeImageKey = nullptr);
    void SetRomName(const char* romName);

    static void Init();
    static void Update(bool force = true);

private:
    void UpdatePresenceInternal();
    const char* GetOverlayNameMP1(uint8_t overlayId) const;
    const char* GetOverlayNameMP2(uint8_t overlayId) const;
    const char* GetOverlayNameMP3(uint8_t overlayId) const;
    bool IsMarioParty1() const;
    bool IsMarioParty2() const;
    bool IsMarioParty3() const;
    // Discord state
    uint64_t m_StartTime;
    uint8_t m_CurrentPlayers;
    uint8_t m_MaxPlayers;
    uint8_t m_CurrentGameState;
    uint8_t m_CurrentTurn;
    uint8_t m_CurrentTotalTurns;
    std::string m_GameTitle;
    std::string m_LargeImageKey;
    std::string m_RomName;

    // Track when to update
    bool m_NeedsUpdate;
    uint64_t m_LastUpdateTime;
    bool m_IsMP1;
    bool m_IsMP2;
    bool m_IsMP3;
};