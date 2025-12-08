#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

//nimd test file:
//spawns nimd server via fork and creates client connections to simulate test cases

#define SERVER_PATH "./nimd_server" 
#define TEST_PORT 34567
#define BUF 512

//connect to server
int connect_client() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }
    return fd;
}

//read server response
int get_msg(int fd, char *out) {
    int n = read(fd, out, BUF-1);
    if (n <= 0) return -1;
    out[n] = 0;
    return n;
}

//send raw message
void send_raw(int fd, const char *s) {
    write(fd, s, strlen(s));
}

//test 1: bad message format
void test_bad_format() {
    printf("\n-- Test: BAD FORMAT MESSAGE --\n");
    int fd = connect_client();
    
    //invalid length
    send_raw(fd, "0|XX|BAD|MESSAGE|");   

    char msg[BUF];
    if (get_msg(fd, msg) > 0)
        printf("Server replied: %s\n", msg);
    close(fd);
}

//test 2: incorrect framing
void test_incorrect_frame() {
    printf("\n-- Test: INCORRECT FRAMING --\n");
    int fd = connect_client();

    send_raw(fd, "thisIsNotAFrame");

    char msg[BUF];
    if (get_msg(fd, msg) > 0)
        printf("Server replied: %s\n", msg);
    close(fd);
}

//test 3: wrong time
void test_wrong_time() {
    printf("\n-- Test: MESSAGE AT WRONG TIME --\n");
    int fd = connect_client();

    //try to MOVE before OPEN
    send_raw(fd, "0|09|MOVE|1|1|");

    char buf[BUF];
    get_msg(fd, buf);
    printf("Server replied: %s\n", buf);

    close(fd);
}

//test 4: invalid move
void test_invalid_move() {
    printf("\n-- Test: INVALID NIM MOVE --\n");
    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|10|OPEN|Alice|");
    get_msg(fd1, (char[BUF]){0}); //WAIT

    send_raw(fd2, "0|09|OPEN|Bob|");
    get_msg(fd1, (char[BUF]){0}); //NAME
    get_msg(fd2, (char[BUF]){0});

    get_msg(fd1, (char[BUF]){0}); //PLAY
    get_msg(fd2, (char[BUF]){0});

    //Alice tries to remove from nonexistent pile 10
    send_raw(fd1, "0|11|MOVE|10|1|");

    char r[BUF];
    get_msg(fd1, r);
    printf("Server replied: %s\n", r);

    close(fd1);
    close(fd2);
}

//test 5: client matching
void test_matchmaking() {
    printf("\n-- Test: MATCHMAKING --\n");

    int fd1 = connect_client();
    send_raw(fd1, "0|11|OPEN|Zed|");

    char buf[BUF];
    get_msg(fd1, buf);
    printf("Player 1 WAIT: %s\n", buf);

    int fd2 = connect_client();
    send_raw(fd2, "0|12|OPEN|Luna|");

    get_msg(fd1, buf);
    printf("Player 1 NAME: %s\n", buf);

    get_msg(fd2, buf);
    printf("Player 2 NAME: %s\n", buf);

    get_msg(fd1, buf);
    printf("Player 1 PLAY: %s\n", buf);

    get_msg(fd2, buf);
    printf("Player 2 PLAY: %s\n", buf);

    close(fd1);
    close(fd2);
}

//test 6: run a full game
void test_full_game() {
    printf("\n-- Test: FULL GAME --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|11|OPEN|AAA|");
    get_msg(fd1,(char[BUF]){0});

    send_raw(fd2, "0|11|OPEN|BBB|");
    get_msg(fd1,(char[BUF]){0}); //NAME
    get_msg(fd2,(char[BUF]){0});

    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    //play deterministic game:
    //pile states: 1 3 5 7 9
    send_raw(fd1, "0|09|MOVE|5|9|"); //remove all 9
    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    send_raw(fd2, "0|09|MOVE|4|7|");
    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    send_raw(fd1, "0|09|MOVE|3|5|");
    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    send_raw(fd2, "0|08|MOVE|2|3|");
    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    send_raw(fd1, "0|07|MOVE|1|1|");

    //expect game to end: fd1 should win
    char final[BUF];
    get_msg(fd1, final);
    printf("Final message to P1: %s\n", final);

    get_msg(fd2, final);
    printf("Final message to P2: %s\n", final);

    close(fd1);
    close(fd2);
}

