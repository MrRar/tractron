#include <tractron.h>
#include <network.h>

#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>

int server_sock = -1;

void close_tcp_socket(int sock) {
	shutdown(sock, SHUT_RDWR);
	close(sock);
}

void send_message(int sock, enum net_msg_id_t msg, unsigned char *buf, unsigned int buf_len) {
	unsigned char out_buf[9 + buf_len];
	*((unsigned int *)out_buf) = SEGMENT_ID;
	out_buf[4] = msg;
	*((unsigned int *)(out_buf + 5)) = htonl(buf_len);

	if (buf_len > 0) {
		memcpy(out_buf + 9, buf, buf_len);
	}
	write(sock, out_buf, sizeof(out_buf));
}

void send_turn(int socket, struct player_t *player, int x, int y) {
	unsigned char buf[4];
	buf[0] = player->color;
	buf[1] = player->dir;
	buf[2] = x;
	buf[3] = y;
	send_message(socket, NET_TURN, buf, sizeof(buf));
}

void send_speed(int socket, struct player_t *player) {
	unsigned char buf[5];
	buf[0] = player->color;
	*((int *)(buf + 1)) = htonl(player->speed);
	send_message(socket, NET_SPEED, buf, sizeof(buf));
}

void send_crash(int socket, struct player_t *player) {
	unsigned char buf[1];
	buf[0] = player->color;
	send_message(socket, NET_CRASH, buf, sizeof(buf));
}

static void receive_turn(unsigned char *buf) {
	struct player_t *player = player_by_color(buf[0]);
	enum dir_t new_dir = buf[1];
	int turn_pos_x = buf[2];
	int turn_pos_y = buf[3];

	int old_x_inc = 0;
	int old_y_inc = 0;
	apply_dir_to_pos(&old_x_inc, &old_y_inc, player->dir);

	if (player->last_turn_x != player->pos_x) {
		if (
			(old_x_inc > 0 && player->pos_x >= turn_pos_x) ||
			(old_x_inc < 0 && player->pos_x <= turn_pos_x)
		) {
			int x_start = min(turn_pos_x, player->pos_x);
			int x_end = max(turn_pos_x, player->pos_x);
			for (int x = x_start; x <= x_end; x++) {
				map_set(x, player->pos_y, ' ', BRIGHT_WHITE);
			}
		} else {
			map_set(player->pos_x, player->pos_y, ' ', BRIGHT_WHITE);
			for (
				int x = turn_pos_x - old_x_inc;
				map_get_char(x, player->pos_y) == ' ';
				x -= old_x_inc
			) {
				map_set(x, player->pos_y, SINGLE_HORIZONTAL, player->color);
			}
		}
	} else if (player->last_turn_y != player->pos_y) {
		if (
			(old_y_inc >= 0 && player->pos_y >= turn_pos_y) ||
			(old_y_inc < 0 && player->pos_y <= turn_pos_y)
		) {
			int y_start = min(turn_pos_y, player->pos_y);
			int y_end = max(turn_pos_y, player->pos_y);
			for (int y = y_start; y <= y_end; y++) {
				map_set(player->pos_x, y, ' ', DARK_YELLOW);
			}
		} else {
			map_set(player->pos_x, player->pos_y, ' ', BRIGHT_WHITE);
			for (
				int y = turn_pos_y - old_y_inc;
				map_get_char(player->pos_x, y) == ' ';
				y -= old_y_inc
			) {
				map_set(player->pos_x, y, SINGLE_VERTICLE, player->color);
			}
		}
	}
	//player->move_timer = ntohl(*(int *)(t_buf + 4));

	int new_x_inc = 0;
	int new_y_inc = 0;
	apply_dir_to_pos(&new_x_inc, &new_y_inc, new_dir);

	player->dir = new_dir;
	player->pos_x = turn_pos_x + new_x_inc;
	player->pos_y = turn_pos_y + new_y_inc;
	player->old_x = turn_pos_x;
	player->old_y = turn_pos_y;
	player->last_turn_x = turn_pos_x;
	player->last_turn_y = turn_pos_y;

	// Server relay the turn
	for (int i = 0; i < players_count; i++) {
		if (players[i].control == CLIENT_CONTROL && &players[i] != player) {
			send_turn(players[i].socket, player, turn_pos_x, turn_pos_y);
		}
	}

	draw_player(player);
	int old_old_x = turn_pos_x - old_x_inc;
	int old_old_y = turn_pos_y - old_y_inc;
	draw_player_trail(player, old_old_x, old_old_y);
	player->state = PLAYING_STATE;
}

