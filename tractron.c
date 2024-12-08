#ifdef __COSMOCC__
#include <cosmo.h>
#include <win.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef __DJGPP__
#define SUPPORTS_NETWORK
#endif

#ifdef __DJGPP__
#include <pc.h>
#include <conio.h>
#endif

#ifdef SUPPORTS_NETWORK
#include <network.h>
#endif

#include <tractron.h>

enum level_t game_level = EASY_LEVEL;

// Code page 437 virtual screen
unsigned char screen_buffer[screen_width * screen_height * 2];

static char *cp437_to_utf8(unsigned char ch) {
	switch (ch) {
	case 0xc9: return "╔";
	case 0xbb: return "╗";
	case 0xc8: return "╚";
	case 0xbc: return "╝";
	case 0xcd: return "═";
	case 0xba: return "║";
	case 0x1e: return "▲";
	case 0x10: return "►";
	case 0x1f: return "▼";
	case 0x11: return "◄";
	case 0xb3: return "│";
	case 0xc4: return "─";
	case 0xda: return "┌";
	case 0xbf: return "┐";
	case 0xc0: return "└";
	case 0xd9: return "┘";
	default: return "?";
	}
}

static char *cga_color_to_ansi(unsigned char ch) {
	switch (ch) {
	case BRIGHT_BLUE: return "\e[94m";
	case BRIGHT_GREEN: return "\e[92m";
	case BRIGHT_CYAN: return "\e[96m";
	case BRIGHT_RED: return "\e[91m";
	case BRIGHT_MAGENTA: return "\e[95m";
	case DARK_YELLOW: return "\e[33m";
	case BRIGHT_YELLOW: return "\e[93m";
	case DARK_BLACK: return "\e[90m";
	case DARK_WHITE: return "\e[37m";
	case BRIGHT_WHITE: return "\e[97m";
	default: return "";
	}
}

static bool is_tile_solid(unsigned char tile) {
	return tile != ' ' && tile != '@' && tile != '*';
}

