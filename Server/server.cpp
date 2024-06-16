#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

constexpr int BUF_SIZE = 1024;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };
class OVER_EXP {
public:
	WSAOVERLAPPED over_;
	WSABUF wsabuf_;
	char send_buf_[BUF_SIZE];
	COMP_TYPE comp_type_;
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
	int id_;
	SOCKET socket_;
	short	x_, y_;
	char	name_[NAME_SIZE];
	int		prev_remain_;
	unsigned int		last_move_time_;
	int		visual_;
	int		hp_;
	int		max_hp_;
	int		exp_;
	int		level_;
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
		SC_REMOVE_OBJECT_PACKET p;
		p.size = sizeof(p);
		p.type = SC_REMOVE_OBJECT;
		p.id = c_id;
		do_send(&p);
	}
};

array<SESSION, MAX_USER> clients;

SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

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
	do_send(&add_packet);
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

void process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id].name_, p->name);
		clients[c_id].send_login_info_packet();
		{
			lock_guard<mutex> ll{ clients[c_id].s_lock_ };
			clients[c_id].state_ = ST_INGAME;
		}
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.s_lock_);
				if (ST_INGAME != pl.state_) continue;
			}
			if (pl.id_ == c_id) continue;
			pl.send_add_object_packet(c_id);
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
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		clients[c_id].x_ = x;
		clients[c_id].y_ = y;

		for (auto& cl : clients) {
			if (cl.state_ != ST_INGAME) continue;
			cl.send_move_packet(c_id);

		}
	}
	}
}

void disconnect(int c_id)
{
	for (auto& pl : clients) {
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
		}
	}
}

int main()
{
	HANDLE h_iocp;

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
	AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buf_, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over_);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
