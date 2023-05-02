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

#define DIM 1024

void rChild(char *file, int fdpipeFP)
{
    int fd;
    char *p;
    FILE *out;
    struct stat stinfo;

    if ((fd = open(file, O_RDONLY)) == -1)
    {
        perror("Error on fopen file");
    }

    if ((out = fdopen(fdpipeFP, "w")) == NULL)
    {
        perror("Error on fdopen 1 \n");
        exit(1);
    }

    if (lstat(file, &stinfo))
    {
        perror("Error on lstat \n");
        exit(1);
    }

    if ((p = (char *)mmap(NULL, stinfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
    {
        perror("error on map \n");
        exit(1);
    }

    close(fd);
    char buffer[DIM];
    memset(buffer, 0, sizeof buffer);

    for (int i = 0; i < stinfo.st_size / stinfo.st_size; i++)
    {
        fprintf(out, "%s\n", &p[i]);
    }
    munmap(p, stinfo.st_size);
    exit(1);
}

void wChild(char *file, int fdpipePF)
{

    
    char buffer[DIM];
    memset(buffer, 0, sizeof buffer);
    while (1)
    {
        int checkRead = read(fdpipePF, buffer, sizeof buffer);
        if (checkRead == -1)
        {
            perror("Error on read \n");
            exit(1);
        }

        if (strcmp(buffer, "end") == 0)
        {
            printf("ok\n");
            break;
        }

        printf("Palindroma %s",buffer);
    }
    exit(0);
}

int main(int argc, char **argv)
{
    int fdpipeFP[2];
    int fdpipePF[2];

    if (pipe(fdpipeFP) == -1)
    {
        perror("Error on pipe \n");
        exit(1);
    }

    if (pipe(fdpipePF) == -1)
    {
        perror("Errore on pipe \n");
        exit(1);
    }

    if (!fork())
    {
        rChild(argv[1], fdpipeFP[1]);
    }

    if (!fork())
    {
        wChild(argv[2], fdpipePF[0]);
    }

    // Padre

    char buffer[DIM];
    char bufferSup[DIM];
    memset(buffer, 0, sizeof buffer);
    memset(bufferSup, 0, sizeof bufferSup);

    FILE *f2 = fdopen(fdpipeFP[0], "r");
    if (f2 == NULL)
    {
        perror("Errore nella  creazione del file \n");
        exit(1);
    }
    int size;
    int palindroma = 1;
    
    while(fgets(bufferSup, DIM, f2) != NULL){

        
         if(strcmp(bufferSup,"\n") == 0){
            break;
        }
        palindroma = 1;
        for (int i = 0, j = strlen(bufferSup) - 2; j > i; i++, j--)
        {
            if (bufferSup[i] != bufferSup[j])
            {
                palindroma = 0;
                break;
            }
        }
        if (palindroma)
        {
            int checkWrite = write(fdpipePF[1], bufferSup, sizeof(bufferSup));
            if (checkWrite == -1)
            {
                perror("Error on write \n");
                exit(1);
            }
        }
        
    }

    memset(bufferSup, 0, sizeof bufferSup);
    strcpy(bufferSup, "end");
    int checkWrite = write(fdpipePF[1], bufferSup, sizeof(bufferSup));
    if (checkWrite == -1)
    {
        perror("Error on write \n");
        exit(1);
    }

    exit(0);
}
