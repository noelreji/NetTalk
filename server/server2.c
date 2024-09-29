/*
    implemented the servicing of new incoming client requests.
    need to implement broadcasting messages to all other active members in the group.
*/



#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/epoll.h>


#define PORT 60006
#define MAX_EVENTS 1024
#define SERVERBACKLOG 10000

typedef struct Message{

    int code;
    char uname[50];
    char message[200];

}Message;

//data structure to store all the incoming messages for broadcasting
struct message_queue{
    Message *msg;
    int sender;
    struct message_queue *next , *prev;
};

struct message_queue *head = NULL;
struct message_queue *message_queue_pointer = NULL;

//data structure for storing active clients metadata
struct active_client_db{
    int client_fd;
    struct active_client_db *next , *prev;
};

struct active_client_db *active_client_head = NULL;
struct active_client_db *current_active_client = NULL;


Message client_message;


int epoll_fd;
int first_client;
struct epoll_event event , events[MAX_EVENTS];

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for thread-safe access to the message queue

void roam_message(Message *to_send_msg, int sender) {
    int data_send;
    struct active_client_db *client = active_client_head;
    
    while (client != NULL) {
        if (client->client_fd != sender) {
            data_send = send(client->client_fd, to_send_msg, sizeof(Message), 0);
            if (data_send > 0) {
                printf("\nSent message from %s to client %d", to_send_msg->uname, client->client_fd);
                fflush(stdout);
            } else {
                printf("\nFailed to send message to client %d", client->client_fd);
            }
        }
        client = client->next;
    }
}


void *broadcast_thread() {
    struct message_queue *next_dest;
    
    while (1) {
        pthread_mutex_lock(&queue_mutex);  // Lock before accessing the queue

        while (head != NULL) {
            printf("\nBroadcasting message from %s", head->msg->uname);
            fflush(stdout);

            roam_message(head->msg, head->sender);
            next_dest = head->next;
            
            // Free the current head after processing
            free(head);
            head = next_dest;
        }

        pthread_mutex_unlock(&queue_mutex);  // Unlock after accessing the queue

        //usleep(1000);  // Avoid busy waiting, sleep for 1 millisecond
    }
}

void add_message_queue(int sender, Message *clientMessage) {
    struct message_queue *new_msg = (struct message_queue *)malloc(sizeof(struct message_queue));
    new_msg->msg = (Message *)malloc(sizeof(Message));  // Allocate memory for message
    memcpy(new_msg->msg, clientMessage, sizeof(Message));  // Copy message content
    new_msg->sender = sender;
    new_msg->next = NULL;

    pthread_mutex_lock(&queue_mutex);  // Lock the queue for thread-safe access

    if (head == NULL) {
        head = new_msg;
        message_queue_pointer = new_msg;
        printf("First message from %s added to the queue", clientMessage->uname);
    } else {
        message_queue_pointer->next = new_msg;
        message_queue_pointer = new_msg;
        printf("Message from %s added to the queue", clientMessage->uname);
    }

    pthread_mutex_unlock(&queue_mutex);  // Unlock the queue
}

void *service_thread(){

    int events_fired;
    ssize_t bytes_read;
    printf("\nServicing Threads");
    while(1){

        events_fired = epoll_wait(epoll_fd,events,MAX_EVENTS,-1);

        for(int i = 0; i < events_fired; ++i){
            if(events[i].events & EPOLLIN ){

                bytes_read = recv(events[i].data.fd,&client_message,sizeof(Message),0);
                if( bytes_read > 0 ){

                    printf("\n%d Joined --> %s",ntohs(client_message.code),client_message.uname);
                    add_message_queue(events[i].data.fd , &client_message);

                }
            }
        }
    }
}

void init_client(int fd){


    struct active_client_db *new_client = ( struct active_client_db * )malloc(sizeof(struct active_client_db));
    new_client->client_fd = fd;
    new_client->next = NULL;

    if( active_client_head == NULL ){

        current_active_client = new_client;
        active_client_head = new_client;
    }
    else{

        current_active_client->next = new_client;
        current_active_client = new_client;

    }


    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event) == -1 ){
        
        perror("\n\nepoll_ctl client");
        exit(1);
    }

    return;
}

int main( int argv , char **argc ){
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int clientfd;
    pthread_t T_service , T_broadcast;
    epoll_fd = epoll_create1(0);

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
        pthread_create(&T_service,NULL,service_thread,NULL);
        pthread_create(&T_broadcast,NULL,broadcast_thread,NULL);

        while (1)
        {
            printf("\nListening...\n\n");
            memset(&clientfd,0,sizeof(int));
            clientfd = accept( sockfd , (struct sockaddr *)&client_addr,&client_len);
            printf("\n\n%d",clientfd);

            
            if( clientfd != -1 )
            {
                if(opt == 1)
                    first_client = clientfd;
                opt++;
                printf("\nClient connected");
                init_client(clientfd);
            }      
            else
            {
                printf("\n\nFailed to accept client");
                continue;
            }  
        }       
    }
    else
        perror("Error in binding socket");


    close(sockfd);
}
