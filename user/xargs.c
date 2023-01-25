#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

//不知道输出在哪啊    find . b | xargs grep hello  sh < xargstest.sh
int main(int argc, char *argv[]){
    char *nargv[MAXARG];
    if(argc < 2){
        fprintf(2,"usage: xargs <command> arg");
    }

    for(int i=1;i<argc;i++){
        nargv[i-1] = argv[i];
        // printf("receive arg:%s\n",nargv[i-1]);
    }
    // printf("i am here\n");
    int cur = argc - 1; //下一个参数位置(第几个参数)
    char curpos[256];  
    nargv[cur] = curpos; 
    while(read(0,nargv[cur],1)){     //如何知道read到的值呢？
        // printf("read a byte %c\n",(*nargv[cur]));

        if((*nargv[cur])=='\n'){
            // printf("read a byte %c\n",(*nargv[cur]++));
            // printf("read a byte %c\n",(*nargv[cur]++));
            // printf("read a byte %c\n",(*nargv[cur]++));
            // printf("read a byte %c\n",(*nargv[cur]++));
            (*nargv[cur]) = '\0';
            nargv[cur] = curpos;
            // printf("receive arg:%s\n",nargv[cur]);
            
            if(fork()==0){
                exec(argv[1],nargv);
            }
            else{
                wait(0);
            }
        
        }
        else
        nargv[cur]++;
    }

    exit(0);       
}