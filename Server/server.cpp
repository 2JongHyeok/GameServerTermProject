#include <iostream>
#include <array>
#include <thread>
#include <vector>
#include <set>
#include <mutex>
#include <concurrent_unordered_set.h>
#include <unordered_set>
#include <concurrent_unordered_map.h>
#include <fstream>
#include <concurrent_priority_queue.h>
#include <string>
#include "protocol.h"
#include "Grid.h"
#include "SESSION.h"


using namespace std;
constexpr int map_count = 24;

char my_map[W_HEIGHT][W_WIDTH];
Grid Sector;

constexpr int VIEW_RANGE = 7;
constexpr int NPC_VIEW_RANGE = 5;

constexpr int SECTOR_SIZE = 20;

enum EVENT_TYPE { EV_RANDOM_MOVE, EV_RESURRECTION, EV_ATTACK};
struct TIMER_EVENT {
	int obj_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;
	int target_id;
	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};
concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;

void Load_Map_info() {
	std::ifstream in{ "mymap.txt" };
	// 파일 열기 실패 여부 확인
	if (!in) {
		std::cerr << "파일을 열지 못했습니다: test1.txt\n";
		if (in.fail()) {
			std::cerr << "오류: 파일을 찾을 수 없습니다 또는 파일을 열 수 없습니다.\n";
		}
		else {
			std::cerr << "알 수 없는 오류가 발생했습니다.\n";
		}

	}
	int index_x = 0;
	int index_y = 0;
	char temp;
	string sinteger = "";
	int count = 0;
	while (in >> temp) {
		if (temp == ',') {
			int integer = stoi(sinteger);
			my_map[index_y][index_x++] = integer;
			sinteger = "";
			if (index_x == 2000) {
				index_y++;
				index_x = 0;
			}
			continue;
		}
		sinteger += temp;
	}
}

HANDLE h_iocp;
SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

enum ATTACK_TYPE { AUTO, SKILL1, SKILL2 };

bool in_my_attack_range(int target, int player, C_CLASS c_class, ATTACK_TYPE attack_type, int dir) {
	int t_x = clients[target].pos_.x_.load();
	int t_y = clients[target].pos_.y_.load();
	int p_x = clients[player].pos_.x_.load();
	int p_y = clients[player].pos_.y_.load();
	if (c_class == WARRIOR) {
		switch (attack_type)
		{
		case AUTO:
			if (dir == 0) {
				if (t_x != p_x) return false;
				if (-1 <= t_y - p_y && t_y - p_y <= 0) {
					return true;
				}
				return false;
			}
			else if (dir == 1) {
				if (t_x != p_x) return false;
				if (0 <= t_y - p_y && t_y - p_y <= 1) {
					return true;
				}
				return false;
			}
			else if (dir == 2) {
				if (t_y != p_y) return false;
				if (-1 <= t_x - p_x && t_x - p_x <= 0) {
					return true;
				}
				return false;
			}
			else if (dir == 3) {
				if (t_y != p_y) return false;
				if (0 <= t_x- p_x && t_x - p_x <= 1) {
					return true;
				}
				return false;
			}
			return false;
			break;
		case SKILL1:
			break;
		case SKILL2:
			break;
		default:
			break;
		}
	}
	else if (c_class == MAGE) {
		switch (attack_type)
		{
		case AUTO:
			if (dir == 0) {
				if (t_x != p_x) return false;
				if (-3 <= t_y - p_y && t_y - p_y <= 0) {
					return true;
				}
				return false;
			}
			else if (dir == 1) {
				if (t_x != p_x) return false;
				if (0 <= t_y - p_y && t_y - p_y <= 3) {
					return true;
				}
				return false;
			}
			else if (dir == 2) {
				if (t_y != p_y) return false;
				if (-3 <= t_x - p_x && t_x - p_x <= 0) {
					return true;
				}
				return false;
			}
			else if (dir == 3) {
				if (t_y != p_y) return false;
				if (0 <= t_x - p_x && t_x - p_x <= 3) {
					return true;
				}
				return false;
			}
			return false;
			break;
		case SKILL1:
			break;
		case SKILL2:
			break;
		default:
			break;
		}
	}
	else if (c_class == PRIST) {
		switch (attack_type)
		{
		case AUTO:
			if (abs(t_x - p_x) > 5) return false;
			return abs(t_y - p_y) <= 5;
			break;
		case SKILL1:
			break;
		case SKILL2:
			break;
		default:
			break;
		}
	}
}
bool is_pc(int object_id)
{
	return object_id < MAX_USER;
}

