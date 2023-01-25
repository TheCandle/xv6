#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void get_child(int p[2]){
    int get_prime;
    close(p[1]);
    if(read(p[0],&get_prime,4)!=4){
        fprintf(2,"read failed in %d",getpid());
        exit(1);
    }
    printf("prime %d\n",get_prime);
    int n;
    int flag = read(p[0],&n,4);
    if(flag){
        int newp[2];
        pipe(newp);
        if(fork()==0){
            get_child(newp);
        }
        else{
            if(n%get_prime){
                write(newp[1],&n,4);
            }
            while(read(p[0],&n,4)){
                if(n%get_prime){
                    write(newp[1],&n,4);
                }
            }
            close(p[0]);
            close(newp[1]);
            close(newp[0]);
            wait(0);
        }
    }
    exit(0);
}

int main(int argc, char *argv[]){
    
    int p[2];
    pipe(p);            //难点在于进程之间的通信，没有参数啊，实现是通过Pipe读完后没有数据返回0
    if(fork()==0){
        get_child(p);
    }                       //并行的关键就在于我在做这个的同时，你也在工作
    else{
        close(p[0]);
        for(int i = 2 ;i<=35;i++){
            if(write(p[1],&i,4)!=4){
                fprintf(2,"write failed in %d",getpid());
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
        exit(0);
    }
    return 0;
}