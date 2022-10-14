// Support library
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFERSIZE 256
#define PROTOCOL "HELLO 1\n"
#define BACKLOG 10
//#define DEBUG

struct client 
{
  int socket = 0;
  struct sockaddr_in address; 
  char nickname[13];
};

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
  if (argc < 2)
  {
    fprintf(stderr, "Program call has too few arguments.\n");
    exit(1);
  }

  if((strlen(argv[1])) < 12 && (strlen(argv[1]) > 33))
  {
    fprintf(stderr, "Address or Port was incorrect.\n");
    exit(1);
  }

  char colDelim[] = ":";
  char* argIP = strtok(argv[1], colDelim);
  char* argPort = strtok(NULL, colDelim);

  struct addrinfo hints;
  struct addrinfo* server;
  struct addrinfo* pointer;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int rv = getaddrinfo(argIP, argPort, &hints, &server);
  if (rv == -1)
  {
    fprintf(stderr, "Error: getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  int argPortInt = atoi(argPort);
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = server->ai_family;
  serverAddr.sin_port = htons(argPortInt);
  inet_aton(argIP, &serverAddr.sin_addr);

  int sockFD;
  int reuse = 1;

  for (pointer = server; pointer != NULL; pointer->ai_next)
  {
    sockFD = socket(pointer->ai_family, pointer->ai_socktype, pointer->ai_protocol);
    if (sockFD == -1)
    {
      perror("PERROR: Socket creation");
      exit(1);
    }

    rv = setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (rv == -1)
    {
      perror("PERROR: sockopt");
      close(sockFD);
      exit(1);
    }

    rv = bind(sockFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (rv == -1)
    {
      perror("PERROR: bind");
      close(sockFD);
      exit(1);
    }

    break;
  }
  freeaddrinfo(server);
  printf("Chatroom established at %s:%d.\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));
  
  int backlog = 10;
  rv = listen(sockFD, backlog);
  if (rv == -1)
  {
    perror("PERROR: listen");
    exit(1);
  }

  int cliSockFD;
  int maxFD = sockFD;
  fd_set readFDS;
  fd_set writeFDS;
  char buffer[BUFFERSIZE];
  char bcBuffer[BUFFERSIZE];
  client clientList[BACKLOG]; 
  char spaceDelim[] = " ";
  char rowDelim[] = "\n";

  while (1)
  {
    FD_ZERO(&readFDS);
    FD_ZERO(&writeFDS);
    FD_SET(sockFD, &writeFDS);
       for (int j = 0; j < BACKLOG; j++)
          {
            if(clientList[j].socket != 0)
            {
              FD_SET(clientList[j].socket, &writeFDS);
            }
          }
    FD_SET(sockFD, &readFDS);
    for (int i = 0; i < BACKLOG; i++)
    {
      if(clientList[i].socket != 0)
      {
        FD_SET(clientList[i].socket, &readFDS);
      }
    }

    for (int i = 0; i < BACKLOG; i++)
    {
      if(clientList[i].socket > maxFD)
      {
        maxFD = clientList[i].socket;
      }
    }
  
    rv = select(maxFD+1, &readFDS, &writeFDS, NULL, 0);
    if (rv == -1)
    {
      perror("PERROR: select");
      continue;
    }
      
    if (FD_ISSET(sockFD, &readFDS))
    {
      struct sockaddr_in clientAddr;
      memset(&clientAddr, 0, sizeof(clientAddr));
      socklen_t clientLen = sizeof(clientAddr);
      int cliSockFD = accept(sockFD, (struct sockaddr*)&clientAddr, &clientLen);
      if (cliSockFD < 1)
      {
        perror("PERROR: accept");
        continue;
      }

      char clientAddrStr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &clientAddr.sin_addr, clientAddrStr, sizeof(clientAddrStr));
      printf("Incoming connection from: %s:%d.\n", clientAddrStr, clientAddr.sin_port);

      bool backlogRdy = false;
      for (int i = 0; i < BACKLOG; i++)
      {
        if (clientList[i].socket == 0)
        {
          backlogRdy = true;
          break;
        }
      }

      if (!backlogRdy)
      {
        printf("Client Rejected due to full backlog.\n");
        close(cliSockFD);
        continue;
      }

      memset(&buffer, 0, sizeof(buffer));
      sprintf(buffer, PROTOCOL);
      rv = send(cliSockFD, buffer, strlen(buffer), 0);
      if (rv < 1)
      {
        perror("PERROR: send HELLO 1");
        close(cliSockFD);
        continue;
      }
     
      memset(&buffer, 0, sizeof(buffer));
      rv = recv(cliSockFD, buffer, BUFFERSIZE-1, 0);
      if (rv == -1)
      {
        perror("PERROR: recv NICK");
        continue;
      }
      
      char* typeHandler = strtok(buffer, spaceDelim);
      char* nickHandler = strtok(NULL, rowDelim);
      
      #ifdef DEBUG
      printf("[%s]:[%s]\n", typeHandler, nickHandler);
      #endif
     
      if (typeHandler != NULL)
      {
        if (strcmp(typeHandler, "NICK") != 0)
        {
          printf("Expected NICK message. \n");
          continue;
        }
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
      bool connection = false;

      if (strlen(nickHandler) < 13 && strlen(nickHandler) > 0)
      {
        reti = regexec(&regExpression, nickHandler, matches, &items, 0);
        if (!reti)
        {
            printf("Nickname approved.\n");
            connection = true;            
        }
        else
        {
            char msgBuffer[100];
            regerror(reti, &regExpression, msgBuffer, sizeof(msgBuffer));
            fprintf(stderr, "Failed Regex: %s\n", msgBuffer);

            printf("Nickname contained not accepted characters.\n");
            connection = false;
        }
      }
      else
      {
          printf("Nickname was too long: <%s>: %zu chars (12 chars max)\n", nickHandler, strlen(nickHandler));
          connection = false;
      }
      regfree(&regExpression);
          
      if (connection)
      {  
        for (int i = 0; i < BACKLOG; i++)
        {
          if (clientList[i].socket == 0)
          {
            sprintf(clientList[i].nickname, "%s", nickHandler);
            clientList[i].socket = cliSockFD;
            clientList[i].address = clientAddr;
            break;
          }
        }
        memset(&buffer, 0, sizeof(buffer));
        sprintf(buffer, "OK\n");
        rv = send(cliSockFD, buffer, strlen(buffer), 0);
        if (rv < 1)
        {
          perror("send OK");
          exit(1);
        }      
        printf("Connection Established.\n\n");
      }
      else
      {
        memset(&buffer, 0, sizeof(buffer));
        sprintf(buffer, "ERROR\n");
        rv = send(cliSockFD, buffer, strlen(buffer), 0);
        if (rv < 1)
        {
          perror("send ERR");
          exit(1);
        }      
        printf("Connection Denied.\n\n");
      }

      #ifdef DEBUG
      for (int i = 0; i < BACKLOG; i++)
      {
        if (i % 2)
        {
          printf("%d. %d\n", i+1, clientList[i].socket);
        }
        else
        {
          printf("%d. %d\t", i+1, clientList[i].socket);
        }
      }
      #endif
    }
 
    // Look for messages from clients
    for (int i = 0; i < BACKLOG; i++)
    {
      if (FD_ISSET(clientList[i].socket, &readFDS))
      {
        memset(buffer, 0, sizeof(buffer));
        rv = recv(clientList[i].socket, buffer, BUFFERSIZE-1, 0);
        if (rv < 1)
        {
          printf("[%d] %s left the chatroom.\n", i, clientList[i].nickname);
          memset(clientList[i].nickname, 0, sizeof(clientList[i].nickname));
          clientList[i].socket = 0;
          continue;

          #ifdef DEBUG
          fprintf(stderr, "Client sent: <%d> [%s]\n", rv, buffer);
          #endif
        }
    
        #ifdef DEBUG
        printf("From Client: %d | %s", rv, buffer);
        #endif
       
        char* typeHandler = strtok(buffer, spaceDelim);
        char* msgHandler = strtok(NULL, rowDelim);
      
        if (strcmp(typeHandler, "MSG") != 0)
        {
          memset(&buffer, 0, sizeof(buffer));
          sprintf(buffer, "ERROR Incorrect message\n");
          rv = send(clientList[i].socket, buffer, strlen(buffer), 0);
          memset(clientList[i].nickname, 0, sizeof(clientList[i].nickname));
          clientList[i].socket = 0;
          if (rv < 1)
          {
            perror("PERROR: send2");
            continue;
          }
        }
        else
        {
          memset(&bcBuffer, 0, sizeof(bcBuffer));
          sprintf(bcBuffer, "%s %s %s\n", typeHandler, clientList[i].nickname, msgHandler);
        }

        for (int j = 0; j < BACKLOG; j++)
        {
          if (clientList[j].socket != 0 && FD_ISSET(clientList[i].socket, &writeFDS))
          {
            rv = send(clientList[j].socket, bcBuffer, strlen(bcBuffer), 0);
            if (rv < 1)
            {
              perror("PERROR: broadcast");
              continue;
            }
            else
            {
              #ifdef DEBUG
              printf("To Client #%d: %d | %s", j, rv, bcBuffer);
              #endif
            }
          }
        }
      }
    }
  }
  close(cliSockFD);
  close(sockFD);
}
