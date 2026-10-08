#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
extern int fzn1_byte_memcmp(const void *, const void *, size_t);
static int sign(int v) { return (v>0)-(v<0); }
int main(int argc, char **argv) {
    unsigned char a[544], b[544];
    for(int i=0;i<544;i++) a[i]=b[i]=(unsigned char)(i*37+11);
    for(int x=0;x<16;x++) for(int y=0;y<16;y++) for(int n=0;n<=512;n++) {
        if(sign(fzn1_byte_memcmp(a+x,b+y,n))!=sign(memcmp(a+x,b+y,n))) return 1;
    }
    for(int i=0;i<512;i++) { b[i]^=0xff; if(sign(fzn1_byte_memcmp(a,b,512))!=sign(memcmp(a,b,512))) return 2; b[i]^=0xff; }
    puts("normal-buffer-test-pass");
    if(argc<2) return 0;
    int fd=open("/dev/cfg2",O_RDONLY);
    if(fd<0) {perror("open cfg2");return 3;}
    volatile unsigned char *p=mmap(NULL,0x20000,PROT_READ,MAP_SHARED,fd,0);
    close(fd);
    if(p==MAP_FAILED) {perror("mmap cfg2");return 4;}
    for(int offset=0;offset<16;offset++) {
        for(int i=0;i<512;i++) b[i]=p[0x6200+offset+i];
        if(fzn1_byte_memcmp((const void *)(p+0x6200+offset),b,512)!=0) return 5;
    }
    munmap((void *)p,0x20000);
    puts("device-mapping-readonly-test-pass");
    return 0;
}
