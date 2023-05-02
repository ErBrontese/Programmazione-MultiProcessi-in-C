#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <sys/msg.h>   //Coda
#include <sys/mman.h>  /*==> Ci permette di gestire la mappatura del file */
#include <sys/types.h> //Funzione tolower
#include <sys/ipc.h>
#include <ctype.h>

#define ALFABETO 26

typedef struct
{

    long type;
    char done;
    int occorense[ALFABETO];
} msg;

#define DIM_CODA sizeof(msg) - sizeof(long)

int createCoda()
{
    int fdcoda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    if (fdcoda == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }

    return fdcoda;
}

void createChild(int fdcoda, int id, char *path)
{
    struct stat stInfo;
    char *p;
    DIR *directory;
    int fdFile;
    msg messaggio;

    fdFile = open(path, O_RDONLY);
    if (fdFile == -1)
    {
        perror("Error on open \n");
        exit(1);
    }

    if (lstat(path, &stInfo))
    {
        perror("Error on lstat \n");
        exit(1);
    }

    if (!S_ISREG(stInfo.st_mode))
    {
        perror("Non abbiamo a che fare con un file \n");
        exit(1);
    }

    if ((p = (char *)mmap(NULL, stInfo.st_size, PROT_READ, MAP_PRIVATE, fdFile, 0)) == MAP_FAILED)
    {
        perror("Error on mmap \n");
        exit(1);
    }

    close(fdFile);

  

    for (int i = 0; i < ALFABETO; i++)
    {
        messaggio.occorense[i] = 0;
    }

    for (int i = 0; i < stInfo.st_size; i++)
    {
        if (tolower(p[i]) >= 'a' && tolower(p[i]) <= 'z')
        {
            messaggio.occorense[tolower(p[i]) - 'a']++;
        }
    }

    printf("Sono il processo T:%d è ho analizzato il file %s \n", id, path);

    messaggio.done = 1;
    messaggio.type = 1;
   

    if (msgsnd(fdcoda, &messaggio, DIM_CODA, 0) == -1)
    {
        perror("Error on msgsnd \n");
        exit(1);
    }

    for (int i = 0; i < ALFABETO; i++)
    {
        printf("%c:%d ", i + 'a', messaggio.occorense[i]);
    }
    printf("\n");

    munmap(p, stInfo.st_size);

    exit(0);
}

int main(int argc, char **argv)
{
    msg messaggio;
    pid_t r,w;
    int fdcoda = createCoda();
    for (int i = 0; i < argc - 1; i++)
    {
        if (!fork())
        {
            createChild(fdcoda, i + 1, argv[i + 1]);
        }
    }

    int globlaoccorense[ALFABETO];
    for (int i = 0; i < ALFABETO; i++)
    {
        globlaoccorense[i] = 0;
    }
    char letti = 0;

    while (1)
    {
        if (msgrcv(fdcoda, &messaggio, DIM_CODA, 1, 0) == -1)
        {
            perror("Error on msgrcv \n");
            exit(1);
        }
        for (int i =0; i < ALFABETO; i++)
        {
            globlaoccorense[i] += messaggio.occorense[i];
        }

        if (messaggio.done)
        {
            letti++;
            if (letti == argc - 1)
            {
                break;
            }
        }
    }

    for (int i = 0; i < argc - 1; i++)
    {
        wait(NULL);
    }

    int max = 0;
    printf("Processo padre \n");

    for (int i = 0; i < ALFABETO; i++)
    {
        if (globlaoccorense[i] > globlaoccorense[max])
        {
            max = i;
        }

        printf(" %c:%d", i + 'a',globlaoccorense[i]);
    }
    printf("\n");

    printf("Il valore massimo è %c \n", max + 'a');

    msgctl(fdcoda, IPC_RMID, NULL);

    exit(0);


}
