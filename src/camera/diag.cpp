#include <jni.h>
#include <pthread.h>
#include <atomic>
#include <link.h>
#include <cstring>
#include <unistd.h>
#include <android/log.h>
extern "C" void* g_real_syms[];
#define PLOG(...) __android_log_print(4,"BRPREVIEW",__VA_ARGS__)
struct SinkSp {void*p;SinkSp():p(nullptr){}SinkSp(const SinkSp&o):p(o.p){}~SinkSp(){};};
static std::atomic<bool> drain_running{false};
static JavaVM* sink_vm;
static jobject sink_reader;
static jobject sink_surface;
static SinkSp native_surface, native_producer;
static uintptr_t runtime_base,gui_base;
static int scan_libs(dl_phdr_info*i,size_t,void*) {
 if(!i->dlpi_name || !strstr(i->dlpi_name,"/system/"))return 0;
 if(strstr(i->dlpi_name,"libandroid_runtime.so"))runtime_base=i->dlpi_addr;
 if(strstr(i->dlpi_name,"libgui.so"))gui_base=i->dlpi_addr;
 return 0;
}
static bool ex(JNIEnv*e,const char*step){if(!e->ExceptionCheck())return false;PLOG("JNI fail %s",step);e->ExceptionDescribe();e->ExceptionClear();return true;}
static void* drain(void*) {
 JNIEnv*e=nullptr;
 if(sink_vm->AttachCurrentThread(&e,nullptr)!=JNI_OK){drain_running=false;return nullptr;}
 jclass c=e->FindClass("android/media/ImageReader");
 jclass image=e->FindClass("android/media/Image");
 jmethodID acquire=e->GetMethodID(c,"acquireLatestImage","()Landroid/media/Image;");
 jmethodID close=e->GetMethodID(image,"close","()V");
 if(ex(e,"drain lookup")||!acquire||!close){sink_vm->DetachCurrentThread();drain_running=false;return nullptr;}
 int frames=0;
 // Diagnostic worker only drains/discards the preview sink, never saves images.
 for(int i=0;i<1800;i++){
  jobject im=e->CallObjectMethod(sink_reader,acquire);
  if(ex(e,"acquireLatestImage"))break;
  if(im){frames++;e->CallVoidMethod(im,close);e->DeleteLocalRef(im);if(frames<=3||frames%30==0)PLOG("sink frames=%d",frames);if(ex(e,"Image.close"))break;}
  usleep(33333);
 }
 PLOG("drain finished frames=%d",frames);
 sink_vm->DetachCurrentThread();drain_running=false;return nullptr;
}
static int ensure_preview(void*cam) {
 JNIEnv*e=nullptr;jsize n=0;
 if(!sink_reader){
  if(JNI_GetCreatedJavaVMs(&sink_vm,1,&n)!=JNI_OK||!sink_vm||!n)return -1;
  bool attached=false;
  if(sink_vm->GetEnv((void**)&e,JNI_VERSION_1_6)!=JNI_OK){if(sink_vm->AttachCurrentThread(&e,nullptr)!=JNI_OK)return -2;attached=true;}
  jclass c=e->FindClass("android/media/ImageReader");if(ex(e,"ImageReader class")||!c)return -3;
  jmethodID make=e->GetStaticMethodID(c,"newInstance","(IIII)Landroid/media/ImageReader;");
  jmethodID surface=e->GetMethodID(c,"getSurface","()Landroid/view/Surface;");
  if(ex(e,"ImageReader methods")||!make||!surface)return -4;
  jobject local=e->CallStaticObjectMethod(c,make,1360,960,34,4);
  if(ex(e,"newInstance PRIVATE")||!local)return -5;
  jobject surf=e->CallObjectMethod(local,surface);if(ex(e,"getSurface")||!surf)return -6;
  dl_iterate_phdr(scan_libs,nullptr);if(!runtime_base||!gui_base)return -7;
  // Offsets and x8 non-trivial return convention verified against this A16 build.
  native_surface=((SinkSp(*)(JNIEnv*,jobject))(runtime_base+0x12e8bc))(e,surf);
  if(!native_surface.p)return -8;
  native_producer=((SinkSp(*)(void*))(gui_base+0xca814))(native_surface.p);
  if(!native_producer.p)return -9;
  sink_reader=e->NewGlobalRef(local);sink_surface=e->NewGlobalRef(surf);
  e->DeleteLocalRef(surf);e->DeleteLocalRef(local);e->DeleteLocalRef(c);
  if(attached)sink_vm->DetachCurrentThread();
  
 }
 if(!drain_running.exchange(true)){pthread_t t;if(pthread_create(&t,nullptr,drain,nullptr)==0)pthread_detach(t);else drain_running=false;}
 int rc=((int(*)(void*,const void*))g_real_syms[11])(cam,&native_producer);
 PLOG("setPreviewTarget PRIVATE sink rc=%d",rc);
 return rc;
}

