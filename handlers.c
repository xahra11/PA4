#include "handlers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

void handle_open(Player *p, Message *msg){
    if(!msg || msg->field_num < 1){
        handle_fail(p, 10, "Invalid");
        return;
    }

    char *name = msg->fields[0];

    if(!is_valid_name(name)){
        handle_fail(p, 21, "Long Name or Invalid");
        return;
    }

    if(p->open){
        handle_fail(p, 23, "Already Open");
        return;
    }

    if(is_active(name)){
        handle_fail(p, 22, "Already Playing");
        return;
    }

    strncpy(p->name, name, 72);
    p->name[72] = '\0';
    p->open = true;

    add_active(p);

    send_wait(p->fd);
    printf("Player '%s' is waiting for an opponent\n", p->name);
}

void handle_move(Game *g, Player *p, Message *msg){
    if(!g || !p || !msg || msg->field_num < 2){
        handle_fail(p, 10, "Invalid");
        return;
    }

    // it is not the player's turn
    if(g->next_p != p->p_num){
        handle_fail(p, 31, "Impatient");
        return;
    }

    // check if move is valid
    int pile = atoi(msg->fields[0]);
    int quantity = atoi(msg->fields[1]);

    if(pile < 1 || pile > 5){
        handle_fail(p, 32, "Pile Index");
        return;
    }

    if(quantity < 1 || quantity > g->piles[pile - 1]){
        handle_fail(p, 33, "Quantity");
        return;
    }

    // make valid move
    g->piles[pile - 1] -= quantity;
    printf("Player %s removed %d from pile %d\n", p->name, quantity, pile);

    // check if player won and all piles are empty
    bool empty = true;
    for(int i = 0; i < 5; i++){
        if(g->piles[i] > 0){
            empty = false;
            break;
        }
    }

    if(empty){
        send_over(g, p->p_num, "");
        return;
    }

    // change player's turn, then send game to both
    g->next_p = (p->p_num == 1) ? 2 : 1;
    send_play(g);
}

void handle_fail(Player *p, int code, const char *msg){
    if(!p){
        return;
    }

    send_fail(p->fd, code, msg);
    printf("Sent FAIL to player %s because %s (%d)\n", p->name, msg, code);
}

void handle_fail_fd(int fd, int code, const char *msg){
    if(fd < 0) {
        return;
    }
    send_fail(fd, code, msg);
    printf("Sent FAIL to fd %d because %s (%d)\n", fd, msg, code);
}