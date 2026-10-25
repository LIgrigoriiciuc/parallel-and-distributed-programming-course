#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    long long* buf;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} queue;

void queue_init(queue* q, size_t capacity){
    q->buf=malloc(sizeof(long long)*capacity);
    if (!q->buf) exit(1);
    q->count=q->head=q->tail=0;
    q->capacity=capacity;
    pthread_mutex_init(&q->mutex,NULL);
    pthread_cond_init(&q->not_full,NULL);
    pthread_cond_init(&q->not_empty,NULL);
}

void queue_destroy(queue* q){
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

void queue_push(queue* q, long long value){
    pthread_mutex_lock(&q->mutex);
    while(q->capacity == q->count)
        pthread_cond_wait(&q->not_full,&q->mutex);
    q->buf[q->tail]=value;
    q->tail=(q->tail+1)%q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

long long queue_pop(queue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    long long value = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return value;
}

typedef struct{
    queue* q;
    int* a;
    int* b;
    size_t n;
    long long result;
} thread_args;

void* producer(void* arg){
    thread_args* t = arg;
    for (size_t i = 0; i < t->n; i++)
        queue_push(t->q, (long long)t->a[i] * t->b[i]);
    return NULL;
}

void* consumer(void* arg) {
    thread_args* t = arg;
    long long sum = 0;
    for (size_t i = 0; i < t->n; i++)
        sum += queue_pop(t->q);
    t->result = sum;
    return NULL;
}

double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
    fprintf(stderr, "Usage: %s <vector_size> <queue_size> <seed>\n", argv[0]);
    exit(1);
    }
    size_t n = atoi(argv[1]);
    size_t queue_size = atoi(argv[2]);
    unsigned int seed = atoi(argv[3]);
    int *a = malloc(n * sizeof(int));
    int *b = malloc(n * sizeof(int));
    if (!a || !b) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    srand(seed);
    for (size_t i = 0; i < n; i++) {
        a[i] = rand() % 1000001;        
        if (rand() % 2) a[i] = -a[i];      
        b[i] = rand() % 1000001;
        if (rand() % 2) b[i] = -b[i];
    }
    queue q;
    queue_init(&q,queue_size);
    thread_args prod_args = {&q,a,b,n,0};
    thread_args cons_args = {&q,a,b,n,0};
    pthread_t tp, tc;
    double start = now_ms();
    pthread_create(&tp, NULL, producer, &prod_args);
    pthread_create(&tc, NULL, consumer, &cons_args);
    pthread_join(tp, NULL);
    pthread_join(tc, NULL);
    double elapsed = now_ms() - start;
    long long expected = 0;
    for (size_t i = 0; i < n; i++)
        expected += (long long)a[i] * b[i];
    printf("n=%zu queue=%zu result=%lld %s time=%.2f ms\n",
           n, queue_size, cons_args.result,
           cons_args.result == expected ? "(OK)" : "(MISMATCH!)",
           elapsed);
    queue_destroy(&q);
    free(a);
    free(b);
    return cons_args.result == expected ? 0 : 1;
}
