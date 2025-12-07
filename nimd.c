#include <stdio.h>    
#include <stdlib.h>   
#include <stdbool.h>
#include <unistd.h>  
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>   

#include "message.h"
#include "send.h"
#include "handlers.h"

// function prototypes
void nimd_game(int p1, int p2);
void create_game(Game *g, Player *p1, Player *p2);

int main(int argc, char *argv[]) {

    int sock_fd, port_number;
    struct sockaddr_in addr;

    if(argc != 2){ // only argument should be port number
        fprintf(stderr, "Only needs one argument, which is port number.");
        exit(EXIT_FAILURE);
    }else{
        port_number = atoi(argv[1]);
    }

    if(port_number <= 0){
        fprintf(stderr, "Invalid port number.");
        exit(EXIT_FAILURE);
    }

    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    addr.sin_port = htons(port_number);

    if(bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sock_fd, 3) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("nimd server listening on port %d\n", port_number);
    bool queue = false; // no one is in queue to play
    int queue_fd = -1;

    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);

        if(client_fd < 0){
            perror("accept");
            continue;
        }

        printf("nimd server accepted connection from client\n");

        if(!queue){
            printf("A player is waiting for an opponent\n");
            queue_fd = client_fd; // fd of player waiting in queue to be matched
            queue = true; 
        }else{
            int p1_fd = queue_fd;
            int p2_fd = client_fd;
            queue = false; // players are paired, queue resets

            printf("2 players have been matched, nimd game has started\n");

            pid_t pid = fork();

            if(pid < 0){
                perror("fork");
                close(p1_fd);
                close(p2_fd);
                continue;
            }

            if(pid == 0){
                // child process to run game in background
                close(sock_fd);
                nimd_game(p1_fd, p2_fd);
                exit(0);
            }

            // close fds to accept two new players
            close(p1_fd);
            close(p2_fd);
        }

    }

    close(sock_fd);
    return 0;

}

void nimd_game(int p1_fd, int p2_fd){
    Player p1 ={
        .fd = p1_fd,
        .p_num = 1,
        .open = false
    };

    Player p2 ={
        .fd = p2_fd,
        .p_num = 2,
        .open = false
    };

    Game g;
    create_game(&g, &p1, &p2);
    Message *msg = NULL;

    // read OPEN message from player 1, get name
    msg = read_message(p1_fd);
    if(!msg || strcmp(msg->type, "OPEN") != 0){
        handle_fail(&p1, 10, "Expected OPEN");
        if(msg){
            free_message(msg);
        }
        return;
    }

    handle_open(&p1, msg);
    free_message(msg);
    msg = NULL;

    // read OPEN message from player 2, get name
    msg = read_message(p2_fd);
    if(!msg || strcmp(msg->type, "OPEN") != 0){
        handle_fail(&p2, 10, "Expected OPEN");
        if(msg){
            free_message(msg);
        }
        return;
    }

    handle_open(&p2, msg);
    free_message(msg);
    msg = NULL;

    // send opponent's name to both players
    send_name(p1_fd, 1, p2.name);
    send_name(p2_fd, 2, p1.name);

    send_play(&g);
    Player *current_p;

    while(1){
        current_p = (g.next_p == 1) ? &p1 : &p2;

        msg = read_message(current_p->fd);
        if(!msg){
            Player *winner = (g.next_p == 1) ? &p2 : &p1;
            send_over(&g, winner->p_num, "Forfeit"); // forfeit, no message
            break;
        }

        if(strcmp(msg->type, "MOVE") != 0){
            handle_fail(current_p, 31, "Expected MOVE");
            free_message(msg);
            msg = NULL;
            continue;
        }

        handle_move(&g, current_p, msg);
        free_message(msg);
        msg = NULL;
    }

    close(p1_fd);
    close(p2_fd);
}

void create_game(Game *g, Player *p1, Player *p2){
    if(!g){
        return;
    }
    g->p1 = p1;
    g->p2 = p2;
    g->piles[0] = 1;
    g->piles[1] = 3;
    g->piles[2] = 5;
    g->piles[3] = 7;
    g->piles[4] = 9;
    g->next_p = 1;
}