static bool check_stdin() {
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

char read_char() {
#ifdef __COSMOCC__
	if (IsWindows()) {
		return windows_read_input();
	}
#endif
	if (!check_stdin()) {
		return EOF;
	}

	char ch;
	read(STDIN_FILENO, &ch, 1);
	if (ch != '\e' || !check_stdin()) {
		return ch;
	}
	read(STDIN_FILENO, &ch, 1); // [
	read(STDIN_FILENO, &ch, 1);
	return ch;
}

static void screen_set(int x, int y, unsigned char ch, unsigned char color) {
	screen_buffer[(y * screen_width + x) * 2] = ch;
	screen_buffer[(y * screen_width + x) * 2 + 1] = color;
}

void clear_screen() {
	for (int x = 0; x < screen_width; x++) {
		for (int y = 0; y < screen_height; y++) {
			screen_set(x, y, ' ', BRIGHT_WHITE);
		}
	}
}

static void str_out(char *str) {
	write(STDOUT_FILENO, str, strlen(str));
}

void update_screen() {
#ifdef __DJGPP__
	ScreenUpdate(screen_buffer);
	return;
#endif
#ifdef __COSMOCC__
	if (IsWindows()) {
		windows_draw_window();
		return;
	}
#endif
	str_out("\e[1;1H");
	for (int i = 0; i < screen_width * screen_height; i++) {
		static unsigned char old_forground = DARK_WHITE;
		//static unsigned char old_background = DARK_BLACK;
		static unsigned char old_blink = TEXT_NO_BLINK;

		unsigned char color = screen_buffer[i * 2 + 1];
		unsigned char ch = screen_buffer[i * 2];
		unsigned char forground = color & 0b00001111;
		//unsigned char background = color & 0b01110000;
		unsigned char blink = color & 0b10000000;
		if (blink != old_blink) {
			if (blink == TEXT_BLINK) {
				str_out("\e[5m");
			} else {
				str_out("\e[25m");
			}
			old_blink = blink;
		}
		if (forground != old_forground && ch != ' ') {
			str_out(cga_color_to_ansi(forground));
			old_forground = forground;
		}

		if (ch < ' ' || ch > '~') {
			str_out(cp437_to_utf8(ch));
		} else {
			char str[2];
			str[0] = ch;
			str[1] = '\0';
			str_out(str);
		}

		if ((i + 1) % screen_width == 0) {// && (i + 1) / screen_width != screen_height) {
			str_out("\n");
		}
	}
}

int max(int a, int b) {
	return a > b ? a : b;
}

int min(int a, int b) {
	return a < b ? a : b;
}

void display_message(char *message) {
	char *continue_message = "Press enter to continue";
	int max_width = max(strlen(message), strlen(continue_message));

	int x_pos = (screen_width - max_width) / 2 - 1;
	int y_pos = screen_height / 2 - 2;
	int width = max_width + 2;
	int height = 5;

	for (int x = x_pos; x < x_pos + width; x++) {
		for (int y = y_pos; y < y_pos + height; y++) {
			screen_set(x, y, ' ', BRIGHT_WHITE);
		}
	}

	draw_rect(x_pos, y_pos, width, height, BRIGHT_YELLOW);

	string_to_screen_centered(screen_height / 2 - 1, message, BRIGHT_YELLOW);
	string_to_screen_centered(screen_height / 2 + 1, continue_message, BRIGHT_YELLOW);
	update_screen();

	while (read_char() != '\r') {
		usleep(100000);
	}
}

void string_to_screen(int x, int y, char *str, unsigned char color) {
	for (int i = 0; i < strlen(str); i++) {
		screen_set(x + i, y, str[i], color);
	}
}

void string_to_screen_centered(int y, char *str, unsigned char color) {
	string_to_screen((screen_width - strlen(str)) / 2, y, str, color);
}

static struct termios old_term;
static void setup_terminal() {
/*#ifdef __COSMOCC__
	if (IsWindows()) {
		system("chcp.com 65001");
	}
#endif*/

	tcgetattr(1, &old_term);

	struct termios term;
	memcpy(&term, &old_term, sizeof(term));
	term.c_iflag |= INLCR;
	term.c_iflag &= ~ICRNL;
	term.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(1, TCSANOW, &term);
#ifndef __DJGPP__
	str_out("\e[?25l"); // Hide TTY cursor
#else
	textmode(C80);
	_setcursortype(_NOCURSOR);
#endif
}

static void reset_terminal() {
	tcsetattr(1, TCSANOW, &old_term);
#ifndef __DJGPP__
	str_out("\e[?25h"); // Show TTY cursor
#else
	textmode(LASTMODE);
	_setcursortype(_NORMALCURSOR);
#endif
}

#define map_width 128
#define map_height 64
static unsigned char map_buffer[map_width * map_height * 2];

void map_set(int x, int y, unsigned char ch, unsigned char color) {
	if (x < 0 || x >= map_width || y < 0 || y >= map_height) {
		printf("map_set out of range: %i, %i %u %u\n", x, y, ch, color);
		abort();
	}
	map_buffer[(y * map_width + x) * 2] = ch;
	map_buffer[(y * map_width + x) * 2 + 1] = color;
}

unsigned char map_get_char(int x, int y) {
	return map_buffer[(y * map_width + x) * 2];
}

static unsigned char map_get_color(int x, int y) {
	return map_buffer[(y * map_width + x) * 2 + 1];
}

static void init_map() {
	for (int x = 1; x < map_width; x++) {
		for (int y = 1; y < map_height; y++) {
			map_set(x, y, ' ', BRIGHT_WHITE);
		}
	}

	// Draw border
	map_set(0, 0, DOUBLE_TOP_LEFT, DARK_WHITE);
	map_set(map_width - 1, 0, DOUBLE_TOP_RIGHT, DARK_WHITE);
	map_set(map_width - 1, map_height - 1, DOUBLE_BOTTOM_RIGHT, DARK_WHITE);
	map_set(0, map_height - 1, DOUBLE_BOTTOM_LEFT, DARK_WHITE);

	for (int x = 1; x < map_width - 1; x++) {
		map_set(x, 0, DOUBLE_HORIZONTAL, DARK_WHITE);
		map_set(x, map_height - 1, DOUBLE_HORIZONTAL, DARK_WHITE);
	}

	for (int y = 1; y < map_height - 1; y++) {
		map_set(0, y, DOUBLE_VERTICLE, DARK_WHITE);
		map_set(map_width - 1, y, DOUBLE_VERTICLE, DARK_WHITE);
	}
}

int players_not_eliminated_count = 0;
int players_count = 0;
struct player_t players[6];

static struct player_t *get_focused_player() {
	struct player_t *focused_player = &players[0];
	for (int i = 0; i < players_count; i++) {
		if (players[i].state != ERASED_STATE) {
			focused_player = &players[i];
			if (players[i].control == KEYBOARD_CONTROL) {
				break;
			}
		}
	}
	return focused_player;
}

static void map_to_screen(bool using_status_bar) {
	struct player_t *player = get_focused_player();

	int usable_height = screen_height;
	int status_bar_height = 0;
	
	if (using_status_bar) {
		usable_height--;
		status_bar_height++;
	}
	int map_x = (map_width - screen_width) / 2 + player->pos_x - map_width / 2;
	int map_y = (map_height - usable_height) / 2 + player->pos_y - map_height / 2;

	if (map_x < 0) {
		map_x = 0;
	} else if (map_x > map_width - screen_width) {
		map_x = map_width - screen_width;
	}

	if (map_y < 0) {
		map_y = 0;
	} else if (map_y > map_height - usable_height) {
		map_y = map_height - usable_height;
	}

	for (int x = 0; x < screen_width; x++) {
		for (int y = 0; y < usable_height; y++) {
			unsigned char ch = map_get_char(map_x + x, map_y + y);
			unsigned char color = map_get_color(map_x + x, map_y + y);
			screen_set(x, y + status_bar_height, ch, color);
		}
	}
}

static void update_status_bar() {
	for (int x = 0; x < screen_width; x++) {
		screen_set(x, 0, ' ', BRIGHT_WHITE);
	}
	for (int i = 0; i < players_count; i++) {
		struct player_t *player = &players[i];
		if (player->round_rank) {
			screen_set(i * 13, 0, player->round_rank + '0', BRIGHT_WHITE);
		}
		unsigned char color = player->color;
		if (player->state == CRASHED_STATE) {
			color |= TEXT_BLINK;
		}
		string_to_screen(2 + i * 13, 0, player->name, color);
	}
}

static struct player_t *player_by_rank(int rank) {
	for (int i = 0; i < players_count; i++) {
		if (players[i].round_rank == rank) {
			return &players[i];
		}
	}
	return NULL;
}

void clear_players() {
#ifdef SUPPORTS_NETWORK
	for (int i = 0; i < players_count; i++) {
		if (players[i].control == CLIENT_CONTROL) {
			close_tcp_socket(players[i].socket);
		}
	}
#endif
	players_count = players_not_eliminated_count = 0;
}

static enum color_t get_unused_color() {
	enum color_t colors[] =
		{BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
		BRIGHT_BLUE, BRIGHT_CYAN, DARK_YELLOW};
	for (int i = 0; i < sizeof(colors); i++) {
		bool color_used = false;
		for (int j = 0; j < players_count; j++) {
			if (players[j].color == colors[i]) {
				color_used = true;
				break;
			}
		}
		if (!color_used) {
			return colors[i];
		}
	}
	return BRIGHT_BLUE;
}

struct player_t *add_player(char *name, enum color_t color, enum control_t control) {
	if (color == COLOR_AUTO) {
		color = get_unused_color();
	}
	struct player_t *player = &players[players_count];
	bzero(player, sizeof(struct player_t));
	player->color = color;
	strncpy(player->name, name, 9);
	player->control = control;
	players_count++;
	players_not_eliminated_count = players_count;
	return player;
}

unsigned int current_time = 0;
static int time_diff = 0;
void update_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned int old_time = current_time;
	current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	time_diff = current_time - old_time;
	if (time_diff > 1000 || time_diff < 0) {
		time_diff = 0;
	}
}

