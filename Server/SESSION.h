#pragma once

#include<mutex>
#include<set>
#include<unordered_set>
#include<array>
#include<atomic>
#include<cstdint>
#include "OVER_EXP.h"
#include "GameObject.h"
#include "protocol.h"

// ST_CLOSING : the session is being torn down but still has outstanding
// operations, so the slot must not be handed to a new client yet.
// Values 0..3 fit in the low 2 bits of SESSION::life_; do not renumber.
enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME, ST_CLOSING };
enum C_CLASS{WARRIOR, MAGE, PRIST};

class SESSION {
	OVER_EXP recv_over_;

public:
	// Slot lifecycle packed into one atomic word so the state and the
	// outstanding-operation count change together in a single CAS, with no
	// per-send lock. bits 0-1 : S_STATE, bits 2-31 : outstanding op count.
	std::atomic<uint32_t> life_;
	static constexpr uint32_t STATE_MASK = 0x3u;
	static constexpr uint32_t OP_ONE = 0x4u;   // one outstanding operation (count starts at bit 2)
	std::atomic_bool	is_active_;
	int id_;
	SOCKET socket_;
	GameObject pos_;
	int respawn_x_;
	int respawn_y_;
	char	name_[NAME_SIZE];
	std::mutex	vl_;
	int		prev_remain_;
	unsigned int		last_move_time_;
	C_CLASS		character_;
	std::atomic<int>		hp_;
	std::mutex	hp_l_;
	int		max_hp_;
	int		exp_;
	int		max_exp_;
	int		level_;
	std::mutex	ll_;
	std::atomic<bool>	in_use_;
	int		dir_;
	int		damage_;
	int		armor_;
	int		login_id_;
	std::unordered_set<int> view_list_;
public:
	SESSION()
	{
		id_ = -1;
		socket_ = 0;
		name_[0] = 0;
		life_ = ST_FREE;
		prev_remain_ = 0;
		in_use_ = true;
		level_ = 1;
		max_hp_ = 100;
		hp_ = max_hp_;
		max_exp_ = 100;
	}

	~SESSION() {}

	// Current state, read without a lock.
	S_STATE state() const {
		return static_cast<S_STATE>(life_.load() & STATE_MASK);
	}

	// Take a slot reference if it is still live. do_recv accepts ALLOC or
	// INGAME, do_send accepts INGAME only. Returns false untouched otherwise.
	bool try_acquire_op(bool ingame_only) {
		uint32_t cur = life_.load();
		for (;;) {
			S_STATE s = static_cast<S_STATE>(cur & STATE_MASK);
			bool ok = (s == ST_INGAME) || (!ingame_only && s == ST_ALLOC);
			if (!ok) return false;
			if (life_.compare_exchange_weak(cur, cur + OP_ONE)) return true;
		}
	}

	// Add a reference when a live one is already held (login queues a DB
	// request while its recv completion is still in flight), so no state check.
	void acquire_op() { life_.fetch_add(OP_ONE); }

	// Retire one reference; the last one of a closing session frees the slot.
	void release_op() {
		uint32_t cur = life_.load();
		for (;;) {
			uint32_t next = cur - OP_ONE;
			if ((next >> 2) == 0 && (cur & STATE_MASK) == ST_CLOSING) next = ST_FREE;
			if (life_.compare_exchange_weak(cur, next)) return;
		}
	}

	// FREE -> ALLOC for a fresh slot; a FREE slot always has a zero count.
	bool try_alloc() {
		uint32_t expected = ST_FREE;
		return life_.compare_exchange_strong(expected, ST_ALLOC);
	}

	// ALLOC -> INGAME once login finishes; false if the client already dropped.
	bool set_ingame() {
		uint32_t cur = life_.load();
		for (;;) {
			if ((cur & STATE_MASK) != ST_ALLOC) return false;
			if (life_.compare_exchange_weak(cur, (cur & ~STATE_MASK) | ST_INGAME)) return true;
		}
	}

	// ALLOC/INGAME -> CLOSING, count preserved. Returns the previous state, or
	// ST_CLOSING/ST_FREE if another completion already closed the slot.
	S_STATE try_close() {
		uint32_t cur = life_.load();
		for (;;) {
			S_STATE s = static_cast<S_STATE>(cur & STATE_MASK);
			if (s != ST_ALLOC && s != ST_INGAME) return s;
			if (life_.compare_exchange_weak(cur, (cur & ~STATE_MASK) | ST_CLOSING)) return s;
		}
	}

	void do_recv();

	void do_send(void* packet);
	void send_login_info_packet();
	void send_get_damage_packet(int c_id, int damage, int hp);
	void send_move_packet(int c_id);
	void send_add_object_packet(int c_id);
	void send_remove_player_packet(int c_id);
	void send_chat_packet(int p_id, const char* mess);
	void send_stat_change_packet(int c_id, int max_hp, int hp, int level, int exp);
	void update_status();
};

extern std::array<SESSION, MAX_USER + MAX_NPC> clients;

// Defined in server.cpp.
void disconnect(int c_id);
// Retires one outstanding operation and frees the slot once a closing session
// has none left.
void on_op_done(int c_id);
