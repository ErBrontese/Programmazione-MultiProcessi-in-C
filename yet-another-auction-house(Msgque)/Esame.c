#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <sys/wait.h>

#define DIM_MSG sizeof(msg) - sizeof(long)
#define DIM 1024

typedef struct
{
    long type;
    char object_description[DIM];
    int minimaOfferta;
    int massimaOffeta;
    int Offerta;
    int numAsta;
    int id;
    char done;
} msg;

int init_msg()
{
    int msg;
    if ((msg = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }

    return msg;
}

void jChild(int fdmsg, char *file, int numeroPartecipanti)
{
    msg messaggio;
    FILE *fd;
    if ((fd = fopen(file, "r")) == NULL)
    {
        perror("Error on fopen \n");
        exit(1);
    }

    char *person;
    char *min;
    char *max;
    int numAsta = 1;

    char buffer[DIM];
    while (fgets(buffer, DIM, fd) != NULL)
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }
        if ((person = strtok(buffer, ",")) != NULL)
        {
            if ((min = strtok(NULL, ",")) != NULL)
            {
                max = strtok(NULL, "");
            }
        }

        int offerte[numeroPartecipanti - 1];

        for (int i = 1; i < numeroPartecipanti; i++)
        {
            messaggio.done = 0;
            messaggio.numAsta = numAsta;
            messaggio.type = i;
            strcpy(messaggio.object_description, person);
            messaggio.minimaOfferta = atoi(min);
            messaggio.massimaOffeta = atoi(max);
            if ((msgsnd(fdmsg, &messaggio, DIM_MSG, 0)) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
        }
        int vincitore = 0;
        int max = 0;
        for (int i = 1; i < numeroPartecipanti; i++)
        {
            if ((msgrcv(fdmsg, &messaggio, DIM_MSG, 1, 0)) == -1)
            {
                perror("Error on msgrcv \n");
                exit(1);
            }

            printf("J: Ricevuta offerta B%d\n", messaggio.id);

            offerte[messaggio.id] = messaggio.Offerta;
            if (offerte[messaggio.id] > max)
            {
                max = offerte[messaggio.id];
                vincitore = messaggio.id;
            }
        }
        printf("L'asta n.%d per %s si è conclusa;Il vincitore è B%d che si aggiudica l'oggetto per %d eur \n", messaggio.numAsta, messaggio.object_description, vincitore, max);

        numAsta++;
    }
    for (int i = 1; i < numeroPartecipanti; i++)
    {
        messaggio.done = 1;
        messaggio.type = i;
        if ((msgsnd(fdmsg, &messaggio, DIM_MSG, 0)) == -1)
        {
            perror("Error on msgsnd \n");
            exit(1);
        }
    }
    fclose(fd);
    exit(1);
}

void bChild(int fdmsg, int id, int numeroPartecipanti)
{
    srand(time(NULL) + id);
    msg messaggio;
   

    while (1)
    {
        
        if ((msgrcv(fdmsg, &messaggio, DIM_MSG, id, 0)) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }

        if (messaggio.done)
        {
            break;
        }
        
            int tpAttesa = rand() % 3 + 1;
            int offerta = 0;
            printf("B%d aspetto per %d secondi\n", id, tpAttesa);
            sleep(tpAttesa);

            offerta = rand() % messaggio.massimaOffeta + 1;
            printf("B%d: Invio offerta di %d per asta n.%d per %s \n", id, offerta, messaggio.numAsta, messaggio.object_description);
            messaggio.Offerta = offerta;
            messaggio.id = id;
            messaggio.type = 1;
            if ((msgsnd(fdmsg, &messaggio, DIM_MSG, 0)) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
       
    }
    exit(1);
}

int main(int argc, char **argv)
{
    int fdmsg;
    fdmsg = init_msg();

    int numeroPartecipanti = 0;
    numeroPartecipanti = atoi(argv[2]);

    for (int i = 0; i < numeroPartecipanti; i++)
    {
        if (!fork())
        {
            bChild(fdmsg, i + 1, numeroPartecipanti);
        }
    }

    if (!fork())
    {
        jChild(fdmsg, argv[1], numeroPartecipanti + 1);
    }

    for (int i = 0; i < numeroPartecipanti; i++)
    {
        wait(NULL);
    }
    msgctl(fdmsg, IPC_RMID, NULL);
    exit(1);
}
