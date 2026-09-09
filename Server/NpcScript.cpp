#include "NpcScript.h"

#include "lua.hpp"

#include <atomic>
#include <cstdio>

namespace {

constexpr char SCRIPT_PATH[] = "npc_ai.lua";
constexpr char DECIDE_FUNC[] = "npc_decide_move";
constexpr char STATS_FUNC[] = "npc_stats";

// A lua_State cannot be shared between threads, and the movement decision
// keeps no per-NPC state, so every worker thread gets its own state instead of
// one state per NPC. That is a handful of states rather than MAX_NPC of them,
// and no lock on the path.
struct ScriptState {
	lua_State* L = nullptr;
	// Set once the script has failed. Without it a broken script would print
	// on every NPC move, which is thousands of lines a second.
	bool broken = false;

	~ScriptState() { if (nullptr != L) lua_close(L); }
};

thread_local ScriptState tls_script;

// Two tables. A rebuild fills the spare one and publishes it with a single
// pointer store, so a worker reading the live table never sees a row that is
// half old and half new.
NPC_STATS g_tables[2][MAX_NPC_LEVEL + 1];
std::atomic<const NPC_STATS*> g_live{ nullptr };
int g_spare = 0;

// Opens a state, runs the script and checks that it defined both entry points.
// Prints the error and returns nullptr on failure.
lua_State* open_script()
{
	lua_State* L = luaL_newstate();
	if (nullptr == L) {
		printf("%s: not enough memory for a lua state\n", SCRIPT_PATH);
		return nullptr;
	}
	luaL_openlibs(L);
	if (LUA_OK != luaL_dofile(L, SCRIPT_PATH)) {
		printf("%s\n", lua_tostring(L, -1));
		lua_close(L);
		return nullptr;
	}
	const char* const entry_points[] = { DECIDE_FUNC, STATS_FUNC };
	for (const char* func : entry_points) {
		if (LUA_TFUNCTION != lua_getglobal(L, func)) {
			printf("%s: function '%s' is missing\n", SCRIPT_PATH, func);
			lua_close(L);
			return nullptr;
		}
		lua_pop(L, 1);
	}
	return L;
}

}	// namespace

bool load_npc_stats()
{
	lua_State* L = open_script();
	if (nullptr == L) return false;

	NPC_STATS* table = g_tables[g_spare];
	for (int level = 1; level <= MAX_NPC_LEVEL; ++level) {
		lua_getglobal(L, STATS_FUNC);
		lua_pushinteger(L, level);
		if (LUA_OK != lua_pcall(L, 1, 4, 0)) {
			printf("%s\n", lua_tostring(L, -1));
			lua_close(L);
			return false;	// whatever table was live stays live
		}
		table[level].max_hp = static_cast<int>(lua_tointeger(L, -4));
		table[level].damage = static_cast<int>(lua_tointeger(L, -3));
		table[level].attack_range = static_cast<int>(lua_tointeger(L, -2));
		table[level].kill_exp = static_cast<int>(lua_tointeger(L, -1));
		lua_pop(L, 4);
	}
	lua_close(L);

	g_live.store(table, std::memory_order_release);
	g_spare = 1 - g_spare;
	return true;
}

const NPC_STATS& npc_stats(int level)
{
	if (level < 1) level = 1;
	if (level > MAX_NPC_LEVEL) level = MAX_NPC_LEVEL;
	return g_live.load(std::memory_order_acquire)[level];
}

bool npc_decide_move(int level, int* dx, int* dy)
{
	ScriptState& script = tls_script;
	if (true == script.broken) return false;
	if (nullptr == script.L) {
		script.L = open_script();
		if (nullptr == script.L) {
			script.broken = true;
			return false;
		}
	}
	lua_State* L = script.L;

	lua_getglobal(L, DECIDE_FUNC);
	lua_pushinteger(L, level);
	// Through pcall so a script error stays inside Lua. Uncaught, it would
	// longjmp straight past this C++ frame.
	if (LUA_OK != lua_pcall(L, 1, 2, 0)) {
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		script.broken = true;
		return false;
	}
	// Anything the script returns that is not a number reads back as 0, which
	// means "stay put".
	*dx = static_cast<int>(lua_tointeger(L, -2));
	*dy = static_cast<int>(lua_tointeger(L, -1));
	lua_pop(L, 2);
	return true;
}
