#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>  
#include<string.h>
#include<unistd.h>
#include<stdlib.h>



#define PORT 60006

typedef struct {

    int code;
    char message[1000];

}Message;

Message message;

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
    
    message.code = htons(69);
    strcpy(message.message , user_name);
    printf("\n%s",message.message);
    
    int bytes_send;
    resend:  bytes_send = send(sockfd , &message , sizeof(message),0);

    if( bytes_send == -1 ) {
        printf("\n\nResending");
        goto resend;
    }
    else
        printf(" Data send\n");

    char *b = (char *)malloc(1000*sizeof(char));

    while(1){

        /*printf("\n--> ");
                fflush(stdout);

        fgets(b,1000,stdin);

        printf("\n%s",b);
                fflush(stdout);

        message.code = 100;
        strcpy(message.message,b);*/

        memset(&message,0,sizeof(message));
       // printf("\nWaiting");
           // fflush(stdout);
        bytes_send = recv(sockfd , &message , sizeof(message),0);

        if( bytes_send == -1 ) {
            printf("\n\nResending");
        }
        else
        {
            printf("\n%s",message.message);
                    fflush(stdout);
        }

        //memset(b,0,sizeof(b));
    }


    free(in_data);
    close(sockfd);
    return 0;
}