static void receive_crash(unsigned char *buf) {
	struct player_t *player = player_by_color(buf[0]);
	if (player->state == CRASHED_STATE) {
		printf("Duplicate crash frame for player %i\n", player->color);
		return;
	}
	crash(player);
}

static void receive_speed(unsigned char *buf) {
	struct player_t *player = player_by_color(buf[0]);
	player->speed = ntohl(*((int *)(buf + 1)));
}

static void receive_bomb(unsigned char *buf) {
	map_set(buf[0], buf[1], '@', DARK_WHITE);
}

bool read_network(int sock) {
	while (true) {
		unsigned char header[9];
		int received_length = read(sock, header, sizeof(header));
		if (received_length != 9 || *((int *)header) != SEGMENT_ID) {
			return true;
		}
		enum net_msg_id_t id = header[4];
		unsigned int buf_len = ntohl(*((unsigned int *)(header + 5)));

		unsigned char buf[buf_len];
		received_length =  read(sock, buf, buf_len);

		if (received_length != buf_len) {
			return true;
		}

		if (id == NET_TURN) {
			receive_turn(buf);
		} else if (id == NET_CRASH) {
			receive_crash(buf);
		} else if (id == NET_BOMB) {
			receive_bomb(buf);
		} else if (id == NET_SPEED) {
			receive_speed(buf);
		} else if (id == NET_LEAVE) {
			display_message("Kicked from server");
			return false;
		}
	}
	return true;
}

void send_bomb(int socket, int x, int y) {
	unsigned char buf[2];
	buf[0] = x;
	buf[1] = y;
	send_message(socket, NET_BOMB, buf, sizeof(buf));
}

enum result_t read_server_start() {
	unsigned char header[9];
	int received_length = read(server_sock, header, sizeof(header));
	if (
		received_length == sizeof(header) &&
		*((unsigned int *)header) == SEGMENT_ID
	) {
		if (header[4] == NET_START) {
			return RESULT_PROCEED;
		} else if (header[4] == NET_LEAVE) {
			display_message("Kicked from server");
			return RESULT_CANCEL;
		}
	}
	return RESULT_NOT_DONE;
}

static void handle_seek_server(int udp_sock, char *player_name) {
	while (true) {
		unsigned char ss_buf[9];
		struct sockaddr_in client_address = {0};
		unsigned int sockaddr_len = sizeof(client_address);
		int received_length = recvfrom(
			udp_sock, ss_buf, sizeof(ss_buf), MSG_DONTWAIT,
			(struct sockaddr *)&client_address, &sockaddr_len);
		if (received_length == EOF) {
			return;
		}
			
		if (*((int *)ss_buf) != SEGMENT_ID) {
			continue;
		}
			
		if (ss_buf[4] == NET_SEEK_SERVER) {
			unsigned char ad_buf[9 + 9];
			*((int *)ad_buf) = SEGMENT_ID;
			ad_buf[4] = NET_SERVER_AD;
			*((int *)(ad_buf + 5)) = htonl(sizeof(ad_buf));
			memcpy(ad_buf + 9, player_name, 9);
			sendto(udp_sock, ad_buf, sizeof(ad_buf), 0,
				(struct sockaddr *)&client_address, sizeof(client_address));
		}
	}
}

static void send_player_list_to_all() {
	unsigned char buf[1 + 10 * players_count];
	buf[0] = players_count;
	for (int i = 0; i < players_count; i++) {
		memcpy(buf + 1 + 10 * i, players[i].name, 9);
		buf[1 + 10 * i + 9] = players[i].color;
	}
	for (int i = 0; i < players_count; i++) {
		if (players[i].control == CLIENT_CONTROL) {
			send_message(players[i].socket, NET_PLAYER_LIST, buf, sizeof(buf));
		}
	}
}

