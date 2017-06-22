#include <stdio.h> 
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define MAX_BUF_SIZE 1000

/* device path */ 
char path[] = "/dev/jit0"; 
char buf[MAX_BUF_SIZE]={0,};
int num=0;
char *p=(char*)&num;

int main() 
{ 
    int f = open(path, O_RDWR); 
    int i=0;
    int ret=0;
    if (f == -1) 
    { 
        printf( "device open error!\n"); 
        return 1; 
    } 

    printf( "=====Read the data from device...\n"); 
    ret=read(f, buf, MAX_BUF_SIZE); /* device read */ 
    //printf( "read %d bytes\n ", ret); //On  success,  the number of bytes read is returned (zero indicates end of file)
    
    printf("buf=\n[\n %s ]\n",buf);

    close(f); 
}
