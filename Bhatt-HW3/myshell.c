/* This example is stolen from Dr. Raju Rangaswami's original 4338
demo and modified to fit into our lecture.
*/
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ARGS 20
#define BUFSIZ 1024

/*
void extract(char *s,char *ta,char *da,int pos,int len)
{
printf("3Whats this: %s\n", s);

s=s+(pos-1);
t=s+len;
while(s!=t)
{
*d=*s;
s++;
d++;
}

printf("2whats this: %s\n", s); puts(d);

*d='\0';

printf("1Whats this: %s\n", d);
} */

int get_args(char* cmdline, char* args[])
{
	int i = 0;

	/* if no args */
	if ((args[0] = strtok(cmdline, "\n\t ")) == NULL)
		return 0;

	while ((args[++i] = strtok(NULL, "\n\t ")) != NULL) {
		if (i >= MAX_ARGS) {
			printf("Too many arguments!\n");
			exit(1);
		}
	}


	return i;
}

void execute(char* cmdline)
{
	int pid, async;
	char* args[MAX_ARGS];

	int nargs = get_args(cmdline, args);
	if (nargs <= 0) return;



	char ta[50];
	char da[50];
	int pos, len;

	int cmds[20];//cmds position in args 
	int i = 0; int cmdc = 0; //commnd counter
	char *ret;//return of pointer to ;  //used to get index
	int index;
	for (i = 0; i < nargs; i++) {
		if (strcmp(args[i], ";") == 0 || (ret = strchr(args[i], ';')) || strcmp(args[i], "|") == 0 || (ret = strchr(args[i], '|'))) {
			cmds[cmdc] = i;
			cmdc++;
			index = (int)(ret - args[i]);
			printf("Return: %s  %d\n", ret, index);

		}//end if
	}//end for

	 //extract("This is a string",ta,da,1,9);
	 //printf("This is it: %s\n", d[0]);
	 //printf("Pos: %d  %d\n", cmds[0], cmdc);
	 //printf("Huh: %s\n", args[1]);
	char* p[20];


	//execvp(args[1],  p);

	if (!strcmp(args[0], "quit") || !strcmp(args[0], "exit")) {
		exit(0);
	}

	/* check if async call */
	if (!strcmp(args[nargs - 1], "&") || !strcmp(args[nargs - 1], "|")) { async = 1; args[--nargs] = 0; }
	else async = 0;


	if (cmdc > 0) {
		// more than one command 
		char* hi[20]; hi[0] = "ls";
		for (i = 0; i <= cmdc; i++) {
			pid = fork();
			if (pid < 0) {
				printf("Error");
				exit(1);
			}
			else if (pid == 0) {
				printf("Child (%d): %d\n", i + 1, getpid());
				execvp("ls", hi);
				exit(0);
			}
			else {
				wait(NULL);
			}
		}

	}
	else {
		//only one command
		printf("HELLO\n");
		pid = fork();
		if (pid == 0) { /* child process */
			printf("Child (%d): %d\n", i + 1, getpid());
			execvp(args[0], args);
			/* return only when exec fails */
			perror("exec failed");
			exit(-1);
		}
		else if (pid > 0) { /* parent process */
			if (!async) waitpid(pid, NULL, 0);
			else printf("this is an async call\n ");
		}
		else { /* error occurred */
			perror("fork failed");
			exit(1);
		}
	}//end else

}//end execute 


int main(int argc, char* argv[])
{
	char cmdline[BUFSIZ];

	for (;;) {
		printf("JohnSmith> ");
		if (fgets(cmdline, BUFSIZ, stdin) == NULL) {
			perror("fgets failed");
			exit(1);
		}
		execute(cmdline);
	}
	return 0;
}



