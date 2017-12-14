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


#define BUFFER_SIZE 512

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

	int sendbuf = atoi(argv[3]);
	int len = sizeof(sendbuf);
	
	/*设置TCP发送缓冲区大小，然后立即读取*/
	setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
	getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);
	printf("the tcp send buffer size after setting is %d\n", sendbuf);

	if(connect(sockfd, (struct sockaddr*)&server_address,
				sizeof(server_address))<0)
	{
		printf("connection failed\n");
	}
	else
	{
		char buffer[BUFFER_SIZE];
		memset(buffer, 'a', BUFFER_SIZE);
		send(sockfd, buffer, BUFFER_SIZE, 0);	
	}
       
	/*关闭socket*/
	close(sockfd);
	return 0;
}
