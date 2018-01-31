#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/*
互斥锁可以用于保护关键代码，以确保其独占式的访问，这有点像一个二进制信号。当
进入关键代码时，我们需要获得互斥锁并将其加锁，这等价于二进制信号量的p操作；
当离开关键代码时，我们需要对互斥锁解锁，以唤醒其他等待该互斥锁的线程，这等价于
二进制信号量的v操作

互斥锁用于同步线程对共享数据的访问

下面是一个使用互斥锁的实例,并且死锁了，可以用pstack命令，观察分析是不是死锁，在印象笔记本有记笔记
*/
int a = 0;
int b = 0;

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* another(void* arg)
{
	/*
	pthread_mutex_lock以原子操作的方式给锁加锁,如果目标互斥锁已经被锁上，则
	pthread_mutex_lock调用将阻塞，直到该互斥锁的占用将其释放。

	pthread_mutex_trylock与pthread_mutex_lock类似，不过它始终立即返回，而不论
	操作的互斥锁是否已经被加锁，相当于pthread_mutex_lock的非阻塞版本。当互斥锁未
	加锁时，pthread_mutex_trylock对互斥锁加锁操作。当互斥锁已经被锁住时，
	pthread_mutex_trylock将返回错误码EBUSY.需要注意的是这里讨论的pthread_mutex_trylock
	和pthread_mutex_lock的行为是这对普通锁而言的。我们将在后面讨论其他类型的锁，
	这两个加锁函数会有不同的行为
	*/
	pthread_mutex_lock(&mutex_b);	
	printf("in child thread , got mutex b, waiting for mutex a\n");
	sleep(5);
	++b;
	pthread_mutex_lock(&mutex_a);  //锁住
	b += a++;

	/*
	pthread_mutex_unlock以原子操作的方式给互斥锁解锁。如果此时有其他线程
	正在等待中国互斥锁，则这些线程中的某一个将获得它。

	上面这些函数成功时返回0，失败时返回错误码
	*/
	pthread_mutex_unlock(&mutex_a); //释放锁
	pthread_mutex_unlock(&mutex_b);	//释放锁
	pthread_exit(NULL);
}


int main()
{
	pthread_t id;
	pthread_mutex_init(&mutex_a, NULL);	//初始化互斥锁
	pthread_mutex_init(&mutex_b, NULL); //初始化互斥锁

	pthread_create(&id, NULL, another, NULL);


	//申请互斥锁a
	pthread_mutex_lock(&mutex_a);
	printf("in parent thread, got mutex a ,waiting for mutex b\n");

	sleep(5);
	//在互斥锁a的包不下操作变量a
	++a;

    //申请互斥锁b
	pthread_mutex_lock(&mutex_b);
	a+=b++;

	pthread_mutex_unlock(&mutex_b);
	pthread_mutex_unlock(&mutex_a);

    /*

	linux中的应用
	在Linux中，默认情况下是在一个线程被创建后，必须使用此函数对创建的线程进行资源回收，
	但是可以设置Threads attributes来设置当一个线程结束时，直接回收此线程所占用的系统资源，
	详细资料查看Threads attributes。
	其实在Linux中，新建的线程并不是在原先的进程中，而是系统通过一个系统调用clone()。
	该系统调用copy了一个和原先进程完全一样的进程，并在这个进程中执行线程函数。
	不过这个copy过程和fork不一样。 copy后的进程和原先的进程共享了所有的变量，
	运行环境。这样，原先进程中的变量变动在copy后的进程中便能体现出来。
	pthread_join的应用
	pthread_join使一个线程等待另一个线程结束。
	代码中如果没有pthread_join主线程会很快结束从而使整个进程结束，从而使创建的线程没有机会开始执行就结束了。
	加入pthread_join后，主线程会一直等待直到等待的线程结束自己才结束，使创建的线程有机会执行。
	所有线程都有一个线程号，也就是Thread ID。其类型为pthread_t。通过调用pthread_self()函数可以获得自身的线程号。
    */
	pthread_join(id, NULL);

	// pthread_mutex_destroy 销毁互斥锁
	pthread_mutex_destroy(&mutex_a);
	pthread_mutex_destroy(&mutex_b);
	return 0;
}