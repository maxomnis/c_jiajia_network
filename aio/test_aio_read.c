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
#define BUFFER_SIZE 1024

int MAX_LIST = 2;

int main(int argc,char **argv)
{
    /*

        struct aiocb
        {
            //要异步操作的文件描述符
            int aio_fildes;
            //用于lio操作时选择操作何种异步I/O类型
            int aio_lio_opcode;
            //异步读或写的缓冲区的缓冲区
            volatile void *aio_buf;
            //异步读或写的字节数
            size_t aio_nbytes;
            //异步通知的结构体
            struct sigevent aio_sigevent;
        }
    */
    //aio操作所需结构体
    struct aiocb rd;

    int fd,ret,couter;

    fd = open("test.txt",O_RDONLY);
    if(fd < 0)
    {
        perror("test.txt");
    }



    //将rd结构体清空
    bzero(&rd,sizeof(rd));


    //为rd.aio_buf分配空间
    rd.aio_buf = malloc(BUFFER_SIZE + 1);

    //填充rd结构体
    rd.aio_fildes = fd;
    rd.aio_nbytes =  BUFFER_SIZE;
    rd.aio_offset = 0;

    //进行异步读操作
    ret = aio_read(&rd);
    if(ret < 0)
    {
        perror("aio_read");
        exit(1);
    }

    couter = 0;
   //  循环等待异步读操作结束
    /*
        aio_error
        aio_error 函数被用来确定请求的状态。其原型如下：
        1
        int aio_error( struct aiocb *aiocbp );
        这个函数可以返回以下内容：
        EINPROGRESS，说明请求尚未完成
        ECANCELLED，说明请求被应用程序取消了
        -1，说明发生了错误，具体错误原因可以查阅 errno
    */
    while(aio_error(&rd) == EINPROGRESS)
    {
        printf("第%d次\n",++couter);
    }
    //获取异步读返回值
    ret = aio_return(&rd);

    printf("\n\n返回值为:%d",ret);


    return 0;
}