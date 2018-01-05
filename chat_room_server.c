#define _GNU_SOURCE 1
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
#include <poll.h>
#include <fcntl.h>


#define USER_LIMIT 5 	//最大用户数量
#define BUFFER_SIZE 64	//读缓冲区的大小
#define FD_LIMIT 65535  //文件描述符数量限制


//客户数据
struct client_data
{
	sockaddr_in address;     //客户端socket地址
	char* write_buf;	    //待写到客户端的数据的位置
	char buf[BUFFER_SIZE];	//从客户端读入的数据
};


/*将文件描述符设置为非阻塞*/
int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
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


	 /*
		创建user数组，分配FD_LIMIT个client_data对象，可以预期：每个可能的socket链接
		都可以获得一个这样的对象，并且socket的值可以直接用来索引（作为数组的下标）
		socket连接对应的client_data对象，这是将socket和客户端数据关联的简单而高效的方式
	 */

	client_data* users = new client_data[FD_LIMIT];

	/*
		struct pollfd
		{
		int fd;               //文件描述符
		short events;        // 等待的事件
		short revents;       // 实际发生了的事件
		} ;

	*/

	//尽管我们分配了足够多的client_data对象，但为了提高poll性能，仍然需要限制用户的数量
	pollfd fds[USER_LIMIT+1];

	int user_counter = 0;

	for(int i=1; i< USER_LIMIT; ++i)
	{
		fds[i].fd = -1;
		fds[i].events = 0;
	}

	fds[0].fd = listenfd;
	fds[0].events = POLLIN|POLLERR|POLLOUT;
	fds[0].revents = 0;

	while(1)
	{
		ret = poll(fds, user_counter+1, -1);
		if(ret<0)
		{
			printf("poll failure\n");
			break;
		}

		for(int i=0; i<user_counter+1; ++i)
		{
			if((fds[i].fd == listenfd) && (fds[i].revents & POLLIN))  //服务端有新连接过来,变得可读,POLLIN 可读
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address,
					&client_addrlength);

				if(connfd<0)
				{
					printf("errno is :%d\n", errno);
					continue;
				}

				//如果请求过多，则关闭新到的链接
				if(user_counter>=USER_LIMIT)
				{
					const char* info ="too many users\n";
					printf("%s", info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				//对于新的连接，同时修改fds和users数组，前文已经提到，
				//users[connfd], 对于新连接文件描述符connfd的客户数据
				user_counter ++;
				users[connfd].address = client_address;

				setnonblocking(connfd);

				fds[user_counter].fd = connfd;
				fds[user_counter].events = POLLIN|POLLRDHUP|POLLERR;
				fds[user_counter].revents = 0;

				printf("comes a new user, now have %d users\n", user_counter);
			}
			else if(fds[i].revents & POLLERR)  //POLLERR(出错)
			{
				printf("get an error from %d\n", fds[i].fd);
				char errors[100];
				memset(errors, '\0', 100);
				socklen_t length = sizeof(errors);
				if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length)<0)
				{
					printf("get socket option failed\n");
				}
				continue;
			}
			else if(fds[i].revents & POLLRDHUP)
			{
				//如果客户端关闭，则服务器也关闭对应的连接，并将用户总数减1
				users[fds[i].fd] = users[fds[user_counter].fd];
				close(fds[i].fd);

				fds[i] = fds[user_counter];
				i--;
				user_counter -- ;
				printf("a client left");

			}
			else if(fds[i].revents & POLLIN)	//可读
			{
				int connfd = fds[i].fd;
				memset(users[connfd].buf, '\0', BUFFER_SIZE);
				ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
				printf("get %d bytes of client data %s from %d\n",
							ret,
							users[connfd].buf,
							connfd);

				if(ret<0)
				{
					//如果读操作出错，则关闭链接
					if(errno != EAGAIN)
					{
						close(connfd);
						users[fds[i].fd] = users[fds[user_counter].fd];
						fds[i] = fds[user_counter];
						i--;
						user_counter--;
					}
				}
				else if(ret == 0)
				{

				}
				else
				{
						//如果接收到客户端数据,则通知其他socket连接准备些数据
					for( int j=1; j< user_counter; ++j)
					{
						if(fds[j].fd == connfd)
						{
							continue;
						}

						fds[i].events |=~POLLIN;
						fds[i].events |= POLLOUT;
						users[fds[j].fd].write_buf = users[connfd].buf;

					}
				}
			} 
			else if(fds[i].revents & POLLOUT) 	//可写
			{
				int connfd = fds[i].fd;
				if(!users[connfd].write_buf)
				{
					continue;
				}

				ret = send(connfd, users[connfd].write_buf, 
							strlen(users[connfd].write_buf), 0);

				users[connfd].write_buf = NULL;

				//写完数据后重新注册fds[i]上的可读事件
				fds[i].events |= ~POLLOUT;
				fds[i].events  |= POLLIN;
			}
		}
	}


	delete [] users;
	close(listenfd);
	return 0;
}
