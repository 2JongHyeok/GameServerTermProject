#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <fstream>
using namespace std;

//#pragma comment (lib, "opengl32.lib")
//#pragma comment (lib, "winmm.lib")
//#pragma comment (lib, "ws2_32.lib")

#include "..\Server\protocol.h"

#define BUF_SIZE 1024

sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;
constexpr int map_count = 13;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;

char my_map[W_HEIGHT][W_WIDTH];

class OBJECT {
private:
	bool m_showing_;
	sf::Sprite m_sprite_;

	sf::Text m_name_;
public:
	int m_x_, m_y_;
	char name_[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing_ = false;
		m_sprite_.setTexture(t);
		m_sprite_.setTextureRect(sf::IntRect(x, y, x2, y2));
	}
	OBJECT() {
		m_showing_ = false;
	}
	void show()
	{
		m_showing_ = true;
	}
	void hide()
	{
		m_showing_ = false;
	}

	void a_move(int x, int y) {
		m_sprite_.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite_);
	}

	void move(int x, int y) {
		m_x_ = x;
		m_y_ = y;
	}
	void draw() {
		if (false == m_showing_) return;
		float rx = (m_x_ - g_left_x) * 65.0f + 1;
		float ry = (m_y_ - g_top_y) * 65.0f + 1;
		m_sprite_.setPosition(rx, ry);
		g_window->draw(m_sprite_);
		auto size = m_name_.getGlobalBounds();
		m_name_.setPosition(rx + 32 - size.width / 2, ry - 10);
		g_window->draw(m_name_);
	}
	void set_name(const char str[]) {
		m_name_.setFont(g_font);
		m_name_.setString(str);
		m_name_.setFillColor(sf::Color(255, 255, 0));
		m_name_.setStyle(sf::Text::Bold);
	}
};
class map_loader {
public:
	int mapnums[13] = { 2,3,4,15,16,17, 28,30, 40,41,42,43,50 };

	sf::Texture* maptexture[13];
	OBJECT tiles[13];

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
		int index_x=0;
		int index_y=0;
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


		for (int i = 0; i < map_count; ++i) {
			std::string TexAddr = "Resource/Map1/BG_";
			TexAddr += to_string(mapnums[i]);
			TexAddr += ".gif";
			maptexture[i] = new sf::Texture;
			maptexture[i]->loadFromFile(TexAddr);
			tiles[i] = OBJECT{ *maptexture[i], 5, 5, TILE_WIDTH, TILE_WIDTH };
		}

	}

	void Draw() {
		for (int i = 0; i < SCREEN_WIDTH; ++i)
			for (int j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int tile_x = i + g_left_x;
				int tile_y = j + g_top_y;

				if ((tile_x < 0) || (tile_y < 0)) continue;
				if ((tile_x > W_WIDTH) || (tile_y > W_HEIGHT)) continue;

				int idx = tile_x + tile_y * W_WIDTH;
				if (idx < 0 || idx >W_WIDTH * W_HEIGHT) continue;

				switch (layer[idx]) {
				case 87:
					tiles[0].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[0].a_draw();
					break;
				case 90:
					tiles[1].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[1].a_draw();
					break;
				case 93:
					tiles[2].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[2].a_draw();
					break;
				case 96:
					tiles[3].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[3].a_draw();
					break;
				case 99:
					tiles[4].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[4].a_draw();
					break;
				case 230:
					tiles[5].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[5].a_draw();
					break;
				case 231:
					tiles[6].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[6].a_draw();
					break;
				case 286:
					tiles[7].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[7].a_draw();
					break;
				case 301:
					tiles[8].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[8].a_draw();
					break;
				case 304:
					tiles[9].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[9].a_draw();
					break;
				case 307:
					tiles[10].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[10].a_draw();
					break;
				case 311:
					tiles[11].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[11].a_draw();
					break;
				case 313:
					tiles[12].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[12].a_draw();
					break;
				case 332:
					tiles[13].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[13].a_draw();
					break;
				}
			}
	}

};
OBJECT avatar;
unordered_map <int, OBJECT> players;

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.move(4, 4);
}

void client_finish()
{
	players.clear();
	delete board;
	delete pieces;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[2])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.m_x_ = packet->x;
		avatar.m_y_ = packet->y;
		g_left_x = packet->x - 4;
		g_top_y = packet->y - 4;
		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 4;
			g_top_y = my_packet->y - 4;
			avatar.show();
		}
		else if (id < MAX_USER) {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		else {
			//npc[id - NPC_START].x = my_packet->x;
			//npc[id - NPC_START].y = my_packet->y;
			//npc[id - NPC_START].attr |= BOB_ATTR_VISIBLE;
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 4;
			g_top_y = my_packet->y - 4;
		}
		else if (other_id < MAX_USER) {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		else {
			//npc[other_id - NPC_START].x = my_packet->x;
			//npc[other_id - NPC_START].y = my_packet->y;
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else if (other_id < MAX_USER) {
			players.erase(other_id);
		}
		else {
			//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = *reinterpret_cast<unsigned short*>(ptr);
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (0 == (tile_x / 3 + tile_y / 3) % 2) {
				white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				black_tile.a_draw();
			}
		}
	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x_, avatar.m_y_);
	text.setString(buf);
	g_window->draw(text);
}

void send_packet(void* packet)
{
	size_t sent = 0;
	s_socket.send(packet, *reinterpret_cast<unsigned short*>(packet), sent);
}

int main()
{
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	string player_name{ "P" };
	player_name += to_string(GetCurrentProcessId());

	strcpy_s(p.name, player_name.c_str());
	send_packet(&p);
	avatar.set_name(p.name);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				int direction = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Up:
					direction = 0;
					break;
				case sf::Keyboard::Down:
					direction = 1;
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (-1 != direction) {
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					send_packet(&p);
				}

			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}