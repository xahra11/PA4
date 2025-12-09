#define _POSIX_C_SOURCE 200809L

#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>

Message *read_message(int sock_fd) {
    char version, bar, len[3];

    // read version
    if (read(sock_fd, &version, 1) <= 0) return NULL;
    if (version != '0') return NULL;

    // read first '|'
    if (read(sock_fd, &bar, 1) <= 0) return NULL;
    if (bar != '|') return NULL;

    // read 2-digit length
    if (read(sock_fd, len, 2) <= 0) return NULL;
    if (!isdigit(len[0]) || !isdigit(len[1])) return NULL;
    len[2] = '\0';

    int msg_len = atoi(len);
    if (msg_len <= 0 || msg_len > MAX_MSG_LENGTH) return NULL;

    // read second '|'
    if (read(sock_fd, &bar, 1) <= 0) return NULL;
    if (bar != '|') return NULL;

    // read message content (including trailing '|')
    char *buf = malloc(msg_len + 1);
    if (!buf) return NULL;

    int bytes_read = 0;
    while (bytes_read < msg_len) {
        int n = read(sock_fd, buf + bytes_read, msg_len - bytes_read);
        if (n <= 0) {
            free(buf);
            return NULL;
        }
        bytes_read += n;
    }
    buf[msg_len] = '\0';

    // ensure last character is '|'
    if (buf[msg_len - 1] != '|') {
        free(buf);
        return NULL;
    }

    // split fields
    char *fields[20];
    int num = 0;
    char *ptr = NULL;
    char *token = strtok_r(buf, "|", &ptr);
    while (token && num < 20) {
        fields[num++] = token;
        token = strtok_r(NULL, "|", &ptr);
    }

    if (num < 1) { // at least message type
        free(buf);
        return NULL;
    }

    Message *msg = malloc(sizeof(Message));
    if (!msg) {
        free(buf);
        return NULL;
    }

    msg->version = version;
    msg->length = msg_len;
    strncpy(msg->type, fields[0], 4);
    msg->type[4] = '\0';

    msg->field_num = num - 1;
    msg->fields = malloc(sizeof(char*) * msg->field_num);
    if (!msg->fields) {
        free(msg);
        free(buf);
        return NULL;
    }

    for (int i = 0; i < msg->field_num; i++) {
        msg->fields[i] = strdup(fields[i + 1] ? fields[i + 1] : "");
        if (!msg->fields[i]) {
            for (int j = 0; j < i; j++) free(msg->fields[j]);
            free(msg->fields);
            free(msg);
            free(buf);
            return NULL;
        }
    }

    free(buf);
    return msg;
}

void free_message(Message *msg) { // free message and all attributes
    if(!msg){
        return;
    } 

    if(msg->fields){
        for(int i = 0; i < msg->field_num; i++){
            free(msg->fields[i]); 
        }
        free(msg->fields);
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