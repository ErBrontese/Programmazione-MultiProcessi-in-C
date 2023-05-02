#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>

#define MSG_DIM sizeof(msg) - sizeof(long)
#define DIM 2024

typedef enum
{
    T_R,
    T_F,
    T_P
} M_TYPE;

typedef struct
{
    long type;
    char contentenuto[DIM];
    char file_name[DIM];
    char parola[DIM];
    int done;
} msg;

int init_coda()
{
    int createcoda;
    if ((createcoda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }
    return createcoda;
}

void rChild(int fdCoda, char *file)
{
    msg messaggio;
    FILE *fd;
    if ((fd = fopen(file, "r")) == NULL)
    {
        perror("Error on open \n");
        exit(1);
    }

    char buffer[DIM];
    size_t dim = sizeof(buffer);

    while (fgets(buffer, DIM, fd) != NULL)
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }

        stpcpy(messaggio.contentenuto, buffer);
        stpcpy(messaggio.file_name, file);
        messaggio.done = 0;
        messaggio.type = T_F;

        if (msgsnd(fdCoda, &messaggio, MSG_DIM, 0) == -1)
        {
            perror("Error on msgsnd \n");
            exit(1);
        }
    }

    messaggio.done = 1;
    messaggio.type = T_F;

    if (msgsnd(fdCoda, &messaggio, MSG_DIM, 0) == -1)
    {
        perror("Error on msgsnd \n");
        exit(1);
    }

    exit(1);
}

void fChild(int fdCoda, int fdpipeFF, char *parola, int file_n, int caseSensitive, int notContent)
{
    char buffer[DIM];
    msg messaggio;
    int fileCounter = 0;
    char bufferSupport[DIM];

    while (1)
    {
        if (msgrcv(fdCoda, &messaggio, MSG_DIM, T_F, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        if (messaggio.done)
        {
            fileCounter++;
            if (fileCounter == file_n)
            {
                break;
            }
            continue;
        }

        if (notContent && caseSensitive)
        {
            if (!strcasestr(messaggio.contentenuto, parola))
            {
                dprintf(fdpipeFF, "%s:%s\n", messaggio.file_name, messaggio.contentenuto);
            }
        }
        else if (notContent && !caseSensitive)
        {
            if (!strstr(messaggio.contentenuto, parola))
            {
                dprintf(fdpipeFF, "%s:%s\n", messaggio.file_name, messaggio.contentenuto);
            }
        }
        else if (!notContent && caseSensitive)
        {
            if (strcasestr(messaggio.contentenuto, parola))
            {
                dprintf(fdpipeFF, "%s:%s\n", messaggio.file_name, messaggio.contentenuto);
            }
        }
    }
    dprintf(fdpipeFF, "-1\n");
    close(fdpipeFF);
    exit(1);
}

int main(int argc, char **argv)
{
    int fdCoda = init_coda();
    int file = 2;
    int file_n = argc - 2;
    int parola = 1;
    int caseSensitive = 0;
    int notContent = 0;
    int fdpipeFF[2];

    if (pipe(fdpipeFF) == -1)
    {
        perror("Error on pipe \n");
        exit(1);
    }

    if (!strcmp(argv[1], "-i") || !strcmp(argv[1], "-v"))
    {
        file++;
        file_n--;
        parola++;
        if (!strcmp(argv[1], "-i"))
        {
            caseSensitive = 1;
        }
        else
        {
            notContent = 1;
        }
    }

    if (!strcmp(argv[2], "-i") || !strcmp(argv[2], "-v"))
    {
        file++;
        file_n--;
        parola++;
        if (!strcmp(argv[2], "-i"))
        {
            caseSensitive = 1;
        }
        else
        {
            notContent = 1;
        }
    }

    for (int i = 0; i < file_n; i++)
    {
        if (!fork())
        {
            rChild(fdCoda, argv[i + file]);
        }
    }

    if (!fork())
    {
        fChild(fdCoda, fdpipeFF[1], argv[parola], file_n, caseSensitive, notContent);
    }

    FILE *fdpipe;

    if ((fdpipe = fdopen(fdpipeFF[0], "r")) == NULL)
    {
        perror("Error on fdopen \n");
        exit(1);
    }
    char ContenutoFiglio[DIM];

    while (1)
    {
        fgets(ContenutoFiglio, DIM, fdpipe);

        if (strcmp(ContenutoFiglio, "-1\n") == 0)
        {
            break;
        }

        printf("%s", ContenutoFiglio);
    }

    msgctl(fdCoda, IPC_RMID, NULL);
    fclose(fdpipe);
    close(fdpipeFF[0]);

    exit(1);
}
