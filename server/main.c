#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>


#define PORT 60004
#define MAX_EVENTS 10
#define SERVERBACKLOG 100

typedef struct {

    int code;
    char message[1000];

} Message;

typedef struct client_config{

    int clientfd;
    int signal_socket;

}client_config;


/*void manage_connection(int *clientfd ,  char *u_name) {

  

}*/

void* handle_in( void *arg )  {

    printf("\nInside Thread");
        fflush(stdout);
    
    char *user_name = (char *)malloc(100);
    if (user_name == NULL) {
        perror("Memory allocation failed");
        pthread_exit(NULL);
    }


    client_config *cc = (client_config *)arg;
    int clientfd = cc->clientfd;
    int signal_socket = cc->signal_socket;

    //epoll instance for sending and receiving to multiple active clients
    struct epoll_event event , all_events[MAX_EVENTS];
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Failed to create epoll file descriptor");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = clientfd;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,clientfd,&event) == -1 ){
        
        perror("epoll_ctl");
        exit(1);
    }

    /*ssize_t bytes_read = read(clientfd, u_name, 99); // Read up to 99 bytes
    if (bytes_read == -1) {
        perror("Error reading data");
        free(u_name); // Free allocated memory on error
        pthread_exit(NULL);
    } 
    else if (bytes_read == 0) {
        printf("Client closed connection\n");
        free(u_name); // Free allocated memory
        pthread_exit(NULL);
    }*/

    manage_connection();

    /*send(clientfd, u_name, bytes_read, 0);
    free(u_name);
    close(clientfd);
    printf("Closed connection\n");
    fflush(stdout);*/


    /*while(1){

        bytes_read = read(clientfd, &user_message, sizeof(user_message)); // Read up to 99 bytes
        if( bytes_read == -1 ){
           printf("\nError reading data from %s ",u_name);
           send(clientfd, "400", strlen("400"), 0);
           continue;
        }

        else if (bytes_read == 0) {
            printf("Client closed connection\n");
            free(u_name); // Free allocated memory
            pthread_exit(NULL);
        }    
        else {


        }
    }*/

    pthread_exit(NULL);
}




int main( int argv , char **argc ){
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int signal_socket = socket(AF_INET,SOCK_STREAM,0);
  

    if(sockfd < 0 ){
        perror("Socket Creation failed");
    }

    struct sockaddr_in server_addr , client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);  
    server_addr.sin_addr.s_addr = INADDR_ANY;  

    socklen_t client_len = sizeof(client_addr);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) >= 0) {
        
        listen(sockfd,SERVERBACKLOG);
        while (1)
        {
            int *clientfd = (int *)malloc(sizeof(int));
            if (clientfd == NULL) {
                perror("Memory allocation failed");
                continue;
            }

            printf("\nListening...\n\n");
            *clientfd = accept( sockfd , (struct sockaddr *)&client_addr,&client_len);

            client_config cc = {
                *clientfd,
                signal_socket
            };


            pthread_t *t_client = (pthread_t *)malloc(sizeof(pthread_t));
            if( *clientfd != -1 )
            {
                
                printf("\nClient connected");
                pthread_create(t_client , NULL , handle_in , &cc);
                
            }        
        }
        
    }
    else
        perror("Error in binding socket");


    close(sockfd);
}