bool is_npc(int object_id)
{
	return !is_pc(object_id);
}

bool can_see(int a, int b)
{
	if (std::abs(clients[a].pos_.x_.load() - clients[b].pos_.x_.load()) > VIEW_RANGE) return false;
	return std::abs(clients[a].pos_.y_.load() - clients[b].pos_.y_.load()) <= VIEW_RANGE;
}

bool can_npc_see(int a, int b)
{
	if (std::abs(clients[a].pos_.x_.load() - clients[b].pos_.x_.load()) > NPC_VIEW_RANGE) return false;
	return std::abs(clients[a].pos_.y_.load() - clients[b].pos_.y_.load()) <= NPC_VIEW_RANGE;
}
bool can_see_d(int from, int to, int* distance)
{
	if (abs(clients[from].pos_.x_ - clients[to].pos_.x_) > NPC_VIEW_RANGE) return false;
	if (abs(clients[from].pos_.y_ - clients[to].pos_.y_) <= NPC_VIEW_RANGE) {
		*distance = abs(clients[from].pos_.x_ - clients[to].pos_.x_);
		if (*distance < abs(clients[from].pos_.y_ - clients[to].pos_.y_))
			*distance = abs(clients[from].pos_.y_ - clients[to].pos_.y_);
		return true;
	}
	return false;
	
}


int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i].s_lock_ };
		if (clients[i].state_ == ST_FREE)
			return i;
	}
	return -1;
}

void WakeUpNPC(int npc_id)
{
	if (clients[npc_id].is_active_) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&clients[npc_id].is_active_, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}

