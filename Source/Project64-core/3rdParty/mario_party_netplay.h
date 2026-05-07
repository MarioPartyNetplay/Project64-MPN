#ifndef MARIO_PARTY_NETPLAY
#define MARIO_PARTY_NETPLAY
#pragma once

const uint32_t MP1_MEM_BOARD = 0x0ED5C0;
const uint32_t MP1_MEM_CURRENT_TURN = 0x0ED5CA;
const uint32_t MP1_MEM_GAMESTATE = 0x0F09F4;
const uint32_t MP1_MEM_GAMETYPE = 0;
const uint32_t MP1_MEM_TOTAL_TURNS = 0x0ED5C4;

static const char* MP1_BOARDS[11] =
{
    "DK's Jungle Adventure",
    "Peach's Birthday Cake",
    "Yoshi's Tropical Island",
    "Wario's Battle Canyon",
    "Luigi's Engine Room",
    "Mario's Rainbow Castle",
    "Bowser's Magma Mountain",
    "Eternal Star",
    "Tutorial Board",
    "Mini-Game Stadium",
    "Mini-Game Island"
};

static const char* MP1_BOARDS_THUMB[8] =
{
    "mp1-dkja",
    "mp1-pbc",
    "mp1-yti",
    "mp1-wbc",
    "mp1-ler",
    "mp1-mrc",
    "mp1-bmm",
    "mp1-es"
    //"mp1-rules",
    //"mp1-mgs",
    //"mp1-mgi"
};

static const char* MP1_MINIS[1] =
{
    "Nothing"
};

const uint32_t MP2_MEM_BOARD = 0x0F93A8;
const uint32_t MP2_MEM_CURRENT_TURN = 0x0F93B2;
const uint32_t MP2_MEM_GAMESTATE = 0x0FA63C;
const uint32_t MP2_MEM_GAMETYPE = 0;
const uint32_t MP2_MEM_TOTAL_TURNS = 0x0F93AC;

static const char* MP2_BOARDS[9] =
{
    "Western Land",
    "Pirate Land",
    "Horror Land",
    "Space Land",
    "Mystery Land",
    "Bowser Land",
    "Mini-Game Stadium",
    "Mini-Game Coaster",
    "Rules Land"
};

static const char* MP2_BOARDS_THUMB[6] =
{
    "mp2-western",
    "mp2-pirate",
    "mp2-horror",
    "mp2-space",
    "mp2-mystery",
    "mp2-bowser"
    //"mp2-mgs",
    //"mp2-mgc",
    //"mp2-rules"
};

static const char* MP2_MINIS[1] =
{
    "Nothing"
};

const uint32_t MP3_MEM_BOARD = 0x0CD05A;
const uint32_t MP3_MEM_CURRENT_TURN = 0x0CD058;
const uint32_t MP3_MEM_GAMESTATE = 0x0CE200;
const uint32_t MP3_MEM_GAMETYPE = 0x0CD05B;
const uint32_t MP3_MEM_TOTAL_TURNS = 0x0CD059;

static const char* MP3_BOARDS[7] =
{
    "Chilly Waters",
    "Deep Bloober Sea",
    "Spiny Desert",
    "Woody Woods",
    "Creepy Cavern",
    "Waluigi's Island",
    "Explanation Map"
};

static const char* MP3_BOARDS_THUMB[7] =
{
    "mp3-chilly",
    "mp3-bloober",
    "mp3-desert",
    "mp3-woody",
    "mp3-creepy",
    "mp3-waluigi",
    "mp3-rules"
};

static const char* MP3_BOARDS_DUEL[7] =
{
    "Gate Guy",
    "Arrowhead",
    "Pipesqueak",
    "Blowhard",
    "Mr. Mover",
    "Backtrack",
    "Duel Rules Map"
};

static const char* MP3_BOARDS_DUEL_THUMB[7] =
{
    "mp3-gateguy",
    "mp3-arrowhead",
    "mp3-pipesqueak",
    "mp3-blowhard",
    "mp3-mrmover",
    "mp3-backtrack",
    "mp3-rulesduel"
};

