#include <stdio.h>    
#include <stdlib.h>   
#include <stdbool.h>
#include <unistd.h>  
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>   

// function prototypes

int main(int argc, char *argv[]) {

    int sock_fd, port_number;
    struct sockaddr_in address;

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

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    address.sin_port = htons(port_number);

    if(bind(sock_fd, (struct sockaddr*)&address, sizeof(address)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sock_fd, 3) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    
}