//test 7: disconnect before game
void test_disconnect_before_game() {
    printf("\n-- Test: DISCONNECT BEFORE MATCH --\n");

    int fd = connect_client();
    send_raw(fd, "0|11|OPEN|Solo|");

    char buf[BUF];
    get_msg(fd, buf);
    printf("WAIT message: %s\n", buf);

    printf("Client disconnecting...\n");
    close(fd);

    sleep(1); //allow server to process
}

//test 8: disconnect during game
void test_disconnect_during_game() {
    printf("\n-- Test: DISCONNECT DURING GAME --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|11|OPEN|A|");
    get_msg(fd1,(char[BUF]){0});

    send_raw(fd2, "0|11|OPEN|B|");
    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    get_msg(fd1,(char[BUF]){0});
    get_msg(fd2,(char[BUF]){0});

    printf("Client 1 disconnects mid-game!\n");
    close(fd1);

    char buf[BUF];
    get_msg(fd2, buf);
    printf("Server reply to remaining player: %s\n", buf);

    close(fd2);
}

//test 9: concurrent games
void test_concurrent_and_duplicate() {
    printf("\n-- Test: CONCURRENT GAMES --\n");

    //game 1 with players A1 and A2
    int A1 = connect_client();
    int A2 = connect_client();

    send_raw(A1, "0|11|OPEN|A1|");
    get_msg(A1, (char[BUF]){0}); //WAIT

    send_raw(A2, "0|11|OPEN|A2|");
    get_msg(A1, (char[BUF]){0}); //NAME
    get_msg(A2, (char[BUF]){0}); //NAME

    get_msg(A1, (char[BUF]){0}); //PLAY
    get_msg(A2, (char[BUF]){0}); //PLAY

    //game 2 with players B1 and B2
    int B1 = connect_client();
    int B2 = connect_client();

    send_raw(B1, "0|11|OPEN|B1|");
    get_msg(B1, (char[BUF]){0}); //WAIT

    send_raw(B2, "0|11|OPEN|B2|");
    get_msg(B1, (char[BUF]){0}); //NAME
    get_msg(B2, (char[BUF]){0}); //NAME

    get_msg(B1, (char[BUF]){0}); //PLAY
    get_msg(B2, (char[BUF]){0}); //PLAY

    printf("Attempting duplicate name A1...\n");

    //try connecting a third client with a duplicate name A1
    int dup = connect_client();
    send_raw(dup, "0|11|OPEN|A1|");

    char reply[BUF];
    if (get_msg(dup, reply) > 0)
        printf("Duplicate-name reply: %s\n", reply);
    else
        printf("Duplicate name connection unexpectedly closed!\n");

    close(dup);

    //clean up  4 real players
    send_raw(A1, "0|07|MOVE|1|1|");
    get_msg(A1,(char[BUF]){0});
    get_msg(A2,(char[BUF]){0});

    send_raw(B1, "0|07|MOVE|1|1|");
    get_msg(B1,(char[BUF]){0});
    get_msg(B2,(char[BUF]){0});

    close(A1);
    close(A2);
    close(B1);
    close(B2);
}

int main() {
    printf("NIMD TEST\n");
    test_bad_format();
    test_incorrect_frame();
    test_wrong_time();
    test_invalid_move();
    test_matchmaking();
    test_full_game();
    test_disconnect_before_game();
    test_disconnect_during_game();
    test_concurrent_and_duplicate();
    printf("\nTESTING COMPLETE\n");
    return 0;
}
