#include <stdio.h> 
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/* device path */ 
char path[] = "/dev/hello0"; 
char buf[128]={0,};
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
    ret=read(f, buf, 128); /* device read */ 
    printf( "read %d bytes\n ", ret);
    //printf( "%u\n ", buf);
    //ok
    //for (;i<4;++i)
    //{
    //    printf("buf[%d]=%x ",i,buf[i]);
    //    p[i]=buf[i];
    //}
    //printf("num=0x%x\n",num);
    //printf("\n");
    
    for (i=0;i<127;++i)
    {
        printf("buf[%d]=0x%x ",i,buf[i]);
    }
    printf("\n");
    printf("buf=%s\n",buf);
    
    num=0xdeadbeef;
    printf( "=====write integer 0x%x to device \n", num); 
    write(f, &num, 4); /* device wirte */ 
    
    printf( "=====Read the data from device again...\n"); 
    read(f, buf, 4); /* device read */ 
    for (i=0;i<4;++i)
    {
        printf("buf[%d]=%.2x ",i,buf[i]);
        p[i]=buf[i];
    }
    printf("\n");
    printf("num=0x%x\n",num);
    
    close(f); 
}
