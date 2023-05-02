
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define DIM 2000

#define DIM_MSG sizeof(msg) - sizeof(long)

typedef struct
{
    long type;
    char righe[DIM];
    char done;
} msg;

int createMSG()
{
    int mycoda;
    if (mycoda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600) == -1)
    {
        perror("Errorn on msgget \n");
        exit(1);
    }

    return mycoda;
}

void rChild(int fdpipe, char *file)
{
    FILE *fd;
    FILE *pipefd;

    if ((fd = fopen(file, "r")) == NULL)
    {
        perror("Error on fopen 1\n");
        exit(1);
    }

    if ((pipefd = fdopen(fdpipe, "w")) == NULL)
    {
        perror("Error on open pipe \n");
        exit(1);
    }

    char buffer[DIM];
    while (fgets(buffer, DIM, fd) != NULL)
    {
        if (buffer[strlen(buffer) - 1] != '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }

        dprintf(fdpipe, "%s", buffer);
    }
    dprintf(fdpipe, "-1\n");
}

void wChild(int mycoda)
{
    msg messaggio;

    while (1)
    {

        if (msgrcv(mycoda, &messaggio, DIM_MSG, 1, 0) == -1)
        {
            perror("Erro on msgrcv \n");
            exit(1);
        }

        if (messaggio.done)
        {
            break;
        }

        printf("%s \n", messaggio.righe);
    }
    printf("Abbiamo finito \n");
}

int main(int argc, char **argv)
{

    int mycoda = createMSG();
    msg messaggio;
    int fdpipe[2];
    if ((pipe(fdpipe)) == -1)
    {
        perror("Error on pipe \n");
        exit(1);
    }

    if (!fork())
    {
        rChild(fdpipe[1], argv[2]);
    }

    if (!fork())
    {
        wChild(mycoda);
    }

    FILE *pipeFile;
    if ((pipeFile = fdopen(fdpipe[0], "r")) == NULL)
    {
        perror("Error on fdopen \n");
        exit(1);
    }
    char parola[DIM];
    memset(parola, 0, sizeof parola);
    strcpy(parola, argv[1]);

    char buffer[DIM];
    messaggio.done = 0;
    messaggio.type = 1;
    while (fgets(buffer, DIM, pipeFile) != NULL)
    {
        if (strcmp(buffer, "-1\n") == 0)
        {
            break;
        }

        if (strstr(buffer, parola))
        {
            if (buffer[strlen(buffer) - 1] != '\n')
            {
                buffer[strlen(buffer) - 1] = '\0';
            }

            strcpy(messaggio.righe, buffer);
            if (msgsnd(mycoda, &messaggio, DIM_MSG, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
        }
    }
    messaggio.done = 1;
    if (msgsnd(mycoda, &messaggio, DIM_MSG, 0) == -1)
    {
        perror("Error on msgsnd \n");
        exit(1);
    }

    exit(1);
}
