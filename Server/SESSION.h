#pragma once

#include "OVER_EXP.h"
#include "protocol.h"
#include<mutex>
#include<set>
#include<unordered_set>
#include<array>

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };

class SESSION {
	OVER_EXP recv_over_;

public:
	std::mutex s_lock_;
	S_STATE state_;
	std::atomic_bool	is_active_;
	int id_;
	SOCKET socket_;
	GameObject pos_;
	char	name_[NAME_SIZE];
	std::mutex	vl_;
	int		prev_remain_;
	unsigned int		last_move_time_;
	int		character_;
	int		hp_;
	int		max_hp_;
	int		exp_;
	int		max_exp_;
	int		level_;
	std::mutex	ll_;
	int		prev_sector_;
	int		now_sector_;
	std::set<int> near_sectors;
	bool	in_use_;
	int		dir_;
	int		damage_;
	int		armor_;
	std::unordered_set<int> view_list_;
	std::unordered_set<int> old_view_list_;


public:
	SESSION()
	{
		id_ = -1;
		socket_ = 0;
		x_ = y_ = 0;
		name_[0] = 0;
		state_ = ST_FREE;
		prev_remain_ = 0;
		in_use_ = true;
		level_ = 1;
		max_hp_ = 100;
		hp_ = max_hp_;
		max_exp_ = 100;
	}

	~SESSION() {}

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

std::array<SESSION, MAX_USER + MAX_NPC> clients;