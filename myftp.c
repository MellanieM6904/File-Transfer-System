# include "myftp.h"

int main(int argc, char const *argv[]) {
    int socketfd; int result; int input;
    struct addrinfo info, *data;
    memset(&info, 0, sizeof(info));
    
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    result = getaddrinfo(argv[1], CHAR_PORT, &info, &data);
    if (result != 0) {
        printf("Fatal error: %s\n", gai_strerror(result));
        return 1;
    }

    if ((socketfd = socket(data -> ai_family, data -> ai_socktype, 0)) == -1) {
        printf("Fatal error: %s\n", strerror(errno));
        return 1;
    }

    if (connect(socketfd, data -> ai_addr, data -> ai_addrlen) != 0) {
        printf("Fatal error: %s\n", strerror(errno));
        return 1;
    }

    printf("%s connected to the server.\n", argv[1]);

    clientReading(socketfd, argv[1]);
    
    return 0;

}

int clientReading(int socketfd, char const *arg) {
    int input;
    char buffer[BUFF_SIZE];
    while (1) {
        int i = 0;
        char **command = (char **)calloc(BUFF_SIZE, sizeof(char *));
        
        while (1) {
            input = read(0, &buffer[i], 1);;
            if(buffer[i] == '\n') {
		buffer[i] = '\0';
	        break;
	    }
            i++;
	}
        if (i == 1) {
            printf("Please type a command\n");
            continue;
        }

        int tokNum = 0;
        char *token = strtok(buffer, " ");
        while (token) {
            command[tokNum] = strdup(token);
            token = strtok(NULL, " ");
            tokNum++;
        }

        if (strcmp(command[0], "exit") == 0) {
            sendCommand(socketfd, "Q\n");
	    getResponse(socketfd);
	    printf("Client disconnecting and closing. Bye!\n");
            return 0;

        } else if (strcmp(command[0], "cd") == 0) {
            if ((tokNum - 1) == 0) {
                printf("[Not enough arguments]\n");
                continue;
            }
            mycd(command[1]);

        } else if (strcmp(command[0], "rcd") == 0) {
            if ((tokNum - 1) == 0) {
                printf("[Not enough arguments]\n");
                continue;
            }
            rcd(socketfd, command[1]);
    
        } else if (strcmp(command[0], "ls") == 0) {
            myls();

        } else if (strcmp(command[0], "rls") == 0) {
            rls(socketfd, arg);

        } else if (strcmp(command[0], "get") == 0) {
            if ((tokNum - 1) == 0) {
                printf("[Not enough arguments]\n");
                continue;
            }
            get(socketfd, command[1], arg);

        } else if (strcmp(command[0], "show") == 0) {
	    if ((tokNum - 1) == 0) {
                printf("[Not enough arguments]\n");
                continue;
            }
            show(socketfd, arg, command[1]);

        } else if (strcmp(command[0], "put") == 0) {
            if ((tokNum - 1) == 0) {
                printf("[Not enough arguments]\n");
                continue;
            }
            put(socketfd, command[1], arg);
        } else {
            printf("[Improper command]\n");
            continue;
        }

        for (int k = 0; k < tokNum; k++) {
            free(command[k]);
        }
        free(command);
        memset(buffer, 0, sizeof(buffer));
    }

}

int sendCommand(int socketfd, char *command) {
    if (write(socketfd, command, strlen(command)) == -1) {
        printf("Error: %s\n", strerror(errno));
	return 1;
    }
    return 0;
}

int getDataConn(int socketfd, char const *arg) {
    int input; int datafd; int i = 0;
    char buffer[BUFF_SIZE];
    char portNum[BUFF_SIZE];
    struct addrinfo info, *data;
    memset(&info, 0, sizeof(info));
    
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    while (1) {
        input = read(socketfd, &buffer[i], 1);;
        if(buffer[i] == '\n')
            break;
        i++;
    }

    for (i = 1; i < BUFF_SIZE; i++) {
        if (buffer[i] == '\n') {
            break;
        }
        portNum[i-1] = buffer[i];
    }
    portNum[i-1] = '\0';

    int result = getaddrinfo(arg, portNum, &info, &data);
    if (result != 0) {
        printf("Error: %s\n", gai_strerror(result));
        return 1;
    }
    if ((datafd = socket(data -> ai_family, data -> ai_socktype, 0)) == -1) {
        printf("Error: %s\n", strerror(errno));
        return 1;
    }
    if (connect(datafd, data -> ai_addr, data -> ai_addrlen) != 0) {
        printf("Error: %s\n", strerror(errno));
        return 1;
    }

    return datafd;
}

int getResponse(int socketfd) {
    char buffer[BUFF_SIZE];
    int i = 0; int input;
    while (1) {
        input = read(socketfd, &buffer[i], 1);;
	if (input <= 0) {
	    printf("Error: EOF on socket, client exiting\n");
            exit(1);
	}
        if(buffer[i] == '\n') {
	    buffer[i] = '\0';
            break;
	}
        i++;
    }

    if (buffer[0] == 'E') {
	char *msg = buffer + 1;
	printf("Error from server: %s\n", msg);
        return -1;
    }
    if (buffer[0] == 'A') return 1;
    return 0;
}

int mycd(char *pathname) {
    if (chdir(pathname) == -1) {
	printf("chdir error on file %s -- %s\n", pathname, strerror(errno));
	return -errno;
    }

    return 0;
}

int rcd(int socketfd, char *command) {
    char *identifier = "C";
    char *concatenate = malloc((strlen(identifier) + strlen(command) + 2)*sizeof(char));
    sprintf(concatenate, "%s%s\n", identifier, command);
    sendCommand(socketfd, concatenate);
    free(concatenate);
    getResponse(socketfd);

    return 0;
}

