#define _POSIX_C_SOURCE 200809L

#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>

int read_n_bytes(int fd, char *buf, int n) {
    int total = 0;
    while(total < n) {
        int r = read(fd, buf + total, n - total);
        if(r <= 0){
            return r; 
        }
        total += r;
    }
    return total;
}

Message *read_message(int sock_fd){
    char version, len[2];

    // read version (always 0)
    if(read_n_bytes(sock_fd, &version, 1) <= 0){
        return NULL;
    }

    // read message length (2 decimal digits)
    if(read_n_bytes(sock_fd, len, 2) <= 0){
        return NULL;
    }

    if(!isdigit(len[0]) || !isdigit(len[1])){
        return NULL;
    } 
    int msg_len = atoi(len);

    // use message length to read remaining bytes
    char *buf = malloc(msg_len + 1);
    if(!buf){
        return NULL;
    }

    if(read_n_bytes(sock_fd, buf, msg_len) <= 0){
        free(buf);
        return NULL;
    }

    buf[msg_len] = '\0';

    char *fields[10];
    int num = 0;
    char *token = strtok(buf, "|");
    while(token && num < 10){
        fields[num++] = token;
        token = strtok(NULL, "|");
    }

    if(num == 0){
        free(buf);
        return NULL;
    }

    Message *msg = malloc(sizeof(Message));
    if(!msg){
        free(buf);
        return NULL;
    }

    msg->version = version;
    msg->length = msg_len;
    strncpy(msg->type, fields[0], 4);
    msg->type[4] = '\0';
    msg->field_num = num - 1;

    for(int i = 0; i < msg->field_num; i++){
        msg->fields[i] = strdup(fields[i + 1]);
    }

    free(buf);
    return msg;
}

void free_message(Message *msg) { // free message and all attributes
    if(!msg){
        return;
    } 

    for(int i = 0; i < msg->field_num; i++){
        free(msg->fields[i]); 
    }

    free(msg);
}

bool is_valid_name(const char *name){
    if(!name){
        return false;
    }

    int length = strlen(name);

    if(length == 0 || length > 72){
        return false;  // maximum name length is 72 characters
    }

    for(int i = 0; i < length; i++){
        unsigned char ch = (unsigned char)name[i];
        if(ch == '|' || ch < 32 || ch > 126){
            return false; 
        }
    }

    return true;
}