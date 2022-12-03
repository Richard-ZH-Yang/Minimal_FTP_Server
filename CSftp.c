#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "dir.h"
#include "usage.h"
#include <pthread.h>

#define BACKLOG 10 // how many pending connetions queue will hold
#define MAX_DATA_SIZE 256

// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.
/* this function is run by the second thread */
void send_string(int fd, char *msg);
int parse_cmd(char *cmd);
int user(int fd, char *username);
int quit();
int cwd(int fd, char *directory);
int cdup(int fd, char initial[]);

typedef enum
{
  USER,
  QUIT,
  CWD,
  CDUP,
  TYPE,
  MODE,
  STRU,
  RETR,
  PASV,
  NLST
} FTP_CMD;

const char *command_str[] = {"USER",
                             "QUIT",
                             "CWD",
                             "CDUP",
                             "TYPE",
                             "MODE",
                             "STRU",
                             "RETR",
                             "PASV",
                             "NLST"};

int sockfd, new_fd, *new_socket;
struct sockaddr_in server, client;
char buf[MAX_DATA_SIZE];
int portNum;
int user_in = 0;
char initial[MAX_DATA_SIZE];

void *inc_x()
{
  printf("x increment finished\n");
  return NULL;
}

void *handler(void *socket);

int main(int argc, char **argv)
{

  // This is some sample code feel free to delete it
  // This is the main program for the thread version of nc

  // int i;
  // pthread_t child;
  // pthread_create(&child, NULL, inc_x, NULL);

  int client_size;

  // Check the command line arguments
  if (argc != 2)
  {
    usage(argv[0]);
    return -1;
  }

  portNum = atoi(argv[1]);

  if (portNum < 1024 || portNum > 65535)
  {
    usage(argv[0]);
    return -1;
  }

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("error opening socket");
    exit(1);
  }

  bzero((char *)&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(portNum);
  server.sin_addr.s_addr = INADDR_ANY;
  bzero(&(server.sin_zero), 8);

  printf("port: %d\n", server.sin_port);

  if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
  {
    perror("bind");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1)
  {
    perror("listen");
    exit(1);
  }

  getcwd(initial, MAX_DATA_SIZE);

  while (1)
  {
    client_size = sizeof(client);
    if ((new_fd = accept(sockfd, (struct sockaddr *)&client, &client_size)) == -1)
    {
      perror("accept error");
      continue;
    }
    printf("server: got connection from %s\n", inet_ntoa(client.sin_addr));

      pthread_t child;
      if (pthread_create(&child, NULL, handler, new_socket) < 0)
      {
        perror("cannot create thread");
        return -1;
      }
      pthread_join(child, NULL);

    // while (1)
    // {
    //   bzero(buf, MAX_DATA_SIZE);
    //   if (recv(new_fd, buf, MAX_DATA_SIZE - 1, 0) < 0 || strcmp(buf, "") == 0)
    //   {
    //     printf("error reading from socket\n");
    //     close(new_fd);
    //     break;
    //   }

    //   if (parse_cmd(buf) == -1)
    //   {
    //     break;
    //   }
    // }

    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor
    // returned for the ftp server's data connection

    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
  }
}

void *handler(void *socket)
{
  printf("thread created!\n");
  char msg[1024];
  char command[24];

  while (recv(new_fd, buf, MAX_DATA_SIZE - 1, 0) > 0)
  {
    if (parse_cmd(buf) == -1)
    {
      // TODO: what to do when invalid command?
      printf("invalid command, please enter a valid command\n");
      continue;
    }
  }
}

int parse_cmd(char *cmd)
{
  char *args;
  char *com;
  FTP_CMD command;

  com = strtok(cmd, " \r\n");
  args = strtok(NULL, " \r\n");

  printf("command %s\n", com);

  for (int i = USER; i <= NLST; ++i)
  {
    if (strcasecmp(com, command_str[i]) == 0)
    {
      command = (FTP_CMD)i;
    }
  }

  printf("%d", command);

  switch (command)
  {
  case USER:
    return user(new_fd, args);
    break;
  case QUIT:
    return quit(new_fd);
    break;
  case CWD:
    return cwd(new_fd, args);
    break;
  case CDUP:
    return cdup(new_fd, initial);
    break;
  case TYPE:
  case MODE:
  case STRU:
  case RETR:
  case PASV:
  case NLST:
  default:
    break;
  }

  return -1;
}

int user(int fd, char *user)
{
  printf("in user\n");
  char msg[MAX_DATA_SIZE];

  if (user_in == 1)
  {
    send_string(fd, "already logged in\n"); // TODO: check error codes
  }
  else if (user == NULL)
  {
    send_string(fd, "please input username\n");
  }
  else
  {
    user_in = 1;
    send_string(fd, "331 Username ok, send password.\n");
  }

  return 0;
}

int quit(int fd)
{
  send_string(fd, "221 Goodbye.\n");
  user_in = 0;
  close(new_fd);
  return -1;
}

// For security reasons you are not accept any CWD command that starts with ./ or ../ or contains ../ in it
int cwd(int fd, char *directory)
{
  char msg[MAX_DATA_SIZE];
  char dir[MAX_DATA_SIZE];

  if (directory == NULL)
  {
    send_string(fd, "Failed to change directory. Need to input directory.\n");
  }
  else
  {
    strcpy(dir, directory);
    char *start = strtok(dir, "/\r\n");
    if (strcmp(start, ".") == 0 || strcmp(start, ".") == 0)
    {
      send_string(fd, "Directory cannot start with ./ or ../");
    }

    start = strtok(NULL, "/\r\n");

    while (start != NULL)
    {
      if (strcmp(start, "..") == 0)
      {
        send_string(fd, "Directory cannot contain ../");
      }
    }

    if (chdir(dir) == -1)
    {
      send_string(fd, "Failed to change directory.\n");
    }
    else
    {
      send_string(fd, "Directory changed successfully.\n");
    }
  }

  return 0;
}

int cdup(int fd, char initial[])
{
  char current[MAX_DATA_SIZE];
  char msg[MAX_DATA_SIZE];

  getcwd(current, MAX_DATA_SIZE);
  if (strcmp(current, initial) == 0)
  {
    send_string(fd, "Cannot process this command from parent directory.\n");
  }

  if (chdir("..") == 0)
  {
    send_string(fd, "Directly successfully changed back to parent directory.\n");
  }
  else
  {
    send_string(fd, "Failed to change directory.\n");
  }

  return 0;
}

void send_string(int fd, char *msg)
{
  if (send(fd, msg, strlen(msg), 0) == -1)
  {
    printf("error when sending message\n");
  }
}