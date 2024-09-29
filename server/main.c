#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<sys/un.h>


#define PORT 60006
#define MAX_EVENTS 10
#define SERVERBACKLOG 100

typedef struct Message{

    int code;
    char message[1000];

}Message;

typedef struct client_config {

    int *clientfd;
    int *signal_socket;

}client_config;

Message shared_message;
int signal_socket;
struct sockaddr_un addr;
pthread_t who_sent;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t allow_sw = PTHREAD_COND_INITIALIZER;


void peer_signal(Message *msg, int signal_socket) {

    memset(&who_sent,0,sizeof(pthread_t));

    //pthread_cond_wait(&allow_sw, &mutex);

    shared_message.code = msg->code;
    strncpy(shared_message.message, msg->message, sizeof(shared_message.message) - 1);
    shared_message.message[sizeof(shared_message.message) - 1] = '\0'; 

    ssize_t bytes_written = sendto(signal_socket, "SIGMSG", sizeof("SIGMSG") - 1,0,  (const struct sockaddr *)&addr,sizeof(addr));  
    printf("\n%s  just sent signal.",msg->message);
        fflush(stdout);

    if (bytes_written == -1) {
        perror("write to signal_socket failed");
    } 
    else if (bytes_written < sizeof("SIGMSG") - 1) {
        fprintf(stderr, "Partial write to signal_socket\n");
        fflush(stderr);
    }
    else
        who_sent = pthread_self();
    
}


void *handle_in( void *arg )  {
        
    char *user_name = (char *)malloc(100);
    int auth = 0;
    int bytes_read;
    char signal_data[10];
    if (user_name == NULL) {
        perror("Memory allocation failed");
        pthread_exit(NULL);
    }


    client_config *cc = (client_config *)arg;
    int clientfd = *(cc->clientfd);
    int signal_socket = *(cc->signal_socket);

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
        
        perror("\n\nepoll_ctl client");
        exit(1);
    }

    event.events = EPOLLIN ;
    event.data.fd  = signal_socket;
 
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,signal_socket,&event) == -1 ){
            
            perror("\n\nepoll_ctl server");
            exit(1);
    }

    int events_fired;
    Message message;
    while(1){
       // printf("\nWaiting for events...");
       // fflush(stdout);
        //printf("\n%d Client Waiting For Events",(unsigned long)pthread_self());
            //fflush( stdout );
        events_fired = epoll_wait(epoll_fd , all_events,MAX_EVENTS,-1);
        
        if (events_fired == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
    
        for(int i=0; i < events_fired; ++i){
            
            if( all_events[i].events & EPOLLIN ){

                if( all_events[i].data.fd == clientfd ){

                    bytes_read = recv(clientfd,&message,sizeof(message),0);
                    if(bytes_read == -1 ){
                        send(clientfd,"404",sizeof(404),0);
                        break;
                    }
                    else if( bytes_read > 0 ){

                        if(auth == 0)
                        {
                            if( 69 == ntohs(message.code))
                            {
                                auth = 1;
                                strcpy(user_name , message.message);
                                printf("\n%s joined the chat. %d",user_name,clientfd);
                                fflush(stdout);
                                if (pthread_mutex_lock(&mutex) != 0) {
                                    perror("pthread_mutex_lock failed");
                                }
                                    peer_signal(&message,signal_socket);

                                pthread_mutex_unlock(&mutex);

                            }
                        }
                        else
                        {
                                printf("\n\nSignal Working");
                                    fflush(stdout);
                                peer_signal(&message,signal_socket);
                        }
                    }
                    else 
                    {
                        printf("\nPoo");
                        fflush(stdout);
                        pthread_exit(NULL);   
                    }
                }
                else if(all_events[i].data.fd == signal_socket) {
                    printf("\n%s Data Send..",user_name);
                    fflush(stdout);

                    if (pthread_equal(who_sent, pthread_self()))
                    {
                        recvfrom(signal_socket,signal_data,10,0,NULL,NULL);
                        printf("\n%s Data retrieval Complete..",user_name);
                        fflush(stdout);
                    }
                    send(clientfd,&shared_message,sizeof(Message),0);
                    printf("\n%s Data Send Complete..",user_name);
                }
            }
        }
    }
    printf("\nPoo");
        fflush(stdout);
    pthread_exit(NULL);
}


int main( int argv , char **argc ){
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    signal_socket = socket(AF_UNIX,SOCK_DGRAM,0);
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/signal_soke.sock", sizeof(addr.sun_path) - 1);
    
    
    unlink("/tmp/signal_soke.sock");

    if (bind(signal_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("\nError Binding Signal Socket");
        exit(0);
    }

    printf("Socket signal = %d",signal_socket);

    if(sockfd < 0 ){
        perror("Socket Creation failed");
    }

    struct sockaddr_in server_addr , client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);  
    server_addr.sin_addr.s_addr = INADDR_ANY;  

    socklen_t client_len = sizeof(client_addr);
    
    int opt =1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sockfd);
        exit(1);
    }

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
            
            client_config *cc = (client_config *)malloc(sizeof(client_config));

            *clientfd = accept( sockfd , (struct sockaddr *)&client_addr,&client_len);
            printf("\n\n%d",*clientfd);

            pthread_t *t_client = (pthread_t *)malloc(sizeof(pthread_t));
            if( *clientfd != -1 )
            {
                
                printf("\nClient connected");

                cc->clientfd = clientfd;
                cc->signal_socket = &signal_socket;

                pthread_create(t_client , NULL , handle_in , cc);
                //free(cc);
            }      
            else
            {
                printf("\n\nFailed to accept client");
                free(cc);
                continue;

            }  
        }       
    }
    else
        perror("Error in binding socket");


    close(sockfd);
}
