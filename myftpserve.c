# include "myftp.h"

int main() {

    int socketfd; int controlfd; int datafd = -1;
    int input; int status;
    char hostName[NI_MAXHOST];
    struct sockaddr_in server; struct sockaddr_in control;
    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT_NUM);
    server.sin_family = AF_INET;

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Fatal error: %s\n", strerror(errno));
        return 1;
    }
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        printf("Error: %s\n", strerror(errno));
    }

    if ((bind(socketfd, (struct sockaddr *) &server, sizeof(server))) == -1) {
        printf("Fatal error: %s\n", strerror(errno));
        return 1;
    }

    if ((listen(socketfd, BACKLOG)) == -1) {
        printf("Fatal error: %s\n", strerror(errno));
        return 1;
    }

    unsigned int length = sizeof(struct sockaddr_in);
    int pid;
    char buffer[BUFF_SIZE];
    while (1) {
        controlfd = accept(socketfd, (struct sockaddr *) &control, &length);
        if (controlfd == -1) printf("Error: %s\n", strerror(errno));

        int hostEntry = getnameinfo((struct sockaddr *) &control, sizeof(control), hostName, sizeof(hostName), NULL, 0, NI_NUMERICSERV);
        if (hostEntry != 0) {
            printf("Error: %s\n", gai_strerror(hostEntry));
        }
        printf("%s connected to the server.\n", hostName);

        pid = fork();
        if (pid) {
            close(controlfd);
            continue;
        }
        
        serverReading(controlfd);
	printf("Client exiting; current child process closing\n");
	return 0;
    }
}

int serverReading(int socketfd) {
    int input;
    char buffer[BUFF_SIZE];

    int datafd = -1;

    while (1) {
        int i = 0;
        while (1) {
            input = read(socketfd, &buffer[i], 1);
	    if (input <= 0) {
		printf("Error: EOF detected in control connection\n");
		return 1;
	    }
            if(buffer[i] == '\n') {
                break;
            }
            i++;
        }
        buffer[i] = '\0';

        if (buffer[0] == 'Q') {
	    sendResponse(socketfd, 'A', NULL);
            return 0;
        } else if (buffer[0] == 'C') {
            changeDir(socketfd, buffer);
        } else if (buffer[0] == 'L') {
            sls(socketfd, datafd);
	    datafd = -1;
        } else if (buffer[0] == 'G') {
            getFile(socketfd, datafd, buffer);
	    datafd = -1;
        } else if (buffer[0] == 'P') {
            putFile(socketfd, datafd, buffer);
	    datafd = -1;
        } else if (buffer[0] == 'D') {
            datafd = dataConn(socketfd);
	    if (datafd == -1) {
	        printf("Data connection could not be established [CMD FAIL]\n");
	    } else {
		printf("Data connection has been established [CMD SUCCESS]\n");
	    }
        }
    }
}

int changeDir(int controlfd, char *buffer) {
    buffer = buffer + 1;

    if (chdir(buffer) == -1) {
	    sendResponse(controlfd, 'E', strerror(errno));
	    printf("Current directory could not be changed to %s [CMD FAIL]\n", buffer);
	    return 1;
    }
    sendResponse(controlfd, 'A', NULL);
    printf("Current directory changed to %s [CMD SUCCESS]\n", buffer);
    return 0;
}

int sls(int controlfd, int datafd) {
    if (datafd == -1) {
        sendResponse(controlfd, 'E', "Data connection has not been established, rls cannot be performed");
	printf("Data connection has not been established, rls cannot be performed [CMD FAIL]\n");
        return 1;
    }

    sendResponse(controlfd, 'A', NULL);

    int status;

    char *lsArgs[3];
    lsArgs[0] = "ls"; lsArgs[1] = "-l"; lsArgs[2] = NULL;

    int forkVal = fork();
    if (forkVal) { 
        close(datafd);
        waitpid(-1, &status, 0);
    } else if (!forkVal) {
        close(1);
        dup(datafd);
        if (execvp("ls", lsArgs) == -1) {
	    sendResponse(controlfd, 'E', strerror(errno));
            printf("For rls: ls -l execution failure -- %s [CMD FAIL]\n", strerror(errno));
            return 1;
        }
    } else if (forkVal == -1) {
	sendResponse(controlfd, 'E', strerror(errno));
        printf("For rls: fork function error -- %s [CMD FAIL]\n", strerror(errno));
        return 1;
    }

    printf("For rls: ls -l report of current directory has been sent to client [CMD SUCCESS]\n");
    return 0;
}

