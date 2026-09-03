#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <array>
#include <memory>
#include <string>

using namespace std;
using namespace chrono;

extern HWND		hWnd;

const static int MAX_TEST = 100000;
const static int MAX_CLIENTS = MAX_TEST * 2;
const static int INVALID_ID = -1;
const static int MAX_PACKET_SIZE = 255;
const static int MAX_BUFF_SIZE = 255;

#pragma comment (lib, "ws2_32.lib")

#include "..\..\Server\protocol.h"

HANDLE g_hiocp;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_MOVE };

high_resolution_clock::time_point last_connect_time;

string server_addr;

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
	int event_target;
};

struct CLIENT {
	int id;
	int x;
	int y;
	atomic_bool connected;

	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
	high_resolution_clock::time_point last_move_time;
};

array<int, MAX_CLIENTS> client_map;
array<CLIENT, MAX_CLIENTS> g_clients;
atomic_int num_connections;
atomic_int active_clients;

int			global_delay;				// display only : p95 round-trip time so far during the hold phase
int			target_clients;				// fixed-load mode : connections to reach and then hold
int			hold_seconds;				// how long the target load is held and measured

// Round-trip time histogram with 1 ms buckets from 0 to 1000 ms; the extra last
// bucket collects everything above. Atomic counters keep the worker threads
// lock-free, so recording a sample costs nothing measurable.
constexpr int RTT_BUCKETS = 1001;
atomic<unsigned> rtt_hist[RTT_BUCKETS + 1];
atomic_bool measuring;						// samples count only while the hold phase runs

vector <thread*> worker_threads;
thread test_thread;

float point_cloud[MAX_TEST * 2];

// 나중에 NPC까지 추가 확장 용
struct ALIEN {
	int id;
	int x, y;
	int visible_count;
};

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러" << lpMsgBuf << std::endl;

	MessageBox(hWnd, lpMsgBuf, L"ERROR", 0);
	LocalFree(lpMsgBuf);
	// while (true);
}

void DisconnectClient(int ci)
{
	bool status = true;
	if (true == atomic_compare_exchange_strong(&g_clients[ci].connected, &status, false)) {
		closesocket(g_clients[ci].client_socket);
		active_clients--;
	}
	// cout << "Client [" << ci << "] Disconnected!\n";
}

void SendPacket(int cl, void* packet)
{
	unsigned short* shortPtr = reinterpret_cast<unsigned short*>(packet);
	int psize = *shortPtr;
	int ptype = reinterpret_cast<unsigned char*>(packet)[2];
	OverlappedEx* over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, psize);
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR*>(over->IOCP_buf);
	over->wsabuf.len = psize;
	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0,
		&over->over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			error_display("Error in SendPacket:", err_no);
	}
	// std::cout << "Send Packet [" << ptype << "] To Client : " << cl << std::endl;
}

void ProcessPacket(int ci, unsigned char packet[])
{
	switch (packet[2]) {
	case SC_MOVE_OBJECT: {
		SC_MOVE_OBJECT_PACKET* move_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(packet);
		if (move_packet->id < MAX_CLIENTS) {
			int my_id = client_map[move_packet->id];
			if (-1 != my_id) {
				g_clients[my_id].x = move_packet->x;
				g_clients[my_id].y = move_packet->y;
			}
			if (ci == my_id) {
				if (0 != move_packet->move_time && measuring) {
					// Both timestamps are truncated to 32 bits, so the unsigned
					// subtraction stays correct even across a wraparound.
					unsigned now_ms = static_cast<unsigned>(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
					unsigned d_ms = now_ms - move_packet->move_time;
					int bucket = (d_ms >= RTT_BUCKETS) ? RTT_BUCKETS : static_cast<int>(d_ms);
					rtt_hist[bucket].fetch_add(1);
				}
			}
		}
	}
					   break;
	case SC_ADD_OBJECT: break;
	case SC_REMOVE_OBJECT: break;
	case SC_LOGIN_INFO:
	{
		g_clients[ci].connected = true;
		active_clients++;
		SC_LOGIN_INFO_PACKET* login_packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet);
		int my_id = ci;
		client_map[login_packet->id] = my_id;
		g_clients[my_id].id = login_packet->id;
		g_clients[my_id].x = login_packet->x;
		g_clients[my_id].y = login_packet->y;

		// Avoid the spawn hot spot: teleport to a random tile right after login so clients spread over the map (see CS_TELEPORT in protocol.h)
		CS_TELEPORT_PACKET t_packet;
		t_packet.size = sizeof(t_packet);
		t_packet.type = CS_TELEPORT;
		SendPacket(my_id, &t_packet);
	}
	break;
	default: break;
		while (true);
	}
}

