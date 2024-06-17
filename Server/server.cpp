#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <fstream>
#include <concurrent_priority_queue.h>
#include <string>
#include "protocol.h"

#include "include/lua.hpp"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

using namespace std;
constexpr int map_count = 24;
char my_map[W_HEIGHT][W_WIDTH];
constexpr int VIEW_RANGE = 7;
enum EVENT_TYPE { EV_RANDOM_MOVE };
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

constexpr int BUF_SIZE = 1024;
class map_loader {
public:
	map_loader() {
	}

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
};

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_AI_HELLO};
class OVER_EXP {
public:
	WSAOVERLAPPED over_;
	WSABUF wsabuf_;
	char send_buf_[BUF_SIZE];
	COMP_TYPE comp_type_;
	int ai_target_obj_;
	OVER_EXP()
	{
		wsabuf_.len = BUF_SIZE;
		wsabuf_.buf = send_buf_;
		comp_type_ = OP_RECV;
		ZeroMemory(&over_, sizeof(over_));
	}
	OVER_EXP(char* packet)
	{
		wsabuf_.len = packet[0];
		wsabuf_.buf = send_buf_;
		ZeroMemory(&over_, sizeof(over_));
		comp_type_ = OP_SEND;
		memcpy(send_buf_, packet, packet[0]);
	}
};

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class SESSION {
	OVER_EXP recv_over_;

public:
	mutex s_lock_;
	S_STATE state_;
	atomic_bool	is_active_;		
	int id_;
	SOCKET socket_;
	short	x_, y_;
	char	name_[NAME_SIZE];
	unordered_set <int> view_list_;
	mutex	vl_;
	int		prev_remain_;
	unsigned int		last_move_time_;
	int		visual_;
	int		hp_;
	int		max_hp_;
	int		exp_;
	int		level_;
	mutex	ll_;
	lua_State* L_;

public:
	SESSION()
	{
		id_ = -1;
		socket_ = 0;
		x_ = y_ = 0;
		name_[0] = 0;
		state_ = ST_FREE;
		prev_remain_ = 0;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&recv_over_.over_, 0, sizeof(recv_over_.over_));
		recv_over_.wsabuf_.len = BUF_SIZE - prev_remain_;
		recv_over_.wsabuf_.buf = recv_over_.send_buf_ + prev_remain_;
		WSARecv(socket_, &recv_over_.wsabuf_, 1, 0, &recv_flag,
			&recv_over_.over_, 0);
	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(socket_, &sdata->wsabuf_, 1, 0, 0, &sdata->over_, 0);
	}
	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.visual = 0;
		p.id = id_;
		p.hp = hp_;
		p.max_hp = max_hp_;
		p.exp = exp_;
		p.level = level_;
		p.x = x_;
		p.y = y_;
		do_send(&p);
	}
	void send_move_packet(int c_id);
	void send_add_object_packet(int c_id);
	void send_remove_player_packet(int c_id)
	{
		vl_.lock();
		if (view_list_.count(c_id))
			view_list_.erase(c_id);
		else {
			vl_.unlock();
			return;
		}
		vl_.unlock();
		SC_REMOVE_OBJECT_PACKET p;
		p.size = sizeof(p);
		p.type = SC_REMOVE_OBJECT;
		p.id = c_id;
		do_send(&p);
	}
	void send_chat_packet(int p_id, const char* mess);
};
array<SESSION, MAX_USER+MAX_NPC> clients;
HANDLE h_iocp;
SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

bool is_pc(int object_id)
{
	return object_id < MAX_USER;
}

bool is_npc(int object_id)
{
	return !is_pc(object_id);
}

bool can_see(int from, int to)
{
	if (abs(clients[from].x_ - clients[to].x_) > VIEW_RANGE) return false;
	return abs(clients[from].y_ - clients[to].y_) <= VIEW_RANGE;
}


void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_OBJECT_PACKET p;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.id = c_id;
	p.x = clients[c_id].x_;
	p.y = clients[c_id].y_;
	p.move_time = clients[c_id].last_move_time_;
	do_send(&p);
}

void SESSION::send_add_object_packet(int c_id)
{
	SC_ADD_OBJECT_PACKET add_packet;
	add_packet.size = sizeof(SC_ADD_OBJECT_PACKET);
	add_packet.type = SC_ADD_OBJECT;
	add_packet.id = c_id;
	add_packet.visual = visual_;
	strcpy_s(add_packet.name, clients[c_id].name_);
	add_packet.x = clients[c_id].x_;
	add_packet.y = clients[c_id].y_;
	vl_.lock();
	view_list_.insert(c_id);
	vl_.unlock();
	do_send(&add_packet);
}