static bool game_keyboard(struct player_t *player) {
	char key = EOF;
	while (true) {
		char temp = read_char();
		if (temp == EOF) {
			break;
		}
		key = temp;
	}

	switch(key) {
	UP_CASES
		player->dir = DIR_UP;
		break;
	RIGHT_CASES
		player->dir = DIR_RIGHT;
		break;
	DOWN_CASES
		player->dir = DIR_DOWN;
		break;
	LEFT_CASES
		player->dir = DIR_LEFT;
		break;
	case '\e':
		return false;
	}
	return true;
}

void draw_player(struct player_t *player) {
	unsigned char player_tile;
	if (player->has_bomb) {
		player_tile = '@';
	} else {
		char arrows[] = { ARROW_UP, ARROW_RIGHT, ARROW_DOWN, ARROW_LEFT };
		player_tile = arrows[player->dir];
	}
	map_set(player->pos_x, player->pos_y, player_tile, player->color);
}

void draw_player_trail(struct player_t *player, int old_old_x, int old_old_y) {
	int x_diff = (old_old_x - player->old_x) + (player->pos_x - player->old_x);
	unsigned char trail;
	if (x_diff == 0) {
		if (player->old_x - old_old_x == 0) {
			trail = SINGLE_VERTICLE;
		} else {
			trail = SINGLE_HORIZONTAL;
		}
	} else {
		int y_diff = (old_old_y - player->old_y) + (player->pos_y - player->old_y);
		if (y_diff > 0) {
			if (x_diff > 0) {
				trail = SINGLE_TOP_LEFT;
			} else {
				trail = SINGLE_TOP_RIGHT;
			}
		} else {
			if (x_diff > 0) {
				trail = SINGLE_BOTTOM_LEFT;
			} else {
				trail = SINGLE_BOTTOM_RIGHT;
			}
		}
	}
	map_set(player->old_x, player->old_y, trail, player->color);
}

static int check_obstacle(int x, int y, enum dir_t dir, int search_dist) {
	int x_forward[] = {0, 1, 0, -1};
	int y_forward[] = {-1, 0, 1, 0};

	int dir_x = x_forward[dir];
	int dir_y = y_forward[dir];
	for (int i = 0; i < search_dist; i++) {
		x += dir_x;
		y += dir_y;
		if (is_tile_solid(map_get_char(x, y))) {
			return i + 1;
		}
	}
	return 10000;
}

