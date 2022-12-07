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
int type(int fd, char *type);
int mode(int fd, char *mode);
int stru(int fd, char *structure);
int pasv(char* port);
int retr(int fd, char *file);
int nlst();

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
char *rep_type;
int pasv_fd;
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
  int client_size;
  rep_type = "A"; // set default type

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
    return type(new_fd, args);
    break;
  case MODE:
    return mode(new_fd, args);
    break;
  case STRU:
    return stru(new_fd, args);
    break;
  case RETR:
    return retr(new_fd, args);
    break;
  case PASV:
    return pasv(args);
    break;
  case NLST:
    return nlst();
    break;
  default:
    break;
  }

  return -1;
}

int user(int fd, char *user)
{
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
    send_string(fd, "230 User logged in, proceed.\n");
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
  char dir[MAX_DATA_SIZE];

  if (directory == NULL)
  {
    send_string(fd, "Failed to change directory. Need to input directory.\n");
  }
  else
  {
    strcpy(dir, directory);
    char *start = strtok(dir, "/\r\n");
    // Directory inputted = '/'
    if (start == NULL)
    {
      send_string(fd, "Cannoto change directory to /\n");
      return 0;
    }
    else if (strcmp(start, ".") == 0 || strcmp(start, "..") == 0)
    {
      send_string(fd, "Directory cannot start with ./ or ../\n");
      return 0;
    }

    while (start != NULL)
    {
      if (strcmp(start, "..") == 0)
      {
        send_string(fd, "Directory cannot contain ../\n");
        return 0;
      }

      start = strtok(NULL, "/\r\n");
    }

    if (chdir(dir) == -1)
    {
      send_string(fd, "Failed to change directory.\n");
      return 0;
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

  getcwd(current, MAX_DATA_SIZE);
  if (strcmp(current, initial) == 0)
  {
    send_string(fd, "Cannot process this command from parent directory.\n");
    return 0;
  }

  if (chdir("..") == 0)
  {
    send_string(fd, "Directly successfully changed back to parent directory.\n");
    return 0;
  }
  else
  {
    send_string(fd, "Failed to change directory.\n");
  }

  return 0;
}

int type(int fd, char *type)
{
  if (type == NULL)
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }
  else if (strcasecmp(type, "A") == 0)
  {
    rep_type = "A";
    send_string(fd, "200 Command okay. Switching to ASCII.\n");
    return 0;
  }
  else if (strcasecmp(type, "I") == 0)
  {
    rep_type = "I";
    send_string(fd, "200 Command okay. Switching to raw binary.\n");
    return 0;
  }
  else
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
  }
  return 0;
}

int mode(int fd, char *mode)
{
  if (mode == NULL)
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }
  else if (strcasecmp(mode, "S") == 0)
  {
    send_string(fd, "200 Command okay. Mode set to stream mode.\n");
    return 0;
  }
  else
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
  }
  return 0;
}

int stru(int fd, char *structure)
{
  if (structure == NULL)
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }
  else if (strcasecmp(structure, "F") == 0)
  {
    send_string(fd, "200 Command okay. Data structure set to file.\n");
    return 0;
  }
  else
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
  }
  return 0;
}

int retr(int fd, char *file) {

  return 0;
}

int pasv(char *args)
{
  // TODO: check syntax error 501

  while (1) {
    int port = rand() % 10000 + 10000;
    int pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pasv_fd == -1) {
      printf("error when creating socket\n");
      return -1;
    }

    struct sockaddr_in pasv_addr;
    pasv_addr.sin_family = AF_INET;
    pasv_addr.sin_port = htons(port);
    pasv_addr.sin_addr.s_addr = INADDR_ANY;
    memset(pasv_addr.sin_zero, '\0', sizeof(pasv_addr.sin_zero));

    if (bind(pasv_fd, (struct sockaddr *) &pasv_addr, sizeof(pasv_addr)) == -1) {
      printf("error when binding socket\n");
      return -1;
    }

    if (listen(pasv_fd, 1) == -1) {
      printf("error when listening\n");
      return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int pasv_new_fd = accept(pasv_fd, (struct sockaddr *) &client_addr, &addr_size);
    if (pasv_new_fd == -1) {
      printf("error when accepting\n");
      return -1;
    }

    char *ip = inet_ntoa(client_addr.sin_addr);
    printf("ip: %s\n", ip);
    printf("port: %d\n", port);

    char *msg = malloc(100);
    sprintf(msg, "227 Entering Passive Mode (%s,%d,%d).\n", ip, port / 256, port % 256);
    send_string(new_fd, msg);
    free(msg);
  }
  return 0;
}

int nlst() {
  return 0;
}


void send_string(int fd, char *msg)
{
  if (send(fd, msg, strlen(msg), 0) == -1)
  {
    printf("error when sending message\n");
  }
}