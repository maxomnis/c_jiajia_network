#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<fcntl.h>
#include<aio.h>

/*
同步读写：应用程序发起读写后，等到读写函数完成返回，才能继续跑后面的代码。 
异步读写：应用程序发起读写后，将读写注册到队列，然后立马返回，
应用程序继续跑后面的代码，速度非常快，当读写动作完成后，系统发消息通知应用程序，然后应用程序接收读写结果。
*/
#define BUFFER_SIZE 1025

int main(int argc,char **argv)
{
    //定义aio控制块结构体
    struct aiocb wr;

    int ret,fd;

    char str[20] = {"hello,world"};

    //置零wr结构体
    bzero(&wr,sizeof(wr));

    fd = open("test.txt",O_WRONLY | O_APPEND);
    if(fd < 0)
    {
        perror("test.txt");
    }

    //为aio.buf申请空间
    wr.aio_buf = (char *)malloc(BUFFER_SIZE);
    if(wr.aio_buf == NULL)
    {
        perror("buf");
    }

    wr.aio_buf = str;

    //填充aiocb结构
    wr.aio_fildes = fd;
    wr.aio_nbytes = 1024;

    //异步写操作
    ret = aio_write(&wr);
    if(ret < 0)
    {
        perror("aio_write");
    }

    //等待异步写完成
    while(aio_error(&wr) == EINPROGRESS)
    {
        printf("hello,world\n");
    }

    //获得异步写的返回值
    ret = aio_return(&wr);
    printf("\n\n\n返回值为:%d\n",ret);

    return 0;
}