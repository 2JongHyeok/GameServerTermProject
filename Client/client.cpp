#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <fstream>
#include <stdio.h>
using namespace std;

//#pragma comment (lib, "opengl32.lib")
//#pragma comment (lib, "winmm.lib")
//#pragma comment (lib, "ws2_32.lib")

#include "..\Server\protocol.h"

#define BUF_SIZE 1024

sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 20;
constexpr auto SCREEN_HEIGHT = 20;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;

char my_map[W_HEIGHT][W_WIDTH];
sf::Texture* my_charactor;

class OBJECT {
private:
	bool m_showing_;
	sf::Sprite m_sprite_;

	sf::Text m_name_;
	sf::Text m_hp_;
public:
	int m_x_, m_y_;
	int hp_;
	int exp_;
	int max_exp_;
	int level_;
	char name_[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing_ = false;
		m_sprite_.setTexture(t);
		m_sprite_.setTextureRect(sf::IntRect(x, y, x2, y2));
		hp_ = 100;
		max_exp_ = 100;
	}
	OBJECT() {
		m_showing_ = false;
	}
	void change_texture(int visual){
		my_charactor = new sf::Texture;
		
		if (visual == 0) {
			my_charactor->loadFromFile("warrior.png");
			m_sprite_.setTexture(*my_charactor);
			m_sprite_.setTextureRect(sf::IntRect(0, 0, 134, 134));
			m_sprite_.setScale(0.5, 0.5);
		}
		else if (visual == 1) {
			my_charactor->loadFromFile("mage.png");
			m_sprite_.setTexture(*my_charactor);
			m_sprite_.setTextureRect(sf::IntRect(0, 0, 130, 130));
			m_sprite_.setScale(0.5, 0.5);

		}
		else if (visual == 2) {
			my_charactor->loadFromFile("prist.png");
			m_sprite_.setTexture(*my_charactor);
			m_sprite_.setTextureRect(sf::IntRect(0, 0, 136, 136));
			m_sprite_.setScale(0.5, 0.5);

		}
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

		size = m_hp_.getGlobalBounds();
		m_hp_.setPosition(rx + 32 - size.width / 2, ry - 30);
		g_window->draw(m_hp_);
	}
	void set_name(const char str[]) {
		m_name_.setFont(g_font);
		m_name_.setString(str);
		m_name_.setFillColor(sf::Color(255, 255, 0));
		m_name_.setStyle(sf::Text::Bold);
	}

	void set_hp(int hp) {
		hp_ = hp;
		char str[5];
		sprintf_s(str, "%d", hp);
		m_hp_.setFont(g_font);
		m_hp_.setString(str);
		m_hp_.setFillColor(sf::Color(255, 255, 0));
		m_hp_.setStyle(sf::Text::Bold);
	}
};
OBJECT avatar;
unordered_map <int, OBJECT> players;
class map_loader {
public:
	int mapnums[MAP_COUNT] = {1,2,3,4,8,9,10,15,16,17,21,22,23, 28,30,34,35,36,38,41,42,43,50,51 };

	sf::Texture* maptexture[MAP_COUNT];
	OBJECT tiles[MAP_COUNT];

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


		for (int i = 0; i < MAP_COUNT; ++i) {
			std::string TexAddr = "images/BG_";
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

				switch (my_map[j + g_top_y][i + g_left_x]) {
				case 1:
					tiles[0].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[0].a_draw();
					break;
				case 2:
					tiles[1].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[1].a_draw();
					break;
				case 3:
					tiles[2].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[2].a_draw();
					break;
				case 4:
					tiles[3].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[3].a_draw();
					break;
				case 8:
					tiles[4].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[4].a_draw();
					break;
				case 9:
					tiles[5].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[5].a_draw();
					break;
				case 10:
					tiles[6].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[6].a_draw();
					break;
				case 15:
					tiles[7].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[7].a_draw();
					break;
				case 16:
					tiles[8].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[8].a_draw();
					break;
				case 17:
					tiles[9].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[9].a_draw();
					break;
				case 21:
					tiles[10].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[10].a_draw();
					break;

				case 22:
					tiles[11].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[11].a_draw();
					break;
				case 23:
					tiles[12].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[12].a_draw();
					break;
				case 28:
					tiles[13].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[13].a_draw();
					break;
				case 30:
					tiles[14].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[14].a_draw();
					break;
				case 34:
					tiles[15].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[15].a_draw();
					break;
				case 35:
					tiles[16].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[16].a_draw();
					break;
				case 36:
					tiles[17].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[17].a_draw();
					break;
				case 38:
					tiles[18].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[18].a_draw();
					break;
				case 41:
					tiles[19].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[19].a_draw();
					break;
				case 42:
					tiles[20].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[20].a_draw();
					break;
				case 43:
					tiles[21].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[21].a_draw();
					break;
				case 50:
					tiles[22].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[22].a_draw();
					break;
				case 51:
					tiles[23].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[23].a_draw();
					break;
				default:
					tiles[23].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					tiles[23].a_draw();
				}
				
			}
	}

};


