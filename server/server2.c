/*
    implemented the servicing of new incoming client requests.
    need to implement broadcasting messages to all other active members in the group.



    -- cleint disconnection management
    -- setting up ip addresses
    -- user authentication / setting username using environment variable
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
#include<signal.h>

#define PORT 60006
#define MAX_EVENTS 1024
#define SERVERBACKLOG 10000
#define MAX_RETRY 5


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

int client_count = 0;

Message client_message;


int epoll_fd;
int first_client;
struct epoll_event event , events[MAX_EVENTS];

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for thread-safe access to the message queue
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;  // Condition variable

void roam_message(Message *to_send_msg, int sender) {
    int data_send;
    struct active_client_db *client = active_client_head;

    while (client != NULL) {
        if (client->client_fd != sender) {
            int retries = 0;
            while (retries < MAX_RETRY) {
                data_send = send(client->client_fd, to_send_msg, sizeof(Message), 0);
                if (data_send > 0) {
                    printf("Sent message from %s to client %d\n", to_send_msg->uname, client->client_fd);
                    fflush(stdout);
                    break;  
                } else {
                    retries++;
                    printf("Failed to send message to client %d (attempt %d/%d)\n", client->client_fd, retries, MAX_RETRY);
                    if (retries == MAX_RETRY) {
                        printf("Max retries reached for client %d. Skipping...\n", client->client_fd);
                    }
                }
            }
        }
        client = client->next;  // Move to the next client
    }
}


void *broadcast_thread() {
    struct message_queue *next_dest;

    while (1) {
        pthread_mutex_lock(&queue_mutex);  // Lock before accessing the queue

        // Wait for a message to arrive in the queue
        while (head == NULL) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        // Process messages in the queue
        while (head != NULL) {
            fprintf(stdout,"Broadcasting /%s/ \n",head->msg->message);

            roam_message(head->msg, head->sender);
            next_dest = head->next;

            // Free the message and the head node
            free(head->msg);
            free(head);

            head = next_dest;
        }

        pthread_mutex_unlock(&queue_mutex);  // Unlock after accessing the queue
    }
    return NULL;
}

void add_message_queue(int sender, Message *clientMessage) {
    struct message_queue *new_msg = (struct message_queue *)malloc(sizeof(struct message_queue));
    new_msg->msg = (Message *)malloc(sizeof(Message));  // Allocate memory for message
    
    memcpy(new_msg->msg, clientMessage, sizeof(Message));  // Copy message content
    new_msg->sender = sender;
    new_msg->next = NULL;
    new_msg->prev = NULL;

    pthread_mutex_lock(&queue_mutex);  // Lock the queue for thread-safe access

    if (head == NULL) {
        head = new_msg;
        message_queue_pointer = new_msg;
//        printf("First message from %s added to the queue", clientMessage->uname);
    } else {
        message_queue_pointer->next = new_msg;
        new_msg->prev = message_queue_pointer;
        message_queue_pointer = new_msg;
//      printf("Message from %s added to the queue", clientMessage->uname);
    }

    pthread_cond_signal(&queue_cond);  // Signal the broadcast thread that a new message has arrived
    pthread_mutex_unlock(&queue_mutex);  // Unlock the queue
}

void handle_client_disconnect(int clientfd) {
    // If the list is empty, there is nothing to remove
    if (active_client_head == NULL) {
        return;
    }

    struct active_client_db *current = active_client_head;

    // Traverse the list to find the client with the given client_fd
    while (current != NULL) {
        if (current->client_fd == clientfd) {
            // Client found, now remove it from the list

            // If the client is the head of the list
            if (current == active_client_head) {
                active_client_head = current->next;  // Move the head to the next node
                if (active_client_head != NULL) {
                    active_client_head->prev = NULL;  // Set previous of new head to NULL
                }
            } else {
                // If the client is in the middle or end of the list
                if (current->prev != NULL) {
                    current->prev->next = current->next;  // Bypass current in the list
                }
                if (current->next != NULL) {
                    current->next->prev = current->prev;  // Link next node to the previous one
                }
            }

            // Free the memory allocated for the client
            free(current);
            return;
        }

        current = current->next;
    }

    
}

void *service_thread() {
    int events_fired;
    ssize_t bytes_read;
    printf("Receiver On\n");

    while (1) {
        // Wait for events on the file descriptors monitored by epoll
        events_fired = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (events_fired < 0) {
            perror("epoll_wait");
            continue;  // Continue on error, or you may decide to break or exit
        }

        // Loop through all triggered events
        for (int i = 0; i < events_fired; ++i) {
            if (events[i].events & EPOLLIN) {
                // Read message from client
                bytes_read = recv(events[i].data.fd, &client_message, sizeof(Message), 0);

                if (bytes_read > 0) {
                    // Successfully received message
                    printf("%s - status : %d\n", client_message.uname, ntohs(client_message.code));
                    if( client_count > 1 )
                        add_message_queue(events[i].data.fd, &client_message);

                } else if (bytes_read == 0) {
                    // Client has closed the connection
                    printf("Client disconnected: FD %d\n", events[i].data.fd);
                    handle_client_disconnect(events[i].data.fd);
                    client_count--;
                    // Clean up: Close the connection and remove from epoll
                    close(events[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                } else {
                    // Error in receiving message
                    perror("recv");
                }
            }
        }
    }
}


void init_client(int fd) {
    // Allocate memory for a new client
    struct active_client_db *new_client = (struct active_client_db *)malloc(sizeof(struct active_client_db));
    
    // Check if memory allocation was successful
    if (new_client == NULL) {
        perror("malloc");
        exit(1);
    }

    // Initialize new client data
    new_client->client_fd = fd;
    new_client->next = NULL;
    new_client->prev = NULL;  // Initialize prev to NULL to handle the first client case

    // Add the new client to the list of active clients
    if (active_client_head == NULL) {
        // This is the first client in the list
        current_active_client = new_client;
        active_client_head = new_client;
    } else {
        // Add the new client at the end of the list
        current_active_client->next = new_client;
        new_client->prev = current_active_client;
        current_active_client = new_client;
    }

    // Set up the epoll event structure
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;  // EPOLLIN for input events, EPOLLET for edge-triggered mode

    // Register the new client with epoll for monitoring
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("epoll_ctl client");
        
        // Clean up allocated memory on failure
        if (new_client->prev != NULL) {
            new_client->prev->next = NULL;  // Remove the new client from the list
        }
        free(new_client);
        exit(1);
    }
    client_count++;
}

void clean(void* input_head, const char* type) {
    if (strcmp(type, "messages") == 0) {

        struct message_queue *main_head = (struct message_queue*)input_head;
        struct message_queue *next_item;
        
        while (main_head != NULL) {
            next_item = main_head->next;
            free(main_head->msg);  
            free(main_head);       
            main_head = next_item; 
        }

        printf("Cleaned Memeory Queue Successfully..\n");
        fflush(stdout);
    } else if (strcmp(type, "client") == 0) {
        
        struct active_client_db *main_head = (struct active_client_db*)input_head;
        struct active_client_db *next_item;

        while (main_head != NULL) {
            next_item = main_head->next;
            free(main_head);        
            main_head = next_item;  
        }
        printf("Cleaned Client Database Successfully..\n");
        fflush(stdout);
    } else {
        fprintf(stderr, "Unknown type: %s\n", type);
    }
}

void handle_exit(){
    clean(head,"messages");
    clean(active_client_head,"client");
    exit(0);
}

int main( int argv , char **argc ){
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);

    if (sockfd == -1) {
        perror("Socket creation failed");
        close(epoll_fd);  // Also close epoll file descriptor in case socket creation fails
        exit(EXIT_FAILURE);
    }

    int clientfd;
    pthread_t T_service , T_broadcast;


    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll creation failed");
        close(sockfd);  // Ensure that socket is closed if epoll creation fails
        exit(EXIT_FAILURE);  // Exit on failure
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

    signal(SIGINT,handle_exit);

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
                if( opt == 1 )
                    first_client = clientfd;
                opt++;
                fprintf( stdout, "\nClient connected");
                fflush(stdout);
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
