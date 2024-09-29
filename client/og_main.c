#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<pthread.h>
#include<ncurses.h>
#include<errno.h>
#include<panel.h>

#define PORT 60006
#define MAX_INPUT_LENGTH 1000
#define DELAY 100000  
#define MAX_RETRIES 7

char *user_name = NULL;

int rows, cols;
int full_win_height; 
int input_win_height;


typedef struct {
    int code;
    char uname[50];
    char message[200];
} Message;

Message message;

struct message_queue{
    Message *msg;
    bool sender;
    struct message_queue *next , *prev;
};

struct message_queue *head = NULL;
struct message_queue *tail = NULL;


int sockfd;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;



void add_message_to_queue( Message *msg , bool self) {

    pthread_mutex_lock(&lock);

    struct message_queue *new_node = malloc(sizeof(struct message_queue));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        pthread_mutex_unlock(&lock);
        return;
    }

    new_node->msg = msg;
    new_node->sender = self; 
    new_node->next = NULL;
    new_node->prev = tail;

    if (tail != NULL) {
        tail->next = new_node;
    }
    tail = new_node;

    if (head == NULL) {
        head = new_node;
    }

    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}


int send_message(Message *new_message) {
    int result;
    int retry_count = 0;

    while (retry_count < MAX_RETRIES) {
        result = send(sockfd, new_message, sizeof(*new_message), 0);

        if (result >= 0) {
            // Success
            return result;
        }

        if (errno == EINTR) {
            // Interrupted by signal, try again immediately
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Non-blocking mode and send would block, retry after a delay
            retry_count++;
            usleep(DELAY);
        } else {
            // Some other error occurred, log it and break out
            perror("send failed");
            break;
        }
    }

    // If we reached here, send failed after all retries
    fprintf(stderr, "Failed to send message after %d retries\n", retry_count);
    return -1;
}

void *render_input_text(void *arg) {

    WINDOW *input_win = (WINDOW *)arg; // Use the input window passed as argument

    if (input_win == NULL) {
        printw("Error: Could not create input window.\n");
        refresh();
        getch();
        endwin();
        return NULL;
    }

    box(input_win, 0, 0);  // Draw border for the input window
    mvwprintw(input_win, 2, 2, "Message");
    wmove(input_win, 2, 2);  // Move cursor to typing position
    wrefresh(input_win);   // Refresh to show the window


    bool has_input = false;  // Flag to track whether input exists
    const char *placeholder = "Message";  // Placeholder text

    char input[MAX_INPUT_LENGTH] = {0};
    int index = 0;
    int bytes_send = 0;
    while (1) {
        int ch = wgetch(input_win);  

        if (ch == 10) {  // Enter key pressed
            if (index > 0) {  
                Message *new_message = malloc(sizeof(Message));
                if (new_message == NULL) {
                    printw("Memory allocation failed\n");
                    break;
                }

                strcpy(new_message->uname, (char *)arg);  // Set the username
                strcpy(new_message->message, input);      // Copy the message from the input buffer
                new_message->code = 100;

                bytes_send = send_message(new_message);
                add_message_to_queue(new_message,true);  // Add message to queue

                // Reset input state for the next message
                memset(input, 0, sizeof(input));
                index = 0;
                has_input = false;

                // Clear the input line and show the placeholder again
                mvwprintw(input_win, 2, 2, "%-*s", cols - 4, "");  // Clear the input line
                mvwprintw(input_win, 2, 2, "%s", placeholder);  // Display placeholder
                wmove(input_win, 2, 2);  // Reset cursor position
            }
        } else if (ch == 127 || ch == KEY_BACKSPACE) {  // Backspace key pressed
            if (index > 0) {
                input[--index] = '\0';  // Remove last character from buffer

                mvwprintw(input_win, 2, 2, "%-*s", cols - 4, "");  // Clear the input line
                if (index == 0) {
                    has_input = false;  // Input is empty, show the placeholder
                    mvwprintw(input_win, 2, 2, "%s", placeholder);
                } else {
                    mvwprintw(input_win, 2, 2, "%s", input);  // Reprint input
                }

                wmove(input_win, 2, index + 2);  // Set cursor to the correct position
            }
        } else if (ch >= 32 && ch <= 126 && index < MAX_INPUT_LENGTH - 1) {  // Printable characters
            if (!has_input) {  
                mvwprintw(input_win, 2, 2, "%-*s", cols - 4, "");  // Clear the placeholder
                has_input = true;
            }

            input[index++] = (char)ch;  // Add character to buffer
            input[index] = '\0';  // Null-terminate the string

            mvwprintw(input_win, 2, 2, "%s", input);  // Display the current input
            wmove(input_win, 2, index + 2);  // Move cursor to the next position
        }

        wrefresh(input_win);  // Refresh the input window to show changes
    }

    wrefresh(input_win);
    return NULL;
}



