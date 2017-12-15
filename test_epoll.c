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
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10  //最多存放10个字节

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
void addfd( int epollfd, int fd, bool enable_et )
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;	//注册可读事件, event.data 用户数据，实例这里没有用到；
	if(enable_et)
	{
		event.events |= EPOLLET;
	}

	//将要交由内核管控的文件描述符加入epoll对象并设置触发条件
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	/*
	EPOLL_CTL_ADD 往事件表中注册fd上的事件
	EPOLL_CTL_MOD 修改fd上的注册事件
	EPOLL_CTL_DEL 删除fd上的注册事件
	*/

}

/*LT模式的工作流程*/
void lt( epoll_event* events , int number , int epollfd , int listenfd )
{
	char buf[BUFFER_SIZE];
	for(int i = 0 ; i < number; i++)
	{
		int sockfd = events[i].data.fd;
		if(sockfd == listenfd )
		{
			struct sockaddr_in client_address;
			socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr*)&client_address,
											&client_addrlength);
			addfd(epollfd, connfd, false); /*对connfd禁用ET模式*/
		}
		else if(events[i].events & EPOLLIN)
		{
			/*只要socket读缓存中还有未读出的数据，这段代码就被触发*/
			printf("event trigger once\n");
			memset(buf, '\0', BUFFER_SIZE);

			int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
			if( ret <=0 )
			{
				close(sockfd);
				continue;
			}
			printf("get %d bytes of content:%s\n", ret , buf);
		}
		else
		{
			printf("something else happend\n");
		}
	}
}

/*ET模式的工作流程*/
void et(epoll_event* events, int number, int epollfd, int listenfd)
{
	char buf[BUFFER_SIZE];
	for( int i=0; i < number; i++)
	{
		int sockfd = events[i].data.fd;
		if(sockfd == listenfd)
		{
			struct sockaddr_in client_address;
			socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr*)&client_address,
											&client_addrlength);
			addfd(epollfd, connfd, true);  /*对connfd开启et模式*/
		}
		else if(events[i].events & EPOLLIN)
		{
			/*这段代码不会被重复触发，所以我们循环读取数据，以确保把socket读缓存中的说有数据读出*/
			printf("event trigger once\n");
			while(1)
			{
				memset(buf, '\0', BUFFER_SIZE);
				int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
				if( ret < 0 )
				{
					/*
					 对于非阻塞IO，下面的条件成立表示数据已经全部读取完毕，此后，epoll就能
					 再次触发sockfd的EPOLLIN事件，以驱动下一次读操作
					*/
					if((errno == EAGAIN) || (errno == EWOULDBLOCK))
					{
						printf("read later\n");
						break;
					}
					close(sockfd);
					break;
				}
				else if ( ret == 0)
				{
					close(sockfd);
				}
				else
				{
					printf("get %d bytes of content: %s\n", ret, buf);
				}
			}
		}
		else
		{
			printf("something else happended");
		}


	}
}



int main(int argc, char* argv[])
{
	if(argc <=2)
	{
		printf("usage:%s ip_address port_number\n", basename(argv[0]));
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


        //结构体epoll_event 被用于注册所感兴趣的事件和回传所发生待处理的事件.
        epoll_event events[MAX_EVENT_NUMBER];

        //epoll_create在内核中创建用于存放epoll关心的文件描述符的事件表
        int epollfd = epoll_create(5);  //size=5，这个参数并不起作用，只是给内核一个提示，告诉
        //它，事件表需要多大


        assert(epollfd !=-1 );

        /*
		将listenfd的事件,注册到内核事件表epollfd中
        */
        addfd(epollfd, listenfd, true);

        while(1)
        {
        	/*
 				在一段超时事件内等待一组文件描述符上的事件,成功时，返回就绪的文件数
        		epoll_wait函数如果检测到事件，就将所有就绪事件从内核事件表（由epollfd
        		参数指定）中复制到它第二个参数events指定的数组中。这个数组只用于输出
        		epoll_wait检测到的就绪事件,而不像select和poll的数组参数那样，既用于
        		传入用户注册的事件，又用于输出内核检测到的就绪事件。这极大的提高了
        		应用程序索引就绪文件的效率。

        		epoll_wait返回就绪的文件数
        	*/
        	int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        	if( ret < 0)
        	{
        		printf("epoll failure\n");
        		break;
        	}

        	//体会下两个模式的区别
        	lt(events, ret, epollfd, listenfd); // 使用LT模式
        	//et(events, ret, epollfd, listenfd); // 使用ET模式
        }

        close(listenfd);
        return 0;

}




