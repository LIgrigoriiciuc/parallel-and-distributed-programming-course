#include <pthread.h>
#include <stdio.h>
void *work(void *arg){
    printf("hello from the thread\n");
    return NULL;
}
int main (void){
    pthread_t t;
    pthread_create(&t,NULL,work,NULL);
    pthread_join(t,NULL);
    return 0;
}