static const char* MP3_MINIS[73] =
{
    "",
    "Hand, Line and Sinker",
    "Coconut Conk",
    "Spotlight Swim",
    "Boulder Ball",
    "Crazy Cogs",
    "Hide and Sneak",
    "Ridiculous Relay",
    "Thwomp Pull",
    "River Raiders",
    "Tidal Toss",
    "Eatsa Pizza",
    "Baby Bowser Broadside",
    "Pump, Pump and Away",
    "Hyper Hydrants",
    "Picking Panic",
    "Cosmic Coaster",
    "Puddle Paddle",
    "Etch 'n' Catch",
    "Log Jam",
    "Slot Synch",
    "Treadmill Grill",
    "Toadstool Titan",
    "Aces High",
    "Bounce 'n' Trounce",
    "Ice Rink Risk",
    "Locked Out",
    "Chip Shot Challenge",
    "Parasol Plummet",
    "Messy Memory",
    "Picture Imperfect",
    "Mario's Puzzle Party",
    "The Beat Goes On",
    "M.P.I.Q.",
    "Curtain Call",
    "Water Whirled",
    "Frigid Bridges",
    "Awful Tower",
    "Cheep Cheep Chase",
    "Pipe Cleaners",
    "Snowball Summit",
    "All Fired Up",
    "Stacked Deck",
    "Three Door Monty",
    "Rockin' Raceway",
    "Merry-Go-Chomp",
    "Slap Down",
    "Storm Chasers",
    "Eye Sore",
    "Vine With Me",
    "Popgun Pick-Off",
    "End of the Line",
    "Bowser Toss",
    "Baby Bowser Bonkers",
    "Motor Rooter",
    "Silly Screws",
    "Crowd Cover",
    "Tick Tock Hop",
    "Fowl Play",
    "Winner's Wheel",
    "Hey, Batter, Batter!",
    "Bobbing Bow-loons",
    "Dorrie Dip",
    "Swinging with Sharks",
    "Swing 'n' Swipe",
    "Chance Time",
    "Stardust Battle",
    "Game Guy's Roulette",
    "Game Guy's Lucky 7",
    "Game Guy's Magic Boxes",
    "Game Guy's Sweet Surprise",
    "Dizzy Dinghies",
    "Mario's Puzzle Party Pro"
};

static const char* MP_CHARACTERS[8] =
{
    "Mario",
    "Luigi",
    "Peach",
    "Yoshi",
    "Wario",
    "DK",
    "Waluigi",
    "Daisy"
};

const uint32_t MK64_MEM_MUSIC = 0x0ea15e;
const uint32_t MK64_MEM_SPEED = 0x0dc548;
const uint32_t MK64_MEM_TRACK = 0x18ee08;
const uint32_t MK64_MEM_CUP = 0x18ee0a;

static const char* MK64_SPEEDS[5] =
{
    "50cc",
    "100cc",
    "150cc",
    "EXTRA",
    "Battle"
};

static const char* MK64_TRACKS[20] =
{
    "Luigi Raceway",
    "Moo Moo Farm",
    "Koopa Troopa Beach",
    "Kalimari Desert",
    "Toad's Turnpike",
    "Frappe Snowland",
    "Choco Mountain",
    "Mario Raceway",
    "Wario Stadium",
    "Sherbert Land",
    "Royal Raceway",
    "Bowser's Castle",
    "D.K.'s Jungle Parkway",
    "Yoshi Valley",
    "Banshee Boardwalk",
    "Rainbow Road",
    "Big Donut",
    "Block Fort",
    "Double Deck",
    "Skyscraper"
};

const uint32_t SSB_MEM_PLAYER = 0x0a4d28;
const uint8_t  SSB_MEM_PLAYER_OFFSET = 0x74;
const uint32_t SSB_MEM_STAGE = 0x0a4d0a;

static const char* SSB_CHARACTERS[12] =
{
    "Mario",
    "Fox",
    "DK",
    "Samus",
    "Luigi",
    "Link",
    "Yoshi",
    "Falcon",
    "Kirby",
    "Pikachu",
    "Jigglypuff",
    "Ness"
};

static const char* SSB_STAGES[9] =
{
    "Peach's Castle",
    "Sector Z",
    "Congo Jungle",
    "Planet Zebes",
    "Hyrule Castle",
    "Yoshi's Island",
    "Dream Land",
    "Saffron City",
    "Mushroom Kingdom"
};

static const char* SSB_STAGES_THUMB[9] =
{
    "ssb-mario",
    "ssb-starfox",
    "ssb-donkey",
    "ssb-metroid",
    "ssb-zelda",
    "ssb-yoshi",
    "ssb-kirby",
    "ssb-pokemon",
    "ssb-mario2"
};

#endif