void process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		clients[c_id].in_use_ = true;
		strcpy_s(clients[c_id].name_, p->name);
		{
			lock_guard<mutex> ll{ clients[c_id].s_lock_};
			clients[c_id].pos_.x_.store(rand() % W_WIDTH);
			clients[c_id].pos_.y_.store(rand() % W_HEIGHT);
			clients[c_id].respawn_x_ = clients[c_id].pos_.x_;
			clients[c_id].respawn_y_ = clients[c_id].pos_.y_;
			clients[c_id].pos_.id_ = c_id;
			
			clients[c_id].level_ = 1;
			int character = rand() % 3;
			if (character == 0) {
				clients[c_id].character_ = WARRIOR;
				clients[c_id].damage_ = WARRIOR_STAT_ATK * clients[c_id].level_;
				clients[c_id].armor_ = WARRIOR_STAT_ARMOR * clients[c_id].level_;
			}
			else if (character == 1) {
				clients[c_id].character_ = MAGE;
				clients[c_id].damage_ = MAGE_STAT_ATK * clients[c_id].level_;
				clients[c_id].armor_ = MAGE_STAT_ARMOR * clients[c_id].level_;
			}
			else if (character == 2) {
				clients[c_id].character_ = PRIST;
				clients[c_id].damage_ = PRIST_STAT_ATK * clients[c_id].level_;
				clients[c_id].armor_ = PRIST_STAT_ARMOR * clients[c_id].level_;
			}
			clients[c_id].state_ = ST_INGAME;
		}
		clients[c_id].send_login_info_packet();
		Sector.addObject(clients[c_id].pos_);
		
		unordered_set <int> vl = Sector.getNearbyObjects(clients[c_id].pos_);
		unordered_set <int> new_vl;
		for (auto& pl : vl) {
			if (clients[pl].in_use_ == false) continue;
			if (pl == c_id) continue;
			if (clients[pl].state_ != ST_INGAME) continue;
			if (false == can_see(pl, c_id)) continue;
			new_vl.insert(pl);
		}

		clients[c_id].vl_.lock();
		unordered_set<int> old_vlist = clients[c_id].view_list_;
		clients[c_id].vl_.unlock();

		for (auto pl : new_vl) {
			auto& cpl = clients[pl];
			if (is_pc(pl)) {
				cpl.vl_.lock();
				if (clients[pl].view_list_.count(c_id)) {
					cpl.vl_.unlock();
					clients[pl].send_move_packet(c_id);
				}
				else {
					cpl.vl_.unlock();
					clients[pl].send_add_object_packet(c_id);
				}
			}

			if (old_vlist.count(pl) == 0) {
				clients[c_id].send_add_object_packet(pl);
				if (false == is_pc(pl))
					WakeUpNPC(pl);
			}
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		clients[c_id].last_move_time_ = p->move_time;
		clients[c_id].dir_ = p->direction;
		int x = clients[c_id].pos_.x_.load();
		int y = clients[c_id].pos_.y_.load();

		switch (p->direction) {
		case 0: {
			if (y <= 0) break;
			if (my_map[y - 1][x] != 50) break;
			y--;
			break; 
		}
		case 1: { 
			if (y >= W_HEIGHT - 1) break;
			if (my_map[y + 1][x] != 50) break;
			y++;
			break;
		}
		case 2: {
			if (x <= 0) break;
			if (my_map[y  ][x-1] != 50) break;
			x--;
			break;
		}
		case 3: {
			if (x >= W_WIDTH - 1) break; 
			if (my_map[y][x+1] != 50) break;
			x++;
			break; 
		}
		}
		Sector.updateObject(clients[c_id].pos_, x, y);
		clients[c_id].pos_.x_.store(x);
		clients[c_id].pos_.y_.store(y);

		
		clients[c_id].vl_.lock();
		unordered_set<int> old_vl = clients[c_id].view_list_;
		clients[c_id].vl_.unlock();
		unordered_set <int> vl = Sector.getNearbyObjects(clients[c_id].pos_);
		unordered_set <int> new_vl;
		for (int pl : vl) {
			if (clients[pl].in_use_ == false) continue;
			if (pl == c_id) continue;
			if (clients[pl].state_ != ST_INGAME) continue;
			if (false == can_see(pl, c_id)) continue;
			new_vl.insert(pl);
		}

		clients[c_id].send_move_packet(c_id);

		for (int  pl : new_vl) {
			auto& cpl = clients[pl];
			if (is_pc(pl)) {
				cpl.vl_.lock();
				if (clients[pl].view_list_.count(c_id)) {
					cpl.vl_.unlock();
					clients[pl].send_move_packet(c_id);
				}
				else {
					cpl.vl_.unlock();
					clients[pl].send_add_object_packet(c_id);
				}
			}
			else WakeUpNPC(pl);

			if (old_vl.count(pl) == 0)
				clients[c_id].send_add_object_packet(pl);
		}

		for (auto& pl : old_vl) {
			if (0 == new_vl.count(pl)) {
				clients[c_id].send_remove_player_packet(pl);
				if (is_pc(pl))
					clients[pl].send_remove_player_packet(c_id);
			}
		}
		clients[c_id].vl_.lock();
		clients[c_id].view_list_ = new_vl;
		clients[c_id].vl_.unlock();
		break;
	}
	case CS_ATTACK: {
		C_CLASS c_class = clients[c_id].character_;
		int dir = clients[c_id].dir_;

		unordered_set <int> vl = Sector.getNearbyObjects(clients[c_id].pos_);
		unordered_set <int> new_vl;

		if (c_class == PRIST) { // 클래스 확인
			for (int pl : vl) {
				if (clients[pl].in_use_ == false) continue;
				if (pl == c_id) continue;
				if (clients[pl].state_ != ST_INGAME) continue;
				if (false == can_see(pl, c_id)) continue;
				if (!in_my_attack_range(pl, c_id, c_class, AUTO, dir)) continue;
				new_vl.insert(pl);
			}
			for (int pl : new_vl) {
				if (is_npc(pl)) {	// NPC이면 공격

					bool kill = false;
					while (true) {	// CAS를 사용해 체력 바꿔주기
						int hp = clients[pl].hp_.load();
						if (hp <= 0) break;
						int now_hp = hp - clients[c_id].damage_;
						if (std::atomic_compare_exchange_strong(&clients[pl].hp_, &hp, now_hp)) {
							if (hp > 0 && now_hp <= 0) {
								clients[pl].in_use_ = false;
								kill = true;
							}
							break;
						}
					}
					if (kill) {
						clients[c_id].exp_ += clients[pl].level_ * 50;
						TIMER_EVENT ev{ pl, chrono::system_clock::now() + 30s, EV_RESURRECTION, 0 };
						timer_queue.push(ev);
						while (true) {	// 레벨업 할 경우 스텟 바꿔주기
							if (clients[c_id].exp_ >= clients[c_id].max_exp_) {
								clients[c_id].exp_ -= clients[c_id].max_exp_;
								clients[c_id].max_exp_ *= 2;
								clients[c_id].level_ += 1;
								clients[c_id].update_status();
							}
							else
								break;
						}
						clients[c_id].send_stat_change_packet(c_id, clients[c_id].max_hp_, clients[c_id].hp_, clients[c_id].level_, clients[c_id].exp_);
						// 죽은 NPC 시야에서 삭제

						unordered_set <int> vl = Sector.getNearbyObjects(clients[pl].pos_);
						unordered_set <int> new_vl;
						for (int pll : vl) {
							if (clients[pll].in_use_ == false) continue;
							if (pll == pl) continue;
							if (clients[pll].state_ != ST_INGAME) continue;
							if (false == can_see(pll, pl)) continue;
							new_vl.insert(pll);
						}
						for (int pll : new_vl) {
							clients[pll].send_remove_player_packet(pl);
						}
					}
					else {
						unordered_set <int> vl = Sector.getNearbyObjects(clients[pl].pos_);
						unordered_set <int> new_vl;
						for (int pll : vl) {
							if (clients[pll].in_use_ == false) continue;
							if (pll == pl) continue;
							if (clients[pll].state_ != ST_INGAME) continue;
							if (false == can_see(pll, pl)) continue;
							new_vl.insert(pll);
						}
						for (int pll : new_vl) {
							clients[pll].send_stat_change_packet(pl, clients[pl].max_hp_,
								clients[pl].hp_, clients[pl].level_, clients[pl].exp_);
						}

					}
				}
				else {	// 플레이어면 힐
					if (clients[pl].hp_ < 100) {
						while (true) {
							int hp = clients[pl].hp_.load();
							int now_hp = hp + clients[c_id].damage_;
							if (now_hp >= 100)
								now_hp = 100;
							if (std::atomic_compare_exchange_strong(&clients[pl].hp_, &hp, now_hp)) {
								break;
							}
						}
						clients[pl].send_stat_change_packet(pl, clients[pl].max_hp_, clients[pl].hp_, clients[pl].level_, clients[pl].exp_);
					}
				}
			}
		}
		else {
			for (int pl : vl) {
				if (clients[pl].in_use_ == false) continue;
				if (pl == c_id) continue;
				if (clients[pl].state_ != ST_INGAME) continue;
				if (false == can_see(pl, c_id)) continue;
				if (!is_npc(pl))continue;
				if (!in_my_attack_range(pl, c_id, c_class, AUTO, dir)) continue;
				new_vl.insert(pl);
			}
			for (int pl : new_vl) {
				bool kill = false;
				while (true) {	// CAS를 사용해 체력 바꿔주기

					int hp = clients[pl].hp_.load();
					if (hp <= 0) break;
					int now_hp = hp - clients[c_id].damage_;
					if (std::atomic_compare_exchange_strong(&clients[pl].hp_, &hp, now_hp)) {
						if (hp > 0 && now_hp <= 0) {
							clients[pl].in_use_ = false;
							kill = true;
						}
						break;
					}
				}
				if (kill) {
					clients[c_id].exp_ += clients[pl].level_ * 50;
					TIMER_EVENT ev{ pl, chrono::system_clock::now() + 30s, EV_RESURRECTION, 0 };
					timer_queue.push(ev);
					while (true) {	// 레벨업 할 경우 스텟 바꿔주기
						if (clients[c_id].exp_ >= clients[c_id].max_exp_) {
							clients[c_id].exp_ -= clients[c_id].max_exp_;
							clients[c_id].max_exp_ *= 2;
							clients[c_id].level_ += 1;
							clients[c_id].update_status();
						}
						else
							break;
					}
					clients[c_id].send_stat_change_packet(c_id, clients[c_id].max_hp_, clients[c_id].hp_, clients[c_id].level_, clients[c_id].exp_);

					// 죽은 NPC 시야에서 삭제

					unordered_set <int> vl = Sector.getNearbyObjects(clients[pl].pos_);
					unordered_set <int> new_vl;
					for (int pll : vl) {
						if (clients[pll].in_use_ == false) continue;
						if (pll == pl) continue;
						if (clients[pll].state_ != ST_INGAME) continue;
						if (false == can_see(pll, pl)) continue;
						new_vl.insert(pll);
					}
					for (int pll : new_vl) {
						clients[pll].send_remove_player_packet(pl);
					}
				}
				else {
					unordered_set <int> vl = Sector.getNearbyObjects(clients[pl].pos_);
					unordered_set <int> new_vl;
					for (int pll : vl) {
						if (clients[pll].in_use_ == false) continue;
						if (pll == pl) continue;
						if (clients[pll].state_ != ST_INGAME) continue;
						if (false == can_see(pll, pl)) continue;
						new_vl.insert(pll);
					}
					for (int pll : new_vl) {
						clients[pll].send_stat_change_packet(pl, clients[pl].max_hp_,
							clients[pl].hp_, clients[pl].level_, clients[pl].exp_);
					}
					
				}
			}
		}
		break;
	}
	case CS_TELEPORT: {
		int x, y;
		while (true) {
			x = rand() % W_WIDTH;
			y = rand() % W_HEIGHT;
			if (my_map[y][x] == 50)
				break;
		}
		Sector.updateObject(clients[c_id].pos_, x, y);
		clients[c_id].pos_.x_.store(x);
		clients[c_id].pos_.y_.store(y);
		clients[c_id].respawn_x_ = x;
		clients[c_id].respawn_y_ = y;
		clients[c_id].send_move_packet(c_id);

		clients[c_id].vl_.lock();
		unordered_set<int> old_vl = clients[c_id].view_list_;
		clients[c_id].vl_.unlock();

		unordered_set <int> vl = Sector.getNearbyObjects(clients[c_id].pos_);
		unordered_set <int> new_vl;
		for (int pl : vl) {
			if (clients[pl].in_use_ == false) continue;
			if (pl == c_id) continue;
			if (clients[pl].state_ != ST_INGAME) continue;
			if (false == can_see(pl, c_id)) continue;
			new_vl.insert(pl);
		}

		for (int pl : new_vl) {
			if (clients[pl].in_use_ == false) continue;
			if (clients[pl].id_ == c_id) continue;
			if (false == can_see(c_id, pl))
				continue;
			if (is_pc(pl)) clients[pl].send_add_object_packet(c_id);
			else WakeUpNPC(pl);
			clients[c_id].send_add_object_packet(pl);
		}

		for (int pl : old_vl) {
			if (0 == new_vl.count(pl)) {
				clients[c_id].send_remove_player_packet(pl);
				if (is_pc(pl))
					clients[pl].send_remove_player_packet(c_id);
			}
		}

		clients[c_id].vl_.lock();
		clients[c_id].view_list_ = new_vl;
		clients[c_id].vl_.unlock();
	}
	}
}

