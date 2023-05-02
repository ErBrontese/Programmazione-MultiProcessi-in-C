#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define DIM 1024
#define DIM2 2014
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct
{
    long type;
    int commando;
    int numeroFile;
    char file[DIM];
    char parola;
    char risposta[DIM];
    int done;
} msg;

int stringCount(char *string, char c, long size)
{
    int count = 0;

    for (char *p = string; *p != 0; ++p)
    {
        if (*p == c)
            ++count;
    }

    return count;
}

int init_msg()
{
    int msg;
    if ((msg = msgget(IPC_PRIVATE, IPC_EXCL | IPC_CREAT | 0600)) == -1)
    {
        perror("Error on msgget \n");
        exit(1);
    }

    return msg;
}

void child(int msgqueue, int id, char *file)
{
    struct dirent *entry;
    struct stat stInfo;
    DIR *directory;
    msg messaggio;

    if ((directory = opendir(file)) == NULL)
    {
        perror("Error on opendir \n");
        exit(1);
    }

    if (lstat(file, &stInfo) == -1)
    {
        perror("Error on lstat \n");
        exit(1);
    }

    while (1)
    {

        if (msgrcv(msgqueue, &messaggio, MSG_DIM, id + 1, 0) == -1)
        {
            perror("Error on msgrcv figlio \n");
            exit(1);
        }

        if (messaggio.done)
        {
            break;
        }

        if (messaggio.commando == 1)
        {

            int count = 0;
            while ((entry = readdir(directory)) != NULL)
            {

                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }
                else
                {
                    if (entry->d_type == DT_REG)
                    {
                        count++;
                    }
                }
            }

            seekdir(directory, 0); // Azzerare il valore della directory precedente
            sprintf(messaggio.risposta, "Il numero di file è %d \n", count);
            messaggio.type = 1;
            messaggio.done = 1;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
        }
        else if (messaggio.commando == 2)
        {

            long count = 0;
            char bufferTemp[DIM];
            struct stat temp;
            while ((entry = readdir(directory)) != NULL)
            {

                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }
                else
                {
                    if (entry->d_type == DT_REG)
                    {
                        sprintf(bufferTemp, "%s/%s", file, entry->d_name);
                        lstat(bufferTemp, &temp);
                        count += temp.st_size;
                    }
                }
            }

            seekdir(directory, 0); // Azzerare il valore della directory precedente
            sprintf(messaggio.risposta, "La dimensione totale dei file regolari è : %ld \n", count);
            messaggio.type = 1;
            messaggio.done = 1;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
        }
        else if (messaggio.commando == 3)
        {
            int fd;
            char *p;
            struct stat stInfo;
            char buffer[DIM2];
            FILE *temp;

            sprintf(buffer, "%s/%s", file, messaggio.file);
            printf("buffer %s \n", buffer);
            if ((fd = open(buffer, O_RDONLY)) == -1)
            {
                perror("Error on fopen \n");
                exit(1);
            }

            FILE *swap;

            if ((swap = fopen("temp.txt", "w")) == NULL)
            {
                perror("Error on fopen \n");
                exit(1);
            }

            if (lstat(buffer, &stInfo))
            {
                perror("Error on lstat \n");
                exit(1);
            }

            if (!S_ISREG(stInfo.st_mode))
            {
                perror("Non è un file regolare \n");
                exit(1);
            }

            if ((p = (char *)mmap(NULL, stInfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
            {
                perror("Error on mapping \n");
                exit(1);
            }

            for (int i = 0; i < stInfo.st_size; i++)
            {
                fputc(p[i], swap);
            }

            fclose(swap);

            if ((swap = fopen("temp.txt", "r")) == NULL)
            {
                perror("Error on fopen \n");
                exit(1);
            }

            char bufferSwap[DIM];
            int occorenza = 0;
            while (fgets(bufferSwap, DIM, swap) != NULL)
            {
                if (bufferSwap[strlen(bufferSwap) - 1] == '\n')
                {
                    bufferSwap[strlen(bufferSwap) - 1] = '\0';
                }

                for(int i=0; i< strlen(bufferSwap);i++){
                    if(bufferSwap[i] == messaggio.parola){
                        occorenza++;
                    }
                }
            }

            //  int occorenze = stringCount((char *)p, messaggio.parola, stInfo.st_size);
              sprintf(messaggio.risposta, "Il numero di occorenze è : %d \n", occorenza);
            messaggio.type = 1;
            messaggio.done = 1;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }
            close(fd);
            munmap(p, stInfo.st_size);
        }
    }
}

int main(int argc, char **argv)
{
    msg messaggio;
    int msgqueue = init_msg();
    for (int i = 0; i < argc - 1; i++)
    {
        if (!fork())
        {
            child(msgqueue, i + 1, argv[i + 1]);
        }
    }
    char *commando;
    char *numeroFile;
    char *nomeFile;
    char *carattere;
    char buffer[DIM];
    while (1)
    {
        printf("shel> ");
        fgets(buffer, DIM, stdin);

        if (buffer[strlen(buffer) - 1] == '\n')
        {
            buffer[strlen(buffer) - 1] = '\0';
        }

        if ((commando = strtok(buffer, " ")) != "NULL")
        {
            if ((numeroFile = strtok(NULL, " ")) != "NULL")
            {
                if ((nomeFile = strtok(NULL, " ")) != "NULL")
                {
                    carattere = strtok(NULL, " ");
                }
            }
        }
        messaggio.type = atol(numeroFile) + 1;
        messaggio.done = 0;

        if ((strcmp(commando, "num-files")) == 0)
        {

            messaggio.numeroFile = atoi(numeroFile);
            messaggio.commando = 1;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }

            while (1)
            {
                if (msgrcv(msgqueue, &messaggio, MSG_DIM, 1, 0) == -1)
                {
                    perror("Error on msgsrcv \n");
                    exit(1);
                }

                printf("%s", messaggio.risposta);

                if (messaggio.done)
                {
                    break;
                }
            }
        }
        else if (strcmp(commando, "total-size") == 0)
        {

            messaggio.numeroFile = atoi(numeroFile);
            messaggio.commando = 2;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }

            while (1)
            {
                if (msgrcv(msgqueue, &messaggio, MSG_DIM, 1, 0) == -1)
                {
                    perror("Error on msgsrcv \n");
                    exit(1);
                }

                printf("%s", messaggio.risposta);

                if (messaggio.done)
                {
                    break;
                }
            }
        }
        else if (strcmp(commando, "search-char") == 0)
        {
            messaggio.numeroFile = atoi(numeroFile);
            messaggio.commando = 3;
            strcpy(messaggio.file, nomeFile);
            messaggio.parola = *carattere;
            if (msgsnd(msgqueue, &messaggio, MSG_DIM, 0) == -1)
            {
                perror("Error on msgsnd \n");
                exit(1);
            }

            while (1)
            {
                if (msgrcv(msgqueue, &messaggio, MSG_DIM, 1, 0) == -1)
                {
                    perror("Error on msgsrcv \n");
                    exit(1);
                }

                printf("%s", messaggio.risposta);

                if (messaggio.done)
                {
                    break;
                }
            }
        }

        /* code */
    }
}
