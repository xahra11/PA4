#include "send.h"
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int format_message(int fd, const char *msg){
    int msg_len = strlen(msg);
    if(msg_len > 104){ // max length is 104
        return -1;
    }

    char header[16];
    snprintf(header, sizeof(header), "0|%02d|", msg_len);

    int total_len = strlen(header) + msg_len;
    char *buf = malloc(total_len + 1);
    if(!buf){
        return -1;
    }

    strcpy(buf, header); // copy header to buffer
    strcat(buf, msg); // concatenate msg to buffer

    // write to file descriptor
    ssize_t bytes = write(fd, buf, total_len);
    free(buf);

    if(bytes == (ssize_t)total_len){
        return 0;
    }else{
        return -1;
    }
}

void format_board(char *buf, int buf_size, int piles[5]){
    int ptr = 0;
    for(int i = 0; i < 5; i++){
        ptr += snprintf(buf+ptr, buf_size-ptr, "%d", piles[i]);
        if(i < 4){
            ptr += snprintf(buf+ptr, buf_size-ptr, " ");
        }
    }
    buf[buf_size - 1] = '\0';
}

int send_wait(int fd){
    return format_message(fd, "WAIT|");
}

int send_name(int fd, int p_num, const char *opp_name){
    char msg[200];
    snprintf(msg, sizeof(msg), "NAME|%d|%s|", p_num, opp_name ? opp_name : "");
    return format_message(fd, msg);
}

int send_play(Game *g){
    if(!g || !g->p1 || !g->p2){
        return -1;
    }

    char board[100];
    format_board(board, sizeof(board), g->piles);

    char msg[200];
    snprintf(msg, sizeof(msg), "PLAY|%d|%s|", g->next_p, board);

    int p1_reply = format_message(g->p1->fd, msg);
    int p2_reply = format_message(g->p2->fd, msg);

    if(p1_reply == 0 && p2_reply == 0){
        return 0;
    }else{
        return -1;
    }
}

int send_over(Game *g, int winner, const char *reason){
    if(!g || !g->p1 || !g->p2){
        return -1;
    }

    char board[100];
    format_board(board, sizeof(board), g->piles);

    char msg[200];
    snprintf(msg, sizeof(msg), "OVER|%d|%s|%s|", winner, board, reason ? reason: "");

    int p1_reply = format_message(g->p1->fd, msg);
    int p2_reply = format_message(g->p2->fd, msg);

    if(p1_reply == 0 && p2_reply == 0){
        return 0;
    }else{
        return -1;
    }
}

int send_fail(int fd, int code, const char *msg_text){
    char buf[200];
    snprintf(buf, sizeof(buf), "FAIL|%02d %s|", code, msg_text ? msg_text : "");
    return format_message(fd, buf);
}
