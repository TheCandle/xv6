#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int p[2];
    pipe(p);
    if(fork()==0){
        char bufC[2];
        if(read(p[0],bufC,1)!=1){
            fprintf(2,"read failed in child\n");
            
            exit(1); 
        }
        close(p[0]);
        fprintf(1,"%d: received ping\n",getpid());
        if(write(p[1],bufC,1)!=1){
            fprintf(2,"write failed in child\n");

            exit(1);
        }
        close(p[1]);
        exit(0);

    }
    else{
        char bufP[2]="a";

        if(write(p[1],bufP,1)!=1){
            fprintf(2,"write failed in parent\n");
            exit(1);
        }
        close(p[1]);
        wait(0);

        if(read(p[0],bufP,1)!=1){
            fprintf(2,"read failed in parent\n");
            exit(1);
        }
        close(p[0]);
        fprintf(1,"%d: received pong\n",getpid());
        exit(0);
    }
}