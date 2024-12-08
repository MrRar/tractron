#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <tractron.h>

#define SEGMENT_ID 0x55111155
#define SERVER_PORT 18000

enum net_msg_id_t {
	NET_SEEK_SERVER,
	NET_SERVER_AD,
	NET_JOIN,
	NET_SET_COLOR,
	NET_SERVER_FULL,
	NET_PLAYER_LIST,
	NET_PING,
	NET_LEAVE,
	NET_START,
	NET_TURN,
	NET_CRASH,
	NET_BOMB,
	NET_SPEED
};

enum result_t {
	RESULT_PROCEED,
	RESULT_CANCEL,
	RESULT_NOT_DONE
};

extern int server_sock;

extern void close_tcp_socket(int sock);

extern void send_message(int sock, enum net_msg_id_t msg, unsigned char *buf, unsigned int buf_len);

extern void send_turn(int socket, struct player_t *player, int x, int y);

extern void send_speed(int socket, struct player_t *player);

extern bool read_network(int sock);

extern void send_bomb(int socket, int x, int y);

extern void send_crash(int socket, struct player_t *player);

extern enum result_t read_server_start();

extern void server();

extern void client();

extern void kick_all_clients();
