#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFERSIZE 256
#define PROTOCOL "1\n"
//#define DEBUG


void* getIPVersion(struct sockaddr* socketAddress)
{
    if (socketAddress->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)socketAddress)->sin_addr);
    }

    return &(((struct sockaddr_in6*)socketAddress)->sin6_addr);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Program call was incorrect.\n");
        exit(1);
    }

    char* expression = "^[A-Za-z0-9_]+$";
    regex_t regExpression;
    int reti;

    reti = regcomp(&regExpression, expression, REG_EXTENDED);
    if (reti)
    {
        fprintf(stderr, "Error with regex. Exiting...\n");
        exit(1);
    }

    int matches = 0;
    regmatch_t items;

    if (strlen(argv[2]) < 13)
    {     
        reti = regexec(&regExpression, argv[2], matches, &items, 0);
        if (!reti)
        {
            printf("Nickname approved.\n");
        }
        else
        {
            printf("Nickname contained not accepted characters.\n");
            exit(1);
        }
    }
    else
    {
        printf("Nickname was too long: <%s> You entered: %zu chars (12 chars max)\n", argv[2], strlen(argv[2]));
        exit(1);
    }
    regfree(&regExpression);

    char delim[] = ":";
    char* firstArg = strdup(argv[1]);
    char* serverIP = strtok_r(firstArg, ":", &firstArg);
    char* serverPort = strtok_r(firstArg, ":", &firstArg);
    char* nickname = strdup(argv[2]);

    #ifdef DEBUG
    printf("IP: %s\t  Port: %s\tClient: %s\n", serverIP, serverPort, nickname);
    #endif

    if (strlen(serverIP) < 7 || strlen(serverIP) > 33)
    {
        printf("Invalid IP Address. Exiting...\n");
        exit(2);
    }

    int portInt = atoi(serverPort);
    if (portInt < 1024 || portInt > 65535)
    {
        printf("Invalid Port Number. Exiting...\n");
        exit(3);
    }

    struct addrinfo hints;
    struct addrinfo* server;
    struct addrinfo* pointer;

    // Create Template Server
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv;
    rv = getaddrinfo(serverIP, serverPort, &hints, &server);
    if (rv > 0)
    {
        fprintf(stderr, "Program stopped at getaddrinfo: %s\n", gai_strerror(rv));
        exit(4);
    }

    int sockFD;    
    // Loop to; Create socket and Connect
    for (pointer = server; pointer != NULL; pointer = pointer->ai_next)
    {
        sockFD = socket(pointer->ai_family, pointer->ai_socktype, pointer->ai_protocol);
        if (sockFD == -1)
        {
            perror("socket creation");
            continue;
        }    

        if (connect(sockFD, pointer->ai_addr, pointer->ai_addrlen) == -1)
        {
            close(sockFD);
            perror("connect");

        }
        break;
    }

    if (pointer == NULL)
    {
        fprintf(stderr, "Failed to connect to the server.\n");
        exit(5);
    }

    char localIP[INET6_ADDRSTRLEN];
    inet_ntop(pointer->ai_family, getIPVersion((struct sockaddr*)pointer->ai_addr), localIP, sizeof(localIP));
    printf("Connected to: %s:%s\n", serverIP, serverPort);
    freeaddrinfo(server);

    char buffer[BUFFERSIZE];
    memset(&buffer, 0, sizeof(buffer));
    // Recv PROTOCOL
    rv = recv(sockFD, buffer, BUFFERSIZE-1, 0);
    if (rv == -1)
    {
        perror("recv1");
        exit(6);        
    }
    else if (rv == 0)
    {
        printf("Connection Closed, Exiting...\n");
        exit(7);
    }
    else
    {
        printf("Server: %s", buffer);
    }

    char spaceDelim[] = " ";
    char* typeHolder = strtok(buffer, spaceDelim);
    char* textHolder = strtok(NULL, spaceDelim);

    if (strcmp(typeHolder, "HELLO")  != 0)
    {
        printf("Error with Hello Message. Quitting...\n");
        exit(8);
    }

    if (strcmp(textHolder, PROTOCOL) != 0)
    {
        printf("Incorrect protocol. Exiting...");
        exit(9);
    }

    // Send Nickname
    memset(&buffer, 0, sizeof(buffer));
    sprintf(buffer, "NICK %s\n", nickname);
    rv = send(sockFD, buffer, strlen(buffer), 0);
    if(rv < 1)
    {
        perror("send1");
        exit(1);
    }

    memset(&buffer, 0, sizeof(buffer));
    rv = recv(sockFD, buffer, BUFFERSIZE-1, 0);
    if (rv < 1)
    {
        perror("recv2");
        exit(1);
    }

    char rowDelim[] = "\n";
    char nullDelim[] = "\0";
    typeHolder = strtok(buffer, rowDelim);
    if (strcmp(typeHolder, "OK") != 0)
    {
        printf("Server sent error on nickname. Exiting...\n");
        exit(1);
    }
    printf("Server: %s\n\n", buffer);

    char stdinBuff[BUFFERSIZE];
    char readBuffer[BUFFERSIZE];
    char* chatMsg;

    fd_set writeFDS;
    fd_set readFDS;
 
    while (1)
    {
        chatMsg = nullptr;
       
        FD_ZERO(&readFDS);
        FD_ZERO(&writeFDS); 

        FD_SET(STDIN_FILENO, &readFDS);
        FD_SET(sockFD, &readFDS);
      
        rv = select(sockFD+1, &readFDS, &writeFDS, NULL, 0);
        if (rv == -1)
        {
            perror("select"); 
            free(nickname);
            close(sockFD);
            exit(1);
        }
   
        // Check for client input on STDIN
        if (FD_ISSET(STDIN_FILENO, &readFDS))
        {
            memset(&stdinBuff, 0, sizeof(stdinBuff));
            if (fgets(stdinBuff, BUFFERSIZE, stdin) != NULL)
            {
                FD_SET(sockFD, &writeFDS);
            }   
            else
            {
                printf("There was nothing to read on STDIN.\n");
            }
        }

        // Send Messages to Server
        if (FD_ISSET(sockFD, &writeFDS))
        {   
            chatMsg = strtok(stdinBuff, rowDelim);
            if (chatMsg != NULL)
            { 
                memset(&buffer, 0, sizeof(buffer));
                sprintf(buffer, "MSG %s\n", chatMsg);
                rv = send(sockFD, buffer, strlen(buffer), 0);
                if (rv < 1)
                {
                    perror("send");
                    continue;
                }
                else
                {
                    #ifdef DEBUG
                    printf("Sent %d bytes. Message was: %s", rv, buffer);
                    #endif
                }   
            }
            else
            {
                if (chatMsg != NULL)
                {
                    printf("Message was too long. You entered %zu (251 chars max)", strlen(chatMsg));
                    free(nickname);
                    close(sockFD);
                    exit(1);
                }
            }
        }

        // Check for messages from server
        if (FD_ISSET(sockFD, &readFDS))
        {   
            memset(&readBuffer, 0, sizeof(readBuffer));
            rv = recv(sockFD, readBuffer, BUFFERSIZE-1, 0);
            char* bufferHolder = strdup(readBuffer);
            bool msgRead = false;
            while (!msgRead)
            {
                char* chatHolder = strtok(bufferHolder, rowDelim);
                char* readHolder = strtok(NULL, nullDelim);
                rv = rv - (strlen(chatHolder) + 1);
            
                if ((rv-1) <= 0)
                {
                    msgRead = true;
                }
             
                if (chatHolder != NULL)
                {   
                    char* msgHolder = strtok(chatHolder, spaceDelim);
                    char* txtHolder = strtok(NULL, rowDelim);
                    
                    if (strcmp(msgHolder, "ERROR") == 0)
                    {
                        printf("Server sent error, closing now.\n");
                        free(bufferHolder);
                        free(nickname);
                        close(sockFD);
                        exit(1);
                    }
                    else if (strcmp(msgHolder, "MSG") == 0)
                    {   
                        if (txtHolder != NULL)
                        {
                            char* nickHolder = strtok(txtHolder, spaceDelim);
                            char* displayHolder = strtok(NULL, rowDelim);
                        
                            if (nickHolder != nullptr && nickname != nullptr)
                            {
                                if (strcmp(nickHolder, nickname) != 0)
                                {
                                    printf("%s: %s\n", nickHolder, displayHolder);
                                }
                                else
                                {
                                    #ifdef DEBUG
                                    printf("Same As Sender: <%s %s>\n", msgHolder, nickHolder);
                                    #endif
                                }
                            }
                        }
                    }
                    else
                    {
                        printf("Message was not MSG/ERROR\n");
                        break;
                    }
                }
                else
                {
                    printf("Chatholder was NULL\n");
                    break;
                }
                bufferHolder = readHolder;
            }
            free(bufferHolder);
        }
    }
    free(nickname);
    close(sockFD);
}
