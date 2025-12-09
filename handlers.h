#ifndef HANDLERS_H
#define HANDLERS_H

#include "message.h"
#include "send.h"

//#define MAX_MSG_LENGTH 72

typedef struct Player {
    int fd;
    char name[73];
    int p_num; // 1 or 2
    bool open;
} Player;

typedef struct Game {
    Player *p1;
    Player *p2;
    int piles[5];     // 5 piles of 1, 3, 5, 7, 9
    int next_p;  // p_num of whose turn it is
} Game;

bool is_active(const char *name);
void add_active(Player *p);
void remove_active(Player *p);

void handle_open(Player *p, Message *msg);
void handle_move(Game *g, Player *p, Message *msg);
void handle_fail(Player *p, int code, const char *msg);
void handle_fail_fd(int fd, int code, const char *msg);

#endif 
