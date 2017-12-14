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

/*
测试服务器接收缓冲区设置
*/


#define BUFF_SIZE 1024

int main(int argc, char* argv[])
{

	if(argc <=2)
	{
		printf("usage:%s ip_address, port_number send_buffer_size\n",
			basename(argv[0]) );
		return 1;
	}

	const char* ip = argv[1];

	int port = atoi(argv[2]);


	/*创建一个ipv4 socket地址*/
	struct sockaddr_in address;

	//bzero() 会将内存块（字符串）的前n个字节清零，其原型为
	bzero(&address, sizeof(address)); 
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

    
       //PF_INET 使用ipv4
       //SOCK_STREAM 使用TCP协议
       //0这个值通常都是唯一的，由前面的PF_INET,SOCK_STREAM已经决定了
	int sock = socket(PF_INET, SOCK_STREAM, 0);

	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(sock >=0);

	int recvbuf = atoi(argv[3]);
	int len = sizeof(recvbuf);

	 /*设置TCP接收缓冲区大小，然后立即读取*/
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t*)&len);
	printf("the tcp receive buffer size after setting is %d\n", recvbuf);

	int ret = bind(sock , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(sock, 5);  //设置内核监听队列的最大长度为5，其实一般是接收比这个值大1的
	assert( ret != -1);

	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);
	
	printf("start connect......\n");
	int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
	printf("end connect......\n");
	if(connfd<0)
	{
		printf("errno is :%d\n", errno);
	}else{
		char buffer[BUFF_SIZE];
		// void *memset(void *s, int ch, size_t n);
		// 将s中当前位置后面的n个字节 （typedef unsigned int size_t ）用 ch 替换并返回 s 。
		// 作用是在一段内存块中填充某个给定的值，它是对较大的结构体或数组进行清零操作的一种最快方法
		memset(buffer, '\0', BUFF_SIZE);
		
		while(recv(connfd, buffer, BUFF_SIZE-1, 0)>0) {};
		
		close(connfd);

	}
	/*关闭socket*/
	close(sock);
	return 0;
}