int getFile(int controlfd, int datafd, char *buffer) {
    if (datafd == -1) {
	sendResponse(controlfd, 'E', "Data connection has not been established, file cannot be sent to client via 'get'");
	printf("Data connection has not been established, file cannot be sent to client via 'get' [CMD FAIL]\n");
        return 1;
    }

    char *fileName = buffer + 1; 

    int filefd = open(fileName, O_RDONLY);
    if (filefd == -1) {
	printf("For get: file cannot be opened -- %s [CMD FAIL]\n", strerror(errno));
	sendResponse(controlfd, 'E', strerror(errno));
	return 1;
    }

    sendResponse(controlfd, 'A', NULL);

    int numRead; char readBuffer[BUFF_SIZE];
    while ((numRead = read(filefd, readBuffer, BUFF_SIZE)) > 0) {
        if (write(datafd, readBuffer, numRead) != numRead) {
            printf("'get' command failure in write -- %s [CMD FAIL]\n", strerror(errno));
	    sendResponse(controlfd, 'E', strerror(errno));
            return 1;
        }
    }
    printf("%s transferred to client via 'get' [CMD SUCCESS]\n", fileName);

    close(filefd); close(datafd);
    return 0;
}

int putFile(int controlfd, int datafd, char *buffer) {
    if (datafd == -1) {
        sendResponse(controlfd, 'E', "Data connection has not been established");
        return 1;
    }

    char *fileName = buffer + 1;

    int result = access(fileName, F_OK);
    if (result == 0) {
        printf("For put: File already exists locally [CMD FAIL]\n");
	sendResponse(controlfd, 'E', "File already exists remotely\n");
        return 1;
    }

    sendResponse(controlfd, 'A', NULL);

    int filefd = open(fileName, O_CREAT | O_WRONLY, 0755);

    int numRead;
    while ((numRead = read(datafd, buffer, BUFF_SIZE)) > 0) {
        if (write(filefd, buffer, numRead) != numRead) {
            printf("'put' command failure in write -- %s [CMD FAIL]\n", strerror(errno));
	    sendResponse(controlfd, 'E', strerror(errno));
            return 1;
        }
    }
    printf("%s transferred to server via 'put' [CMD SUCCESS]\n", fileName);


    return 0;
}

int sendResponse(int socketfd, char res, char *msg) {
    if (res == 'A') {
	char *send = malloc(3*sizeof(char));
	sprintf(send, "%c\n", res);
        if (write(socketfd, send, strlen(send)) == -1) {
            printf("Error: %s\n", strerror(errno));
	    return 1;
	}	
    } else if (res == 'E') {
        char *concatenate = malloc((strlen(msg) + 3)*sizeof(char));
	sprintf(concatenate, "%c%s\n", res, msg);
	if (write(socketfd, concatenate, strlen(concatenate)) == -1) {
            printf("Error: %s\n", strerror(errno));
	    return 1;
        }
	free(concatenate);
    }
    return 0;
}

int dataConn(int controlfd) {
    int dataSock;
    struct sockaddr_in data; struct sockaddr_in empty; struct sockaddr_in client;
    memset(&empty, 0, sizeof(empty)); memset(&data, 0, sizeof(data));
    data.sin_addr.s_addr = htonl(INADDR_ANY);
    data.sin_port = htons(0);
    data.sin_family = AF_INET;

    if ((dataSock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        sendResponse(controlfd, 'E', strerror(errno));
        return -1;
    }
    if (setsockopt(dataSock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        sendResponse(controlfd, 'E', strerror(errno));
    }

    if ((bind(dataSock, (struct sockaddr *) &data, sizeof(data))) == -1) {
	sendResponse(controlfd, 'E', strerror(errno));
        return -1;
    }

    unsigned int length = sizeof(struct sockaddr_in);
    if (getsockname(dataSock, (struct sockaddr *) &empty, &length) == -1) {
        sendResponse(controlfd, 'E', strerror(errno));
    }

    int portNum = ntohs(empty.sin_port);

    char *acknowledgement = "A";
    char *msg = malloc(8*sizeof(char));
    sprintf(msg, "%c%d\n", 'A', portNum);
    if (write(controlfd, msg, strlen(msg)) == -1) {
	sendResponse(controlfd, 'E', strerror(errno));
    }

    if ((listen(dataSock, 1)) == -1) {
	sendResponse(controlfd, 'E', strerror(errno));
        return -1;
    }

    int datafd = accept(dataSock, (struct sockaddr *) &client, &length);
    if (datafd == -1) sendResponse(controlfd, 'E', strerror(errno));
    free(msg);
    
    return datafd;
}
