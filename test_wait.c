#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h> 
#include <errno.h>
#include <stdio.h>


/*
 *fork调用一次返回两次，返回等于0的时候是当前运行的是子进程，返回非0的时候，当前运行的是父进程,fork()返回的是子进程的id
 * 
 * 
 */

int main(int argc, char *argv[])
{
	pid_t cpid, w;
	int wstatus;

	cpid = fork();
	if (cpid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (cpid == 0) {            /* Code executed by child */
		printf("Child PID is %ld\n", (long) getpid());
		if (argc == 1)
			pause();                    /* Wait for signals */
		_exit(atoi(argv[1]));

	} else {                    
	
		 
		/* Code executed by parent */
		do {
			printf("Parent PID is %ld\n", (long) getpid());
		
			// wstatus 存储子进程退出时候的状态	
			w = waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
			if (w == -1) {
				perror("waitpid");
				exit(EXIT_FAILURE);
			}

			if (WIFEXITED(wstatus)) {
				printf("exited, status=%d\n", WEXITSTATUS(wstatus));
			} else if (WIFSIGNALED(wstatus)) {
				printf("killed by signal %d\n", WTERMSIG(wstatus));
			} else if (WIFSTOPPED(wstatus)) {
				printf("stopped by signal %d\n", WSTOPSIG(wstatus));
			} else if (WIFCONTINUED(wstatus)) {
				printf("continued\n");
			}
		} 
		//while(1);
		while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
		exit(EXIT_SUCCESS);
	}
}