void SESSION::send_chat_packet(int p_id, const char* mess)
{
	SC_CHAT_PACKET packet;
	packet.id = p_id;
	packet.size = sizeof(packet);
	packet.type = SC_CHAT;
	strcpy_s(packet.mess, mess);
	do_send(&packet);
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

void WakeUpNPC(int npc_id, int waker)
{
	OVER_EXP* exover = new OVER_EXP;
	exover->comp_type_ = OP_AI_HELLO;
	exover->ai_target_obj_ = waker;
	PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->over_);

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
		strcpy_s(clients[c_id].name_, p->name);
		{
			lock_guard<mutex> ll{ clients[c_id].s_lock_};
			clients[c_id].x_ = rand() % W_WIDTH;
			clients[c_id].y_ = rand() % W_HEIGHT;
			clients[c_id].state_ = ST_INGAME;
		}
		clients[c_id].send_login_info_packet();
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.s_lock_);
				if (ST_INGAME != pl.state_) continue;
			}
			if (pl.id_ == c_id) continue;
			if (false == can_see(c_id, pl.id_))
				continue;
			if (is_pc(pl.id_)) pl.send_add_object_packet(c_id);
			else WakeUpNPC(pl.id_, c_id);
			clients[c_id].send_add_object_packet(pl.id_);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		clients[c_id].last_move_time_ = p->move_time;
		short x = clients[c_id].x_;
		short y = clients[c_id].y_;
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
		clients[c_id].x_ = x;
		clients[c_id].y_ = y;

		unordered_set<int> near_list;
		clients[c_id].vl_.lock();
		unordered_set<int> old_vlist = clients[c_id].view_list_;
		clients[c_id].vl_.unlock();

		for (auto& cl : clients) {
			if (cl.state_ != ST_INGAME) continue;
			if (cl.id_ == c_id) continue;
			if (can_see(c_id, cl.id_))
				near_list.insert(cl.id_);
		}

		clients[c_id].send_move_packet(c_id);

		for (auto& pl : near_list) {
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
			else WakeUpNPC(pl, c_id);

			if (old_vlist.count(pl) == 0)
				clients[c_id].send_add_object_packet(pl);
		}

		for (auto& pl : old_vlist) {
			if (0 == near_list.count(pl)) {
				clients[c_id].send_remove_player_packet(pl);
				if (is_pc(pl))
					clients[pl].send_remove_player_packet(c_id);
			}
		}
		break;
	}
	case CS_WARRIOR_AUTO_ATTACK: {

	}
	}
}

int API_get_x(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = clients[user_id].x_;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = clients[user_id].y_;
	lua_pushnumber(L, y);
	return 1;
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	clients[user_id].send_chat_packet(my_id, mess);
	return 0;
}

void InitializeNPC()
{
	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		clients[i].x_ = rand() % W_WIDTH;
		clients[i].y_ = rand() % W_HEIGHT;
		clients[i].id_ = i;
		sprintf_s(clients[i].name_, "NPC%d", i);
		clients[i].state_ = ST_INGAME;

		auto L = clients[i].L_ = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);
		// lua_pop(L, 1);// eliminate set_uid from stack after call

		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
}

void do_timer()
{
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::system_clock::now();
		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				timer_queue.push(ev);		// 최적화 필요
				// timer_queue에 다시 넣지 않고 처리해야 한다.
				this_thread::sleep_for(1ms);  // 실행시간이 아직 안되었으므로 잠시 대기
				continue;
			}
			switch (ev.event_id) {
			case EV_RANDOM_MOVE:
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type_ = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->over_);
				break;
			}
			continue;		// 즉시 다음 작업 꺼내기
		}
		this_thread::sleep_for(1ms);   // timer_queue가 비어 있으니 잠시 기다렸다가 다시 시작
	}
}

void disconnect(int c_id)
{
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
	std::cout << "랜덤하게 움질일게잇" << std::endl;
	SESSION& npc = clients[npc_id];
	unordered_set<int> old_vl;
	for (auto& obj : clients) {
		if (ST_INGAME != obj.state_) continue;
		if (true == is_npc(obj.id_)) continue;
		if (true == can_see(npc.id_, obj.id_))
			old_vl.insert(obj.id_);
	}

	int x = npc.x_;
	int y = npc.y_;
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
	npc.x_ = x;
	npc.y_ = y;

	unordered_set<int> new_vl;
	for (auto& obj : clients) {
		if (ST_INGAME != obj.state_) continue;
		if (true == is_npc(obj.id_)) continue;
		if (true == can_see(npc.id_, obj.id_))
			new_vl.insert(obj.id_);
	}

	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {
			// 플레이어의 시야에 등장
			clients[pl].send_add_object_packet(npc.id_);
		}
		else {
			// 플레이어가 계속 보고 있음.
			clients[pl].send_move_packet(npc.id_);
		}
	}
	///vvcxxccxvvdsvdvds
	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			clients[pl].vl_.lock();
			if (0 != clients[pl].view_list_.count(npc.id_)) {
				clients[pl].vl_.unlock();
				clients[pl].send_remove_player_packet(npc.id_);
			}
			else {
				clients[pl].vl_.unlock();
			}
		}
	}
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
				clients[client_id].x_ = 0;
				clients[client_id].y_ = 0;
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
		case OP_SEND:
			delete ex_over;
			break;

		case OP_NPC_MOVE: {
			bool keep_alive = false;
			for (int j = 0; j < MAX_USER; ++j) {
				if (clients[j].state_ != ST_INGAME) continue;
				if (can_see(static_cast<int>(key), j)) {
					keep_alive = true;
					break;
				}
			}
			if (true == keep_alive) {
				do_npc_random_move(static_cast<int>(key));
				TIMER_EVENT ev{ key, chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
			else {
				clients[key].is_active_ = false;
			}
			delete ex_over;
			break;
		}
		}
	}
}

int main()
{
	printf("맵 정보 준비중\n");
	map_loader ml;
	ml.Load_Map_info();
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
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	thread timer_thread{ do_timer };
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