static void ai_control(struct player_t *player) {
	// Turn randomly
	if (rand() % 64 == 0) {
		enum dir_t dir = rand() % 4;
		if (check_obstacle(player->pos_x, player->pos_y, dir, 2) > 2) {
			player->dir = dir;
			return;
		}
	}

	int chance = 2 + game_level;
	if (check_obstacle(player->pos_x, player->pos_y, player->dir, 6) > 6 || rand() % chance == 0) {
		return;
	}

	/*if (rand() % 128 == 0) {
		player->dir = rand() % 4;
		return;
	}*/

	int dists[] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		dists[i] = check_obstacle(player->pos_x, player->pos_y, i, 6);
	}
	int max_dist = -1;
	enum dir_t best_dir;
	if (rand() % 2) {
		for (int i = 0; i < 4; i++) {
			if (dists[i] > max_dist) {
				max_dist = dists[i];
				best_dir = i;
			}
		}
	} else {
		for (int i = 3; i >= 0; i--) {
			if (dists[i] > max_dist) {
				max_dist = dists[i];
				best_dir = i;
			}
		}
	}
	player->dir = best_dir;
}

void apply_dir_to_pos(int *x, int *y, enum dir_t dir) {
	switch (dir) {
	case DIR_DOWN:
		(*y)++;
		break;
	case DIR_UP:
		(*y)--;
		break;
	case DIR_RIGHT:
		(*x)++;
		break;
	case DIR_LEFT:
		(*x)--;
		break;
	}
}

struct player_t *player_by_color(unsigned char color) {
	for (int i = 0; i < players_count; i++) {
		if (players[i].color == color) {
			return &players[i];
		}
	}
	return NULL;
}

void crash(struct player_t *player) {
	player->move_timer = 0;
	player->round_rank = players_not_eliminated_count;
	players_not_eliminated_count--;
	player->state = CRASHED_STATE;
	update_status_bar();
#ifdef SUPPORTS_NETWORK
	for (int i = 0; i < players_count; i++) {
		if (players[i].control == CLIENT_CONTROL) {
			send_crash(players[i].socket, player);
		}
	}
#endif
}

struct exploding_bomb_t {
	int pos_x;
	int pos_y;
	int counter;
};

static bool pos_in_bounds(int x, int y) {
	return x > 1 && x < map_width - 1 && y > 1 && y < map_height - 1;
}

static struct exploding_bomb_t bomb = {0};

static void update_exploding_bombs() {
	if (bomb.counter == 0) {
		return;
	}
	if (bomb.counter == 5 || bomb.counter == 1) {
		int radius = 4;
		for (int x = bomb.pos_x - radius; x < bomb.pos_x + radius; x++) {
			for (int y = bomb.pos_y - radius; y < bomb.pos_y + radius; y++) {
				if (
					!pos_in_bounds(x, y) ||
					(bomb.counter == 1 && map_get_char(x, y) != '*')
				) {
					continue;
				}
				map_set(x, y, ' ', BRIGHT_WHITE);
			}
		}
	}
	if (bomb.counter > 1 && bomb.counter < 5) {
		const int radius = 2 + (4 - bomb.counter);
		for (int x = bomb.pos_x - radius; x < bomb.pos_x + radius; x++) {
			for (int y = bomb.pos_y - radius; y < bomb.pos_y + radius; y++) {
				if (
					!pos_in_bounds(x, y) ||
					!(rand() % 3)
				) {
					continue;
				}
				unsigned char color = rand() % 2 ? BRIGHT_YELLOW : BRIGHT_RED;
				map_set(x, y, '*', color);
			}
		}
	}
	bomb.counter--;
}

static void explode_bomb(int origin_x, int origin_y) {
	if (bomb.counter != 0) {
		bomb.counter = 1;
		update_exploding_bombs();
	}
	bomb.pos_x = origin_x;
	bomb.pos_y = origin_y;
	bomb.counter = 5;
	update_exploding_bombs();
}

static void erase_player(struct player_t *player) {
	bool human_player_exists = false;
	for (int i = 0; i < players_count; i++) {
		if (
			players[i].control != AI_CONTROL &&
			players[i].state == PLAYING_STATE
		) {
			human_player_exists = true;
			break;
		}
	}
	if (!human_player_exists && player->control != AI_CONTROL) {
		for (int i = 0; i < players_count; i++) {
			players[i].speed *= 8;
#ifdef SUPPORTS_NETWORK
			for (int j = 0; j < players_count; j++) {
				if (players[j].control == CLIENT_CONTROL) {
					send_speed(players[j].socket, &players[i]);
				}
			}
#endif
		}
	}
	for (int i = 0; i < map_width * map_height; i++) {
		unsigned char color = map_buffer[i * 2 + 1];
		if (color == player->color) {
			map_buffer[i * 2] = ' ';
			map_buffer[i * 2 + 1] = BRIGHT_WHITE;
		}
	}
	player->state = ERASED_STATE;
	update_status_bar();
}

