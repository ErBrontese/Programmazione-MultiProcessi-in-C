#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define DIM_CODA sizeof(msg) - sizeof(long)
#define DIM 2048

typedef struct
{
    long type;
    char righa[DIM];
    int done;
} msg;

int createCoda()
{
    int coda;
    if ((coda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }

    return coda;
}

void fChild(int fdCoda, int id, char *filter)
{
    printf("\n");

    msg messaggio;
    char parola[20];
    char *word1;
    char *word2;
    strcpy(parola, filter + 1);

    printf("%s \n", parola);

    if (filter[0] == '&')
    {
        sscanf(parola, "%s|%s", word1, word2);
    }

    if ((word1 = strtok(parola, "|")) != NULL)
    {
        word2 = strtok(NULL, "|");
    }

    printf("%s \n", word2);

    if(word2[strlen(word2)] == '\n'){
        word2[strlen(word2)]='\0';
    }

    if (parola[strlen(parola) - 1] == '\n')
    {
        parola[strlen(parola) - 1] = '\0';
    }

    while (1)
    {

        if (msgrcv(fdCoda, &messaggio, DIM_CODA, id, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        if (messaggio.done)
        {
            break;
        }
        if (filter[0] == '^')
        {
            char *buffer;

            while (buffer = strstr(messaggio.righa, parola))
            {
                for (int i = 0; i < strlen(parola); i++)
                {
                    buffer[i] = toupper(buffer[i]);
                }
            }
        }
        else if (filter[0] == '_')
        {
            char *buffer;

            while (buffer = strstr(messaggio.righa, parola))
            {
                for (int i = 0; i < strlen(parola); i++)
                {
                    buffer[i] = tolower(buffer[i]);
                }
            }
        }
        else if (filter[0] == '%')
        {
            char *buffer;
            while (buffer = strstr(messaggio.righa, word1))
            {
                for (int i = 0; i < strlen(word2); i++)
                {
                    buffer[i] = word2[i];
                }
            }
        }

        messaggio.done = 0;
        messaggio.type = id + 1;
        if (msgsnd(fdCoda, &messaggio, DIM_CODA, 0) == -1)
        {
            perror("Error on msgsnd \n");
            exit(1);
        }
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    msg messaggio;
    int fdCoda;
    fdCoda = createCoda();

    for (int i = 0; i < argc - 2; i++)
    {
        if (!fork())
        {
            fChild(fdCoda, i + 1, argv[i + 2]);
        }
    }

    FILE *fd;
    if ((fd = fopen(argv[1], "r")) == NULL)
    {
        perror("Error on fopen \n");
        exit(1);
    }

    char buffer[DIM];
    messaggio.done = 0;
    while ((fgets(buffer, DIM, fd)) != NULL)
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }
        messaggio.type = 1;
        strcpy(messaggio.righa, buffer);
        if (msgsnd(fdCoda, &messaggio, DIM_CODA, 0) == -1)
        {
            perror("Error on msgsnd \n");
            exit(1);
        }

        if (msgrcv(fdCoda, &messaggio, DIM_CODA, argc - 1, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        printf("%s \n", messaggio.righa);
    }
    fflush(stdout);
    return 0;
}