void InitializeNPC()
{
	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		int x, y;
		while (true) {
			x = rand() %W_WIDTH; 
			y = rand() % W_HEIGHT;
			if (my_map[y][x] == 50)
				break;
		}
		clients[i].in_use_ = true;
		clients[i].pos_.x_ = x;
		clients[i].pos_.y_ = y;
		clients[i].pos_.id_ = i;
		Sector.addObject(clients[i].pos_);
		clients[i].id_ = i;
		clients[i].state_ = ST_INGAME;
		if (rand() % 20 == 1) {
			sprintf_s(clients[i].name_, "BOSS");
			int level = rand() % 40 + 100;
			clients[i].level_ = level;
			clients[i].damage_ = level * 2;
			clients[i].hp_ = level * 50;
		}
		else {
			sprintf_s(clients[i].name_, "NPC%d", i);
			int level = rand() % 40 + 1;
			if(level <= 10)
				sprintf_s(clients[i].name_, "slime%d", i);
			else if(level <= 20)
				sprintf_s(clients[i].name_, "KingKong%d", i);
			else if (level <= 30)
				sprintf_s(clients[i].name_, "KillerRobot%d", i);
			else if (level <= 40)
				sprintf_s(clients[i].name_, "Dog%d", i);
			else {
				sprintf_s(clients[i].name_, "Cat%d", i);
			}
			clients[i].level_ = level;
			clients[i].damage_ = level * 2;
			clients[i].hp_ = level * 50;
		}
	}
}

