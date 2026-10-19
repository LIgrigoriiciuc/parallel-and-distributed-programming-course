#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

typedef uint64_t u64;
typedef struct {
    u64 value;
    pthread_mutex_t lock;
    int in1,in2; //ids of the two vars it sums are for the secondary variables only
    int n_affected;
    int *affected; //affected id list are for the primary variables only
    //primary = nobody computes it, it's the original input data
    int *mult; //mult[k] is number of paths from current to affected[k]
} Var;

Var *vars;
int N; //total number of variables
int NP; //number of primary variables
int use_global; //0=one mutex per variable fine grained
int workers_done = 0;
pthread_mutex_t done_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

void build_graph(){
    vars = calloc(N, sizeof(Var));
    for (int i = 0; i<N;i++)
        pthread_mutex_init(&vars[i].lock,NULL);
    for (int i = 0 ; i<NP;i++)
        vars[i].value = rand()%1000;
    for (int i = NP; i<N;i++){
        vars[i].in1=rand()%i;
        do {
            vars[i].in2=rand()%i;
        } while (vars[i].in1 == vars[i].in2);
        vars[i].value = vars[vars[i].in1].value+vars[vars[i].in2].value;
    }
    //paths[p] = 1; paths[s] = paths[in1] + paths[in2]
    int* paths = malloc(N*sizeof(int));
    for (int p=0;p<NP;p++){
        memset(paths,0,N*sizeof(int));
        paths[p]=1;
        int cnt=0;
        for (int s=NP;s<N;s++){
            int c = paths[vars[s].in1]+paths[vars[s].in2];
            paths[s]=c;
            if (c) cnt++;
        }
        vars[p].n_affected=cnt;
        vars[p].affected=malloc(cnt*sizeof(int));
        vars[p].mult=malloc(cnt*sizeof(int));
        int k=0;
        for(int s=NP;s<N;s++){
            if(paths[s]){
                vars[p].affected[k]=s;
                vars[p].mult[k]=paths[s];
                k++;
            }
        }
    }
    free(paths);
}

void update(int pid, u64 new_value){
    Var *p = &vars[pid];
    if (use_global) pthread_mutex_lock(&global_lock);
    else pthread_mutex_lock(&p->lock);
    u64 delta = new_value - p->value;
    p->value = new_value;
    for(int k=0;k<p->n_affected;k++){
        int s = p->affected[k];
        u64 d = delta * p->mult[k];
        if (!use_global)
        pthread_mutex_lock(&vars[s].lock);
        vars[s].value += d;
        if (!use_global)
        pthread_mutex_unlock(&vars[s].lock);
    }
    if (use_global) pthread_mutex_unlock(&global_lock);
    else pthread_mutex_unlock(&p->lock);
}

int check(){
    if (use_global) pthread_mutex_lock(&global_lock);
    else {
        for (int i=0;i<N;i++)
            pthread_mutex_lock(&vars[i].lock);
    }
    for (int i=NP;i<N;i++){
        u64 v = vars[vars[i].in1].value + vars[vars[i].in2].value;
        if (v != vars[i].value){
            printf("check failed for %d: %llu != %llu + %llu\n",i,(unsigned long long)vars[i].value,(unsigned long long)vars[vars[i].in1].value,(unsigned long long)vars[vars[i].in2].value);
            exit(1);
        }
    }
    if (use_global) pthread_mutex_unlock(&global_lock);
    else {
        for (int i=0;i<N;i++)
            pthread_mutex_unlock(&vars[i].lock);
    }
    return 1;
}

typedef struct {
    int ops; //how many updates the thread will do
    unsigned int seed;
} ThreadArgs;

void *thread_func(void *arg){
    ThreadArgs *args = (ThreadArgs*)arg;
    for (int i=0;i<args->ops;i++){
        int pid = rand_r(&args->seed)%NP;
        u64 new_value = rand_r(&args->seed)%1000;
        update(pid,new_value);
    }
    pthread_mutex_lock(&done_lock);
    workers_done++;
    pthread_mutex_unlock(&done_lock);
    return NULL;
}
    
int main(int argc, char** argv){
    int threads=4,ops=100000;
    N=1000;NP=500; use_global=0;
    if(argc > 1) threads=atoi(argv[1]);
    if (argc >2) ops = atoi(argv[2]);
    if (argc > 3) N = atoi(argv[3]);
    if (argc > 4) NP = atoi(argv[4]);
    if (argc>5) use_global=(strcmp(argv[5], "global_lock") == 0);
    if (threads < 1 || ops < 1 || NP < 2 || N <= NP){
        fprintf(stderr, "bad arguments (need threads>=1, ops>=1, 2<=primary<vars)\n");
        return 1;
    }
    srand(42); //worker threads won't use this, they have 1000,1001 etc.
    build_graph();
    if (!check()) {
        printf("check failed after build_graph\n");
        return 1;
    }
    pthread_t *tids = malloc(threads*sizeof(pthread_t));
    ThreadArgs *args = malloc(threads*sizeof(ThreadArgs));
    struct timespec start, end; //built-in struct
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < threads; i++){
        args[i].ops=ops;
        args[i].seed=1000+i;
        pthread_create(&tids[i],NULL,thread_func,&args[i]);
    }
    int tick=0;
    while(1){
        pthread_mutex_lock(&done_lock);
        if (workers_done>=threads)
            {
                pthread_mutex_unlock(&done_lock);
                break;
            }
        pthread_mutex_unlock(&done_lock);
        usleep(1000);
        if(++tick%5==0)check(); //so about every 5 ms
    }
    for (int i = 0; i < threads; i++)
        pthread_join(tids[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    check();
    printf("threads=%d ops=%d vars=%d primary=%d mode=%s time=%.3f s\n",
           threads, ops, N, NP, use_global ? "global" : "fine", secs);
    printf("PASS\n");
    free(tids);
    free(args);
    return 0;

}