static void remove_player(int i) {
	players_count--;
	for (; i < players_count; i++) {
		players[i] = players[i + 1];
	}
}

static void send_confirm_join(int socket, unsigned char color) {
	unsigned char buf[2];
	buf[0] = color;
	buf[1] = game_level;
	send_message(socket, NET_SET_COLOR, buf, sizeof(buf));
}

static void receive_join(unsigned char *buf, int sock) {
	if (players_count < 6) {
		struct player_t *player = add_player((char *)buf, COLOR_AUTO, CLIENT_CONTROL);
		player->socket = sock;

		send_confirm_join(player->socket, player->color);

		send_player_list_to_all();
	} else {
		send_message(sock, NET_SERVER_FULL, NULL, 0);
	}
}

static void handle_join_leave(int tcp_sock) {
	struct sockaddr_in client_address = {0};
	unsigned int sockaddr_len = sizeof(client_address);
	while (true) {
		int child_sock = accept(tcp_sock, (struct sockaddr *)&client_address, &sockaddr_len);
		if (child_sock < 0) {
			break;
		}

		fcntl(child_sock, F_SETFL, O_NONBLOCK);

		unsigned char header[9];
		int received_length = read(child_sock, header, sizeof(header));
		if (received_length != sizeof(header) || ((int *)header)[0] != SEGMENT_ID) {
			close_tcp_socket(child_sock);
			continue;
		}
		unsigned int buf_len = ntohl(*((unsigned int *)(header + 5)));

		unsigned char buf[buf_len];

		received_length = read(child_sock, buf, buf_len);

		if (header[4] == NET_JOIN && received_length == sizeof(buf)) {
			receive_join(buf, child_sock);
		}
	}

	for (int i = 0; i < players_count; i++) {
		if (players[i].control != CLIENT_CONTROL) {
			continue;
		}
		char header[9];
		int sock = players[i].socket;
		int received_length = read(sock, header, sizeof(header));
		if (
			received_length == sizeof(header) &&
			((int *)header)[0] == SEGMENT_ID &&
			header[4] == NET_LEAVE
		) {
			remove_player(i);
			close_tcp_socket(sock);
			send_player_list_to_all();
			continue;
		}
	}
}

static enum result_t server_waiting_room_keyboard() {
	while (true) {
		char key = read_char();
		if (key == EOF) {
			return RESULT_NOT_DONE;
		}
		switch(key) {
		case '\e':
			return RESULT_CANCEL;
		case '\r':
			for (int i = 0; i < players_count; i++) {
				if (players[i].control != CLIENT_CONTROL) {
					continue;
				}
				send_message(players[i].socket, NET_START, NULL, 0);
			}
			return RESULT_PROCEED;
		}
	}
}

static void draw_waiting_room(char *server_name, bool is_server) {
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);
	string_to_screen_centered(2, "Waiting for game to start.", DARK_WHITE);
	char *keyboard_options = is_server ?
		"Press escape to exit. Press enter to start the game." : "Press escape to exit.";

	string_to_screen_centered(3, keyboard_options, DARK_WHITE);
	int host_x = (screen_width - 6 - strlen(server_name)) / 2;
	string_to_screen(host_x, 4, "Host: ", DARK_WHITE);
	string_to_screen(host_x + 6, 4, server_name, BRIGHT_WHITE);

	for (int i = 0; i < players_count; i++) {
		string_to_screen_centered(6 + i * 2, players[i].name, players[i].color);
	}
	update_screen();
}

