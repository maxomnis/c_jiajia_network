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
统一事件源
信号是一种异步事件
信号处理函数和程序的主循环是两条不同的执行线路，很显然，
=信号处理函数需要尽快的执行完毕，以确保该信号不被屏蔽（为了
避免一些竞争条件，信号在处理期间，系统不会再次触发它）太久，
一种典型的解决方案是：把信号的主要处理逻辑放到程序的主循环中，当信号
处理函数被触发时，它只是简单地通知主循环程序接收到信号，并把信号值传递给主
循环，主循环再根据接收到信号执行对应的逻辑代码。通过管道传递给主循环，

*/

#define MAX_EVENT_NUMBER 1024

static int pipefd[2];


/*将文件描述符设置为非阻塞*/
int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

/*
将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中，参数
enable_et指定是否对fd启用ET模式
*/
void addfd( int epollfd, int fd )
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;	//注册可读事件, event.data 用户数据，实例这里没有用到；
	


	//往事件表epollfd注册（EPOLL_CTL_ADD指添加）文件描述符(fd)的可读事件(&evnet,上面设置为可读事件)
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

	/*
	EPOLL_CTL_ADD 往事件表中注册fd上的事件
	EPOLL_CTL_MOD 修改fd上的注册事件
	EPOLL_CTL_DEL 删除fd上的注册事件
	*/

}


//信号处理函数
void sig_handler(int sig)
{
	//保留原来的errno,在函数最后恢复，以保证函数的可重入性

	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char *)&msg, 1, 0); //将信号值写入管道，以通知主循环
	errno = save_errno;
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



	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd !=-1);

	//注册tcp socekt, udp socket上的可读事件
	addfd(epollfd, listenfd);


	/*
	socketpair()函数的声明：

	#include <sys/types.h>
	#include <sys/socket.h>
	int socketpair(int d, int type, int protocol, int sv[2])；

	socketpair()函数用于创建一对无名的、相互连接的套接子。 
	如果函数成功，则返回0，创建好的套接字分别是sv[0]和sv[1]；否则返回-1，错误码保存于errno中。

	基本用法： 
	1. 这对套接字可以用于全双工通信，每一个套接字既可以读也可以写。
	例如，可以往sv[0]中写，从sv[1]中读；或者从sv[1]中写，从sv[0]中读； 

	2. 如果往一个套接字(如sv[0])中写入后，再从该套接字读时会阻塞，
	只能在另一个套接字中(sv[1])上读成功； 

	3. 读、写操作可以位于同一个进程，也可以分别位于不同的进程，
	如父子进程。如果是父子进程时，一般会功能分离，一个进程用来读，
	一个用来写。因为文件描述副sv[0]和sv[1]是进程共享的，所以读的进程要关闭写描述符, 反之，写的进程关闭读描述符
	*/

	ret = socketpair(PF_UNIX , SOCK_STREAM, 0, pipefd);
	assert(ret != -1);
	setnonblocking(pipefd[1]);

	// 注册pipefd[0]上的可读事件
	addfd(epollfd, pipefd[0]);


	//设置一些信号的处理函数
	addsig(SIGHUP);
	addsig(SIGCHLD);
	addsig(SIGTERM);
	addsig(SIGINT);

	bool stop_server = false;

	while (!stop_server)
	{
		/*


 				在一段超时事件内等待一组文件描述符上的事件,成功时，返回就绪的文件数
        		epoll_wait函数如果检测到事件，就将所有就绪事件从内核事件表（由epollfd
        		参数指定）中复制到它第二个参数events指定的数组中。这个数组只用于输出
        		epoll_wait检测到的就绪事件,而不像select和poll的数组参数那样，既用于
        		传入用户注册的事件，又用于输出内核检测到的就绪事件。这极大的提高了
        		应用程序索引就绪文件的效率。

        		epoll_wait返回就绪的文件数

        		进程一直阻塞在epoll_wait这里，等待文件描述符变的可读，等待文件描述符可读后，就
        		执行epoll_wait后面的内容
        	*/

        	int number  = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        	if( number < 0)
        	{
        		printf("epoll failure\n");
        		break;
        	}

       
       		for(int i = 0 ; i < number; i++)
			{
				int sockfd = events[i].data.fd;

				//客户端每新来一个链接都会触发这个，如果是已经连上的，然后发送消息的，不会触发这个
				if(sockfd == listenfd )
				{
					struct sockaddr_in client_address;
					socklen_t client_addrlength = sizeof(client_address);
					int connfd = accept(listenfd, (struct sockaddr*)&client_address,
													&client_addrlength);

					//将该客户端链接，放到epollfd事件表
					addfd(epollfd, connfd); 

				}
				
				// 如果就绪的文件描述符是pipfd[0],则处理信号
				else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
				{
					int sig;
					char signals[1024];
					ret = recv(pipefd[0], signals, sizeof(signals), 0) ;

					if(ret == -1)
					{
						continue;
					}
					else if(ret == 0)
					{
						continue;
					}
					else
					{
						//因为每个信号占用一个字节，所以按字节逐个接收信号，
						//我们以SIGTERM为例来说明如何安全的终止服务器主循环
						for(int i=0; i<ret; i++)
						{
							switch(signals[i])
							{
								case SIGCHLD:
								case SIGHUP:
								{
									continue;
								}
								case SIGTERM:
								case SIGINT:
								{
									stop_server = true;
								}
							}
						}
					}
				}
				else
				{

				}
			}
	}

	printf("close fds\n");
	close(listenfd);
	close(pipefd[1]);
	close(pipefd[0]);
	return 0;
}