void Worker_Thread()
{
	while (true) {
		DWORD io_size;
		unsigned long long ci;
		OverlappedEx* over;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci,
			reinterpret_cast<LPWSAOVERLAPPED*>(&over), INFINITE);
		// std::cout << "GQCS :";
		int client_id = static_cast<int>(ci);
		if (FALSE == ret) {
			int err_no = WSAGetLastError();
			if (64 == err_no) DisconnectClient(client_id);
			else {
				// error_display("GQCS : ", WSAGetLastError());
				DisconnectClient(client_id);
			}
			if (OP_SEND == over->event_type) delete over;
		}
		if (0 == io_size) {
			DisconnectClient(client_id);
			continue;
		}
		if (OP_RECV == over->event_type) {
			//std::cout << "RECV from Client :" << ci;
			//std::cout << "  IO_SIZE : " << io_size << std::endl;
			unsigned char* buf = g_clients[ci].recv_over.IOCP_buf;
			unsigned psize = g_clients[ci].curr_packet_size;
			unsigned pr_size = g_clients[ci].prev_packet_data;
			while (io_size > 0) {
				if (0 == psize) psize = *reinterpret_cast<short*>(buf);
				if (io_size + pr_size >= psize) {
					// 지금 패킷 완성 가능
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[ci].packet_buf, pr_size);
					memcpy(packet + pr_size, buf, psize - pr_size);
					ProcessPacket(static_cast<int>(ci), packet);
					io_size -= psize - pr_size;
					buf += psize - pr_size;
					psize = 0; pr_size = 0;
				}
				else {
					memcpy(g_clients[ci].packet_buf + pr_size, buf, io_size);
					pr_size += io_size;
					io_size = 0;
				}
			}
			g_clients[ci].curr_packet_size = psize;
			g_clients[ci].prev_packet_data = pr_size;
			DWORD recv_flag = 0;
			int ret = WSARecv(g_clients[ci].client_socket,
				&g_clients[ci].recv_over.wsabuf, 1,
				NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
			if (SOCKET_ERROR == ret) {
				int err_no = WSAGetLastError();
				if (err_no != WSA_IO_PENDING)
				{
					//error_display("RECV ERROR", err_no);
					DisconnectClient(client_id);
				}
			}
		}
		else if (OP_SEND == over->event_type) {
			if (io_size != over->wsabuf.len) {
				// std::cout << "Send Incomplete Error!\n";
				DisconnectClient(client_id);
			}
			delete over;
		}
		else if (OP_DO_MOVE == over->event_type) {
			// Not Implemented Yet
			delete over;
		}
		else {
			std::cout << "Unknown GQCS event!\n";
			while (true);
		}
	}
}

constexpr int ACCEPT_DELY = 20;

int id = 1;
mutex id_lock;

// Fixed-load mode : add one connection every ACCEPT_DELY ms until the target
// is reached, and top it up again if a client drops during the hold phase.
void Adjust_Number_Of_Client()
{
	if (active_clients >= target_clients) return;
	if (num_connections >= MAX_CLIENTS) return;

	auto duration = high_resolution_clock::now() - last_connect_time;
	if (ACCEPT_DELY > duration_cast<milliseconds>(duration).count()) return;

	last_connect_time = high_resolution_clock::now();
	g_clients[num_connections].client_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(PORT_NUM);
	ServerAddr.sin_addr.s_addr = inet_addr(server_addr.c_str());


	int Result = WSAConnect(g_clients[num_connections].client_socket, (sockaddr*)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);
	if (0 != Result) {
		error_display("WSAConnect : ", GetLastError());
	}

	g_clients[num_connections].curr_packet_size = 0;
	g_clients[num_connections].prev_packet_data = 0;
	ZeroMemory(&g_clients[num_connections].recv_over, sizeof(g_clients[num_connections].recv_over));
	g_clients[num_connections].recv_over.event_type = OP_RECV;
	g_clients[num_connections].recv_over.wsabuf.buf =
		reinterpret_cast<CHAR*>(g_clients[num_connections].recv_over.IOCP_buf);
	g_clients[num_connections].recv_over.wsabuf.len = sizeof(g_clients[num_connections].recv_over.IOCP_buf);

	DWORD recv_flag = 0;
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_clients[num_connections].client_socket), g_hiocp, num_connections, 0);

	CS_LOGIN_PACKET l_packet;

	int temp = num_connections;
	sprintf_s(l_packet.name, "%d", temp);
	id_lock.lock();
	l_packet.id = id++;
	id_lock.unlock();
	l_packet.size = sizeof(l_packet);
	l_packet.type = CS_LOGIN;
	SendPacket(num_connections, &l_packet);


	int ret = WSARecv(g_clients[num_connections].client_socket, &g_clients[num_connections].recv_over.wsabuf, 1,
		NULL, &recv_flag, &g_clients[num_connections].recv_over.over, NULL);
	if (SOCKET_ERROR == ret) {
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING)
		{
			error_display("RECV ERROR", err_no);
			goto fail_to_connect;
		}
	}
	num_connections++;
fail_to_connect:
	return;
}