#include <android/log.h>
extern "C" void *g_real_syms[];
#define L(...) __android_log_print(4,"BRDIAG",__VA_ARGS__)
extern "C" int _ZN7android6Camera11sendCommandEiii(void* p,int c,int a,int b){
 int r=((int(*)(void*,int,int,int))g_real_syms[4])(p,c,a,b); L("sendCommand cmd=%d arg1=%d arg2=%d rc=%d",c,a,b,r);return r;
}
extern "C" int _ZN7android6Camera13setParametersERKNS_7String8E(void*p,const void*s){
 int r=((int(*)(void*,const void*))g_real_syms[8])(p,s);L("setParameters rc=%d",r);return r;
}
extern "C" int _ZN7android6Camera12startPreviewEv(void*p){int sink=ensure_preview(p);if(sink){L("preview sink failed rc=%d",sink);return sink;}int r=((int(*)(void*))g_real_syms[7])(p);L("startPreview rc=%d",r);return r;}
struct S8 {char*p;S8():p(nullptr){}S8(const S8&o):p(o.p){}~S8(){};};
extern "C" S8 _ZNK7android6Camera13getParametersEv(void*p){S8 r=((S8(*)(void*))g_real_syms[19])(p);L("getParameters %.1800s",r.p?r.p:"<null>");return r;}

#include <link.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
static void *orig[5];
static int tr_open(void*p){int r=((int(*)(void*))orig[0])(p);L("SDLAPI_Open rc=%d",r);return r;}
static int tr_attach(void*p){auto lev=(void(*)(int,int))dlsym(RTLD_DEFAULT,"dbgSetLevel");if(lev)lev(0,100);int r=((int(*)(void*))orig[1])(p);L("SDLAPI_Attach rc=%d",r);return r;}
static int tr_start(){int r=((int(*)())orig[2])();L("SDLAPI_StartScan rc=%d",r);return r;}
static int tr_set(int id,unsigned char t,void*p){int r=((int(*)(int,unsigned char,void*))orig[3])(id,t,p);L("SDLAPI_SetParameter id=%d type=%u rc=%d",id,t,r);return r;}
static int tr_get(int id,unsigned char*t,void*p){int r=((int(*)(int,unsigned char*,void*))orig[4])(id,t,p);L("SDLAPI_GetParameter id=%d rc=%d",id,r);return r;}
static int hook_bcr(dl_phdr_info*i,size_t,void*){
 if(!i->dlpi_name||!strstr(i->dlpi_name,"libbarcodereader80.so"))return 0;
 const char*n[]={"SDLAPI_Open","SDLAPI_Attach","SDLAPI_StartScan","SDLAPI_SetParameter","SDLAPI_GetParameter"};
 const uintptr_t off[]={0xce58,0xce70,0xcec0,0xcde0,0xcdd8};
 void*h[]={ (void*)tr_open,(void*)tr_attach,(void*)tr_start,(void*)tr_set,(void*)tr_get};
 uintptr_t base=i->dlpi_addr, pg=(base+off[0])&~(uintptr_t)(getpagesize()-1);
 for(int k=0;k<5;k++){orig[k]=dlsym(RTLD_DEFAULT,n[k]);if(!orig[k]){L("SDLtrace unresolved %s",n[k]);return 1;}}
 if(mprotect((void*)pg,getpagesize(),PROT_READ|PROT_WRITE)){L("SDLtrace mprotect failed");return 1;}
 for(int k=0;k<5;k++)*(void**)(base+off[k])=h[k];
 mprotect((void*)pg,getpagesize(),PROT_READ);
 L("SDLtrace installed in %s",i->dlpi_name);return 1;
}
__attribute__((constructor)) static void trace_init(){dl_iterate_phdr(hook_bcr,nullptr);}

