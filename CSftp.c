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
int quit(int fd);
int cwd(int fd, char *directory);
int cdup(int fd, char initial[]);
int type(int fd, char *type);
int mode(int fd, char *mode);
int stru(int fd, char *structure);
int pasv(int fd, char *port);
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
    send_string(new_fd, "220 connection ready.\n");

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
      printf("invalid command, please enter a valid command\n");
      send_string(new_fd, "500 Syntax error, command unrecognized.\n");
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
    return pasv(new_fd, args);
    break;
  case NLST:
    return nlst(new_fd, args);
    break;
  default:
    break;
  }

  return -1;
}

int user(int fd, char *user)
{
  if (user == NULL)
  {
    send_string(fd, "501 Syntax Error in parameters or arguments. Please input username\n");
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

  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }

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
      send_string(fd, "550 Requested action not taken. Cannot change directory to /\n");
      return 0;
    }
    else if (strcmp(start, ".") == 0 || strcmp(start, "..") == 0)
    {
      send_string(fd, "550 Requested action not taken. Directory cannot start with ./ or ../\n");
      return 0;
    }

    while (start != NULL)
    {
      if (strcmp(start, "..") == 0)
      {
        send_string(fd, "550 Requested action not taken. Directory cannot contain ../\n");
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
      send_string(fd, "250 Requested file action okay, completed. Directory changed successfully.\n");
    }
  }

  return 0;
}

int cdup(int fd, char initial[])
{
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
  char current[MAX_DATA_SIZE];

  getcwd(current, MAX_DATA_SIZE);
  if (strcmp(current, initial) == 0)
  {
    send_string(fd, "Cannot process this command from parent directory.\n");
    return 0;
  }

  if (chdir("..") == 0)
  {
    send_string(fd, "200 Command Ok. Directly successfully changed back to parent directory.\n");
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
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
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
  else if (strcasecmp(type, "E") == 0 || strcasecmp(type, "L") == 0)
  {
    send_string(fd, "504 command not implemented for that representation type.\n");
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
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
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
  else if (strcasecmp(mode, "B") == 0 || strcasecmp(mode, "C") == 0)
  {
    send_string(fd, "504 command not implemented for that transmission mode.\n");
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
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
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
  else if (strcasecmp(structure, "R") == 0 || strcasecmp(structure, "P") == 0)
  {
    send_string(fd, "504 command not implemented for that structure.\n");
    return 0;
  }
  else
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
  }
  return 0;
}

int retr(int fd, char *file)
{
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
  if (file == NULL)
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }

  if (pasv_fd == -1)
  {
    send_string(fd, "425 Use PORT or PASV first.\n");
    return 0;
  }

  if (rep_type == NULL)
  {
    send_string(fd, "425 Use TYPE first.\n");
    return 0;
  }

  if (rep_type[0] == 'A')
  {
    send_string(fd, "425 Use TYPE I first.\n");
    return 0;
  }

  FILE *fp = fopen(file, "rb");
  if (fp == NULL)
  {
    send_string(fd, "550 File not found.\n");
    return 0;
  }

  send_string(fd, "150 File status okay; about to open data connection.\n");

  int new_pasv_fd = accept(pasv_fd, NULL, NULL);
  if (new_pasv_fd == -1)
  {
    printf("error when accepting connection\n");
    return -1;
  }

  char buf[MAX_DATA_SIZE];
  int n;
  while ((n = fread(buf, 1, MAX_DATA_SIZE, fp)) > 0)
  {
    if (send(new_pasv_fd, buf, n, 0) == -1)
    {
      printf("error when sending data\n");
      return -1;
    }
  }

  fclose(fp);
  close(new_pasv_fd);
  close(pasv_fd);
  pasv_fd = -1;

  send_string(fd, "226 Closing data connection. Requested file action successful.\n");
  return 0;
}

int pasv(int fd, char *args)
{
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }
  // TODO: check syntax error 501
  // if (args != NULL) {
  //   send_string(fd, "501 Syntax error in parameters or arguments.\n");
  //   return 0;
  // }

  int port = rand() % 65535 + 1024;
  pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (pasv_fd == -1)
  {
    printf("error when creating socket\n");
    return -1;
  }
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = INADDR_ANY;
  memset(server.sin_zero, '\0', sizeof(server.sin_zero));
  if (bind(pasv_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
  {
    printf("error when binding socket\n");
    return -1;
  }

  if (listen(pasv_fd, 1) == -1)
  {
    printf("error when listening\n");
    return -1;
  }

  char *ip = inet_ntoa(client.sin_addr);
  printf("ip: %s\n", ip);
  printf("port: %d\n", port);

  char *msg = malloc(100);
  sprintf(msg, "227 Entering Passive Mode (%s,%d,%d).\n", ip, port / 256, port % 256);
  send_string(fd, msg);
  free(msg);
  return 0;
}

int nlst(int fd, char *args)
{
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }

  // if (args != NULL) {
  //   send_string(fd, "501 Syntax error in parameters or arguments.\n");
  //   return 0;
  // }

  if (pasv_fd == -1)
  {
    send_string(fd, "425 Use PORT or PASV first.\n");
    return 0;
  }

  // TODO: check if use type first

  send_string(fd, "150 File status okay; about to open data connection.\n");

  int new_pasv_fd = accept(pasv_fd, NULL, NULL);
  if (new_pasv_fd == -1)
  {
    printf("error when accepting connection\n");
    return -1;
  }

  char *msg = malloc(100);
  sprintf(msg, "226 Closing data connection. Requested file action successful.\n");
  send_string(fd, msg);
  free(msg);

  // TODO: make sure the file is in the current directory
  listFiles(new_pasv_fd, ".");

  close(new_pasv_fd);
  close(pasv_fd);
  pasv_fd = -1;

  return 0;
}

void send_string(int fd, char *msg)
{
  if (send(fd, msg, strlen(msg), 0) == -1)
  {
    printf("error when sending message\n");
  }
}