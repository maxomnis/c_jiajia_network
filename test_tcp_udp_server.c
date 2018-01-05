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
服务器如果要同时监听多个端口，就必须创建多个socket,
并将它们分别绑定到各个端口上。这样一来，服务器程序
就需要同时管理多个监听socket，I/O复用技术就有了用武之地。
另外，即使是同一个端口，如果服务器需要同时处理该端口上的
TCP和UDP请求，则也需要创建两个不同的socket，一个流socket，
另一个是数据报socket，并将它们都绑定到该端口上；

下面的例子就是能同时处理TCP和UDP请求
*/

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

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



	// 创建UDP socket,并将其绑定到端口prot上
	bzero(&address, sizeof(address)); 
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);
	int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
	assert(udpfd>=0);

	ret = bind(udpfd , (struct sockaddr*)&address, sizeof(address));


	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd !=-1);

	//注册tcp socekt, udp socket上的可读事件
	addfd(epollfd, listenfd);
	addfd(epollfd, udpfd);


	while (1)
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
				else if(sockfd == udpfd)
				{
					 char buf[UDP_BUFFER_SIZE];
					 memset(buf, '\0', UDP_BUFFER_SIZE);
					 struct sockaddr_in client_address;
					 socklen_t client_addrlength = sizeof(client_address);

					 ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE-1,0,
					 	(struct sockaddr*)&client_address, &client_addrlength);

					 if(ret>0)
					 {
					 	sendto(udpfd, buf, UDP_BUFFER_SIZE-1, 0,
					 		(struct sockaddr*)&client_address, client_addrlength);
					 }
				}
				
				// 如果是已经连上了，然后发送消息的，就会触发这个
				else if(events[i].events & EPOLLIN)
				{
					char buf[TCP_BUFFER_SIZE];

					memset(buf, '\0', TCP_BUFFER_SIZE);
					int ret = recv(sockfd, buf, TCP_BUFFER_SIZE-1, 0);
					if( ret <=0 )
					{
						if((errno == EAGAIN) || (errno == EWOULDBLOCK))
						{
							break;
						}
						close(sockfd);
						break;
					}
					else if(ret == 0)
					{
						close(sockfd);
					}
					else
					{
						send(sockfd, buf, ret, 0);
					}
				
				}

				else
				{
					printf("something else happend\n");
				}
			}
	}

	/*关闭socket*/
	close(listenfd);
	return 0;
}
