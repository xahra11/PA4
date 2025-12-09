#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_PATH "./nimd_server"
#define TEST_PORT 34567
#define BUF 512

//nimd test file:
//spawns nimd server via fork and creates client connections to simulate test cases

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

//send raw message
void send_raw(int fd, const char *s) {
    write(fd, s, strlen(s));
}

//read server response
int get_msg(int fd, char *out) {
    int n = read(fd, out, BUF-1);
    if (n > 0) {
        out[n] = 0;
        return n;
    }
    return n;
}

// helper that requires a message type
void expect_type(int fd, const char *type) {
    char buf[BUF];
    int n = get_msg(fd, buf);
    if (n <= 0) {
        printf("Expected %s but connection closed\n", type);
        exit(1);
    }

    if (strstr(buf, type) == NULL) {
        printf("Expected %s but got: %s\n", type, buf);
        exit(1);
    }

    printf("Got %s: %s\n", type, buf);
}

/*// ensure server closed connection for failed messages
void check_server_close(int fd, char *msg) {
    int n = get_msg(fd, msg);
    if (n > 0)
        printf("Server replied: %s\n", msg);
    else if (n == 0)
        printf("Server closed connection.\n");
    else
        printf("Error reading from server.\n");
}*/

//test 1: bad message format
void test_bad_format() {
    printf("\n-- Test: BAD FORMAT --\n");

    int fd = connect_client();
    char buf[BUF];

    send_raw(fd, "0|XX|BAD|MESSAGE|");
    get_msg(fd, buf);
    printf("Reply: %s\n", buf);
    close(fd);
}

//test 2: incorrect framing
/*void test_incorrect_frame() {
    printf("\n-- Test: INCORRECT FRAMING --\n");
    int fd = connect_client();
    char msg[BUF];

    // unframed message
    send_raw(fd, "thisIsNotAFrame");
    if (get_msg(fd, msg) > 0)
        printf("Server replied: %s\n", msg);
    check_server_close(fd, msg);
    close(fd);

    // no | at the end
    fd = connect_client();
    send_raw(fd, "0|11|OPEN|Darren");
    if (get_msg(fd, msg) > 0)
        printf("Server replied: %s\n", msg);
    check_server_close(fd, msg);
    close(fd);
}*/

//test 3: wrong time
void test_wrong_time() {
    printf("\n-- Test: WRONG TIME (MOVE before OPEN) --\n");

    int fd = connect_client();
    char buf[BUF];

    send_raw(fd, "0|09|MOVE|1|1|");
    get_msg(fd, buf);
    printf("Reply: %s\n", buf);
    close(fd);
}

//test 5: client matching
void test_matchmaking() {
    printf("\n-- Test: MATCHMAKING --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|11|OPEN|Alice|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|09|OPEN|Bob|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");

    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    close(fd1);
    close(fd2);
}

//test 4: invalid move
void test_invalid_move() {
    printf("\n-- Test: INVALID MOVE --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|11|OPEN|Alice|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|09|OPEN|Bob|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");

    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    // Alice makes invalid move
    send_raw(fd1, "0|10|MOVE|10|1|");

    expect_type(fd1, "FAIL");
    expect_type(fd1, "PLAY");

    close(fd1);
    close(fd2);
}

//test 6: run a full game
void test_full_game() {
    printf("\n-- Test: FULL GAME --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|09|OPEN|AAA|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|09|OPEN|BBB|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");

    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    // Play deterministic game
    send_raw(fd1, "0|09|MOVE|5|9|");
    expect_type(fd1, "PLAY");

    send_raw(fd2, "0|09|MOVE|4|7|");
    expect_type(fd1, "PLAY");

    send_raw(fd1, "0|09|MOVE|3|5|");
    expect_type(fd1, "PLAY");

    send_raw(fd2, "0|09|MOVE|2|3|");
    expect_type(fd1, "PLAY");

    send_raw(fd1, "0|09|MOVE|1|1|");
    expect_type(fd1, "OVER");
    expect_type(fd2, "OVER");

    close(fd1);
    close(fd2);
}

//test 7: disconnect before match
void test_disconnect_before_match() {
    printf("\n-- Test: DISCONNECT BEFORE MATCH --\n");

    int fd = connect_client();
    send_raw(fd, "0|10|OPEN|Solo|");
    expect_type(fd, "WAIT");
    close(fd);
}

//test 8: disconnect during game
void test_disconnect_during_game() {
    printf("\n-- Test: DISCONNECT DURING GAME --\n");

    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|07|OPEN|A|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|07|OPEN|B|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");

    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    printf("Closing player 1\n");
    close(fd1);

    expect_type(fd2, "OVER");
    close(fd2);
}

//test 9: concurrent games
void test_concurrent_and_duplicate() {
    printf("\n-- Test: CONCURRENT GAMES --\n");

    //game 1 with players A1 and A2
    int A1 = connect_client();
    int A2 = connect_client();

    send_raw(A1, "0|08|OPEN|A1|");
    get_msg(A1, (char[BUF]){0}); //WAIT

    send_raw(A2, "0|08|OPEN|A2|");
    get_msg(A1, (char[BUF]){0}); //NAME
    get_msg(A2, (char[BUF]){0}); //NAME

    get_msg(A1, (char[BUF]){0}); //PLAY
    get_msg(A2, (char[BUF]){0}); //PLAY

    //game 2 with players B1 and B2
    int B1 = connect_client();
    int B2 = connect_client();

    send_raw(B1, "0|08|OPEN|B1|");
    get_msg(B1, (char[BUF]){0}); //WAIT

    send_raw(B2, "0|08|OPEN|B2|");
    get_msg(B1, (char[BUF]){0}); //NAME
    get_msg(B2, (char[BUF]){0}); //NAME

    get_msg(B1, (char[BUF]){0}); //PLAY
    get_msg(B2, (char[BUF]){0}); //PLAY

    printf("Attempting duplicate name A1...\n");

    //try connecting a third client with a duplicate name A1
    int dup = connect_client();
    send_raw(dup, "0|08|OPEN|A1|");

    char reply[BUF];
    if (get_msg(dup, reply) > 0)
        printf("Duplicate-name reply: %s\n", reply);
    else
        printf("Duplicate name connection unexpectedly closed!\n");

    close(dup);

    //clean up  4 real players
    send_raw(A1, "0|09|MOVE|1|1|");
    get_msg(A1,(char[BUF]){0});
    get_msg(A2,(char[BUF]){0});

    send_raw(B1, "0|09|MOVE|1|1|");
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
    //test_incorrect_frame();
    test_wrong_time();
    test_matchmaking();
    test_invalid_move();
    test_full_game();
    test_disconnect_before_match();
    test_disconnect_during_game();
    test_concurrent_and_duplicate();
    printf("\nTESTING COMPLETE\n");
    return 0;
}
