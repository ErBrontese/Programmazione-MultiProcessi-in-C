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

#define NAME_DIM 1024
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum
{
	C_LIST,
	C_SIZE,
	C_SEARCH
} C_TYPE;

int createCoda()
{
	int fdCoda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
	if (fdCoda == -1)
	{
		perror("Error on msgget \n");
		exit(1);
	}

	return fdCoda;
}

typedef struct
{

	long type;
	char string[NAME_DIM];
	char parola[NAME_DIM];
	char file[NAME_DIM];
	char command[NAME_DIM];
	int done; // Ci permette di capire se dobbiamo ancora aspettare

} msg;

void createChild(int fdcoda, int id, char *path)
{
	struct stat stinfo;
	struct dirent *entry;
	DIR *directory;
	msg messaggio;

	directory = opendir(path);
	if (directory == NULL)
	{
		perror("Errore on opendir \n");
		exit(1);
	}

	if (lstat(path, &stinfo) == -1)
	{
		perror("Error on lstat \n");
		exit(1);
	}

	while (1)
	{

		if (msgrcv(fdcoda, &messaggio, MSG_DIM, id + 1, 0) == -1)
		{
			perror("Error on msgrcv \n");
			exit(1);
		}

		if (messaggio.done == 1)
		{
			break;
		}

		if (!strcmp(messaggio.command, "list"))
		{
			while ((entry = readdir(directory)) != NULL)
			{
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				{
					continue;
				}

				messaggio.type = 1;
				sprintf(messaggio.string, "Info %s", entry->d_name);

				if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
				{
					perror("Error on msgsnd \n");
					exit(1);
				}
			}

			seekdir(directory, 0); // Azzerare il valore della directory precedente
			messaggio.type = 1;
			messaggio.done = 1;

			if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}
		}
		else if (!strcmp(messaggio.command, "size"))
		{
			struct stat stInfoTemp;
			char bufferSupport[NAME_DIM];
			memset(bufferSupport, 0, sizeof bufferSupport);
			strcpy(bufferSupport, path);
			strncat(bufferSupport, "/", sizeof(bufferSupport) - sizeof(bufferSupport[0]));
			strncat(bufferSupport, messaggio.file, sizeof(bufferSupport) - sizeof(bufferSupport[0]));

			if (lstat(bufferSupport, &stInfoTemp))
			{
				perror("Error on lstat temp \n");
				exit(1);
			}

			messaggio.type = 1;

			sprintf(messaggio.string, "La dimensione del file è %ld", stInfoTemp.st_size);
			if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}
		}
		else if (!strcmp(messaggio.command, "search"))
		{

			char bufferSupport[NAME_DIM];
			memset(bufferSupport, 0, sizeof bufferSupport);
			strcpy(bufferSupport, path);
			strncat(bufferSupport, "/", sizeof(bufferSupport) - sizeof(bufferSupport[0]));
			strncat(bufferSupport, messaggio.file, sizeof(bufferSupport) - sizeof(bufferSupport[0]));
			printf("%s \n",bufferSupport);
			FILE *fd;

			fd = fopen(bufferSupport, "r");
			if (fd == NULL)
			{
				perror("Error on fopen \n");
				exit(1);
			}

			char line[NAME_DIM];
			int occurrences = 0;
			int not_occurrences = 0;

			while (fgets(line, NAME_DIM, fd))
			{
				if (strstr(line, messaggio.parola))
				{
					occurrences++;
				}
				not_occurrences++;
			}

			messaggio.type = 1;
			sprintf(messaggio.string, "Ripetizione della parola %d", occurrences);
			if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			messaggio.type = 1;
			sprintf(messaggio.string, "Numero di righe %d", occurrences + not_occurrences);
			if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			messaggio.done = 1;
			if (msgsnd(fdcoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}
		}
	}

	closedir(directory);

	exit(0);
}

int main(int argc, char *argv[])
{

	int mycoda = createCoda();
	char stringa[NAME_DIM];
	char *commando;
	char *NumeroIdentificativo;
	char *NomeFile;
	char *parola;
	msg messaggio;

	for (int i = 0; i < argc - 1; i++)
	{
		if (!fork())
		{
			createChild(mycoda, i + 1, argv[i + 1]);
		}
	}

	while (1)
	{
		printf("file-shell> ");
		fgets(stringa, NAME_DIM, stdin);

		if (stringa[strlen(stringa) - 1] == '\n')
		{
			stringa[strlen(stringa) - 1] = '\0';
		}

		if ((commando = strtok(stringa, " ")) != NULL)
		{
			if ((NumeroIdentificativo = strtok(NULL, " ")) != NULL)
			{
				if ((NomeFile = strtok(NULL, " ")) != NULL)
				{
					parola = strtok(NULL, " ");
				}
			}
		}

	
		messaggio.type = atol(NumeroIdentificativo) + 1;
		messaggio.done = 0;

		if (!strcmp(commando, "list"))
		{
			strcpy(messaggio.command, "list");
			if (msgsnd(mycoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			while (1)
			{
				if (msgrcv(mycoda, &messaggio, MSG_DIM, 1, 0) == -1)
				{
					perror("Error on msgrcv \n");
					exit(1);
				}

				if (messaggio.done == 1)
				{
					break;
				}

				printf("Ricevuto: %s \n", messaggio.string);
			}
		}
		else if (!strcmp(commando, "size"))
		{
			strcpy(messaggio.command, "size");
			strcpy(messaggio.file, NomeFile);

			if (msgsnd(mycoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			if (msgrcv(mycoda, &messaggio, MSG_DIM, 1, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			printf("Ricevuto: %s \n", messaggio.string);
		}
		else if (!strcmp(commando, "search"))
		{
			strcpy(messaggio.command, "search");
			strcpy(messaggio.file, NomeFile);
			strcpy(messaggio.parola, parola);
			if (msgsnd(mycoda, &messaggio, MSG_DIM, 0) == -1)
			{
				perror("Error on msgsnd \n");
				exit(1);
			}

			while (1)
			{
				if (msgrcv(mycoda, &messaggio, MSG_DIM, 1, 0) == -1)
				{
					perror("Error on msgrcv \n");
					exit(1);
				}

				if (messaggio.done == 1)
				{
					break;
				}

				printf("Ricevuto: %s \n", messaggio.string);
			}
		}
	}

	return 0;
}
