#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define _GNU_SOURCE

#include <ctype.h>

#define DIM_SHM sizeof(shm)
#define DIM 1024

#define P_MUTEX 0

int WAIT(int sem_des, int num_semaforo)
{
    struct sembuf ops[1] = {{num_semaforo, -1, 0}};
    return semop(sem_des, ops, 1);
}

int SIGNAL(int sem_des, int num_semaforo)
{
    struct sembuf ops[1] = {{num_semaforo, +1, 0}};
    return semop(sem_des, ops, 1);
}

typedef struct
{
    int done;
    char righa[DIM];
} shm;

int init_shm()
{
    int shm;
    if ((shm = shmget(IPC_PRIVATE, DIM_SHM, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on shmget \n");
        exit(1);
    }

    return shm;
}

int init_sem(int argc)
{
    int sem;
    if ((sem = semget(IPC_PRIVATE, argc, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on semget \n");
        exit(1);
    }

    if ((semctl(sem, P_MUTEX, SETVAL, 1)) == -1)
    {
        perror("Error on semctl 1 \n");
        exit(1);
    }
    for (int i = 0; i < argc - 1; i++)
    {
        if ((semctl(sem, i + 1, SETVAL, 0)) == -1)
        {
            perror("Error on semctl2 \n");
            exit(1);
        }
    }

    return sem;
}

void cChild(shm *data, int fdSemaforo, char *filter, int id, int argc)
{
    char parola[DIM];

    char *word1;
    char *word2;
    strcpy(parola, filter + 1);
    if (filter[0] == '%')
    {
        
        if ((word1 = strtok(parola, "|")) != NULL)
        {
            word2 = strtok(NULL, "|");
        }
      
    }

   

   
    while (1)
    {
        WAIT(fdSemaforo, id);
        if (data->done)
        {
            break;
        }

        if (filter[0] == '^')
        {
            char *p;

            while ((p = strstr(data->righa, parola)) != NULL)
            {
                {
                    for (int i = 0; i < strlen(parola); i++)
                    {
                        p[i] = toupper(parola[i]);
                    }
                }
            }
        }
        else if (filter[0] == '_')
        {
            char *p;

            while ((p = strstr(data->righa, parola)) != NULL)
            {
                {
                    for (int i = 0; i < strlen(parola); i++)
                    {
                        p[i] = tolower(parola[i]);
                    }
                }
            }
        } else if (filter[0] == '%')
        {
            char *p;

            while ((p = strstr(data->righa, word1)) != NULL)
            {
                {
                    for (int i = 0; i < strlen(word2); i++)
                    {
                        p[i] = word2[i];
                    }
                }
            }
        }
        //    printf("Ricevuto : %s \n", data->righa);

        if (id != argc)
        {
            SIGNAL(fdSemaforo, id + 1);
            continue;
        }
        else
        {
            printf("Ricevuto : %s \n", data->righa);
            SIGNAL(fdSemaforo, P_MUTEX);
        }
    }
    SIGNAL(fdSemaforo, id + 1);
    exit(0);
}

int main(int argc, char **argv)
{
    int fdMemoria, fdSemaforo;

    fdMemoria = init_shm();
    fdSemaforo = init_sem(argc - 1);

    shm *data;
    if ((data = (shm *)shmat(fdMemoria, NULL, 0)) == (shm *)-1)
    {
        perror("Error on shmat \n");
        exit(1);
    }

    for (int i = 0; i < argc - 2; i++)
    {
        if (!fork())
        {
            printf("ciao\n");
            cChild(data, fdSemaforo, argv[i + 2], i + 1, argc - 2);
        }
    }

    FILE *fd;
    if ((fd = fopen(argv[1], "r")) == NULL)
    {
        perror("Error on fopen \n");
        exit(1);
    }

    char buffer[DIM];
    data->done = 0;

    while (fgets(buffer, DIM, fd) != NULL)
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }

        WAIT(fdSemaforo, P_MUTEX);
        strcpy(data->righa, buffer);
        SIGNAL(fdSemaforo, 1);
    }
    WAIT(fdSemaforo, P_MUTEX);
    data->done = 1;
    SIGNAL(fdSemaforo, 1);

    for (int i = 0; i < argc - 2; i++)
    {
        wait(NULL);
    }
    fclose(fd);
    shmctl(fdMemoria, IPC_RMID, NULL);

    semctl(fdMemoria, 0, IPC_RMID, 0);
    exit(3);
}