int myls() {
    int pipefd[2];
    int read; int write; int status; int status2;

    char *lsArgs[3];
    char *moreArgs[3];
    lsArgs[0] = "ls"; lsArgs[1] = "-l"; lsArgs[2] = NULL;
    moreArgs[0] = "more"; moreArgs[1] = "-20"; moreArgs[2] = NULL;

    if (pipe(pipefd) < 0) {
        printf("1%s\n", strerror(errno));
        return errno;
    }
    read = pipefd[0]; write = pipefd[1];

    int forkVal = fork();
    if (forkVal) { 
        close(write);
        waitpid(-1, &status, 0);
    } else if (!forkVal) {
        close(1);
        dup(write);
        close(read);
        if (execvp("ls", lsArgs) == -1) {
            printf("3%s\n", strerror(errno));
            return errno;
        }
    } else if (forkVal == -1) {
        printf("%s\n", strerror(errno));
        return errno;
    }

    int forkVal2 = fork();
    if (forkVal2) {
        close(read);
        waitpid(-1, &status2, 0);
    } else if (!forkVal2) {
        close(0);
        dup(read);
        close(write);
        if (execvp("more", moreArgs)) {
            printf("%s\n", strerror(errno));
            return errno;
        }
    } else if (forkVal2 == -1) {
        printf("%s\n", strerror(errno));
        return errno;
    }

    return 0;
}

int rls(int socketfd, char const *arg) {
    sendCommand(socketfd, "D\n");
    int fd = getDataConn(socketfd, arg);
    sendCommand(socketfd, "L\n");
    getResponse(socketfd);
    int status;

    char *moreArgs[3];
    moreArgs[0] = "more"; moreArgs[1] = "-20"; moreArgs[2] = NULL;

    int forkVal = fork();
    if (forkVal) {
        waitpid(-1, &status, 0);
    } else if (!forkVal) {
        close(0);
        dup(fd);
        if (execvp("more", moreArgs) == -1) {
            printf("%s\n", strerror(errno));
            return errno;
        }
    } else if (forkVal == -1) {
        printf("%s\n", strerror(errno));
        return errno;
    }

    close(fd);

    return 0;
}

int get(int socketfd, char *command, char const *arg) {

    char buffer[BUFF_SIZE];
    char delim = '/';
    char *fileName = strrchr(command, delim);
    if (fileName == NULL) {
	fileName = command;
    } else {
	fileName = fileName + 1;
    }
    int result = access(fileName, F_OK);
    if (result == 0) {
	printf("File already exists locally\n");
	return 1;
    }

    char *identifier = "G";
    sendCommand(socketfd, "D\n");
    int datafd = getDataConn(socketfd, arg);
    char *concatenate = malloc((strlen(identifier) + strlen(command) + 4)*sizeof(char));
    sprintf(concatenate, "%s%s\n", identifier, command);
    sendCommand(socketfd, concatenate);
    int response = getResponse(socketfd);
    if (response == -1) return 1; 
    free(concatenate);

    int filefd = open(fileName, O_CREAT | O_WRONLY, 0755);

    int numRead;
    while ((numRead = read(datafd, buffer, BUFF_SIZE)) > 0) {
        if (write(filefd, buffer, numRead) != numRead) {
	    printf("Client error: %s\n", strerror(errno));
	    return 1;
	}
    }
    printf("file transferred\n");

    close(datafd); close(filefd);
    return 0;
}

int show(int socketfd, char const *arg, char *command) {
    char *identifier = "G";
    sendCommand(socketfd, "D\n");
    int datafd = getDataConn(socketfd, arg);
    char *concatenate = malloc((strlen(identifier) + strlen(command) + 4)*sizeof(char));
    sprintf(concatenate, "%s%s\n", identifier, command);
    sendCommand(socketfd, concatenate);
    int response = getResponse(socketfd);
    if (response == -1) return 1;
    free(concatenate);

    int status;
    char *moreArgs[3];
    moreArgs[0] = "more"; moreArgs[1] = "-20"; moreArgs[2] = NULL;

    int forkVal = fork();
    if (forkVal) {
        waitpid(-1, &status, 0);
    } else if (!forkVal) {
        close(0);
        dup(datafd);
        if (execvp("more", moreArgs) == -1) {
            printf("%s\n", strerror(errno));
            return errno;
        }
    } else if (forkVal == -1) {
        printf("%s\n", strerror(errno));
        return errno;
    }

    close(datafd);

    return 0;
}

int put(int socketfd, char *command, char const *arg) {

    int filefd = open(command, O_RDONLY);
    if (filefd == -1) {
        printf("File cannot be opened -- %s\n", strerror(errno));
        return 1;
    }
    char delim = '/';
    char *fileName = strrchr(command, delim) + 1;

    char *identifier = "P";
    sendCommand(socketfd, "D\n");
    int datafd = getDataConn(socketfd, arg);
    char *concatenate = malloc((strlen(identifier) + strlen(fileName) + 4)*sizeof(char));
    sprintf(concatenate, "%s%s\n", identifier, fileName);
    sendCommand(socketfd, concatenate);
    free(concatenate);
    int response = getResponse(socketfd);
    if (response == -1) return 1;

    int numRead; char buffer[BUFF_SIZE];
    while ((numRead = read(filefd, buffer, BUFF_SIZE)) > 0) {
        if (write(datafd, buffer, numRead) != numRead) {
	    printf("Client error: %s\n", strerror(errno));
            return 1;
        }
    }

    close(datafd); close(filefd);

    return 0;
}
