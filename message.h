#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdbool.h>

#define MAX_MSG_LENGTH 104

typedef struct {
    char version; // should always be 0
    char type[5]; // message type (OPEN, WAIT, NAME, PLAY, MOVE, OVER, FAIL), 4 ASCII characters
    int field_num; // number of fields, depending on message type
    char **fields; // array of fields
} Message;

Message *read_message(int sock_fd);
void free_message(Message *msg);
bool is_valid_name(const char *name);

#endif 
