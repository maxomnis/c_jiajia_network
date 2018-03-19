#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/*

*/
void* another()
{
       int n;
        printf("%d\n",n);
	printf("Thread #%u working on task1\n", (int)pthread_self());
}
int main()
{
  int i;
  for(i=0; i<5; i++)
  {
	pthread_t id;
 	pthread_create(&id, NULL, another, NULL);     
  }
  sleep(10);
  return 0;
}
