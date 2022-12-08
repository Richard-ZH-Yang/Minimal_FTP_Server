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
int nlst(int fd, char *file);

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

int sockfd, new_fd, *new_socket, pasv_fd;
struct sockaddr_in server, client;
char buf[MAX_DATA_SIZE];
int portNum;
int user_in = 0;
char initial[MAX_DATA_SIZE];
char *curr_path;
char *rep_type;
void *inc_x()
{
  printf("x increment finished\n");
  return NULL;
}

void *handler(void *socket);

void setCurrPath(char* path);

void getRightPath (char* result, char* path);

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
  curr_path = malloc(MAX_DATA_SIZE);
  curr_path = initial;
  int client_size;
  rep_type = "A"; // set default type

  while (1)
  {
    client_size = sizeof(client);
    if ((new_fd = accept(sockfd, (struct sockaddr *)&client, &client_size)) == -1)
    {
      printf("451 Requested action aborted. Local error in processing.\n");
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
  }
  return 0;

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
  char *line;
  FTP_CMD command;

  line = strtok(cmd, "\r\n");
  com = strtok(line, " ");
  args = strtok(NULL, " ");
  if (args != NULL && strtok(NULL, " ") != NULL) {
    send_string(new_fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }

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
  else if (strcmp(user, "cs317") == 0)
  {
    user_in = 1;
    send_string(fd, "230 User logged in, proceed.\n");
  } else {
    send_string(fd, "530 Not logged in.\n");
  }

  return 0;
}

int quit(int fd)
{
  user_in = 0;
  send_string(fd, "221 Goodbye.\n");
  close(new_fd);
  close(pasv_fd);
  close(new_socket);
  free(curr_path);
  return 0;
}

// For security reasons you are not accept any CWD command that starts with ./ or ../ or contains ../ in it
int cwd(int fd, char *directory)
{

  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }

  if (directory == NULL)
  {
    send_string(fd, "501 Invalid argument. Need to input directory.\n");
  }
  else
  {

    setCurrPath(directory);
    if (access(curr_path, F_OK) == -1) {
      send_string(fd, "550 Requested action not taken. File unavailable (e.g., file not found, no access).\n");
      return 0;
    }
    send_string(fd, "250 Requested file action okay, completed. Directory changed successfully.\n");

  }

  return 0;
}

int cdup(int fd, char initial[])
{
  if (user_in == 0)
  {
    send_string(fd, "425 Not logged in.\n");
    return 0;
  }

  if (strcmp(curr_path, initial) == 0)
  {
    send_string(fd, "550 Cannot process this command from parent directory.\n");
    return 0;
  }

  strcpy(curr_path, initial);
  send_string(fd, "200 Command Ok. Directly successfully changed back to parent directory.\n");

  return 0;
}

int type(int fd, char *type)
{
  if (user_in == 0)
  {
    send_string(fd, "425 Not logged in.\n");
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
    send_string(fd, "425 Not logged in.\n");
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
    send_string(fd, "425 Not logged in.\n");
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
    send_string(fd, "425 Not logged in.\n");
    return 0;
  }
  if (file == NULL)
  {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }

  if (pasv_fd == -1)
  {
    send_string(fd, "425 Use PASV first.\n");
    return 0;
  }

  char* currFile = malloc(MAX_DATA_SIZE);
  getRightPath(currFile, file);
  if (access(currFile, F_OK) == -1) {
    send_string(fd, "550 Requested action not taken. File unavailable (e.g., file not found, no access).\n");
    return 0;
  }

  FILE *fp = fopen(currFile, "rb");
  if (!fp) {
    send_string(fd, "550 File not found.\n");
    return 0;
  }

  send_string(fd, "150 File status okay; about to open data connection.\n");

  int new_pasv_fd = accept(pasv_fd, NULL, NULL);
  if (new_pasv_fd == -1) {
    printf("451 Requested action aborted. Local error in processing.\n");
    return 0;

  }

  char buf[MAX_DATA_SIZE];
  int n;
  while ((n = fread(buf, 1, MAX_DATA_SIZE, fp)) > 0) {
    if (send(new_pasv_fd, buf, n, 0) == -1) {
      printf("Error when sending data\n");
      return 0;

    }
  }

  free(currFile);
  fclose(fp);
  close(new_pasv_fd);
  close(pasv_fd);
  pasv_fd = -1;

  send_string(fd, "226 Closing data connection. Requested file action success.\n");
  return 0;
}