#include <cstdarg>
#include <cstdio>
static void tr_dbg(const char*f,...){char b[1100];va_list a;va_start(a,f);vsnprintf(b,sizeof b,f,a);va_end(a);__android_log_print(4,"SDLDBG","%s",b);}
static int hook_dbg(dl_phdr_info*i,size_t,void*){
 if(!i->dlpi_name)return 0;
 uintptr_t off=0;
 if(strstr(i->dlpi_name,"/libIAL.so"))off=0x56b50;
 if(strstr(i->dlpi_name,"/libSDL.so"))off=0x33e408;
 if(!off)return 0;
 uintptr_t addr=i->dlpi_addr+off,pg=addr&~(uintptr_t)(getpagesize()-1);
 if(mprotect((void*)pg,getpagesize(),PROT_READ|PROT_WRITE)==0){*(void**)addr=(void*)tr_dbg;mprotect((void*)pg,getpagesize(),PROT_READ);L("debug hook %s",i->dlpi_name);}
 return 0;
}
__attribute__((constructor)) static void dbg_init(){dl_iterate_phdr(hook_dbg,nullptr);}

// Stock HAL1 BcrEnablerImpl::stopPreview returns 0 for the BCR device;
// QCamera2HardwareInterface::stop_preview skips its ordinary stop path then.
// Preserve that session behaviour in HAL3: decoder stop must not power down
// CAMSS before se47xx's following ARM-acquisition-stop ioctl resets VFE.
// This does NOT intercept disconnect/destruction, or change the engine command.
static std::atomic<void*> bcr_cam{nullptr};
static std::atomic<bool> stop_pending{false}, arm_stop_done{false}, cold_resume{false};
static std::atomic<int> last_live_flags{7};
extern "C" void br_register_camera(void* cam,int id) {
 if(!cam)return;
 if(id==2){bcr_cam.store(cam);stop_pending=false;arm_stop_done=false;cold_resume=false;}
 else {void* expected=cam;bcr_cam.compare_exchange_strong(expected,nullptr);}
 L("camera registered id=%d ptr=%p",id,cam);
}
extern "C" void _ZN7android6Camera11stopPreviewEv(void* cam) {
 if(cam && cam==bcr_cam.load()) {
  stop_pending=true;L("BCR stop deferred through engine reset AND decode-mode script");
  return;
 }
 ((void(*)(void*))g_real_syms[6])(cam);
 L("ordinary camera stopPreview forwarded");
}

