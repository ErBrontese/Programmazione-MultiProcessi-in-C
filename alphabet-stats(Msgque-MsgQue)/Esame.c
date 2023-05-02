#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <ctype.h>

#define DIM_MSG sizeof(msg) - sizeof(long)

#define DIM_MSG2 sizeof(msg2) - sizeof(long)

#define DIM 2048
#define ALFABETO 26

typedef struct
{
    long type;
    char righa[DIM];
    char risposta[DIM];
    char done;
} msg;

typedef struct
{
    long type;
    int occorenze[ALFABETO];
    char done;
} msg2;

typedef struct
{
    int occorenze[ALFABETO];
} global_occorenze;
int createCoda()
{
    int fdmsg;
    if ((fdmsg = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }

    return fdmsg;
}

void rChild(int fdmsg, int id, char *file)
{
    FILE *fd;
    msg messaggio;
    if ((fd = fopen(file, "r")) == NULL)
    {
        perror("Error on fopen \n");
        exit(1);
    }

    char buffer[DIM];
    while (fgets(buffer, DIM, fd))
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }

        messaggio.type = id;
        strcpy(messaggio.righa, buffer);
        messaggio.done = 0;

        if (msgsnd(fdmsg, &messaggio, DIM_MSG, 0) == -1)
        {
            perror("Error on msgsnd \n");
            exit(1);
        }
    }
    messaggio.done = 1;
    messaggio.type = id;
    if (msgsnd(fdmsg, &messaggio, DIM_MSG, 0) == -1)
    {
        perror("Error on msgsnd \n");
        exit(1);
    }
    fclose(fd);
    exit(1);
}

void cChild(int fdmsg, int fdmsgCP, int argc)
{
    msg messaggio;
    msg2 msg2;

    msg2.done = 0;
    msg2.type = 1;
    int doneCounter = 0;

    while (1)
    {
        for (int i = 0; i < ALFABETO; i++)
        {
            msg2.occorenze[i] = 0;
        }

        if (msgrcv(fdmsg, &messaggio, DIM_MSG, 0, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        if (messaggio.done)
        {
            doneCounter++;
            if (doneCounter == argc)
            {
                break;
            }

            continue;
        }

        //  printf("Ricevuto: %s \n",messaggio.righa);
        msg2.type = messaggio.type;
        for (int i = 0; i < strlen(messaggio.righa); i++)
        {
            if (tolower(messaggio.righa[i]) >= 'a' && tolower(messaggio.righa[i]) <= 'z')
            {
                msg2.occorenze[tolower(messaggio.righa[i]) - 'a']++;
            }
        }

        if (msgsnd(fdmsgCP, &msg2, DIM_MSG2, 0) == -1)
        {
            perror("Error on msgsnd msg2 \n");
            exit(1);
        }
    }
    printf("CIao\n");
    msg2.done = 1;
    if (msgsnd(fdmsgCP, &msg2, DIM_MSG2, 0) == -1)
    {
        perror("Error on msgsnd msg2 \n");
        exit(1);
    }

    exit(1);
}

int main(int argc, char **argv)
{
    int fdmsg = createCoda();
    int fdmsgCP = createCoda();

    msg2 msg2;

    global_occorenze occorenzaAlfabeto[argc - 1];

    for (int i = 0; i < argc - 1; i++)
    {
        if (!fork())
        {
            rChild(fdmsg, i + 1, argv[i + 1]);
        }
    }

    if (!fork())
    {
        cChild(fdmsg, fdmsgCP, argc - 1);
    }

    for (int j = 0; j < argc - 1; j++)
    {
        for (int i = 0; i < ALFABETO; i++)
        {
            occorenzaAlfabeto[j].occorenze[i] = 0;
        }
    }

    while (1)
    {

        if (msgrcv(fdmsgCP, &msg2, DIM_MSG2, 0, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        if (msg2.done)
        {
            break;
        }

        for (int i = 0; i < ALFABETO; i++)
        {
            occorenzaAlfabeto[msg2.type - 1].occorenze[i] += msg2.occorenze[i];
        }
    }

    for (int j = 0; j < argc - 1; j++)
    {
        printf("////// \n");
        for (int i = 0; i < ALFABETO; i++)
        {
            printf("%c = %d \n", i + 'a', occorenzaAlfabeto[j].occorenze[i]);
        }
    }

    for (int i = 0; i < argc; i++)
    {
        wait(NULL);
    }

    return 0;
}
