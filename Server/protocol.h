#pragma once
constexpr int PORT_NUM = 4000;
constexpr int NAME_SIZE = 20;
constexpr int CHAT_SIZE = 300;

constexpr int MAX_USER = 10'000;
constexpr int MAX_NPC = 200'000;

constexpr int MAP_COUNT = 24;

constexpr int W_WIDTH = 2000;
constexpr int W_HEIGHT = 2000;

constexpr int WARRIOR_STAT_ATK = 200;
constexpr int WARRIOR_STAT_ARMOR = 100;
constexpr int MAGE_STAT_ATK = 20;
constexpr int MAGE_STAT_ARMOR = 100;
constexpr int PRIST_STAT_ATK = 5;
constexpr int PRIST_STAT_ARMOR = 100;

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_CHAT = 2;
constexpr char CS_ATTACK = 3;			// 4 방향 공격
constexpr char CS_TELEPORT = 4;			// RANDOM한 위치로 Teleport, Stress Test할 때 Hot Spot현상을 피하기 위해 구현
constexpr char CS_LOGOUT = 5;			// 클라이언트에서 정상적으로 접속을 종료하는 패킷


constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_LOGIN_FAIL = 3;
constexpr char SC_ADD_OBJECT = 4;
constexpr char SC_REMOVE_OBJECT = 5;
constexpr char SC_MOVE_OBJECT = 6;
constexpr char SC_CHAT = 7;
constexpr char SC_STAT_CHANGE = 8;
constexpr char SC_GET_DAMAGE = 9;

#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned short size;
	char	type;
	int		id;
	char	name[NAME_SIZE];
};

struct CS_MOVE_PACKET {
	unsigned short size;
	char	type;
	char	direction;  // 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT
	unsigned	move_time;
};

struct CS_CHAT_PACKET {
	unsigned short size;			// 크기가 가변이다, mess가 작으면 size도 줄이자.
	char	type;
	char	mess[CHAT_SIZE];
};

struct CS_TELEPORT_PACKET {			// 랜덤으로 텔레포트 하는 패킷, 동접 테스트에 필요
	unsigned short size;
	char	type;
};

struct CS_LOGOUT_PACKET {
	unsigned short size;
	char	type;
};

struct CS_ATTACK_PACKET {
	unsigned short size;
	char	type;
	
};

// ==============================================================================================

struct SC_LOGIN_INFO_PACKET {
	unsigned short size;
	char	type;
	int		character;				// 0 : 전사, 1 : 마법사, 2 : 사제
	int		id;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	short	x, y;
};

struct SC_ADD_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int		character;		// 0 : 플레이어,  1 : NPC
	short	x, y;
	int		hp;
	char	name[NAME_SIZE];
};

struct SC_REMOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int     target_id;
};

struct SC_MOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	short	x, y;
	unsigned int move_time;
};

struct SC_CHAT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	char	mess[CHAT_SIZE];
};

struct SC_LOGIN_FAIL_PACKET {
	unsigned short size;
	char	type;
};

struct SC_STAT_CHANGE_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
};

struct SC_GET_DAMAGE_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int		hp;
	int		got_damage;
};

#pragma pack (pop)