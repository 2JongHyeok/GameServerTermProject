#pragma once
#include "protocol.h"

#include <WS2tcpip.h>
#include <MSWSock.h>

#include <windows.h>  
#include <stdio.h>  
#include <locale.h>  
#include <string>  

#define UNICODE  
#include <sqlext.h>  


#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

constexpr int BUF_SIZE = 1024;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_NPC_RESURRECTION, OP_NPC_ATTACK };


class OVER_EXP {
public:
	WSAOVERLAPPED over_;
	WSABUF wsabuf_;
	char send_buf_[BUF_SIZE];
	COMP_TYPE comp_type_;
	int ai_target_obj_;
	OVER_EXP();
	OVER_EXP(char* packet);
};