static bool move_player(struct player_t *player) {
	int threshold = 1000 / player->speed;

	if (player->dir == DIR_UP || player->dir == DIR_DOWN) {
		threshold = threshold * 2;
	}
	while (player->move_timer > threshold) {
		if (player->control == AI_CONTROL) {
			ai_control(player);
		}
		int new_x = player->pos_x;
		int new_y = player->pos_y;
		apply_dir_to_pos(&new_x, &new_y, player->dir);

		unsigned char tile = map_get_char(new_x, new_y);
		if (is_tile_solid(tile)) {
			if (
				!player->has_bomb ||
				!pos_in_bounds(player->pos_x, player->pos_y)
			) {
				update_status_bar();
				draw_player(player);
				return false;
			}
			explode_bomb(player->pos_x, player->pos_y);
			player->has_bomb = false;
		}

		if (tile == '@') {
			player->has_bomb = true;
		}

		player->move_timer -= threshold;

#ifdef SUPPORTS_NETWORK
		if (new_x - player->old_x != 0 && new_y - player->old_y != 0) {
			int turn_x = player->pos_x;
			int turn_y = player->pos_y;
			for (int i = 0; i < players_count; i++) {
				if (players[i].control == CLIENT_CONTROL) {
					send_turn(players[i].socket, player, turn_x, turn_y);
				}
			}
			if (server_sock != -1) {
				send_turn(server_sock, player, turn_x, turn_y);
			}
		}
#endif
		
		int old_old_x = player->old_x;
		int old_old_y = player->old_y;
		player->old_x = player->pos_x;
		player->old_y = player->pos_y;
		player->pos_x = new_x;
		player->pos_y = new_y;
		draw_player(player);
		draw_player_trail(player, old_old_x, old_old_y);
	}
	return true;
}

static bool update_player(struct player_t *player) {
	if (player->control == KEYBOARD_CONTROL) {
		if (!game_keyboard(player)) {
			return false;
		}
	} else if (player->control == CLIENT_CONTROL) {
#ifdef SUPPORTS_NETWORK
		read_network(player->socket);
#endif
	}
	player->move_timer += time_diff;

	switch(player->state) {
	case PLAYING_STATE:
		if(!move_player(player)) {
			if (player->control == SERVER_CONTROL) {
				player->state = CRASH_WAIT_STATE;
			} else if (
				player->control == KEYBOARD_CONTROL ||
				player->control == AI_CONTROL
			) {
#ifdef SUPPORTS_NETWORK
				if (server_sock != -1) {
					send_crash(server_sock, player);
					player->state = CRASH_WAIT_STATE;
				} else {
					crash(player);
				}
#else
				crash(player);
#endif
			}
		}
		break;
	case CRASH_WAIT_STATE:
#ifdef SUPPORTS_NETWORK
		if (server_sock == -1 && player->move_timer > 10000) {
			crash(player);
		}
#endif
		break;
	case CRASHED_STATE:
		if (player->move_timer > 2500) {
			erase_player(player);
		}
		break;
	case ERASED_STATE:
		break;
	}
	return true;
}

void draw_rect(int x, int y, int width, int height, enum color_t color) {
	screen_set(x, y, SINGLE_TOP_LEFT, color);
	screen_set(x + width - 1, y, SINGLE_TOP_RIGHT, color);
	screen_set(x + width - 1, y + height - 1, SINGLE_BOTTOM_RIGHT, color);
	screen_set(x, y + height - 1, SINGLE_BOTTOM_LEFT, color);

	for (int _x = x + 1; _x < x + width - 1; _x++) {
		screen_set(_x, y, SINGLE_HORIZONTAL, color);
		screen_set(_x, y + height - 1, SINGLE_HORIZONTAL, color);
	}

	for (int _y = y + 1; _y < y + height - 1; _y++) {
		screen_set(x, _y, SINGLE_VERTICLE, color);
		screen_set(x + width - 1, _y, SINGLE_VERTICLE, color);
	}
}

void init_players() {
	enum dir_t dirs[] = {DIR_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_LEFT, DIR_LEFT};
	for (int i = 0; i < players_count; i++) {
		struct player_t *player = &players[i];
		player->dir = dirs[i];
		player->pos_x = map_width / 2 + i / 2 * 32 - 16;
		player->pos_y = map_height / 2 + i % 2 * 16 - 8;
		int dir_x = 0;
		int dir_y = 0;
		apply_dir_to_pos(&dir_x, &dir_y, player->dir);
		player->old_x = player->pos_x - dir_x;
		player->old_y = player->pos_y - dir_y;
		player->last_turn_x = player->old_x;
		player->last_turn_y = player->old_y;
		player->speed = 20 + 8 * game_level;
		player->move_timer = 0;
		player->round_rank = 0;
		player->state = PLAYING_STATE;
		player->has_bomb = false;
	}
}

static void draw_select_ais(int count) {
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);

	int x_start = screen_width / 3 * 2;

	char *ai_count_msg = "AI count (press a number key):";
	string_to_screen(
		x_start - strlen(ai_count_msg) - 2,
		screen_height / 2 - 6, ai_count_msg, DARK_WHITE);
	draw_rect(x_start - 1, screen_height / 2 - 7, 3, 3, DARK_WHITE);
	screen_set(x_start, screen_height / 2 - 6, '0' + count, BRIGHT_WHITE);

	char *game_level_msg = "Game level (press e, m, or h):";
	string_to_screen(
		x_start - strlen(game_level_msg) - 2,
		screen_height / 2 - 3, game_level_msg, DARK_WHITE);

	char *levels[] = {"Easy", "Medium", "Hard"};
	string_to_screen(x_start, screen_height / 2 - 3, levels[game_level], BRIGHT_WHITE);
	draw_rect(x_start - 1, screen_height / 2 - 4, strlen(levels[game_level]) + 2, 3, DARK_WHITE);

	string_to_screen_centered(screen_height / 2, "Press enter to start the game", BRIGHT_GREEN);
	string_to_screen_centered(screen_height / 2 + 2, "Press the escape key to cancel", BRIGHT_RED);
	update_screen();
}

