#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<pthread.h>

#define PORT 60006

typedef struct {
    int code;
    char uname[50];
    char message[200];
} Message;

Message message;

int sockfd;

// Function to handle receiving data from the server in a separate thread
void *receive_data(void *arg) {
    int bytes_received;
    Message received_message;

    while (1) {
        memset(&received_message, 0, sizeof(received_message));
        bytes_received = recv(sockfd, &received_message, sizeof(received_message), 0);

        if (bytes_received <= 0) {
            // If recv returns 0, the server has likely closed the connection
            printf("\nServer disconnected or error occurred.\n");
            break;
        } else {
            printf("\n%s: %s\n", received_message.uname, received_message.message);
            fflush(stdout);  // Ensure that output is printed immediately
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *user_name = argv[1];
    struct sockaddr_in addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address details
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    } else {
        printf("Connected to server...\n");
    }

    // Initialize message data
    message.code = htons(69);
    strcpy(message.uname, user_name);

    // Create a thread for receiving data from the server
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_data, NULL) != 0) {
        perror("Thread creation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Main loop for sending data
    while (1) {
        printf("Enter message: ");
        fgets(message.message, sizeof(message.message), stdin);
        message.message[strcspn(message.message, "\n")] = '\0';  // Remove trailing newline

        int bytes_sent = send(sockfd, &message, sizeof(message), 0);
        if (bytes_sent == -1) {
            perror("Failed to send message");
        } else {
            //printf("Message sent: %s\n", message.message);
        }
    }

    // Wait for the receiving thread to finish (if ever needed)
    pthread_join(recv_thread, NULL);

    // Clean up
    close(sockfd);
    return 0;
}