void server() {
    sigaction(SIGPIPE, &(struct sigaction){{SIG_IGN}}, NULL);

	clear_players();
	char player_name[10] = {0};
	text_input_field("Enter your username to start a server", player_name, 9);
	if (player_name[0] == '\0') {
		return;
	}

	int ai_count = 2;
	if (!select_ais(&ai_count)) {
		return;
	}

	add_player(player_name, COLOR_AUTO, KEYBOARD_CONTROL);

	char *names[] = {"Bob", "Luke", "Kate", "Jane", "Fred"};
	for (int i = 0; i < ai_count; i++) {
		add_player(names[i], COLOR_AUTO, AI_CONTROL);
	}

	struct sockaddr_in any_addr = {0};
	any_addr.sin_family = AF_INET;
	any_addr.sin_port = htons(SERVER_PORT);
	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  	bind(udp_sock, (struct sockaddr *)&any_addr, sizeof(any_addr));

	int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	int option_value = 1;
	setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, 
		(const void *)&option_value, sizeof(int));
	setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY,
		(const void *)&option_value, sizeof(int));
	bind(tcp_sock, (struct sockaddr *)&any_addr, 
		sizeof(any_addr));
	listen(tcp_sock, 10);
	fcntl(tcp_sock, F_SETFL, fcntl(tcp_sock, F_GETFL, 0) | O_NONBLOCK);

	while (true) {
		handle_seek_server(udp_sock, player_name);
		handle_join_leave(tcp_sock);
		draw_waiting_room(player_name, true);
		enum result_t result = server_waiting_room_keyboard();
		if (result != RESULT_NOT_DONE) {
			close_tcp_socket(tcp_sock);
			close(udp_sock);
			if (result == RESULT_CANCEL) {
				kick_all_clients();
				clear_players();
				return;
			}
			break;
		}
		usleep(100000);
  	}

	game();
}

static void send_server_seek(int udp_sock) {
	struct sockaddr_in any_addr = {0};
	any_addr.sin_family = AF_INET;
	any_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	any_addr.sin_port = htons(SERVER_PORT);
	connect(udp_sock, (struct sockaddr *)&any_addr, sizeof(any_addr));

	int option_value = 1;
	setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &option_value, sizeof(int));

	struct sockaddr_in broadcast_address;
	broadcast_address.sin_family = AF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	broadcast_address.sin_port = htons(SERVER_PORT);

	char buf[9];
	((int *)buf)[0] = SEGMENT_ID;
	buf[4] = NET_SEEK_SERVER;
	*((int *)(buf + 5)) = 0;
	sendto(udp_sock, buf, sizeof(buf), 0,
		(struct sockaddr *)&broadcast_address, sizeof(broadcast_address));
}

static void draw_server_list(int server_index, int server_count,
	char *server_names, struct sockaddr_in *server_sockaddrs) {
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);

	string_to_screen_centered(1, "Select a server from the list.", BRIGHT_WHITE);
	string_to_screen_centered(2, "Press the escape key to cancel.", DARK_WHITE);
	string_to_screen_centered(3, "Press the R key to refresh the list.", DARK_WHITE);

	for (int i = 0; i < server_count; i++) {
		int name_x = (screen_width - 25) / 2;

		char color = DARK_WHITE;

		if (i == server_index) {
			if (current_time % 1024 > 512) {
				color = BRIGHT_GREEN;
			} else {
				color = BRIGHT_WHITE;
			}
			draw_rect(name_x - 1, 4 + i * 2, 25, 3, color);
		}

		string_to_screen(name_x, 5 + i * 2, server_names + 10 * i, color);

		int ip_x = name_x + 10;
		char *ip_str = inet_ntoa(server_sockaddrs[i].sin_addr);
		string_to_screen(ip_x, 5 + i * 2, ip_str, color);
	}
	update_screen();
	update_time();
}

static void server_list(char *player_name, char *server_name_out, struct sockaddr_in *sockaddr_out) {
	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	int server_count = 0;
	char server_names[10 * 10] = {0};
	struct sockaddr_in server_sockaddrs[10];

	send_server_seek(udp_sock);

	int server_index = 0;

	while (true) {
		char ad_buf[18];
		unsigned int sockaddr_len = sizeof(struct sockaddr_in);
		struct sockaddr_in server_sockaddr;
		int received_length = recvfrom(udp_sock, ad_buf, sizeof(ad_buf), MSG_DONTWAIT,
			(struct sockaddr *)&server_sockaddr, &sockaddr_len);

		if (
			received_length == sizeof(ad_buf) &&
			*((int *)ad_buf) == SEGMENT_ID &&
			ad_buf[4] == NET_SERVER_AD &&
			server_count < 10
		) {
			memcpy(server_names + 10 * server_count, ad_buf + 9, 9);
			server_sockaddrs[server_count] = server_sockaddr;
			server_count++;
		}

		while (true) {
			char key = read_char();
			if (key == EOF) {
				break;
			}
			switch(key) {
			case '\e':
				return;
			case 'r':
				server_count = 0;
				server_index = 0;
				send_server_seek(udp_sock);
				continue;
			DOWN_CASES
				server_index++;
				if (server_index >= server_count) {
					server_index = 0;
				}
				continue;
			UP_CASES
				server_index--;
				if (server_index < 0) {
					server_index = 0;
				}
				continue;
			case '\r':
				if (server_count == 0) {
					return;
				}
				strcpy(server_name_out, server_names + 10 * server_index);
				*sockaddr_out = server_sockaddrs[server_index];
				return;
			}
		}
		draw_server_list(server_index, server_count, server_names, server_sockaddrs);
		usleep(100000);
	}
}

