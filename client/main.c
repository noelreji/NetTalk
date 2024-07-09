#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<unistd.h>
#include<stdlib.h>



#define PORT 60004


int main( int argc , char *argv[] ){


    char *in_data = (char *)malloc(100);
    if (in_data == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }    
    
    char *user_name = argv[1];
    int client_fd;
    struct sockaddr_in addr;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);

    if(sockfd < 0 ) {
        perror("Socket creation failed");
        free(in_data);
        exit(EXIT_FAILURE);    
        
    }

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT); 

    if(connect(sockfd , (struct sockaddr *)&addr,(socklen_t )sizeof(addr)) < 0 ){
        printf("Connection failed..\n");
        free(in_data);
        exit(EXIT_FAILURE);
    }
    else
        printf("\n\nConnected..\n\n");
    int op;
    
    send(sockfd , user_name , strlen(user_name),0);

    while(1){

        int bytes_read = read(sockfd, in_data, 99);
        if (bytes_read > 0) {
            in_data[bytes_read] = '\0';  // Null-terminate the received data
            printf("%s just joined the chat\n", in_data);
            memset(in_data, 0, 100);  // Clear the buffer for the next read
        } 
        else if (bytes_read == 0) {
            printf("Server closed connection\n");
            break;
        } 
        else {
            perror("Error reading data");
            break;
        }
    }


    free(in_data);
    close(sockfd);
    return 0;
}