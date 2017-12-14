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


static bool stop = false;

/*SIGTERM信号的处理函数,触发时结束主程序的循环*/

static void handle_term(int sig)
{
	stop = true;
}


int main(int argc, char* argv[])
{
	signal(SIGTERM , handle_term);

	if(argc <=3)
	{
		printf("usage:%s ip_address, port_number backlog\n",
			basename(argv[0]) );
		return 1;
	}

	const char* ip = argv[1];

	int port = atoi(argv[2]);
	int backlog = atoi(argv[3]);

    //PF_INET 使用ipv4
    //SOCK_STREAM 使用TCP协议
    //0这个值通常都是唯一的，由前面的PF_INET,SOCK_STREAM已经决定了
	int sock = socket(PF_INET, SOCK_STREAM, 0);

	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(sock >=0);

	/*创建一个ipv4 socket地址*/
	struct sockaddr_in address;

	//bzero() 会将内存块（字符串）的前n个字节清零，其原型为
	bzero(&address, sizeof(address)); 
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int ret = bind(sock , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(sock, backlog);  //设置内核监听队列的最大长度为5，其实一般是接收比这个值大1的，不同的平台可能不同
	assert( ret != -1);

	while(!stop)
	{
		sleep(1);
	}

	/*关闭socket*/
	close(sock);
	return 0;
}
