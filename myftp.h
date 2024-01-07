/*
Name: Mellanie Martin
Class: CS360
Date: 10/10/2023
*/

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <netdb.h>
# include <unistd.h>

# define PORT_NUM 49776
# define CHAR_PORT "49776"
# define BACKLOG 4
# define BUFF_SIZE 1024

int clientReading(int socketfd, char const *arg);
int sendCommand(int socketfd, char *command);
int getDataConn(int socketfd, char const *arg);
int getResponse(int socketfd);
int mycd(char *pathname);
int rcd(int socketfd, char *command);
int myls();
int rls(int socketfd, char const *arg);
int get(int socketfd, char *command, char const *arg);
int show(int socketfd, const char *arg, char *command);
int put(int socketfd, char *command, char const *arg);

int serverReading(int socketfd);
int changeDir(int controlfd, char *buffer);
int dataConn(int controlfd);
int sls(int controlfd, int datafd);
int getFile(int controlfd, int datafd, char *buffer);
int putFile(int controlfd, int datafd, char *buffer);
int sendResponse(int socketfd, char res, char *msg);
