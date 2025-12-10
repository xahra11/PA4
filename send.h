#ifndef SEND_H
#define SEND_H

struct Game;
typedef struct Game Game;
struct Player;
typedef struct Player Player;

int send_wait(int fd);
int send_name(int fd, int p_num, const char *opp_name);
int send_play(Game *g);
int send_play_single(Player *p, Game *g);
int send_over(Game *g, int winner, const char *reason);
int send_fail(int fd, int code, const char *msg);

#endif
