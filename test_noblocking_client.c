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
非阻塞IO的client
*/

#define BUFFER_SIZE 1023

int setnoblocking (int fd)
{
	int old_option = fcntl(fd, F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option );
	return old_option;
}


/*
超时连接函数，参数分别是服务器IP地址，端口号和超时时间（毫秒），函数成功返回时返回已经处于连接状态的socket
失败则返回-1
*/

int unblock_connect (const char* ip, int port ,int time)
{
	int ret = 0;
	struct sockaddr_int address;
	bzero(&address , sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int sockft = socket(PF_INET, SOCK_STREAM, 0 );
	int fdopt = setnoblocking(socdfd);
	ret = connect(sockdf, (struct sockaddr*)&address, sizeof(address));
	
	if(ret == 0 )
	{
		//如果连接成功，则恢复sockfd的属性，并立即返回之
		printf("connect with server immediately\n");
		fcntl(sockfd, F_SETFL, fdopt);
		return sockfd;
	}
	else if(errno != EINPROGRESS)
	{
		//如果连接没有立刻建立，那么只有当error是EINPROGRESS时才表示连接还在进行，否则出错返回
		printf("unblock connect not support\n");
		return -1;
	}

	fd_set readfds;
	fd_set writefds;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &writefds);

	timeout.tv_sec = time;
	timeout.tv_usec = 0;
	ret = select(sockfd+1, NULL, &writefds, NULL, &timeout);

	if(ret<=0)
	{
		//select 超时或者出错，立即返回
		printf("connection time out");
		close(sockfd);
		return -1;
	}

	//FD_ISSET检查sockfd是否在可写列表中
	if( !FD_ISSET(sockfd, &writefds))
	{
		printf("no events on sockfd\n", );
		close(sockfd);
		return -1;
	}	

	int error = 0;

	socklen_t length = sizeof(error);

	//调用getsockopt来获取并清除sockfd上的错误
	if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length)<0)
	{
		printf("get socket option failed\n");
		close(sockfd);
		return -1
	}

	//错误号不为0表示连接错误
	if(error != 0 )
	{
		printf("connection failed after select with the error :%d \n", error);
		close(sockfd);
		return -1;
	}

	//连接成功
	printf("connection ready after select with the socket:%d\n", socket);

	fcntl(sockfd, F_SETFL, fdopt);
	return socdfd;
}



int main(int argc, char* argv[])
{

	if(argc <=2)
	{
		printf("usage:%s ip_server_address, port_number backlog\n",
			basename(argv[0]) );
		return 1;
	}

	const char* ip = argv[1];

	int port = atoi(argv[2]);

	
	int sockfd = unblock_connect(ip, port 10);
	if(sockfd<0)
	{
		return 1;
	}

	close(sockfd);
	return 0;
}