// Copies the histogram into a plain array and returns the sample count.
unsigned long long Snapshot(unsigned long long* hist)
{
	unsigned long long total = 0;
	for (int i = 0; i <= RTT_BUCKETS; ++i) {
		hist[i] = rtt_hist[i];
		total += hist[i];
	}
	return total;
}

// Smallest bucket at which the running count reaches the given fraction of all samples.
int Percentile(const unsigned long long* hist, unsigned long long total, double fraction)
{
	unsigned long long need = static_cast<unsigned long long>(total * fraction);
	if (need < 1) need = 1;
	unsigned long long acc = 0;
	for (int i = 0; i <= RTT_BUCKETS; ++i) {
		acc += hist[i];
		if (acc >= need) return i;
	}
	return RTT_BUCKETS;
}

string BucketLabel(int bucket)
{
	return (bucket >= RTT_BUCKETS) ? string(">1000") : to_string(bucket);
}

void Test_Thread()
{
	bool holding = false;
	high_resolution_clock::time_point hold_start;
	high_resolution_clock::time_point last_display;

	while (true) {
		Adjust_Number_Of_Client();

		if (false == holding && active_clients >= target_clients) {
			// Target reached : drop the ramp-up samples and start the hold phase.
			for (auto& b : rtt_hist) b = 0;
			measuring = true;
			holding = true;
			hold_start = high_resolution_clock::now();
			last_display = hold_start;
		}
		if (holding) {
			auto now = high_resolution_clock::now();
			if (now - last_display >= 60s) {
				// Only feeds the on-screen "Delay" text; nothing is printed here.
				last_display = now;
				unsigned long long hist[RTT_BUCKETS + 1];
				unsigned long long total = Snapshot(hist);
				global_delay = (total > 0) ? Percentile(hist, total, 0.95) : 0;
			}
			if (now - hold_start >= seconds(hold_seconds)) {
				measuring = false;
				unsigned long long hist[RTT_BUCKETS + 1];
				unsigned long long total = Snapshot(hist);
				int max_bucket = 0;
				for (int i = RTT_BUCKETS; i >= 0; --i) {
					if (hist[i] > 0) { max_bucket = i; break; }
				}
				cout << "[final] clients=" << active_clients
					<< " hold=" << hold_seconds << "s"
					<< " samples=" << total
					<< " p50=" << BucketLabel(Percentile(hist, total, 0.50))
					<< " p95=" << BucketLabel(Percentile(hist, total, 0.95))
					<< " p99=" << BucketLabel(Percentile(hist, total, 0.99))
					<< " max=" << BucketLabel(max_bucket)
					<< " over1000=" << hist[RTT_BUCKETS] << endl;
				exit(0);
			}
		}

		for (int i = 0; i < num_connections; ++i) {
			if (false == g_clients[i].connected) continue;
			if (g_clients[i].last_move_time + 1s > high_resolution_clock::now()) continue;
			g_clients[i].last_move_time = high_resolution_clock::now();
			CS_MOVE_PACKET my_packet;
			my_packet.size = sizeof(my_packet);
			my_packet.type = CS_MOVE;
			switch (rand() % 4) {
			case 0: my_packet.direction = 0; break;
			case 1: my_packet.direction = 1; break;
			case 2: my_packet.direction = 2; break;
			case 3: my_packet.direction = 3; break;
			}
			my_packet.move_time = static_cast<unsigned>(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
			SendPacket(i, &my_packet);
		}
	}
}

void InitializeNetwork()
{
	// stdin : <server address> <target connections> <hold seconds>
	cin >> server_addr >> target_clients >> hold_seconds;
	if (!cin || target_clients <= 0 || target_clients > MAX_TEST || hold_seconds <= 0) {
		cout << "usage: <server_ip> <target_clients 1.." << MAX_TEST << "> <hold_seconds>" << endl;
		exit(1);
	}
	for (auto& cl : g_clients) {
		cl.connected = false;
		cl.id = INVALID_ID;
	}

	for (auto& cl : client_map) cl = -1;
	num_connections = 0;
	last_connect_time = high_resolution_clock::now();

	WSADATA	wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);

	for (int i = 0; i < 6; ++i)
		worker_threads.push_back(new std::thread{ Worker_Thread });

	test_thread = thread{ Test_Thread };
}

void ShutdownNetwork()
{
	test_thread.join();
	for (auto pth : worker_threads) {
		pth->join();
		delete pth;
	}
}

void Do_Network()
{
	return;
}

void GetPointCloud(int* size, float** points)
{
	int index = 0;
	for (int i = 0; i < num_connections; ++i)
		if (true == g_clients[i].connected) {
			point_cloud[index * 2] = static_cast<float>(g_clients[i].x);
			point_cloud[index * 2 + 1] = static_cast<float>(g_clients[i].y);
			index++;
		}

	*size = index;
	*points = point_cloud;
}

