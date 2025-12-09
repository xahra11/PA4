#include <stdio.h>    
#include <stdlib.h>   
#include <stdbool.h>
#include <unistd.h>  
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>   
#include <sys/select.h>
#include <sys/wait.h>

#include "message.h"
#include "send.h"
#include "handlers.h"

Player **active_players = NULL; // dynamic list of all players currently in an active game
int total_active = 0;
int active_cap = 0;

// function prototypes
void nimd_game(Player *p1, Player *p2);
void create_game(Game *g, Player *p1, Player *p2);
bool is_active(const char *name);
void add_active(Player *p);
void remove_active(Player *p);

int main(int argc, char *argv[]) {
    int sock_fd, port_number;
    struct sockaddr_in addr;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    port_number = atoi(argv[1]);
    if (port_number <= 0) {
        fprintf(stderr, "Invalid port number.\n");
        exit(EXIT_FAILURE);
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_number);

    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(EXIT_FAILURE); }
    if (listen(sock_fd, 5) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    printf("nimd server listening on port %d\n", port_number);

    Player *queue_player = NULL;

    while (1) {
        while (waitpid(-1, NULL, WNOHANG) > 0) ; // reap zombies

        // Check if queued player disconnected
        if (queue_player) {
            char tmp;
            int r = recv(queue_player->fd, &tmp, 1, MSG_PEEK | MSG_DONTWAIT);
            if (r == 0) {
                remove_active(queue_player);
                close(queue_player->fd);
                free(queue_player);
                queue_player = NULL;
            }
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("nimd server accepted connection from client\n");

        Message *msg = read_message(client_fd);
        if (!msg) {
            handle_fail_fd(client_fd, 10, "Invalid");
            close(client_fd);
            continue;
        }

        if (strcmp(msg->type, "MOVE") == 0) {
            handle_fail_fd(client_fd, 24, "Not Playing");
            close(client_fd);
            free_message(msg);
            continue;
        }

        if (strcmp(msg->type, "OPEN") != 0) {
            handle_fail_fd(client_fd, 10, "Invalid");
            close(client_fd);
            free_message(msg);
            continue;
        }

        Player *player = malloc(sizeof(Player));
        player->fd = client_fd;
        player->open = false;
        player->p_num = 0;

        handle_open(player, msg);
        free_message(msg);

        if (!player->open) { // already handled fail
            close(client_fd);
            free(player);
            continue;
        }

        if (!queue_player) {
            queue_player = player;
            send_wait(queue_player->fd);
            printf("A player is waiting for an opponent\n");
            fflush(stdout);
        } else {
            // Match found
            Player *p1 = queue_player;
            Player *p2 = player;
            queue_player = NULL;

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                // fail both players
                handle_fail(p1, 50, "Server Error");
                handle_fail(p2, 50, "Server Error");
                close(p1->fd); close(p2->fd);
                remove_active(p1); remove_active(p2);
                free(p1); free(p2);
                continue;
            }

            if (pid == 0) {
                // Child process handles the game
                close(sock_fd); // child does not accept new connections
                nimd_game(p1, p2);
                exit(0);
            }

            // Parent does NOT close p1/p2 FDs; child owns them
        }
    }

    close(sock_fd);
    return 0;
}

void nimd_game(Player *p1, Player *p2) {
    Game g;
    create_game(&g, p1, p2);

    send_name(p1->fd, 1, p2->name);
    send_name(p2->fd, 2, p1->name);
    send_play(&g);

    while (1) {
        Player *current_p = (g.next_p == 1) ? p1 : p2;

        Message *msg = read_message(current_p->fd);
        if (!msg) {
            Player *winner = (g.next_p == 1) ? p2 : p1;
            send_over(&g, winner->p_num, "Forfeit");
            break;
        }

        if (strcmp(msg->type, "MOVE") != 0) {
            handle_fail(current_p, 31, "Expected MOVE");
            free_message(msg);
            continue;
        }

        handle_move(&g, current_p, msg);
        free_message(msg);

        bool empty = true;
        for (int i = 0; i < 5; i++)
            if (g.piles[i] > 0) { empty = false; break; }

        if (empty) break;
    }

    remove_active(p1); remove_active(p2);
    close(p1->fd); close(p2->fd);
    free(p1); free(p2);
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

bool is_active(const char *name){
    for(int i = 0; i < total_active; i++){
        if(active_players[i] && strcmp(active_players[i]->name, name) == 0){
            return true;
        }
    }
    return false;
}

void add_active(Player *p){
    if(total_active >= active_cap){
        int new_cap = (active_cap == 0) ? 20 : active_cap * 2;
        Player **new_list = realloc(active_players, new_cap * sizeof(Player*));
        if(!new_list){
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        active_players = new_list;
        active_cap = new_cap;
    }
    active_players[total_active++] = p;
}

void remove_active(Player *p){
    for(int i = 0; i < total_active; i++){
        if(active_players[i] == p){
            memmove(&active_players[i], &active_players[i + 1], (total_active - i - 1)*sizeof(Player*));
            total_active--;
            return;
        }
    }
}