map_loader ml;
sf::Texture* pieces;

void client_initialize()
{
	pieces = new sf::Texture;
	pieces->loadFromFile("chess2.png");
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.move(4, 4);
}

void client_finish()
{
	players.clear();
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
		avatar.hp_ = packet->hp;
		avatar.level_ = packet->level;
		avatar.exp_ = packet->exp;
		g_left_x = packet->x - 10;
		g_top_y = packet->y - 10;
		avatar.change_texture(packet->visual);
		avatar.set_hp(avatar.hp_);
		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 10;
			g_top_y = my_packet->y - 10;
			avatar.show();
		}
		else if (id < MAX_USER) {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].change_texture(my_packet->visual);
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].set_hp(my_packet->hp);
			players[id].show();
		}
		else {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].set_hp(my_packet->hp);
			players[id].show();
		}
		break;
	} 
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 10;
			g_top_y = my_packet->y - 10;
		}
		else if (other_id < MAX_USER) {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		else {
			players[other_id].move(my_packet->x, my_packet->y);
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
			players[other_id].hide();
		}
		break;
	}

	case SC_STAT_CHANGE: {
		SC_STAT_CHANGE_PACKET* my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hp_ = my_packet->hp;
			avatar.level_ = my_packet->level;
			avatar.exp_ = my_packet->exp;
			avatar.max_exp_ = pow(2,(avatar.level_-1))*100;
		}
		else {
			players[other_id].hp_ = my_packet->hp;
			players[other_id].level_ = my_packet->level;
			players[other_id].exp_ = my_packet->exp;
			players[other_id].set_hp(my_packet->hp);
		}
		break;
		
	}

	case SC_GET_DAMAGE: {
		SC_GET_DAMAGE_PACKET* my_packet = reinterpret_cast<SC_GET_DAMAGE_PACKET*>(ptr);
		printf("Player Get %d Damage From Monster Id : %d\n", my_packet->got_damage, my_packet->id);
		avatar.hp_ -= my_packet->got_damage;
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[2]);
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
	ml.Draw();
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

	
	avatar.draw();
	for (auto& pl : players) pl.second.draw();

	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x_, avatar.m_y_);
	text.setString(buf);
	g_window->draw(text);

	sf::Text hp_text;
	hp_text.setFont(g_font);
	sprintf_s(buf, "hp : %d", avatar.hp_);
	hp_text.setString(buf);
	hp_text.move(0, 30);
	g_window->draw(hp_text);

	sf::Text level_text;
	level_text.setFont(g_font);
	sprintf_s(buf, "level : %d", avatar.level_);
	level_text.setString(buf);
	level_text.move(0, 60);
	g_window->draw(level_text);

	sf::Text exp_text;
	exp_text.setFont(g_font);
	sprintf_s(buf, "exp : %d / %d", avatar.exp_, avatar.max_exp_);
	exp_text.setString(buf);
	exp_text.move(0, 90);
	g_window->draw(exp_text);
}

void send_packet(void* packet)
{
	size_t sent = 0;
	s_socket.send(packet, *reinterpret_cast<unsigned short*>(packet), sent);
}

int main()
{
	string player_name;
	cout << "Enter player_name : ";
	cin >> player_name;
	ml.Load_Map_info();
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
				case sf::Keyboard::A: {
					CS_ATTACK_PACKET p;
					p.size = sizeof(p);
					p.type = CS_ATTACK;
					send_packet(&p);
					break;
				}
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