bool select_ais(int *count) {
	while (true) {
		draw_select_ais(*count);
		char ch = EOF;
		while (true) {
			ch = read_char();
			if (ch != EOF) {
				break;
			}
			usleep(200000);
		}
		if (ch >= '0' && ch <= '5') {
			*count = ch - '0';
		} else if (ch == 'e') {
			game_level = EASY_LEVEL;
		} else if (ch == 'm') {
			game_level = MEDIUM_LEVEL;
		} else if (ch == 'h') {
			game_level = HARD_LEVEL;
		} else if (ch == '\r') {
			return true;
		} else if (ch == '\e') {
			return false;
		}
	}
}

static void rank_players(struct player_t **players_ranked) {
	for (int i = 0; i < players_count; i++) {
		players_ranked[i] = &players[i];
	}
	for (int i = 0; i < players_count; i++) {
		for (int j = i; j < players_count; j++) {
			if (players_ranked[j]->score > players_ranked[i]->score) {
				struct player_t *loser = players_ranked[i];
				players_ranked[i] = players_ranked[j];
				players_ranked[j] = loser;
			}
		}
	}
}

static bool end_screen(int round, bool is_final_round) {
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);

	if (is_final_round) {
		string_to_screen_centered(2, "Game Over", DARK_WHITE);
	} else {
		char round_message[16];
		strcpy(round_message, "Round 0 results");
		round_message[6] += round;
		string_to_screen_centered(2, round_message, DARK_WHITE);
	}

	struct player_t *old_players_ranked[players_count];
	if (round > 1) {
		rank_players(old_players_ranked);
	}
	for (int i = 0; i < players_count; i++) {
		players[i].score += (players_count - players[i].round_rank);
	}
	if (round == 1) {
		rank_players(old_players_ranked);
	}

	struct player_t *players_ranked[players_count];
	rank_players(players_ranked);

	char *end_str;
	int winner_name_length;
	struct player_t *winner;
	bool is_tie = false;
	if (is_final_round) {
		if (players_ranked[0]->score == players_ranked[1]->score) {
			is_tie = true;
			end_str = "It's a tie!";
		} else {
			winner = players_ranked[0];
			end_str = " wins the game!";
			winner_name_length = strlen(winner->name);
		}
	} else {
		winner = player_by_rank(1);
		end_str = " wins the round!";
		winner_name_length = strlen(winner->name);
	}

	int x_start = (screen_width - (winner_name_length + strlen(end_str))) / 2;
	if (is_tie) {
		string_to_screen_centered(4, end_str, BRIGHT_WHITE);
	} else {
		string_to_screen(x_start, 4, winner->name, BRIGHT_WHITE);
		string_to_screen(x_start + winner_name_length, 4, end_str, BRIGHT_WHITE);
	}

	x_start = (screen_width - 11) / 2;
	for (int i = 0; i < players_count; i++) {
		struct player_t *player = players_ranked[i];
		int old_rank = 0;
		for (int j = 0; j < players_count; j++) {
			if (player == old_players_ranked[j]) {
				old_rank = j;
				break;
			}
		}
		int y = 6 + 2 * i;
		screen_set(x_start, y, '+', BRIGHT_WHITE);
		screen_set(x_start + 1, y, players_count - player->round_rank + '0', BRIGHT_WHITE);
		if (i < old_rank) {
			screen_set(x_start + 3, y, ARROW_UP, BRIGHT_GREEN);
		} else if (i > old_rank) {
			screen_set(x_start + 3, y, ARROW_DOWN, BRIGHT_RED);
		}
		if (player->score >= 10) {
			screen_set(x_start + 4, y, player->score / 10 + '0', BRIGHT_WHITE);
			screen_set(x_start + 5, y, player->score % 10 + '0', BRIGHT_WHITE);
		} else {
			screen_set(x_start + 4, y, player->score + '0', BRIGHT_WHITE);
		}
		
		string_to_screen(x_start + 7, y, player->name, player->color);
	}

	char *exit_str = "Press enter for the next round. Press escape to exit";
	if (is_final_round) {
		exit_str = "Press escape to exit.";
#ifdef SUPPORTS_NETWORK
	} else if (server_sock != -1) {
		exit_str = "Wait for the next round to start. Press escape to exit";
#endif
	}
	x_start = (screen_width - strlen(exit_str)) / 2;
	string_to_screen(x_start, screen_height - 4, exit_str, BRIGHT_WHITE);

	update_screen();

	while (true) {
#ifdef SUPPORTS_NETWORK
		if (server_sock != -1) {
			enum result_t result = read_server_start();
			if (result == RESULT_PROCEED) {
				return true;
			} else if (result == RESULT_CANCEL) {
				return false;
			}
		}
#endif
		char key = read_char();
#ifdef SUPPORTS_NETWORK
		if (key == '\r' && server_sock == -1) {
			for (int i = 0; i < players_count; i++) {
				if (players[i].control != CLIENT_CONTROL) {
					continue;
				}
				send_message(players[i].socket, NET_START, NULL, 0);
			}
#else
		if (key == '\r') {
#endif
			return true;
		} else if (key == '\e') {
			return false;
		}

		if (key == EOF) {
			usleep(100000);
		}
	}
}