static enum result_t read_join_response(enum color_t *player_color) {
	char header[9];
	int received_length = read(server_sock, header, sizeof(header));
	if (
		*((unsigned int *)header) != SEGMENT_ID ||
		received_length != sizeof(header)
	) {
		return RESULT_NOT_DONE;
	}
	enum net_msg_id_t msg_id = header[4];

	unsigned int buf_len = ntohl(*((unsigned int *)(header + 5)));
	unsigned char buf[buf_len];
	if (buf_len > 0) {
		read(server_sock, buf, buf_len);
	}

	if (msg_id == NET_SET_COLOR) {
		*player_color = buf[0];
		game_level = buf[1];
	} else if (msg_id == NET_PLAYER_LIST) {
		clear_players();
		int count = buf[0];
		for (int i = 0; i < count; i++) {
			enum color_t color = buf[1 + 10 * i + 9];
			enum control_t control = color == *player_color?
				KEYBOARD_CONTROL : SERVER_CONTROL;
			/*if (control == KEYBOARD_CONTROL) {
				control = AI_CONTROL;
			}*/
			add_player((char *)buf + 1 + 10 * i, color, control);
		}
	} else if (msg_id == NET_SERVER_FULL) {
		display_message("Server full");
		return RESULT_CANCEL;
	} else if (msg_id == NET_LEAVE) {
		display_message("Kicked from server");
		return RESULT_CANCEL;
	} else if (msg_id == NET_START) {
		return RESULT_PROCEED;
	} else if (msg_id == NET_PING) {
		send_message(server_sock, NET_PING, NULL, 0);
		return RESULT_NOT_DONE;
	}
	return RESULT_NOT_DONE;
}

static bool join_server(char* player_name, char *server_name, struct sockaddr_in *server_sockaddr) {
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	enum color_t player_color;
	connect(server_sock, (struct sockaddr *)server_sockaddr, sizeof(struct sockaddr_in));
	fcntl(server_sock, F_SETFL, O_NONBLOCK);
	int option_value = 1;
	setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY,
		(const void *)&option_value, sizeof(int));

	send_message(server_sock, NET_JOIN, (unsigned char *)player_name, 9);

	while (true) {
		enum result_t result = read_join_response(&player_color);
		if (result == RESULT_CANCEL) {
			return false;
		} else if (result == RESULT_PROCEED) {
			return true;
		}

		while (true) {
			char key = read_char();
			if (key == EOF) {
				break;
			}
			switch(key) {
			case '\e':
				send_message(server_sock, NET_LEAVE, NULL, 0);
				return false;
			}
		}

		draw_waiting_room(server_name, false);
		usleep(100000);
	}
}

void client() {
    sigaction(SIGPIPE, &(struct sigaction){{SIG_IGN}}, NULL);

	char player_name[10] = {0};
	text_input_field("Enter your username to join a server", player_name, 9);
	if (player_name[0] == '\0') {
		return;
	}

	char server_name[10];
	struct sockaddr_in server;
	server_list(player_name, server_name, &server);
	if (server_name[0] == '\0') {
		return;
	}
	if (!join_server(player_name, server_name, &server)) {
		clear_players();
		return;
	}
	game();
	close_tcp_socket(server_sock);
	server_sock = -1;
}

void kick_all_clients() {
	for (int i = 0; i < players_count; i++) {
		if (players[i].control == CLIENT_CONTROL) {
			send_message(players[i].socket, NET_LEAVE, NULL, 0);
			close_tcp_socket(players[i].socket);
		}
	}
}
