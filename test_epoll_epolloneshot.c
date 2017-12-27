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

/*

使用EPOLLONESHOT事件，防止两个线程操作一个socket的情况

*/
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10  //最多存放10个字节

struct fds
{
	int epollfd;
	int sockfd;
};

/*将文件描述符设置为非阻塞*/
int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}


/*
将fd上的epollin和epollet事件注册到epollfd指示的epoll内核事件表中，
参数oneshot指定是否注册fd上的EPOLLONESHOT事件
*/
void addfd( int epollfd, int fd, bool oneshot )
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;	//EPOLLIN注册可读事件, event.data 用户数据，实例这里没有用到；EPOLLET 启用ET模式
	if(oneshot)
	{
		event.events |= EPOLLONESHOT;	//
	}

	//往事件表epollfd添加（EPOLL_CTL_ADD指添加）文件描述符(fd)的可读事件(&evnet,上面设置为可读事件)
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

	/*
	EPOLL_CTL_ADD 往事件表中注册fd上的事件
	EPOLL_CTL_MOD 修改fd上的注册事件
	EPOLL_CTL_DEL 删除fd上的注册事件
	*/

	setnonblocking(fd);

}


/*
重置fd上的事件.这样操作之后，尽管fd上的EPOLLONESHOT事件被注册，
但是操作系统仍然会触发fd上的EPOLLIN事件，且只触发一次
*/
void reset_onshot(int epollfd , int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


//工作线程
void* worker(void* arg)
{
	int sockfd = ((fds*)arg)->sockfd;

	int epollfd = ((fds*)arg)->epollfd;
	printf("start new thread to receive data on fd:%d\n", sockfd);
	char buf[BUFFER_SIZE];
	memset(buf, '\0', BUFFER_SIZE);

	//循环读取socket上的数据，直到遇到EAGAIN错误
	while (1)
	{
		int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
		if(ret == 0)
		{
			close(sockfd);
			printf("foreiner close the connection\n");
			break;
		}
		else if(ret <0)
		{
			if(errno == EAGAIN)
			{
				reset_onshot(epollfd, sockfd);
				printf("read later\n");
				break;
			}
		}
		else
		{
			printf("get connect:%s\n", buf);
			//睡眠5s，模拟数据处理
			sleep(5);
		}
	}
	printf("end thread receiving data on fd:%d\n", sockfd);
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
        int epollfd = epoll_create(5);  
        //size=5，这个参数并不起作用，只是给内核一个提示，告诉它，事件表需要多大


        assert(epollfd !=-1 );

        /*
		   注意，监听socket listend上是不能注册EPOLLONESHOT事件的，
		   否则应用程序只能处理一个客户连接。因为后续的客户连接将不再
		   触发listenfd上的EPOLLIN事件
        */
        addfd(epollfd, listenfd, false);

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

        		进程一直阻塞在epoll_wait这里，等待文件描述符变的可读，等待文件描述符可读后，就
        		执行epoll_wait后面的内容
        	*/

        	int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        	if( ret < 0)
        	{
        		printf("epoll failure\n");
        		break;
        	}

       
       		for( int i=0; i<ret ; i++)
       		{
       			int sockfd = events[i].data.fd;
       			if(sockfd == listenfd)
       			{
       				struct sockaddr_in client_address;
       				socklen_t client_addrlength = sizeof(client_address);
       				int connfd = accept(listenfd, (struct sockaddr*)&client_address, 
       									&client_addrlength);

       				//对每个非监听文件描述符都注册到EPOLLONESHOT事件
       				addfd(epollfd, connfd, false);
       			}
       			else if(events[i].events & EPOLLIN)
       			{
       				pthread_t thread;
       				fds fds_for_new_worker;
       				fds_for_new_worker.epollfd = epollfd;
       				fds_for_new_worker.sockfd = sockfd;

       				//启用一个新工作线程为sockfd服务
       				pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
       			}
       			else
       			{
       				printf("something else happend \n");
       			}
       		}
        }

        close(listenfd);
        return 0;

}




