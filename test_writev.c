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

/*
readv,writev都是高级的I/O函数，虽然不常用，但是在特定的条件下表现性能高
readv 将数据从文件描述符读到分散的内存块中
writev函数将多个分散的内存数据一并写入到描述符中

下面使用writev实现下载的功能

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
		/*用于保存http应答的状态行,头部字段和一个空行的缓存区*/
		char header_buf[BUFF_SIZE];
		// void *memset(void *s, int ch, size_t n);
		// 将s中当前位置后面的n个字节 （typedef unsigned int size_t ）用 ch 替换并返回 s 。
		// 作用是在一段内存块中填充某个给定的值，它是对较大的结构体或数组进行清零操作的一种最快方法
		memset(header_buf, '\0', BUFF_SIZE);
	
		/*用于存放目标文件内容的应用程序缓存*/
		char* file_buf;
		
		/*用于存放目标文件的属性，比如是否为目录，文件大小等*/
		struct stat file_stat;

		/*记录目标文件是否是有效文件*/
		bool valid = true;

		/*缓存区header_buf目前已经使用了多少字节的空间*/
		int len =0 ;

		

		// 函数说明：stat()用来将参数file_name 所指的文件状态, 复制到参数buf 所指的结构中
		if( stat(file_name, &file_stat)<0 )
		{
			printf("errno is:%d\n", errno);
			valid = false;
		}
		else
		{
			/*目标文件是一个目录*/
			if(S_ISDIR(file_stat.st_mode))  
			{
			  
			  valid = false;
			}
			else if (file_stat.st_mode & S_IROTH) //当前用户有读取目标文件的权限
			{
			  /*
				动态分配缓存区file_buf, 并指定其大小大小为目标文件的大小file_stat.st_size加1,然后将目标文件读入缓存区file_buf中
			  */

			 int fd = open(file_name, O_RDONLY);
			 file_buf = new char[file_stat.st_size + 1];
			 memset(file_buf, '\0', file_stat.st_size+1);

				if(read(fd, file_buf, file_stat.st_size)<0)
				{
					printf("errno is:%d\n", errno);
					valid = false;
				}
			}
			else
			{
			   printf("errno is:%d\n", errno);
			   valid = false;
			}
		}
		//如果目标文件有效,则发送正常的http应答
		if(valid)
		{
			/*
			 下面这部分将http应答的状态行,"Content-length"头部字段和一个空行依次加入header_buf中
			*/
				
		       ret = snprintf(header_buf, BUFF_SIZE-1, "%s %s\r\n", "HTTP/1.1", status_line[0]);
		       
		       len += ret;
		       ret = snprintf(header_buf+len, BUFF_SIZE-1-len, "Content-Length:%ld\r\n", file_stat.st_size);

		       len += ret;
		       ret = snprintf(header_buf + len, BUFF_SIZE-1-len, "%s", "\r\n");
			
		       //利用writev将header_buf和file_buf的内容一并写出
		       struct iovec iv[2];
			iv[0].iov_base = header_buf;
			iv[0].iov_len = strlen(header_buf);
			iv[1].iov_base = file_buf;
			iv[1].iov_len = file_stat.st_size;
			ret = writev(connfd, iv, 2); 
		       
		}
		else
		{
			ret = snprintf(header_buf, BUFF_SIZE-1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
			len +=ret;
			ret = sprintf(header_buf + len, header_buf-1-len, "%s", "\r\n");
			send(connfd, header_buf, strlen(header_buf), 0);
		}
		close(connfd);
		delete [] file_buf;
	}
	/*关闭socket*/
	close(sock);
	return 0;
}
