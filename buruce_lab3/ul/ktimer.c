
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printManPage(void);
void sighandler(int);

int main(int argc, char **argv) {
	char line[256];
	int ii, count = 0;
	int lenNum;
	int lenName;
	int i;

	struct sigaction action, inter;
	
	/* Check to see if the mytimer successfully has mknod run
	   Assumes that mytimer is tied to /dev/mytimer */
	FILE * pFile;
	pFile = fopen("/dev/mytimer", "r+");
	if (pFile==NULL) {
		fputs("mytimer module isn't loaded\n",stderr);
		return -1;
	}

	// Check if timer set
	if (argc >= 4 && strcmp(argv[1], "-s") == 0) {
		lenNum = strlen(argv[2]);
		lenName = strlen(argv[3]);
		char *ptr = malloc(lenNum+lenName+4);
		strncat(ptr, argv[1], 2);//flag
		strncat(ptr," ", 1);
		strncat(ptr, argv[2], lenNum);//timer length
		strncat(ptr," ", 1);
		strncat(ptr, argv[3], lenName);//message
		fputs(ptr, pFile);
		pause();
		while (fgets(line, 256, pFile) != NULL) {
			printf("%s", line);
		}
	}

	// List all active timers
	else if (argc == 2 && strcmp(argv[1], "-l") == 0) {
		char *ptr = malloc(4);
		strncat(ptr, argv[1], 2);//flag
		strncat(ptr," ", 1);
		strncat(ptr, "0", 1);//# of timers set to 0, - handles -l case
		fputs(ptr, pFile);
		while (fgets(line, 256, pFile) != NULL) {
			printf("%s", line);
		}	
	}


	// Otherwise invalid
	else {
		printManPage();
	}

	fclose(pFile);
	return 0;
}

void printManPage() {
	printf("Error: invalid use.\n");
	printf(" -l: list timers from the mytimer module\n");	
	printf(" ktimer [-flag] [number]\n");
	printf(" -s: add timer to the mytimer module\n");
	printf(" ktimer [-flag] [number] [message]\n");
}

void sighandler(int signo)
{
	exit(0);
}