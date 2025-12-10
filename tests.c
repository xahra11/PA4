#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define TEST_PORT 34567
#define BUF 512

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


void send_raw(int fd, const char *s) {
    write(fd, s, strlen(s));
}

int get_msg(int fd, char *out) {
    char header[5]; 
    int bytes_read = 0;

    while (bytes_read < 2) {
        int n = read(fd, header + bytes_read, 2 - bytes_read);
        if (n <= 0) return n;
        bytes_read += n;
    }

    if (header[0] != '0' || header[1] != '|') return -1;

    while (bytes_read < 4) {
        int n = read(fd, header + bytes_read, 4 - bytes_read);
        if (n <= 0) return n;
        bytes_read += n;
    }

    if (!isdigit(header[2]) || !isdigit(header[3])) return -1;

    int msg_len = (header[2]-'0')*10 + (header[3]-'0');

    char bar;
    if (read(fd, &bar, 1) <= 0) return -1;
    if (bar != '|') return -1;

    int total = 0;
    while (total < msg_len) {
        int n = read(fd, out + total, msg_len - total);
        if (n <= 0) return n;
        total += n;
    }

    out[msg_len] = '\0'; 
    return total;
}


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

// Add small delay to allow server to clean up child processes
void small_delay() {
    usleep(50 * 1000); // 50 ms
}

// ---- TESTS ----

void test_bad_format() {
    printf("\n-- Test: BAD FORMAT --\n");
    int fd = connect_client();
    char buf[BUF];

    send_raw(fd, "0|XX|BAD|MESSAGE|");
    get_msg(fd, buf);
    printf("Reply: %s\n", buf);
    close(fd);
    small_delay();
}

void test_wrong_time() {
    printf("\n-- Test: WRONG TIME (MOVE before OPEN) --\n");
    int fd = connect_client();
    char buf[BUF];

    send_raw(fd, "0|09|MOVE|1|1|");
    get_msg(fd, buf);
    printf("Reply: %s\n", buf);
    close(fd);
    small_delay();
}

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

    // Close game after test
    close(fd1);
    close(fd2);
    small_delay();
}

void test_invalid_move() {
    printf("\n-- Test: INVALID MOVE --\n");
    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|13|OPEN|Charlie|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|11|OPEN|Delta|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");
    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    // Charlie makes invalid move
    send_raw(fd1, "0|10|MOVE|10|1|");
    expect_type(fd1, "FAIL");
    expect_type(fd1, "PLAY");

    close(fd1);
    close(fd2);
    small_delay();
}

void test_full_game() {
    printf("\n-- Test: FULL GAME --\n");
    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|09|OPEN|Eve|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|11|OPEN|Frank|");

    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");
    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    // Play deterministic moves
    send_raw(fd1, "0|09|MOVE|5|9|"); expect_type(fd1, "PLAY");
    send_raw(fd2, "0|09|MOVE|4|7|"); expect_type(fd1, "PLAY");
    send_raw(fd1, "0|09|MOVE|3|5|"); expect_type(fd1, "PLAY");
    send_raw(fd2, "0|09|MOVE|2|3|"); expect_type(fd1, "PLAY");
    send_raw(fd1, "0|09|MOVE|1|1|"); expect_type(fd1, "OVER"); expect_type(fd2, "OVER");

    close(fd1);
    close(fd2);
    small_delay();
}

void test_disconnect_before_match() {
    printf("\n-- Test: DISCONNECT BEFORE MATCH --\n");
    int fd = connect_client();
    send_raw(fd, "0|10|OPEN|Solo|");
    expect_type(fd, "WAIT");
    close(fd);
    small_delay();
}

void test_disconnect_during_game() {
    printf("\n-- Test: DISCONNECT DURING GAME --\n");
    int fd1 = connect_client();
    int fd2 = connect_client();

    send_raw(fd1, "0|08|OPEN|H1|");
    expect_type(fd1, "WAIT");

    send_raw(fd2, "0|08|OPEN|I1|");
    expect_type(fd1, "NAME");
    expect_type(fd2, "NAME");
    expect_type(fd1, "PLAY");
    expect_type(fd2, "PLAY");

    close(fd1); // disconnect player 1
    expect_type(fd2, "OVER");

    close(fd2);
    small_delay();
}

void test_concurrent_and_duplicate() {
    printf("\n-- Test: CONCURRENT GAMES & DUPLICATE NAMES --\n");
    int A1 = connect_client();
    int A2 = connect_client();
    send_raw(A1, "0|08|OPEN|A1|"); get_msg(A1, (char[BUF]){0}); //WAIT
    send_raw(A2, "0|08|OPEN|A2|"); get_msg(A1, (char[BUF]){0}); //NAME
    get_msg(A2, (char[BUF]){0}); //NAME
    get_msg(A1, (char[BUF]){0}); //PLAY
    get_msg(A2, (char[BUF]){0}); //PLAY

    int B1 = connect_client();
    int B2 = connect_client();
    send_raw(B1, "0|08|OPEN|B1|"); get_msg(B1, (char[BUF]){0}); //WAIT
    send_raw(B2, "0|08|OPEN|B2|"); get_msg(B1, (char[BUF]){0}); //NAME
    get_msg(B2, (char[BUF]){0}); //NAME
    get_msg(B1, (char[BUF]){0}); //PLAY
    get_msg(B2, (char[BUF]){0}); //PLAY

    // Duplicate name
    int dup = connect_client();
    send_raw(dup, "0|08|OPEN|A1|");
    char reply[BUF];
    if (get_msg(dup, reply) > 0)
        printf("Duplicate-name reply: %s\n", reply);
    else
        printf("Duplicate name connection unexpectedly closed!\n");
    close(dup);

    // Clean up ongoing games
    send_raw(A1, "0|09|MOVE|1|1|"); get_msg(A1,(char[BUF]){0}); get_msg(A2,(char[BUF]){0});
    send_raw(B1, "0|09|MOVE|1|1|"); get_msg(B1,(char[BUF]){0}); get_msg(B2,(char[BUF]){0});

    close(A1); close(A2); close(B1); close(B2);
    small_delay();
}

int main() {
    printf("NIMD TEST\n");
    test_bad_format();
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
