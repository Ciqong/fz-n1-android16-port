#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <dirent.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
static std::atomic<bool> running(true);
static std::mutex mx; static std::queue<std::pair<std::string,int>> jobs;
static int appuid;
static const char *qdir="/data/user/0/com.panasonic.mobile.userbuttonservice.compat/files/button-queue";
static void stop(int){running=false;}
static const char *button(int c){switch(c){case 158:return "button_a1";case 102:return "button_a2";case 139:return "button_a3";case 228:return "button_side_l";case 229:return "button_side_r";default:return nullptr;}}
static void execwait(const char *path,char *const av[]){int fd[2];if(pipe(fd))return;pid_t p=fork();if(p==0){close(fd[0]);dup2(fd[1],1);dup2(fd[1],2);close(fd[1]);execv(path,av);_exit(127);}close(fd[1]);char buf[512];ssize_t n;while((n=read(fd[0],buf,sizeof(buf)))>0)write(2,buf,n);close(fd[0]);if(p>0){int st;while(waitpid(p,&st,0)<0&&errno==EINTR){};}}
static void worker(){while(running){std::pair<std::string,int> j;bool got=false;{std::lock_guard<std::mutex> l(mx);if(!jobs.empty()){j=jobs.front();jobs.pop();got=true;}}if(got){fprintf(stderr,"button %s %s\n",j.first.c_str(),j.second?"down":"up");char *av[]={(char*)"am",(char*)"broadcast",(char*)"-a",(char*)"com.panasonic.toughpad.android.api.appbutton.intent.APPBUTTON_FOR_UBS",(char*)"-p",(char*)"com.panasonic.mobile.userbuttonservice.compat",(char*)"--es",(char*)"KEY_EVENT",(char*)j.first.c_str(),(char*)"--es",(char*)"com.panasonic.toughpad.android.api.appbutton.intent.EXTRA_APPBUTTON_STATE",(char*)(j.second?"down":"up"),nullptr};execwait("/system/bin/am",av);}else usleep(10000);}}
static void requests(){while(running){DIR *d=opendir(qdir);if(d){dirent *e;while((e=readdir(d))){std::string n=e->d_name;if(n.size()<4||n.substr(n.size()-4)!=".req")continue;std::string p=std::string(qdir)+"/"+n;int f=open(p.c_str(),O_RDONLY|O_NOFOLLOW);if(f<0)continue;struct stat s{};bool ok=fstat(f,&s)==0&&S_ISREG(s.st_mode)&&s.st_uid==appuid&&s.st_size>0&&s.st_size<5&&time(nullptr)-s.st_mtime<10;char b[8]={};int z=read(f,b,7);close(f);unlink(p.c_str());if(!ok||z<=0)continue;bool digits=true;for(int i=0;i<z;i++)if(b[i]<'0'||b[i]>'9')digits=false;int code=atoi(b);if(!digits||code<1||code>288)continue;fprintf(stderr,"inject keycode %d\n",code);char *av[]={(char*)"input",(char*)"keyevent",b,nullptr};execwait("/system/bin/input",av);}closedir(d);}usleep(20000);}}
int main(int argc,char **argv){if(argc!=2||(appuid=atoi(argv[1]))<10000)return 2;setbuf(stderr,nullptr);signal(SIGTERM,stop);signal(SIGINT,stop);int src=-1;char path[64],name[128];for(int i=0;i<32;i++){snprintf(path,sizeof(path),"/dev/input/event%d",i);int f=open(path,O_RDONLY|O_NONBLOCK);if(f<0)continue;memset(name,0,sizeof(name));ioctl(f,EVIOCGNAME(sizeof(name)),name);if(strcmp(name,"soc:gpio_keys")==0){src=f;break;}close(f);}if(src<0)return 3;unsigned long bits[(KEY_MAX+8*sizeof(long))/(8*sizeof(long))]={};if(ioctl(src,EVIOCGBIT(EV_KEY,sizeof(bits)),bits)<0)return 4;int virt=open("/dev/uinput",O_WRONLY|O_NONBLOCK);if(virt<0)return 5;if(ioctl(virt,UI_SET_EVBIT,EV_KEY)<0)return 6;ioctl(virt,UI_SET_EVBIT,EV_SYN);for(int c=0;c<=KEY_MAX;c++)if(bits[c/(8*sizeof(long))]&(1UL<<(c%(8*sizeof(long))))){if(!button(c)&&ioctl(virt,UI_SET_KEYBIT,c)<0)return 7;}uinput_user_dev u{};strcpy(u.name,"FZN1 normal keys passthrough");u.id.bustype=BUS_VIRTUAL;u.id.vendor=0x18d1;u.id.product=0xf001;write(virt,&u,sizeof(u));if(ioctl(virt,UI_DEV_CREATE)<0)return 8;usleep(200000);if(ioctl(src,EVIOCGRAB,1)<0){ioctl(virt,UI_DEV_DESTROY);return 9;}fprintf(stderr,"READY source=%s uid=%d\n",path,appuid);std::thread a(worker),b(requests);while(running){pollfd p{src,POLLIN,0};int rc=poll(&p,1,100);if(rc<0&&errno!=EINTR)break;if(rc<=0)continue;input_event e;if(read(src,&e,sizeof(e))!=sizeof(e))continue;const char *btn=e.type==EV_KEY?button(e.code):nullptr;if(btn){if(e.value==0||e.value==1){std::lock_guard<std::mutex> l(mx);if(jobs.size()<32)jobs.emplace(btn,e.value);}continue;}if(e.type==EV_SYN||e.type==EV_KEY)write(virt,&e,sizeof(e));}running=false;a.join();b.join();ioctl(src,EVIOCGRAB,0);ioctl(virt,UI_DEV_DESTROY);close(virt);close(src);return 0;}
