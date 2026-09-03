#include "SESSION.h"

std::array<SESSION, MAX_USER + MAX_NPC> clients;

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&recv_over_.over_, 0, sizeof(recv_over_.over_));
	recv_over_.wsabuf_.len = BUF_SIZE - prev_remain_;
	recv_over_.wsabuf_.buf = recv_over_.send_buf_ + prev_remain_;
	// Take the reference under the lock. disconnect() flips the state under the
	// same lock, so once it has run no new operation can be attached and the
	// last decrement really is the last one.
	{
		std::lock_guard<std::mutex> ll(s_lock_);
		if (ST_ALLOC != state_ && ST_INGAME != state_) return;
		pending_ops_.fetch_add(1);
	}
	int ret = WSARecv(socket_, &recv_over_.wsabuf_, 1, 0, &recv_flag,
		&recv_over_.over_, 0);
	// A synchronous failure produces no completion notification. Without a
	// pending recv nothing else will notice this session is gone, so close it here.
	if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError()) {
		disconnect(id_);
		on_op_done(id_);
	}
}

void SESSION::do_send(void* packet)
{
	// NPC slots have no socket, and a closing session must not take a new
	// reference. Callers check the state before sending, but that check and this
	// call are not atomic, so re-check it here while holding the lock.
	{
		std::lock_guard<std::mutex> ll(s_lock_);
		if (ST_INGAME != state_ || id_ >= MAX_USER) return;
		pending_ops_.fetch_add(1);
	}
	OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	int ret = WSASend(socket_, &sdata->wsabuf_, 1, 0, 0, &sdata->over_, 0);
	// No completion notification on a synchronous failure, so release it here.
	// The pending recv will report the dead socket and drive the disconnect.
	if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError()) {
		delete sdata;
		on_op_done(id_);
	}
}

void SESSION::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.character = character_;
	p.id = id_;
	p.hp = hp_;
	p.max_hp = max_hp_;
	p.exp = exp_;
	p.level = level_;
	p.x = pos_.x_.load();
	p.y = pos_.y_.load();
	do_send(&p);
}

 void SESSION::send_get_damage_packet(int c_id, int damage, int hp) {

	SC_GET_DAMAGE_PACKET p;
	p.size = sizeof(SC_GET_DAMAGE_PACKET);
	p.type = SC_GET_DAMAGE;
	p.id = c_id;
	p.got_damage = damage;
	p.hp = hp;
	do_send(&p);
}

 void SESSION::send_move_packet(int c_id)
 {
	 SC_MOVE_OBJECT_PACKET p;
	 p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	 p.type = SC_MOVE_OBJECT;
	 p.id = c_id;
	 p.x = clients[c_id].pos_.x_;
	 p.y = clients[c_id].pos_.y_;
	 p.move_time = clients[c_id].last_move_time_;
	 do_send(&p);
 }


void SESSION::send_add_object_packet(int c_id)
 {
	 SC_ADD_OBJECT_PACKET add_packet;
	 add_packet.size = sizeof(SC_ADD_OBJECT_PACKET);
	 add_packet.type = SC_ADD_OBJECT;
	 add_packet.id = c_id;
	 add_packet.hp = clients[c_id].hp_;
	 add_packet.character = clients[c_id].character_;
	 strcpy_s(add_packet.name, clients[c_id].name_);
	 add_packet.x = clients[c_id].pos_.x_;
	 add_packet.y = clients[c_id].pos_.y_;
	 vl_.lock();
	 view_list_.insert(c_id);
	 vl_.unlock();
	 do_send(&add_packet);
 }

 void SESSION::send_remove_player_packet(int c_id)
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

void SESSION::send_chat_packet(int p_id, const char* mess)
 {
	 SC_CHAT_PACKET packet;
	 packet.id = p_id;
	 packet.size = sizeof(packet);
	 packet.type = SC_CHAT;
	 strcpy_s(packet.mess, mess);
	 do_send(&packet);
 }

void SESSION::send_stat_change_packet(int c_id, int max_hp, int hp, int level, int exp) {
	 SC_STAT_CHANGE_PACKET p;
	 p.size = sizeof(p);
	 p.type = SC_STAT_CHANGE;
	 p.id = c_id;
	 p.max_hp = max_hp;
	 p.hp = hp;
	 p.level = level;
	 p.exp = exp;
	 do_send(&p);
 }

 void SESSION::update_status() {
	 while (true) {
		 int hp = hp_;
		 if (std::atomic_compare_exchange_strong(&hp_, &hp, 100)) {
			 break;
		 }
	 }
	 if (character_ == WARRIOR) {
		 damage_ = level_ * WARRIOR_STAT_ATK;
		 armor_ = level_ * WARRIOR_STAT_ARMOR;
	 }
	 else if (character_ == MAGE) {
		 damage_ = level_ * MAGE_STAT_ATK;
		 armor_ = level_ * MAGE_STAT_ARMOR;
	 }
	 else if (character_ == PRIST) {
		 damage_ = level_ * PRIST_STAT_ATK;
		 armor_ = level_ * PRIST_STAT_ARMOR;
	 }
 }
