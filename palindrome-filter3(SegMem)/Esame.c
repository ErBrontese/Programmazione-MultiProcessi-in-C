#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SHM_DIM sizeof(shm)
#define DIM 1024

typedef enum
{
    P_MUTEX,
    R_MUTEX,
    W_MUTEX,

} S_TYPE;

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
    long done;
    char stringa[DIM];
} shm;

int init_shm()
{
    int shm;
    if ((shm = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on shmget \n");
        exit(1);
    }
    return shm;
}

int init_sem()
{
    int sem;
    if ((sem = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("Error on semget \n");
        exit(1);
    }

    if ((semctl(sem, R_MUTEX, SETVAL, 1)) == -1)
    {
        perror("Error on semctl 1 \n");
        exit(1);
    }

    if (semctl(sem, P_MUTEX, SETVAL, 0) == -1)
    {
        perror("Error on semctl 2 \n");
        exit(1);
    }

    if (semctl(sem, W_MUTEX, SETVAL, 0) == -1)
    {
        perror("Error on semctl 3 \n");
        exit(1);
    }

    return sem;
}

void rChild(shm *data, int fdSemaforo, char *file)
{
    int fd;
    FILE *swap;
    struct stat stInfo;

    if ((fd = open(file, O_RDONLY)) == -1)
    {
        perror("Error on open \n");
        exit(1);
    }

    if ((swap = fopen("temp.txt", "w")) == NULL)
    {
        perror("Error on open \n");
        exit(1);
    }

    if (lstat(file, &stInfo))
    {
        perror("Error on lstat \n");
        exit(1);
    }

    if (!S_ISREG(stInfo.st_mode))
    {
        perror("Errore non è un file regolare \n");
        exit(1);
    }

    char *p;
    if ((p = (char *)mmap(NULL, stInfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
    {
        perror("Error on mmap");
        exit(1);
    }

    for (int i = 0; i < stInfo.st_size; i++)
    {
        fputc(p[i], swap);
    }

    fclose(swap);

    if ((swap = fopen("temp.txt", "r")) == NULL)
    {
        perror("Error on open \n");
        exit(1);
    }
    char buffer[DIM];

    while (fgets(buffer, DIM, swap) != NULL)
    {
        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }
        WAIT(fdSemaforo, R_MUTEX);
        strcpy(data->stringa, buffer);
        data->done = 0;
        SIGNAL(fdSemaforo, P_MUTEX);
    }

    WAIT(fdSemaforo, R_MUTEX);
    data->done = 1;
    SIGNAL(fdSemaforo, P_MUTEX);
}

void wChild(shm *data2, int fdSemaforo, char *file, int argc)
{
    int fd1;
    if (argc == 3)
    {
        if ((fd1 = open(file, O_CREAT | O_WRONLY)) == -1)
        {
            perror("Error on fopen \n");
            exit(1);
        }
    }

    int i = 0;
    char buffer[DIM];
    while (1)
    {
        WAIT(fdSemaforo, W_MUTEX);
        if (data2->done)
        {
            break;
        }
        strcpy(buffer, data2->stringa);
        strcat(buffer, "\n");
        if (argc == 3)
        {
            int check = write(fd1, buffer, strlen(buffer));
            if (check == -1)
            {
                perror("Error on write \n");
            }
        }
        else
        {
            printf("Palindrome : %s", buffer);
        }

        memset(buffer, 0, sizeof buffer);
        SIGNAL(fdSemaforo, R_MUTEX);
    }

    printf("Il mio lavoro è finito \n");
}

int main(int argc, char **argv)
{
    int fdMemoria[2];
    int fdSemaforo;

    for (int i = 0; i < 2; i++)
    {
        fdMemoria[i] = init_shm();
    }

    fdSemaforo = init_sem();

    shm *data;
    if ((data = (shm *)shmat(fdMemoria[0], NULL, 0)) == (shm *)-1)
    {
        perror("Error on data \n");
        exit(1);
    }

    shm *data2;
    if ((data2 = (shm *)shmat(fdMemoria[1], NULL, 0)) == (shm *)-1)
    {
        perror("Error on data \n");
        exit(1);
    }

    if (!fork())
    {
        rChild(data, fdSemaforo, argv[1]);
    }

    if (!fork())
    {
        wChild(data2, fdSemaforo, argv[2], argc);
    }

    while (1)
    {

        WAIT(fdSemaforo, P_MUTEX);
        if (data->done)
        {
            break;
        }

        int palindroma = 1;
        for (int i = 0, j = strlen(data->stringa) - 1; j > i; i++, j--)
        {
            if (data->stringa[i] != data->stringa[j])
            {
                palindroma = 0;
                break;
            }
        }
        char buffer[DIM];

        if (palindroma)
        {
            SIGNAL(fdSemaforo, W_MUTEX);
            strcpy(data2->stringa, data->stringa);
        }
        else
        {
            SIGNAL(fdSemaforo, R_MUTEX);
        }
    }
    SIGNAL(fdSemaforo, W_MUTEX);
    data2->done = 1;
    printf("Ciao \n");

    exit(1);
}
