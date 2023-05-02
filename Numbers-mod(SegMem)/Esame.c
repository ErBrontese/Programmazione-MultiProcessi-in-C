#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

typedef enum
{
    S_MUTEX,
    S_EMPTY,
    S_FULL_NUM,
    S_FULL_MOD,

} S_TYPE;

typedef enum
{
    T_NUM,
    T_MOD,
    T_EMPTY,
    T_DONE,

} T_TYPE;

typedef struct
{
    long number;
    char type;
} shm;

#define NUM_DIM 1024
#define BUFF_DIM 10
#define SHM_DIM sizeof(shm) * BUFF_DIM

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
    int shm_des;

    if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("shmget");
        exit(1);
    }

    return shm_des;
}

int init_sem()
{
    int sem_des;

    if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
        perror("semget");
        exit(1);
    }

    if (semctl(sem_des, S_MUTEX, SETVAL, 1) == -1)
    {
        perror("semctl S_MUTEX");
        exit(1);
    }

    if (semctl(sem_des, S_EMPTY, SETVAL, BUFF_DIM) == -1)
    {
        perror("semctl S_EMPTY");
        exit(1);
    }

    if (semctl(sem_des, S_FULL_NUM, SETVAL, 0) == -1)
    {
        perror("semctl S_FULL_NUM");
        exit(1);
    }

    if (semctl(sem_des, S_FULL_MOD, SETVAL, 0) == -1)
    {
        perror("semctl S_FULL_MOD");
        exit(1);
    }

    return sem_des;
}

void mChild(shm *data, int fd_semaforo, long mod){
    int done=0;
    while(1){
        WAIT(fd_semaforo,S_FULL_NUM);
        WAIT(fd_semaforo,S_MUTEX);

        for(int i=0; i< BUFF_DIM;i++){
            if(data[i].type== T_DONE){
                done=1;
            }

            if(data[i].type == T_NUM){
                data[i].number%=mod;
                data[i].type=T_MOD;
                break;
            }
        }

        SIGNAL(fd_semaforo,S_MUTEX);
        SIGNAL(fd_semaforo,S_FULL_MOD);

        if(done){
            break;
        }
    }
    exit(0);
}


void oChild(shm *data , int fd_semaforo){
    int done=0;
    while(1){
        WAIT(fd_semaforo,S_FULL_MOD);
        WAIT(fd_semaforo,S_MUTEX);

        for(int i =0; i < BUFF_DIM;i++){
            if(data[i].type == T_DONE){
                done=1;
            }

            if(data[i].type == T_MOD){
                printf("%ld \n",data[i].number);
                data[i].type=T_EMPTY;
                break;
            }

        }

        SIGNAL(fd_semaforo,S_MUTEX);
        SIGNAL(fd_semaforo,S_EMPTY);

        if(done){
            break;
        }
    }

    exit(0);
}

int main(int argc, char const *argv[])
{
    int fd_memoria, fd_semaforo;
    FILE *fd;

    fd_memoria = init_shm();
    fd_semaforo = init_sem();

    shm *data;

    if ((data = (shm *)shmat(fd_memoria, NULL, 0)) == (shm *)-1)
    {
        perror("Error on shm \n");
        exit(1);
    }

    char buffer[NUM_DIM];

    for (int i = 0; i < BUFF_DIM; i++)
    {
        data[i].type = T_EMPTY;
    }

    if ((fd = fopen(argv[1], "r")) == NULL)
    {
        perror("Error on fopen \n");
        exit(1);
    }


    if(!fork()){
        mChild(data,fd_semaforo,atol(argv[2]));
    }

    if(!fork()){
        oChild(data,fd_semaforo);
    }

    while (fgets(buffer, NUM_DIM, fd) != NULL)
    {

        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }
        WAIT(fd_semaforo, S_EMPTY);
        WAIT(fd_semaforo, S_MUTEX);

        for(int i=0;i<BUFF_DIM;i++){
            if(data[i].type == T_EMPTY){
                data[i].number=atol(buffer);
                data[i].type=T_NUM;
                break;
            }
        }

        SIGNAL(fd_semaforo,S_MUTEX);
        SIGNAL(fd_semaforo,S_FULL_NUM);

    }

    WAIT(fd_semaforo,S_EMPTY);
    WAIT(fd_semaforo,S_MUTEX);

    for(int i=0;i<BUFF_DIM;i++){
        if(data[i].type == T_EMPTY){
            data[i].type=T_DONE;
        }
    }

    SIGNAL(fd_semaforo,S_MUTEX);
    SIGNAL(fd_semaforo,S_FULL_NUM);

    shmctl(fd_memoria,IPC_RMID,0);
    semctl(fd_semaforo,0,IPC_RMID,0);
    fclose(fd);

}
