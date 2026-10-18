#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    
void *work(void *arg){
    printf("hello from the thread\n");
    return NULL;
}
int main(){
    pthread_t t;
    pthread_create(&t,NULL,work,NULL);
    pthread_join(t,NULL);
    return 0;
}