void do_timer()
{
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::system_clock::now();
		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				timer_queue.push(ev);
				if (ev.event_id == EV_RANDOM_MOVE)
					this_thread::sleep_for(1s);
				continue;
			}
			switch (ev.event_id) {
			case EV_RANDOM_MOVE: {
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type_ = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->over_);
				break;
			}
			case EV_RESURRECTION: {
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type_ = OP_NPC_RESURRECTION;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->over_);
				break;
			}
			}
			continue;		// 즉시 다음 작업 꺼내기
		}
		this_thread::sleep_for(1ms);   // timer_queue가 비어 있으니 잠시 기다렸다가 다시 시작
	}
}

void disconnect(int c_id)
{
	clients[c_id].in_use_ = false;
	clients[c_id].vl_.lock();
	unordered_set <int> vl = clients[c_id].view_list_;
	clients[c_id].vl_.unlock();
	for (auto& p_id :vl) {
		if (is_npc(p_id)) continue;
		auto& pl = clients[p_id];
		{
			lock_guard<mutex> ll(pl.s_lock_);
			if (ST_INGAME != pl.state_) continue;
		}
		if (pl.id_ == c_id) continue;
		pl.send_remove_player_packet(c_id);
	}
	closesocket(clients[c_id].socket_);

	lock_guard<mutex> ll(clients[c_id].s_lock_);
	clients[c_id].state_ = ST_FREE;
}
void do_npc_random_move(int npc_id)
{
	if (clients[npc_id].in_use_ == false)
		return;
	int distance;
	int min_distance = 5;
	int nearest = -1;

	unordered_set<int> vl = Sector.getNearbyObjects(clients[npc_id].pos_);

	for (int obj : vl) {
		if (clients[obj].in_use_ == false) continue;
		if (ST_INGAME != clients[obj].state_) continue;
		if (true == is_npc(obj)) continue;
		if (true == can_see_d(clients[npc_id].id_, obj, &distance)) {
			if (distance < min_distance) {
				min_distance = distance;
				nearest = obj;
			}
		}
	}
	if (min_distance <= 3) {
		clients[nearest].vl_.lock();
		int damage = clients[npc_id].damage_ - clients[nearest].armor_;
		if (damage < 0) damage = 0;
		if (clients[npc_id].level_ <= 10) {
			if (min_distance <= 3) {
				clients[nearest].hp_ -= damage;
			}
		}
		else if (clients[npc_id].level_ <= 20) {
			if (min_distance <= 2) {
				clients[nearest].hp_ -= damage;
			}
		}
		else if (clients[npc_id].level_ <= 30) {
			if (min_distance <= 1) {
				clients[nearest].hp_ -= damage;
			}
		}
		else if (clients[npc_id].level_ <= 40) {
			if (min_distance <= 1) {
				clients[nearest].hp_ -= damage;
			}
		}
		else {
			if (min_distance <= 1) {
				clients[nearest].hp_ -= damage;
			}
		}
		if (clients[nearest].hp_ > 0) {
			clients[nearest].vl_.unlock();
			clients[nearest].send_get_damage_packet(npc_id, damage, clients[nearest].hp_);
		}
		else {
			clients[nearest].hp_ = 100;
			Sector.updateObject(clients[nearest].pos_, clients[nearest].respawn_x_, clients[nearest].respawn_y_);
			clients[nearest].pos_.x_.store(clients[nearest].respawn_x_);
			clients[nearest].pos_.y_.store(clients[nearest].respawn_y_);
			clients[nearest].exp_ /= 2;
			clients[nearest].send_stat_change_packet(nearest, clients[nearest].max_hp_,
				clients[nearest].hp_, clients[nearest].level_, clients[nearest].exp_);
			clients[nearest].vl_.unlock();
			clients[nearest].vl_.lock();
			unordered_set<int> old_vl = clients[nearest].view_list_;
			clients[nearest].vl_.unlock();
			unordered_set <int> vl = Sector.getNearbyObjects(clients[nearest].pos_);
			unordered_set <int> new_vl;
			for (int pl : vl) {
				if (clients[pl].in_use_ == false) continue;
				if (pl == nearest) continue;
				if (clients[pl].state_ != ST_INGAME) continue;
				if (false == can_see(pl, nearest)) continue;
				new_vl.insert(pl);
			}

			clients[nearest].send_move_packet(nearest);

			for (int pl : new_vl) {
				auto& cpl = clients[pl];
				if (is_pc(pl)) {
					cpl.vl_.lock();
					if (clients[pl].view_list_.count(nearest)) {
						cpl.vl_.unlock();
						clients[pl].send_move_packet(nearest);
					}
					else {
						cpl.vl_.unlock();
						clients[pl].send_add_object_packet(nearest);
					}
				}
				else WakeUpNPC(pl);

				if (old_vl.count(pl) == 0)
					clients[nearest].send_add_object_packet(pl);
			}

			for (auto& pl : old_vl) {
				if (0 == new_vl.count(pl)) {
					clients[nearest].send_remove_player_packet(pl);
					if (is_pc(pl))
						clients[pl].send_remove_player_packet(nearest);
				}
			}
			clients[nearest].vl_.lock();
			clients[nearest].view_list_ = new_vl;
			clients[nearest].vl_.unlock();

		}
		return;
	}
	if (clients[npc_id].level_ <= 10) return;

	int x = clients[npc_id].pos_.x_;
	int y = clients[npc_id].pos_.y_;
	
	switch (rand() % 4) {
	case 0: {
		if (y <= 0) break;
		if (my_map[y - 1][x] != 50) break;
		y--;
		break;
	}
	case 1: {
		if (y >= W_HEIGHT - 1) break;
		if (my_map[y + 1][x] != 50) break;
		y++;
		break;
	}
	case 2: {
		if (x <= 0) break;
		if (my_map[y][x - 1] != 50) break;
		x--;
		break;
	}
	case 3: {
		if (x >= W_WIDTH - 1) break;
		if (my_map[y][x + 1] != 50) break;
		x++;
		break;
	}
	}
		Sector.updateObject(clients[npc_id].pos_, x, y);
		clients[npc_id].pos_.x_.store(x);
		clients[npc_id].pos_.y_.store(y);

	vl = Sector.getNearbyObjects(clients[npc_id].pos_);
	unordered_set<int> new_vl;

	clients[npc_id].vl_.lock();
	unordered_set<int>old_vl = clients[npc_id].view_list_;
	clients[npc_id].vl_.unlock();

	for (int obj : vl) {
		if (clients[obj].in_use_ == false) continue;
		if (ST_INGAME != clients[obj].state_) continue;
		if (is_npc(obj)) continue;
		if (can_npc_see(clients[npc_id].id_, obj))
			new_vl.insert(obj);
	}

	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {
			// 플레이어의 시야에 등장
			clients[pl].send_add_object_packet(clients[npc_id].id_);
		}
		else {
			// 플레이어가 계속 보고 있음.
			clients[pl].send_move_packet(clients[npc_id].id_);
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			clients[pl].vl_.lock();
			if (0 != clients[pl].view_list_.count(clients[npc_id].id_)) {
				clients[pl].vl_.unlock();
				clients[pl].send_remove_player_packet(clients[npc_id].id_);
			}
			else {
				clients[pl].vl_.unlock();
			}
		}
	}

	clients[npc_id].vl_.lock();
	clients[npc_id].view_list_ = new_vl;
	clients[npc_id].vl_.unlock();
}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->comp_type_ == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->comp_type_ == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->comp_type_ == OP_RECV) || (ex_over->comp_type_ == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->comp_type_ == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->comp_type_) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(clients[client_id].s_lock_);
					clients[client_id].state_ = ST_ALLOC;
				}
				clients[client_id].pos_.x_ = 0;
				clients[client_id].pos_.y_ = 0;
				clients[client_id].pos_.id_ = client_id;
				clients[client_id].id_ = client_id;
				clients[client_id].name_[0] = 0;
				clients[client_id].prev_remain_ = 0;
				clients[client_id].socket_ = g_c_socket;

				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over.over_, sizeof(g_a_over.over_));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buf_, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over_);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + clients[key].prev_remain_;
			char* p = ex_over->send_buf_;
			while (remain_data > 0) {
				int packet_size = *reinterpret_cast<unsigned short*>(p);
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key].prev_remain_ = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->send_buf_, p, remain_data);
			}
			clients[key].do_recv();
			break;
		}
		case OP_SEND: {
			delete ex_over;
			break;
		}
		case OP_NPC_MOVE: {
			bool keep_alive = false;

			unordered_set<int> vl = Sector.getNearbyObjects(clients[key].pos_);

			for (int pl : vl) {
				if (clients[pl].in_use_ == false) continue;
				if (pl == key) continue;
				if (clients[pl].state_ != ST_INGAME) continue;
				if (false == can_npc_see(pl, key)) continue;
				if (!is_pc(pl)) continue;
				keep_alive = true;
				break;
			}

			if (keep_alive) {
				do_npc_random_move(static_cast<int>(key));
				if (clients[static_cast<int>(key)].in_use_ == false)
					break;
				TIMER_EVENT ev{ key, chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
			else {
				clients[key].is_active_ = false;
			}
			delete ex_over;
			break;
		}
		case OP_NPC_RESURRECTION: {
			clients[key].in_use_ = true;
			clients[key].level_*= 2;
			clients[key].hp_ = clients[key].level_*50;
			clients[key].damage_ = clients[key].level_*2;

			unordered_set<int> vl = Sector.getNearbyObjects(clients[key].pos_);
			unordered_set<int> new_vl;
			
			for (int pl : vl) {
				if (clients[pl].in_use_ == false) continue;
				if (pl == key) continue;
				if (clients[pl].state_ != ST_INGAME) continue;
				if (false == can_see(pl, key)) continue;
				if (!is_pc(pl)) continue;
				new_vl.insert(pl);
			}
			
			for (auto& pl : new_vl) {
				if (clients[pl].in_use_ == false) continue;
				clients[pl].send_add_object_packet(key);
			}
		}
		}
	}
}

int main()
{
	printf("맵 정보 준비중\n");
	Load_Map_info();
	printf("맵 정보 준비완료\n");

	printf("몬스터 정보 준비중\n");
	InitializeNPC();
	printf("몬스터 정보 준비완료\n");

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.comp_type_ = OP_ACCEPT;
	printf("서버 시작\n");
	AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buf_, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over_);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency()-1;
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	thread timer_thread{ do_timer };
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