// Function to handle receiving data from the server in a separate thread
void *receive_data(void *arg) {

    Message received_message;
    while (1) {
        // Receive message from server
        int bytes_received = recv(sockfd, &received_message, sizeof(received_message), 0);
        if (bytes_received <= 0) {
            printf("Server disconnected or error occurred.\n");
            break;
        }

        add_message_to_queue(&received_message,false);
        memset(&received_message,0,sizeof(Message));
    }
    return NULL;
}

void *render_ui(void *arg) {
    initscr();              // Start ncurses mode
    cbreak();               // Disable line buffering
    noecho();               // Don't echo input characters
    curs_set(0);            // Hide cursor
    start_color();          // Start color functionality

    // Define color pairs
    init_pair(1, COLOR_RED, COLOR_BLACK);   // Color pair 1: Red text on black background
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // Color pair 2: Green text on black background

    // Get the size of the terminal
    getmaxyx(stdscr, rows, cols);  // Get terminal size in rows and columns
    full_win_height = rows - 6;    // Leave space for the input window (5 rows)
    input_win_height = 5;

    // Create the main content window (upper part of the screen)
    WINDOW *full_win = newwin(full_win_height, cols, 0, 0);
    box(full_win, 0, 0);  // Draw a border around the window
    wrefresh(full_win);

    // Create the input window (bottom part of the screen)
    WINDOW *input_win = newwin(input_win_height, cols, full_win_height, 0);
    box(input_win, 0, 0);  // Draw a border around the input window
    wrefresh(input_win);

    // Spawn the input thread with the input window
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, render_input_text, input_win);

    // Rest of the UI rendering
    int message_row = 1;

    while (1) {
        pthread_mutex_lock(&lock);

        // Wait until there's a message to display (signal from add_message_to_queue)
        while (head == NULL) {
            pthread_cond_wait(&cond, &lock);
        }

        // Retrieve the message from the queue
        struct message_queue *current_node = head;
        Message *msg = current_node->msg;

        // Remove the node from the queue
        head = head->next;
        if (head == NULL) {
            tail = NULL;
        } else {
            head->prev = NULL;
        }

        // Unlock the mutex so other threads can access the queue
        pthread_mutex_unlock(&lock);

        // Display the message in the full window
        if(current_node->sender)
        {
            wattron(full_win, COLOR_PAIR(2));  // Use green color for now (can customize based on logic)
            mvwprintw(full_win, message_row, cols/2, "[you]: %s", msg->message);
            wattroff(full_win, COLOR_PAIR(2));
        }
        else
             mvwprintw(full_win, message_row, 1, "[%s]: %s", msg->uname, msg->message);



        // Free the message structure after displaying
        free(msg);
        free(current_node);

        // Move to the next row in the window
        message_row++;
        if (message_row >= full_win_height - 1) {
            message_row = 1;  // Reset the row if it reaches the bottom of the window
            werase(full_win);  // Clear the window to avoid overflow
            box(full_win, 0, 0);  // Redraw the border
        }

        // Refresh the full window to reflect changes
        wrefresh(full_win);
    }

    // Clean up
    delwin(full_win);   // Delete the full window
    delwin(input_win);  // Delete the input window
    endwin();           // End ncurses mode

    return NULL;
}

int main(int argc, char *argv[]) {

    pthread_t ui_thread , input_thread , recv_thread;

    if (argc < 2) {
        printf("Usage: %s <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    user_name = argv[1];

    if (pthread_create(&ui_thread,NULL,render_ui,NULL) != 0) {
            perror("Thread creation failed");
            close(sockfd);
            exit(EXIT_FAILURE);
    }

    if (pthread_create(&recv_thread, NULL, receive_data, NULL) != 0) {
        perror("Thread creation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

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
    } 
    else {
        printf("Connected to server...\n");
    }

    // Create a thread for receiving data from the server
 

    pthread_join(recv_thread, NULL);
    pthread_join(ui_thread, NULL);
    pthread_join(input_thread, NULL);

    // Clean up
    close(sockfd);
    return 0;
}
