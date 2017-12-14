#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sendfile.h>

/*
高级的I/O函数，虽然不常用，但是在特定的条件下表现性能高

sendfile函数在两个文件描述符之间直接传递数据（完全在内核中操作），从而避免了内核缓冲区
和用户缓冲区之间的数据拷贝，效率高，被称为零拷贝

下面使用sendfile实现下载的功能

相当于实现了一个http服务器
*/
#define BUFF_SIZE 1024

static const char* status_line[2] = {"200 ok", "500 Internal server error"};

int main(int argc, char* argv[])
{

	if(argc <=3)
	{
		printf("usage:%s ip_address, port_number filename\n",
			basename(argv[0]) );
		return 1;
	}


	const char* ip = argv[1];

	int port = atoi(argv[2]);

	const char* file_name = argv[3];


	int filefd = open(file_name, O_RDONLY);
	assert(filefd>0);

	struct stat stat_buf;
	fstat(filefd ,&stat_buf);


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


	int ret = bind(sock , (struct sockaddr*)&address, sizeof(address));
	assert( ret != -1);

	ret = listen(sock, 5);  //设置内核监听队列的最大长度为5，其实一般是接收比这个值大1的
	assert( ret != -1);

	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);
	
	printf("start connect......\n");
	int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
	printf("end connect......\n");
	if(connfd<0)
	{
		printf("errno is :%d\n", errno);
	}else{
		sendfile(connfd, filefd, NULL, stat_buf.st_size);
		close(connfd);
	}

	/*关闭socket*/
	close(sock);
	return 0;
}
