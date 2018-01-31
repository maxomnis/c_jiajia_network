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
#include <sys/mman.h>


/*
共享内存实例
聊天室服务器程序,
*/
#define USER_LIMIT 5 	//最大用户数量
#define BUFFER_SIZE 1024	//读缓冲区的大小
#define FD_LIMIT 65535  //文件描述符数量限制
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65536

//客户数据
struct client_data
{
	sockaddr_in address;     //客户端socket地址
	int connfd;				 //socket文件描述符
	pit_t pid; 				 //处理这个链接的进程的Pid
	int pipefd[2];			//和父进程同学的管道
};


static const char* shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char* share_mem = 0;

//客户连接数组，进程用客户连接的编号来索引这个数组，即可取得相关的客户连接数据
client_data* users = 0;

//子进程和客户连接的映射关系表,用进程的PID来索引这个数组，即可取得该进行所处理的客户连接的
//编号

int* sub_process = 0;

//当前客户数量
int user_count = 0;

bool stop_child = false;



/*将文件描述符设置为非阻塞*/
int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}


void addfd(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events  = EPOLLIN | EPOLLET;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}


void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(sig_pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}


void addsig(int sig, void(*handler)(int), bool restart = true)
{
	struct sigaction  sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if(restart)
	{
		sa.sa_flags != SA_RESTART;
	}

	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) !=-1);
}


void del_resource()
{
	close(sig_pipefd[0]);
	close(sig_pipefd[1]);
	close(listenfd);
	close(epollfd);
	shm_unlik(shm_name);
	delete [] users;
	delete [] sub_process;
}


//停止一个子进程
void child_term_handler(int sig)
{
	stop_child = true;
}


/*
子进程运行的函数，参数idx指出该子进程处理的客户的链接的编号，
users是保存所有客户连接数据的数组，参数share_mem指出共享内存
的起始地址
*/
int run_child(int idx, client_data* users, char* share_mem)
{
	epoll_event events[MAX_EVENT_NUMBER];

	int child_epollfd = epoll_create(5);
	assert(child_epollfd != -1);

	int connfd = users[idx].connfd;
	addfd(child_epollfd, connfd);

	int pipefd = users[idx].pipefd[1];
	addfd(child_epollfd, pipefd);

	int ret;

	//子进程需要设置自己的信号处理函数
	addsig(SIGTERM ,child_term_handler, false);

	while(!stop_child)
	{
		int number = eopll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);

		if( (number<0) && (errno != EINTR))
		{
			printf("epoll failure\n");
			break;
		}


		for(int i=0; i<number; i++)
		{
			int sockfd = events[i].data.fd;

			//本子进程负责的客户连接有数据到达
			if((sockfd = connfd) && (events[i].events&EPOLLIN))
			{
				memset(share_mem+idx*BUFFER_SIZE, '\0', BUFFER_SIZE);
				//将客户数据读取到对应的读缓存中,该读缓存是共享内存的一段,他开始于
				//idx*BUFFER_SIZE处,长度为BUFFER_SIZE字节。因此，各个客户连接的读缓存是共享的

				ret = recv(connfd, share_mem+idx*BUFFER_SIZE, BUFFER_SIZE-1, 0);
				if(ret<0)
				{
					if(errno != EAGAIN)
					{
						stop_child = true;
					}
				}
				else if( ret == 0)
				{
					stop_child = true;
				}
				else
				{
					//成功读取客户端数据后就通知主进程（通过管道）来处理
					send(pipefd, (char *)&idx, sizeof(idx), 0);
				}
			}
				//主进程通知本进程（通过管道）将client个客户的数据发送到本进程负责的客户端
			else if((sockfd == pipefd) && (events[i].events & EPOLLIN))
			{
				int client = 0;

				//接收主进程发送的数据，即有客户端数据到达的连接的编号
				ret = recv(sockfd, (char *)&client, sizeof(client), 0);
				if(ret<0)
				{
					if(errno != EAGAIN)
					{
						stop_child = true;
					}
				}
				else if(ret == 0)
				{
					stop_child = true;
				}
				else
				{
					send(connfd, share_mem+client*BUFFER_SIZE, BUFFER_SIZE, 0);
				}
			}
			else
			{
				continue;
			}

		}
	}

	close(connfd);
	close(pipefd);
	close(child_epollfd);
	return 0;
}


int main(int argc, char* argv[])
{

	if(argc <=2)
	{
		printf("usage:%s ip_address, port_number \n",
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

	user_cont = 0;

	client_data* users = new client_data[FD_LIMIT];

	/*
		struct pollfd
		{
		int fd;               //文件描述符
		short events;        // 等待的事件
		short revents;       // 实际发生了的事件
		} ;

	*/

	sub_process = new int[PROCESS_LIMIT];

	for(int i=0; i< PROCESS_LIMIT; ++i)
	{
		sub_process[i] = -1;
	}

	epoll_event events[MAX_EVENT_NUMBER];
	epollfd = epoll_create(5);
	assert( epollfd != -1);
	addfd(epollfd, listenfd);

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0 , sig_pipefd);
	assert( ret!= -1);

	setnonblocking(sig_pipefd[1]);
	addfd(epollfd, sig_pipefd[0]);

	addsig(SIGCHLD, sig_handler);
	addsig(SIGTERM, sig_handler);
	addsig(SIGINT,  sig_handler);
	addsig(SIGPIPE, SIG_IGH);

	bool stop_server = false;
	bool terminate = false;

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
