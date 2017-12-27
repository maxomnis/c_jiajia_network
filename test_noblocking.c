#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#define STDIN 0 

#define BUFF_SIZE 1024

/*将文件描述符设置为非阻塞*/
int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

/*
   非阻塞式模型
 */

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


	//PF_INET 使用ipv4
	//SOCK_STREAM 使用TCP协议
	//0这个值通常都是唯一的，由前面的PF_INET,SOCK_STREAM已经决定了
	int sock = socket(PF_INET, SOCK_STREAM, 0);

	setnonblocking(sock);
		
	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(sock >=0);


	ret = bind(sock , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(sock, 5);  //设置内核监听队列 的最大长度为5，其实一般是接收比这个值大1的
	assert( ret != -1);


	while (1)
	{
		struct sockaddr_in client;
		socklen_t client_addrlength = sizeof(client);

		printf("start connect......\n");

		/*
		 在阻塞模式下，在I/O操作完成前，执行操作的函数一直等候而不会立即返回，
		该函数所在的线程会阻塞在这里。相反，在非阻塞模式下，套接字函数会立即返回，
		而不管I/O是否完成，该函数所在的线程会继续运行,所以下面的accept会一直返回错误，
		因为非阻塞模式下，accpet不会阻塞，会不停的accpet, 如果没有数据返回就报错,并不会像阻塞模型那样
		一直阻塞在accpet，等到第一个用户连接之后，后面就会一直阻塞在recv，等待数据过来;
		*/
		int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
		printf("end connect......\n");
		if(connfd<0)
		{
			printf("errno is :%d\n", errno);

		}else{
			char buffer[BUFF_SIZE];

			while(1)
			{
				memset(buffer, '\0', BUFF_SIZE);

				ret = recv(connfd, buffer, sizeof(buffer)-1, 0 );
				if(ret <=0 )
				{
					close(connfd);
					break;		//如果没有收到数据，则跳出循环，关掉链接
				}
				printf("get %d bytes of normal data:%s\n", ret, buffer);

			}	
		}
	}
	close(sock);
	return 0;
}

