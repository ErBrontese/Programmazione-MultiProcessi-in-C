#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DIM_SHM sizeof(shm)
#define DIM 1024

typedef enum
{
    P1_MUTEX,
    P2_MUTEX,
    G_MUTEX,
    T_MUTEX
} S_TYPE;

typedef struct
{
    long done;
    char mossaP1;
    char mossaP2;
    int vittoria;

} shm;

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

int init_shm()
{
    int fdshm;
    if ((fdshm = shmget(IPC_PRIVATE, DIM_SHM, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on shmget \n");
        exit(1);
    }

    return fdshm;
}

int sem_init()
{
    int fdSemaforo;
    if ((fdSemaforo = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on semget \n");
        exit(1);
    }

    if ((semctl(fdSemaforo, P1_MUTEX, SETVAL, 1)) == -1)
    {
        perror("Error on semctl \n");
        exit(1);
    }

    if ((semctl(fdSemaforo, P2_MUTEX, SETVAL, 1)) == -1)
    {
        perror("Error on semctl \n");
        exit(1);
    }

    if ((semctl(fdSemaforo, G_MUTEX, SETVAL, 0)) == -1)
    {
        perror("Error on semctl \n");
    }

    if ((semctl(fdSemaforo, T_MUTEX, SETVAL, 0)) == -1)
    {
        perror("Error on semctl \n");
    }

    return fdSemaforo;
}

void pChild(shm *data, int fdSemaforo, int id)
{
    srand(time(NULL)+id);

    char *mosse[3] = {"carta", "forbice", "sasso"};

    while (1)
    {
        if (id == P1_MUTEX)
        {
            WAIT(fdSemaforo, P1_MUTEX);
            if (data->done)
            {
                break;
            }

            data->mossaP1 = rand() % 3;
            printf("Il P1 mossa %s\n", mosse[data->mossaP1]);
        }
        else
        {

            WAIT(fdSemaforo, P2_MUTEX);
            if (data->done)
            {
                break;
            }
            data->mossaP2 = rand() % 3;
            printf("Il P2 mossa %s \n", mosse[data->mossaP2]);
        }

        SIGNAL(fdSemaforo, G_MUTEX);
    }
    SIGNAL(fdSemaforo, G_MUTEX);

    exit(0);
}

void gFather(shm *data, int fdSemaforo)
{
     int match = 1;
    while (1)
    {
        WAIT(fdSemaforo, G_MUTEX);
        WAIT(fdSemaforo, G_MUTEX);
        if (data->done)
        {
            break;
        }
        if (data->mossaP1 == data->mossaP2)
        {
            printf("Abbiamo avuto un pareggio \n");
            SIGNAL(fdSemaforo, P1_MUTEX);
            SIGNAL(fdSemaforo, P2_MUTEX);
            continue;
        }
        else if ((data->mossaP1 == 1 && data->mossaP2 == 0) || (data->mossaP1 == 0 && data->mossaP2 == 2) || (data->mossaP1 == 2 && data->mossaP2 == 1))
        {
            data->vittoria = 1;
            printf("Ha vinto il processo P1 partita %d\n", match);
        }
        else if ((data->mossaP2 == 1 && data->mossaP1 == 0) || (data->mossaP2 == 0 && data->mossaP1 == 2) || (data->mossaP2 == 2 && data->mossaP1 == 1))
        {
            data->vittoria = 2;
            printf("Ha vinto il processo P2 partita %d \n", match);
        }

        match++;
        SIGNAL(fdSemaforo, T_MUTEX);
    }
    exit(0);
}

void tChild(shm *data, int fdSemaforo, int matches)
{

    int macth = 1;
    int vittorieP1 = 0;
    int vittorieP2 = 0;

    data->done = 0;
    while (macth <= matches)
    {
        WAIT(fdSemaforo, T_MUTEX);
        if (data->done)
        {
            break;
        }
        if (data->vittoria == 1)
        {
            vittorieP1++;
        }
        else
        {
            vittorieP2++;
        }

        if (macth == matches)
        {
            printf("Classifica finale P1:%d P2:%d \n", vittorieP1, vittorieP2);
            data->done = 1;
        }
        else
        {
            printf("Classifica temporanea P1:%d P2:%d \n", vittorieP1, vittorieP2);
        }

        macth++;
        SIGNAL(fdSemaforo, P1_MUTEX);
        SIGNAL(fdSemaforo, P2_MUTEX);
    }

    if (vittorieP1 > vittorieP2)
    {
        printf("Ha vinto il processo P1 \n");
    }

    if (vittorieP1 < vittorieP2)
    {
        printf("Ha vinto il processo P2 \n");
    }
    else
    {

        printf("E finita con un pareggio \n");
    }

    exit(0);
}

int main(int argc, char **argv)
{

    int fdMemoria, fdSemaforo;
    fdMemoria = init_shm();
    fdSemaforo = sem_init();
    int matches = atoi(argv[1]);

    shm *data;
    if ((data = (shm *)shmat(fdMemoria, NULL, 0)) == (shm *)-1)
    {
        perror("Error on shmat \n");
        exit(1);
    }

    for (int i = 0; i < 2; i++)
    {
        if (!fork())
        {
            pChild(data, fdSemaforo, i);
        }
    }

    if (!fork())
    {
        gFather(data, fdSemaforo);
    }

    if (!fork())
    {
        tChild(data, fdSemaforo, matches);
    }

    for (int i = 0; i < 4; i++)
    {
        wait(NULL);
    }

    shmctl(fdMemoria, IPC_RMID, NULL);
    semctl(fdSemaforo, 0, IPC_RMID, 0);

    exit(0);
}