using CmdFn=int(*)(void*,const void*,int,void*,int*,unsigned,int);
static CmdFn original_command;
static thread_local bool stopping_ioctl_scope=false;
extern "C" void _ZN7android6Camera23setPreviewCallbackFlagsEi(void* cam,int flags) {
 if(cam && cam==bcr_cam.load() && flags)last_live_flags=flags;
 ((void(*)(void*,int))g_real_syms[12])(cam,flags);
 L("setPreviewCallbackFlags flags=%d",flags);
}
static int ordered_command(void* dev,const void* cmd,int len,void* response,int* response_len,unsigned timeout,int flags) {
 const unsigned char* b=(const unsigned char*)cmd;
 bool pending=stop_pending.load(); bool arm_start=b && len>=2 && b[0]==0x58 && b[1]==0 && !pending;
 bool arm_stop=b && len>=2 && b[0]==0x58 && pending;
 bool decode_script=b && len>=2 && b[0]==0x77 && b[1]==3;
 void* cam=bcr_cam.load();
 if(arm_start && cam && cold_resume.exchange(false)) {
  L("ORDER resume preview BEFORE engine ARM start");
  int sink=ensure_preview(cam);
  if(!sink){((void(*)(void*,int))g_real_syms[12])(cam,last_live_flags.load());
   int rc=((int(*)(void*))g_real_syms[7])(cam);L("ORDER resume rc=%d",rc);}
  else L("ORDER resume sink failed %d",sink);
 }
 bool old_scope=stopping_ioctl_scope;stopping_ioctl_scope=arm_stop;
 int rc=original_command(dev,cmd,len,response,response_len,timeout,flags);
 stopping_ioctl_scope=old_scope;
 if(arm_start || arm_stop || decode_script)L("ORDER command %02x %02x rc=%d pending=%d",b[0],b[1],rc,(int)stop_pending.load());
 if(arm_stop && stop_pending.load() && rc==0)arm_stop_done=true;
 // No driver command is changed. Wait for both stock stop/reset and the
 // subsequent decode-mode script to return before stopping the HAL3 stream.
 if(decode_script && cam && rc==0 && arm_stop_done.load() && stop_pending.exchange(false)) {
  L("ORDER engine reset + mode script returned; now stop HAL3 preview");
  ((void(*)(void*))g_real_syms[6])(cam);
  arm_stop_done=false;cold_resume=true;
 }
 return rc;
}
static int hook_command(dl_phdr_info*i,size_t,void*) {
 if(!i->dlpi_name || !strstr(i->dlpi_name,"/libSDL.so"))return 0;
 uintptr_t addr=i->dlpi_addr+0x33fcb8,pg=addr&~(uintptr_t)(getpagesize()-1);
 original_command=(CmdFn)dlsym(RTLD_DEFAULT,"IAL_CommandDevice");
 if(!original_command){L("ORDER unresolved IAL_CommandDevice");return 1;}
 if(mprotect((void*)pg,getpagesize(),PROT_READ|PROT_WRITE)){L("ORDER GOT mprotect failed");return 1;}
 *(void**)addr=(void*)ordered_command;mprotect((void*)pg,getpagesize(),PROT_READ);
 L("ORDER IAL command wrapper installed");return 1;
}
__attribute__((constructor))static void order_init(){dl_iterate_phdr(hook_command,nullptr);}

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <cstdlib>
using IoctlFn=int(*)(int,int,void*);
static IoctlFn original_ioctl;
static bool eligible_stop_ioctl(int fd,int request,void* arg) {
 if(!stopping_ioctl_scope || request!=0x707 || !arg)return false;
 const i2c_rdwr_ioctl_data* data=(const i2c_rdwr_ioctl_data*)arg;
 if(data->nmsgs!=1 || !data->msgs || data->msgs[0].addr!=0x5c)return false;
 char path[80],target[160];snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);
 ssize_t n=readlink(path,target,sizeof(target)-1);if(n<0)return false;target[n]=0;
 return strcmp(target,"/dev/moto_sdl")==0;
}
static bool clear_stop_state() {
 const char* path="/proc/se47xx/stop_acq_going";
 FILE* f=fopen(path,"rb");if(!f){L("STATE open read failed");return false;}
 char buf[24]={};size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);
 if(!n){L("STATE empty read");return false;}
 char* end=nullptr;long value=strtol(buf,&end,16);
 if(end==buf || (value!=0 && value!=1)){L("STATE unexpected value, no write");return false;}
 if(value==0){L("STATE already idle 0");return true;}
 f=fopen(path,"wb");if(!f){L("STATE open write failed");return false;}
 bool ok=fwrite("0",1,1,f)==1;int close_rc=fclose(f);
 L("STATE stop_acq_going 1->0 before stock stop ioctl: write=%d close=%d",(int)ok,close_rc);
 return ok && close_rc==0;
}
static int guarded_ioctl(int fd,int request,void* arg) {
 if(eligible_stop_ioctl(fd,request,arg))clear_stop_state();
 return original_ioctl(fd,request,arg);
}
static int hook_ioctl(dl_phdr_info*i,size_t,void*) {
 if(!i->dlpi_name || !strstr(i->dlpi_name,"/libIAL.so"))return 0;
 uintptr_t addr=i->dlpi_addr+0x56ca8,pg=addr&~(uintptr_t)(getpagesize()-1);
 original_ioctl=(IoctlFn)dlsym(RTLD_DEFAULT,"ioctl");if(!original_ioctl){L("STATE ioctl unresolved");return 1;}
 if(mprotect((void*)pg,getpagesize(),PROT_READ|PROT_WRITE)){L("STATE GOT protection failed");return 1;}
 *(void**)addr=(void*)guarded_ioctl;mprotect((void*)pg,getpagesize(),PROT_READ);
 L("STATE stop-only ioctl wrapper installed");return 1;
}
__attribute__((constructor))static void state_init(){dl_iterate_phdr(hook_ioctl,nullptr);}
