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
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>

/*
内核通知应用程序外带数据主要有两种方法：
一种是第9章介绍的I/O复用技术，select等待系统调用在接收
到外带数据时将返回，并向应用程序报告socket上的异常事件
另外一种方法就是使用sigurg信号
*/

#define BUF_SIZE 1024

static int connfd;



//信号处理函数
void sig_urg(int sig)
{
	//保留原来的errno,在函数最后恢复，以保证函数的可重入性

	int save_errno = errno;
	char buffer[BUF_SIZE];
	memset(buffer, '\0', BUF_SIZE);

	int ret = recv(connfd, buffer, BUF_SIZE-1, MSG_OOB); //接收带外数据
	printf("get % bytes of oob data '%s' \n", ret , buffer);
	errno = save_errno
}


//设置信号的处理函数
void addsig(int sig)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = sig_handler;

	// 设置 SA_RESTART属性, 那么当信号处理函数返回后, 被该信号中断的系统调用将自动恢复.
	sa.sa_flags |= SA_RESTART;

	// sigfillset(sigset_t *set)用来将参数set信号集初始化，然后把所有的信号加入到此信号集里。
	sigfillset(&sa.sa_mask);

	// sigaction设置信号处理函数
	assert(sigaction(sig, &sa, NULL) != -1);
}


int main(int argc, char* argv[])
{

	if(argc <=2)
	{
		printf("usage:%s ip_address, port_number backlog\n",
			basename(argv[0]) );
		return 1;
	}

	const char* ip = argv[1];

	int port = atoi(argv[2]);


	int ret = 0;


	/*创建一个ipv4 socket地址*/
	struct sockaddr_in address;

	//bzero() 会将内存块（字符串）的前n个字节清零，其原型为
	bzero(&address, sizeof(address)); 
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);



	//创建TCP socket ,并将其绑定到端口port上    
   //PF_INET 使用ipv4
    //SOCK_STREAM 使用TCP协议
   //0这个值通常都是唯一的，由前面的PF_INET,SOCK_STREAM已经决定了
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);

	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(listenfd >=0);


	ret = bind(listenfd , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(listenfd, 5);  //设置内核监听队列的最大长度为5，其实一般是接收比这个值大1的
	assert( ret != -1);



	struct sockaddr_in client_address;
					socklen_t client_addrlength = sizeof(client_address);
	int connfd = accept(listenfd, (struct sockaddr*)&client_address,
													&client_addrlength);

	if(connfd<0)
	{
		printf("errno is %d\n", errno);
	}
	else
	{
		 addsig(SIGURG, sig_urg);

		 //使用sigurg信号之前，我们必须设置socket的宿主进程或者进程组
		 fcntl(connfd, F_SETOWN, getpid());

		 char buffer[BUF_SIZE];

		 while(1)
		 {
		 	memset(buffer, '\0', BUF_SIZE);
		 	ret = recv(connfd, buffer, BUF_SIZE-1, 0);
		 	if(ret<=0)
		 	{
		 		break;
		 	}
		 	printf("get %d bytes of normal data '%s'\n", ret, buffer);
		 }
		 close(connfd);
	}

	close(listenfd);
	return 0;
}