static void update_bombs() {
	update_exploding_bombs();

#ifdef SUPPORTS_NETWORK
	if (server_sock != -1) {
		return;
	}
#endif

	static int place_bomb_timer = 1000;
	place_bomb_timer -= time_diff;
	if (place_bomb_timer > 0) {
		return;
	}
	place_bomb_timer = 500 + rand() % 5000;
	int x = 1 + rand() % (map_width - 2);
	int y = 1 + rand() % (map_height - 2);
	if (map_get_char(x, y) == ' ') {
		map_set(x, y, '@', DARK_WHITE);
#ifdef SUPPORTS_NETWORK
		for (int i = 0; i < players_count; i++) {
			if (players[i].control == CLIENT_CONTROL) {
				send_bomb(players[i].socket, x, y);
			}
		}
#endif
	}
}

static void game_sleep() {
	static int old_sleep_time = 0;
	int sleep_time = (50 - (time_diff - old_sleep_time));
	sleep_time = sleep_time < 0 ? 0 : sleep_time;
	usleep(sleep_time * 1000);
	old_sleep_time = sleep_time;
}

static bool run_round(int round, bool is_final_round) {
	players_not_eliminated_count = players_count;
	init_players();
	clear_screen();
	init_map();
	update_status_bar();
	char *messages[] = {"Round 0", "READY!", "SET!", "GO!"};
	char round_msg[8];
	strcpy(round_msg, messages[0]);
	round_msg[6] += round;
	messages[0] = round_msg;
	for (int i = 0; i < 4; i++) {
		for (int i = 0; i < players_count; i++) {
			draw_player(&players[i]);
		}
		map_to_screen(true);
		int len = strlen(messages[i]);
		draw_rect((screen_width - len) / 2 - 1, screen_height / 4, len + 2, 3, BRIGHT_GREEN);
		string_to_screen_centered(screen_height / 4 + 1, messages[i], BRIGHT_WHITE);
		update_screen();
		usleep(500000);
	}

	int end_round_timer = 0;
	update_time();
	while (true) {
		update_time();

#ifdef SUPPORTS_NETWORK
		if (server_sock != -1) {
			if (!read_network(server_sock)) {
				return false;
			}
		}
#endif

		for (int i = 0; i < players_count; i++) {
			if (!update_player(&players[i])) {
				return false;
			}
		}
		update_bombs();

		if (players_not_eliminated_count <= 1) {
			end_round_timer += time_diff;
			if (end_round_timer > 4000) {
				struct player_t *player = player_by_rank(0);
				if (player) {
					player->round_rank = 1;
				}
				return end_screen(round, is_final_round);
			}
		}

		map_to_screen(true);
		update_screen();
		game_sleep();
	}
}

void game() {
	for (int i = 1; i <= 4; i++) {
		if (!run_round(i, i == 4)) {
#ifdef SUPPORTS_NETWORK
			kick_all_clients();
#endif
			return;
		}
	}
}

void text_input_field(char *message, char *out, int max_length) {
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);
	string_to_screen((screen_width - strlen(message)) / 2, 2, message, DARK_WHITE);
	draw_rect((screen_width - max_length) / 2 - 1, 4, max_length + 2, 3, BRIGHT_WHITE);

	string_to_screen_centered(8, "Press the escape key to cancel.", DARK_WHITE);
	string_to_screen_centered(10, "Press the enter key when done.", DARK_WHITE);
	update_screen();

	while (true) {
		char ch = read_char();
		if (ch == '\r') {
			return;
		} else if (ch == '\e') {
			out[0] = '\0';
			return;
		} else if (ch == '\x7f' && out[0] != '\0') {
			out[strlen(out) - 1] = '\0';
		} else if (ch >= 32 && ch <= 126) {
			int out_len = strlen(out);
			if (out_len < max_length) {
				out[out_len] = ch;
			}
		} else if (ch == EOF) {
			int x_start = (screen_width - max_length) / 2;
			for (int x = x_start; x < x_start + max_length; x++) {
				screen_set(x, 5, ' ', DARK_WHITE);
			}

			if (current_time % 1024 > 512) {
				screen_set(
					x_start + strlen(out),
					5, SINGLE_VERTICLE, BRIGHT_WHITE);
			}
			string_to_screen(x_start, 5, out, BRIGHT_WHITE);

			update_screen();
			usleep(50000);
			update_time();
		}
	}
}

