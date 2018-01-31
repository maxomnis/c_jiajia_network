#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 信号量
 当多个进程同时访问系统上的某个资源时候，比如同事写一个数据库的某条记录，
 或者修改某个文件，就需要考虑进程同步的问题，以确保任何一时刻只有一个进程
 可以拥有对资源的独占访问。通常，程序对共享资源的访问的代码只是一个很短的时间段
 ，但就是这一段代码引发了进程之间的竞争条件。我们称这段代码为关键代码段，或者临界区。
 对进程同步，也就是确保任何一个时刻只有一个进程能进入关键代码段。信号量就是起这个作用。
 信号量两种操作,p,v操作（p passeren 传递，就好像进入临界区; vriigeven 释放，就好像退出临界区）。
 假设有信号量sv,则对它p,v操作含义如下：
 1. p(sv),如果sv的值大于0，则将它减1；如果sv的值为0，则挂起进程的执行
 2. v(sv),如果有其他进程因为等待sv而挂起，则唤醒之；如果没有，则将sv加1

 信号量的值可以是任何自然数，但最常用的，最简单的信号量是二进制信号量，它能取0和1两个值.
 本书仅讨论二进制信号量，使用二进制信号量同步两个进程，以确定关键代码段的独占式访问。
*/

union semun
{
	int val;
	struct semid_ds* buf;
	unsigned short int* array;
	struct seminfo* __buf;
};

// op为-1时,执行p操作，op为1时,执行v操作
void pv(int sem_id, int op)
{
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = op;
	sem_b.sem_flg = SEM_UNDO;
	semop(sem_id, &sem_b, 1);
}

int main(int argc, char* argv[])
{
	/*
	IPC_PRIVATE(其值为0)
	*/
	int sem_id = semget(IPC_PRIVATE, 1, 0666);

	union semun sem_un;
	sem_un.val = 1;
	semctl(sem_id, 0, SETVAL, sem_un);

	pid_t id = fork();

	if(id<0)
	{
		return 1;
	}
	else if( id == 0)
	{
		//在子进程中

		printf("child try to get binary sem\n");

		//在父子进程间共享IPC_PRIVATE信号量的关键就在于两者都可以操作信号量的
		//标识符 sem_id
		pv(sem_id, -1);
		printf("child get the sem and would relase it after 5 senconds\n");
		sleep(5);
		pv(sem_id, 1);
		exit(0);
	}
	else
	{
		printf("parent try to get binary sem\n");
		pv(sem_id, -1);
		printf("parent get the sem and would relase it after 5 senconds\n");
		sleep(5);
		pv(sem_id, 1);
	}

	waitpid(id, NULL, 0);
	semctl(sem_id, 0, IPC_RMID, sem_un); //删除信号量
	return 0;
}