int pasv(int fd, char* args)
{
  if (args != NULL) {
    send_string(fd, "501 Syntax error in parameters or arguments.\n");
    return 0;
  }

  if (user_in == 0)
  {
    send_string(fd, "425 Not logged in.\n");
    return 0;
  }

    pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pasv_fd == -1) {
      printf("Error when creating socket\n");
      return 0;
    }
    int port = rand() % 65535 + 1024;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    memset(server.sin_zero, '\0', sizeof(server.sin_zero));
    if (bind(pasv_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
      printf("Error when binding socket\n");
      return 0;
    }

    if (listen(pasv_fd, 1) == -1) {
      printf("Error when listening\n");
      return 0;
    }


  char *ip = inet_ntoa(client.sin_addr);
  printf("ip: %s\n", ip);
  printf("port: %d\n", port);

  char *msg = malloc(100);
  int IPNum[4] = {0,0,0,0};
  if (sscanf(ip, "%d.%d.%d.%d", &IPNum[0], &IPNum[1], &IPNum[2], &IPNum[3]) != 4){
    printf("Error when parsing ip address\n");
    return 0;
  }
  sprintf(msg, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\n",IPNum[0],IPNum[1],IPNum[2],IPNum[3], port / 256, port % 256);
  send_string(fd, msg);
  free(msg);
  return 0;
}

int nlst(int fd, char *file)
{
  if (user_in == 0)
  {
    send_string(fd, "530 Not logged in.\n");
    return 0;
  }

  if (pasv_fd == -1) {
    send_string(fd, "425 Need call PASV first.\n");
    return 0;
  }

  if (strcmp(rep_type, "A") != 0) {
    send_string(fd, "425 Need change to TYPE A first.\n");
    return 0;
  }
  printf("%s\n", curr_path);
  char* currDir = malloc(MAX_DATA_SIZE);
  getRightPath(currDir, file);
  if (access(currDir, F_OK) == -1) {
    send_string(fd, "550 Requested action not taken. File unavailable (e.g., file not found, no access).\n");
    return 0;
  }

  send_string(fd, "150 File status okay; about to open data connection.\n");

  int new_pasv_fd = accept(pasv_fd, NULL, NULL);
  if (new_pasv_fd == -1) {
    printf("451 Requested action aborted. Local error in processing.\n");
    return 0;

  }

  char *msg = malloc(100);
  sprintf(msg, "226 Closing data connection. Requested file action success.\n");
  send_string(fd, msg);
  free(msg);

  // TODO: make sure the file is in the current directory
  if (listFiles(new_pasv_fd, currDir) == -1) {
    sprintf(msg, "450 Requested file action not taken. File unavailable (e.g., file busy).\n");
    return 0;
  }

  free(currDir);
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

// EFFECTS: get a path in a abosulte path format e.g. /home/usr/......./a3_rzhyang_yliu8912/path
void setCurrPath(char* path) {
  // reset curr_path if it's not valid
  if (access(curr_path, F_OK) == -1) {
    strcpy(curr_path, initial);
  }

  if (path == NULL || strlen(path) == 1 && path[0] == '.') {
    return;
  } else if (strlen(path) == 0) {
    strcpy(curr_path, initial);
  } else if (strlen(path) > 0 && path[0] == '/') {
    strcpy(curr_path, initial);
    strcat(curr_path, path);
  } else {
    strcpy(curr_path, initial);
    strcat(curr_path, "/");
    strcat(curr_path, path);
  }

}

// EFFECTS: get a path in a abosulte path format e.g. /home/usr/......./a3_rzhyang_yliu8912/path
void getRightPath (char* result, char* path) {
  if (path == NULL || strlen(path) == 1 && path[0] == '.') {
    strcpy(result, curr_path);
  } else if (strlen(path) == 0) {
    strcpy(result, curr_path);
  } else if (strlen(path) > 0 && path[0] == '/') {
    strcpy(result, initial);
    strcat(result, path);
  } else {
    strcpy(result, initial);
    strcat(result, "/");
    strcat(result, path);
  }

}