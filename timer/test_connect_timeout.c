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
定时器
定时是指在一段时间之后触发某段代码的机制，我们可以在这段代码
中依次处理所有到期的定时器。换言之，定时机制是定时器得以被
处理的原动力。

linux他提供了三种定时方法：
1. socket选项 SO_RECVTIMEO和SO_SNDTIMEO
   SO_RECVTIMEO和SO_SNDTIMEO分别用来设置socket的接收数据超时时间和发送数据超时时间
   因此这两个选项仅对于数据接收和发送相关的socket专用系统调用

2. SIGALRM信号
3. I/O复用系统调用时的超时参数
*/

//超时连接函数

int timeout_connect(const char* ip, int port , int time)
{
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

	/*
	  通过选项SO_RCVTIMEO和SO_SNDTIMEO所设置的超时时间的类型是timeval,这和
	  select系统调用的超时参数类型相同
	*/

	struct timeval timeout;
	timeout.tv_sec = time;
	timeout.tv_usec = 0;
	socklen_t len = sizeof(timeout);
	ret = setsockopt(listenfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);

	ret = connect(listenfd, (struct sockaddr*)&address, sizeof(address));

	if(ret == -1)
	{
		//超时对应的错误是EINPROGRESS, 下面这个条件如果成立，我们就可以处理定时任务了

		if(errno == EINPROGRESS)
		{
			printf("connecting timeout ,process timeout logic\n", );
			return -1;
		}

		printf("error occur when connecting to server\n");
		return -1;
	}

	return sockfd;
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

	int sockfd = timeout_connect(ip, port, 10);
	if(sockfd<0)
	{
		return 1;
	}
	return 0;
}
