#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define BUFSIZE 256
#define W 1
typedef enum{
    DATA,
    STOP
} payload_type;

typedef struct{
    long msgtype;
    payload_type pltype;
    char payload[BUFSIZE];
} msg;

int main(int argc, char* argv[]){
    int working = 1;
    int pipefd[2];
    int msqid, size;
    char readbuffer[BUFSIZE];
    FILE* input;
    msg message;
    if (argc < 3){
        fprintf(stderr, "usage: another-grep <parola> <file>\n");
        exit(1);
    }

    if ((msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0660)) == -1){
        perror("error getting message queue descriptor");
        exit(1);
    }
    if (pipe(pipefd) == -1){
        perror("error creating pipe");
        exit(1);
    }
    
    if ((input = fopen(argv[2], "r")) == NULL){
        perror("error opening input file");
        exit(1);
    }

    // processo READER
    if (fork() == 0){
        close(pipefd[0]);   // chiudo la pipe in lettura
        while(fgets(readbuffer, BUFSIZE, input) != NULL){
            readbuffer[strcspn(readbuffer, "\n")] = 0;
            //printf("Ho letto la riga: %s\n", readbuffer);
            if (write(pipefd[1], readbuffer, strlen(readbuffer)+1) == -1){
                perror("error writing on pipe");
                exit(1);
            }
            //printf("Riga scritta sulla pipe\n");
            usleep(500);
        }
        if (write(pipefd[1], "***STOP***", 11) == -1){
            perror("error writing on pipe");
            exit(1);
        }
        printf("Quitting reader...\n");
        exit(0);
    }
    // processo WRITER
    if (fork() == 0){
        int working = 1;
        while(working){
            if (msgrcv(msqid, &message, sizeof(message), W, 0) == -1){
                perror("error receiving message");
                exit(1);
            }
            if (message.pltype == STOP){
                printf("Fermo tutto!\n");
                working = 0;
            }
            else
                printf("%s\n", message.payload);
        }
        printf("Quitting writer...\n");
        exit(0);
    }

    // processo PARENT
    close(pipefd[1]); // chiudo la pipe in scrittura
    message.msgtype = W;
    while(working){
        do {
            if ((size = read(pipefd[0], readbuffer, BUFSIZE)) == -1){
                perror("error reading from pipe");
                exit(1);
            }
            //printf("Ho letto dalla pipe la riga: %s\n", readbuffer);
            char line[BUFSIZE];
            if (strcmp("***STOP***", readbuffer) == 0){
                message.pltype = STOP;
                working = 0;
                fclose(input);
                if (msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0) == -1){
                    perror("error sending message");
                    exit(1);
                }
                break;
            }
            strncpy(line, readbuffer, BUFSIZE);
            //printf("Cerco la parola: %s\n", argv[1]);
            char* tok = strtok(readbuffer, " ,.:;!?");
            do{
                if (strcmp(tok, argv[1]) == 0){
                    //printf("Ho trovato '%s' nella riga: %s\n", argv[1], line);
                    strncpy(message.payload, line, BUFSIZE);
                    message.pltype = DATA;
                    if (msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0) == -1){
                        perror("error sending message");
                        exit(1);
                    }
                    break;
                }
            }
            while((tok = strtok(NULL, " ,.:;!?")) != NULL);
        }
        while(size == BUFSIZE);
    }

    close(pipefd[0]);
    printf("Quitting parent...\n");
    exit(0);


}