static void single_player_game() {
	int ai_count = 5;
	if (!select_ais(&ai_count)) {
		return;
	}

	add_player("User", COLOR_AUTO, KEYBOARD_CONTROL);

	char *names[] = {"Bob", "Luke", "Kate", "Jane", "Fred"};
	for (int i = 0; i < ai_count; i++) {
		add_player(names[i], COLOR_AUTO, AI_CONTROL);
	}
	game();
}

static void info_screen() {
	char *message =
		"You are the driver of a lightcycle. "
		"Try not to crash into the trail left by your lightcycle or other lightcycles. "
		"The game objective is to last as long as possible without crashing. "
		"Use the arrow keys to steer the lightcycle. "
		"Try to collect bombs (@). "
		"If you have a bomb, you can explode trails by crashing into them.\n\n"
		"Tractron verson " TRACTRON_VERSON "\n"
		"Created by Johannes Fritz\n"
		"https://github.com/MrRar/tractron";

	int message_len = strlen(message);
	
	clear_screen();
	draw_rect(0, 0, screen_width, screen_height, DARK_WHITE);

	int margin = 3;
	int line_start = 0;
	int y = margin - 1;
	while (true) {
		int line_end = line_start + screen_width - margin * 2;
		if (line_end < message_len) {
			for (int i = line_end; i > line_end - 20; i--) {
				if (message[i] == ' ') {
					line_end = i;
					break;
				}
			}
		} else {
			line_end = message_len;
		}
		for (int i = line_start; i < line_end; i++) {
			if (message[i] == '\n') {
				line_end = i;
				break;
			}
		}
		for (int i = line_start; i < line_end; i++) {
			screen_set(i - line_start + margin, y, message[i], BRIGHT_WHITE);
		}
		
		if (line_end == message_len) {
			break;
		}
		line_start = line_end + 1;
		y += 2;
	}

	string_to_screen_centered(screen_height - 3, "Press enter to continue", BRIGHT_WHITE);

	update_screen();
	while (read_char() != '\r') {
		usleep(100000);
	}
}

static void draw_main_menu() {
	map_to_screen(false);

	char *title = 
		" ______             __              "
		"/_  __/______ _____/ /________  ___ "
		" / / / __/ _ `/ __/ __/ __/ _ \\/ _ \\"
		"/_/ /_/  \\_,_/\\__/\\__/_/  \\___/_//_/";
	int title_height = 4;

	unsigned char title_colors[] = {BRIGHT_YELLOW, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_BLUE};
	unsigned char title_color = title_colors[current_time % 4000 / 1000];

	int title_str_len = strlen(title);
	int title_width = title_str_len / title_height;
	int x_start = (screen_width - title_width) / 2;
	for (int i = 0; i < title_str_len; i++) {
		screen_set(x_start + i % title_width, i / title_width + 1, title[i], title_color);
	}

	int x = current_time / 32 % (title_width * 2);
	for (int y = 0; y < title_height; y++) {
		for (int i = 0; i < 2; i++) {
			if (i == 1) {
				x--;
			}
			if (x < 0 || x >= title_width) {
				continue;
			}
			char ch = title[y * title_width + x];
			screen_set(x_start + x, y + 1, ch, BRIGHT_WHITE);
		}
	}
	
	char *options[] = {
		"Press 1 for single player game",
#ifdef SUPPORTS_NETWORK
		"Press 2 to join a server",
		"Press 3 to start a server",
#endif
		"Press 4 for information",
		"Press the escape key to exit"
	};

	int options_count = sizeof(options) / sizeof(options[0]);
	int x_offset = title_height + (screen_height - title_height - options_count * 2) / 2;

	for (int i = 0; i < options_count; i++) {
		string_to_screen_centered(x_offset + i * 2, options[i], BRIGHT_WHITE);
	}
	
}

static void init_menu() {
	clear_players();
	for (int i = 0; i < 6; i++) {
		add_player("AI", COLOR_AUTO, AI_CONTROL);
	}
	init_players();
	clear_screen();
	init_map();
}

static void main_menu() {
	init_menu();
	while(true) {
		update_time();
		unsigned char key = read_char();
		if (key == '1') {
			clear_players();
			single_player_game();
#ifdef SUPPORTS_NETWORK
		} else if (key == '2') {
			clear_players();
			client();
		} else if (key == '3') {
			clear_players();
			server();
#endif
		} else if (key == '4') {
			info_screen();
		} else if (key == '\e') {
			return;
		}
		for (int i = 0; i < players_count; i++) {
			update_player(&players[i]);
		}
		if (players_not_eliminated_count == 0) {
			init_menu();
		}
		draw_main_menu();
		update_screen();
		game_sleep();
	}
}

int main(int argc, char *argv[]) {
#ifdef __COSMOCC__
	ShowCrashReports();
	if (IsWindows()) {
		windows_init();
	}
#endif
	setup_terminal();
	main_menu();
	reset_terminal();
}
