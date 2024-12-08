#pragma once

#include <stdbool.h>

#define TRACTRON_VERSON "1.0"

#define screen_width 80
#define screen_height 25

#define UP_CASES case 'w': case 'k': case 'A':
#define RIGHT_CASES case 'd': case 'l': case 'C':
#define DOWN_CASES case 's': case 'j': case 'B':
#define LEFT_CASES case 'a': case 'h': case 'D':

enum dir_t {
	DIR_UP,
	DIR_RIGHT,
	DIR_DOWN,
	DIR_LEFT
};

enum color_t {
	DARK_BLACK = 0,
	BRIGHT_BLUE = 0x9,
	BRIGHT_GREEN = 0xa,
	BRIGHT_CYAN = 0xb,
	BRIGHT_RED = 0xc,
	BRIGHT_MAGENTA = 0xd,
	DARK_YELLOW = 0x6,
	BRIGHT_YELLOW = 0xe,
	DARK_WHITE = 0x7,
	BRIGHT_WHITE = 0xf,
	TEXT_BLINK = 0b10000000,
	TEXT_NO_BLINK = 0,
	COLOR_AUTO = 0xff
};

enum control_t {
	KEYBOARD_CONTROL,
	AI_CONTROL,
	CLIENT_CONTROL,
	SERVER_CONTROL
};

enum player_state_t {
	PLAYING_STATE,
	CRASH_WAIT_STATE,
	CRASHED_STATE,
	ERASED_STATE
};

enum level_t {
	EASY_LEVEL,
	MEDIUM_LEVEL,
	HARD_LEVEL,
	NO_LEVEL,
};

enum line_t {
	DOUBLE_TOP_LEFT = 0xc9, // ╔
	DOUBLE_TOP_RIGHT = 0xbb, // ╗
	DOUBLE_BOTTOM_RIGHT = 0xbc, // ╝
	DOUBLE_BOTTOM_LEFT = 0xc8, // ╚
	DOUBLE_VERTICLE = 0xba, // ║
	DOUBLE_HORIZONTAL = 0xcd, // ═
	SINGLE_VERTICLE = 0xb3, // │
	SINGLE_HORIZONTAL = 0xc4, // ─
	SINGLE_TOP_LEFT = 0xda, // ┌
	SINGLE_TOP_RIGHT = 0xbf, // ┐
	SINGLE_BOTTOM_LEFT = 0xc0, // └
	SINGLE_BOTTOM_RIGHT = 0xd9, // ┘
	ARROW_UP = 0x1e, // ▲
	ARROW_RIGHT = 0x10, // ►
	ARROW_DOWN = 0x1f, // ▼
	ARROW_LEFT = 0x11 // ◄
};

struct player_t {
	enum dir_t dir;
	int pos_x;
	int pos_y;
	int old_x;
	int old_y;
	int last_turn_x;
	int last_turn_y;
	int move_timer;
	int speed; // tiles per millisecond
	enum color_t color;
	char name[10];
	int round_rank;
	int score;
	enum player_state_t state;
	enum control_t control;
	int socket;
	bool has_bomb;
};

extern void update_time();

extern int players_not_eliminated_count;

extern int players_count;

extern struct player_t players[6];

extern unsigned char screen_buffer[screen_width * screen_height * 2];

extern unsigned int current_time;

extern enum level_t game_level;

extern char read_char();

extern void apply_dir_to_pos(int *x, int *y, enum dir_t dir);

extern void map_set(int x, int y, unsigned char ch, unsigned char color);

extern unsigned char map_get_char(int x, int y);

extern void draw_player(struct player_t *player);

extern void draw_player_trail(struct player_t *player, int old_old_x, int old_old_y);

extern struct player_t *player_by_color(unsigned char color);

extern void crash(struct player_t *player);

extern void clear_players();

extern void clear_screen();

extern void draw_rect(int x, int y, int width, int height, enum color_t color);

extern void string_to_screen_centered(int y, char *str, unsigned char color);

extern void string_to_screen(int x, int y, char *str, unsigned char color);

extern void update_screen();

extern void text_input_field(char *message, char *out, int max_length);

extern struct player_t *add_player(char *name, enum color_t color, enum control_t control);

extern bool select_ais(int *count);

extern void game();

extern void display_message(char *message);

extern int min(int a, int b);

extern int max(int a, int b);
