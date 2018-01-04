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


#define BUFFER_SIZE 64


/*
客户端使用poll同时监听用户输入和网络连接，并利用splice将用户输入内容
直接定向网络连接上以发送之，从而实现数据零拷贝,提高程序的执行效率
*/


int main(int argc, char* argv[])
{

	if(argc <=2)
	{
		printf("usage:%s ip_server_address, port_number\n",
			basename(argv[0]) );
		return 1;
	}

	const char* ip = argv[1];

	int port = atoi(argv[2]);

	
	/*创建一个ipv4 socket地址*/
	struct sockaddr_in server_address;

	//bzero() 会将内存块（字符串）的前n个字节清零，其原型为
	bzero(&server_address, sizeof(server_address)); 
	server_address.sin_family = AF_INET;
	
	inet_pton(AF_INET, ip, &server_address.sin_addr);
	server_address.sin_port = htons(port);

		
    //PF_INET 使用ipv4
    //SOCK_STREAM 使用TCP协议
    //0这个值通常都是唯一的，由前面的PF_INET,SOCK_STREAM已经决定了
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);

	//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
	assert(sockfd >=0);

	if(connect(sockfd, (struct sockaddr*)&server_address,
				sizeof(server_address))<0)
	{
		printf("connection failed\n");
		close(sockfd);
		return 1;
	}
	
	pollfd fds[2];

	//注册文件描述符0(标准输入)和文件描述符sockfd的可读事件
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];

    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret !=-1);


    while(1)
    {
    	ret = poll(fds, 2, -1);
    	if(ret < 0)
    	{
    		printf("poll failure");
    		break;
    	}

    	if(fds[1].revents & POLLRDHUP)
    	{
    		printf("server close the connection\n");
    		break;
    	}
    	else if (fds[1].revents & POLLIN)
    	{
    		memset(read_buf, '\0', BUFFER_SIZE);
    		recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
    		printf("%s\n", read_buf);
    	}

    	if(fds[0].revents & POLLIN)
    	{
    		//使用splice将用户输入的数据拷贝到sockfd上（零拷贝）
    		/*
				splice() moves data between two file descriptors without copying
	       between kernel address space and user address space.  It transfers up
	       to len bytes of data from the file descriptor fd_in to the file
	       descriptor fd_out, where one of the file descriptors must refer to a
	       pipe
    		*/
    		ret = splice(0, NULL, pipefd[1], NULL, 32768,
    								SPLICE_F_MORE | SPLICE_F_MOVE);
    		ret = splice(pipefd[0], NULL, sockfd, NULL, 32768,
    								 SPLICE_F_MORE | SPLICE_F_MOVE);
    	}
    }

	/*关闭socket*/
	close(sockfd);
	return 0;
}
