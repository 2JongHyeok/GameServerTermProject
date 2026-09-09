#pragma once

// NPC behaviour lives in npc_ai.lua, so it can be changed by editing the
// script and restarting the server, with no rebuild.
//
// Stats that depend only on the level are read from the script once per level
// at startup and cached in a table, so play itself costs no Lua calls for
// them. Only the per-turn movement decision goes to the script every time.

// One row of that table.
struct NPC_STATS {
	int max_hp;
	int damage;
	int attack_range;	// hits a player this many tiles away or closer
	int kill_exp;		// what a player gains for killing this NPC
};

// Highest level the table holds. InitializeNPC() tops out at 139 (a boss) and
// an NPC's level never changes after that, so this leaves room to spare.
constexpr int MAX_NPC_LEVEL = 199;

// Builds the stat table from the script. Must be called before the table is
// read, so before InitializeNPC(). A failure prints the reason, leaves any
// table already in use untouched, and returns false.
bool load_npc_stats();

// The cached row for this level. Levels outside the table clamp to its ends.
const NPC_STATS& npc_stats(int level);

// Asks the script how an NPC of this level moves. Writes one step into dx and
// dy; 0, 0 means the NPC stays where it is. Returns false when the script
// failed, leaving dx and dy untouched.
bool npc_decide_move(int level, int* dx, int* dy);
