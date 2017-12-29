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
#define STDIN 0 

#define BUFF_SIZE 1024

/*
 测试select io模型
 这个只支持一个客户端
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

	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(sock >=0);


	ret = bind(sock , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(sock, 5);  //设置内核监听队列 的最大长度为5，其实一般是接收比这个值大1的
	assert( ret != -1);

	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);
	
	printf("start connect......\n");

	//进程会卡在这里等待客户端的连接
	int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
	printf("end connect......\n");
	if(connfd<0)
	{
		printf("errno is :%d\n", errno);
		close(sock);

	}else{
		char buffer[BUFF_SIZE];

		fd_set read_fds;
		fd_set exception_fds;

		/*

		FD_ZERO(fd_set *fdset);将指定的文件描述符集清空，在对文件描述符集合进行设置前，必须对其进行初始化，
		如果不清空，由于在系统分配内存空间后，通常并不作清空处理，所以结果是不可知的。
		*/
		FD_ZERO(&read_fds);  //
		FD_ZERO(&exception_fds);
		while(1)
		{
			memset(buffer, '\0', BUFF_SIZE);

			/*
				FD_SET(fd_set *fdset);用于在文件描述符集合中增加一个新的文件描述符
			*/

			/*
			 每次调用select前都要重新在read_fds和exception_fds中设置文件描述符connfd，
			 因为事件发生之后，
			 文件描述符的集合将被内核修改

			 下面需要把connfd复制到read_fds，exception_fds，如果有可写列表，还要增加
			 可写列表，但是epoll，只需要注册事件就行，不需要这样复制加到可读，可写，异常
			 列表
			*/
			FD_SET(connfd, &read_fds);		//将connfd加入到集合read_fds中
			FD_SET(connfd, &exception_fds);	//将connfd加入到集合exception_fds中
			
			//进程会阻塞在这里，等待客户端有信息发过来;如果有消息过来，就执行下面的recv
			//等recv执行完之后，就进行下一次的while循环
			ret = select(connfd+1, &read_fds, NULL, &exception_fds, NULL);
			
			if( ret <0 )
			{
				printf("selection failure\n");
				break;
			}

			/*
				
				FD_ISSET(int fd,fd_set *fdset)，用于测试指定的文件描述符是否在该集合中，

			*/
			//这里用于判断connfd，是否在可读列表(read_fdsz)中
			if(FD_ISSET(connfd, &read_fds))			
			{
				//对于可读事件，采用普通的recv函数读取数据
				ret = recv(connfd, buffer, sizeof(buffer)-1, 0 );
				if(ret <=0 )
				{
					break;		//如果没有收到数据，则跳出循环，进程return 0 ，进程结束
				}
				printf("get %d bytes of normal data:%s\n", ret, buffer);
			}
			else if(FD_ISSET(connfd, &exception_fds))
			{
				ret = recv(connfd, buffer, sizeof(buffer)-1, MSG_OOB);	//MSG_OOB表示外带数据
				if( ret<= 0)
				{
					break;
				}
				printf("get %d bytes of normal data:%s\n", ret, buffer);
			}
			
		}	
		close(connfd);
		close(sock);
		return 0;	//进程退出
}
}
