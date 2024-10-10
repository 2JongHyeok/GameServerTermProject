#include "OVER_EXP.h"

inline OVER_EXP::OVER_EXP()
{
	wsabuf_.len = BUF_SIZE;
	wsabuf_.buf = send_buf_;
	comp_type_ = OP_RECV;
	ZeroMemory(&over_, sizeof(over_));
}

inline OVER_EXP::OVER_EXP(char* packet)
{
	wsabuf_.len = packet[0];
	wsabuf_.buf = send_buf_;
	ZeroMemory(&over_, sizeof(over_));
	comp_type_ = OP_SEND;
	memcpy(send_buf_, packet, packet[0]);
}
