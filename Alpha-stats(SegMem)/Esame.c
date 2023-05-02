#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>
#include <time.h>

#define ALFABETO 26
#define S_MUTEX 0 

#define SHM_DIM sizeof(shm)

typedef struct{
    float occorenze[ALFABETO];
}shm;


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


int init_shm(){
    int shm;
    if((shm=shmget(IPC_PRIVATE,SHM_DIM, IPC_CREAT | IPC_EXCL| 0600)) == -1){
        perror("Error on shmget \n");
        exit(1);
    }

    return shm;
}

int init_sem(){
    int sem;
    if((sem=semget(IPC_PRIVATE,1,IPC_CREAT | IPC_EXCL | 0600)) == -1){
        perror("Error on semgt \n");
    }

    if(semctl(sem,S_MUTEX,SETVAL,1) == -1){
        perror("Error on semctl \n");
        exit(1);
    }

    return sem;
}


void child(shm *data, int fdSemaforo, int id, char *argv){
    srand(time(NULL) + id);
    int fd;
    struct stat stInfo;

    if((fd = open(argv,O_RDONLY)) == -1){
        perror("Error on fopen \n");
        exit(1);
    }

    if(fstat(fd, &stInfo) == -1){
        perror("Error on lstat \n");
        exit(1);
    }

	if (!S_ISREG(stInfo.st_mode)) {
		fprintf(stderr, "%s non è un file regolare\n", argv);
		exit(1);
	}
    char *file;
    if((file=(char *)mmap(NULL,stInfo.st_size,PROT_READ,MAP_PRIVATE,fd,0)) == MAP_FAILED){
        perror("Error on mmap \n");
        exit(1);
    }
    
    close(fd);

    for(int i=0;i<stInfo.st_size;i++){
        if(tolower(file[i]) >= 'a' && tolower(file[i])<= 'z'){
            WAIT(fdSemaforo,S_MUTEX);
            data->occorenze[tolower(file[i]) - 'a']++;
            SIGNAL(fdSemaforo,S_MUTEX);
            usleep(((rand() % 10) + 1) / 10);
        }
    }

    munmap(file,stInfo.st_size);
    exit(1);
}


int main(int argc, char  **argv)
{   
    int fdMemoria,fdSemaforo;

    fdMemoria=init_shm();
    fdSemaforo=init_sem();

    shm *data;
    if((data=(shm*) shmat(fdMemoria,NULL,0)) == (shm *) -1){
        perror("Error on shmat");
        exit(1);
    }

    for(int i=0;i<argc-1;i++){
        if(!fork()){
            child(data,fdSemaforo,i+1,argv[i+1]);
        }
    }
for (int i = 0; i < argc - 1; i++) {
		wait(NULL);
	}
    float total=0;

    for(int i=0; i<ALFABETO; i++){
        total+=data->occorenze[i];
    }
    
   for (int i = 0; i < ALFABETO; i++) {
		printf("%c: %.2f%%\n", i + 'a', (data->occorenze[i] / total) * 100);
	}

	shmctl(fdMemoria, IPC_RMID, NULL);
	semctl(fdSemaforo, 0, IPC_RMID, 0);

	exit(0);
}
