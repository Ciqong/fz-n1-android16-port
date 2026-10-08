#include <atomic>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef HOST_TEST
#include <android/log.h>
#include <dlfcn.h>
#include <android/dlext.h>
#include <fcntl.h>
#define LOG(...) __android_log_print(4, "BCRHALGATE", __VA_ARGS__)
#else
#include <stdio.h>
#define LOG(...) ((void)0)
#endif

// ARM32 ABI from Android 11 hardware.h/camera_common.h/camera3.h.
// The vendor HMI symbol is exactly 0xb0 bytes. No internal HAL fields are touched.
struct Module;
struct Device;
struct ModuleMethods { int (*open)(const Module*, const char*, Device**); };
struct Module {
    uint32_t tag; uint16_t module_version, hal_version;
    const char *id, *name, *author;
    ModuleMethods *methods; void *dso; uint32_t reserved[25];
};
struct CameraModule { Module common; void *tail[12]; };
struct Device {
    uint32_t tag, version; Module *module; uint32_t reserved[12];
    int (*close)(Device*);
};
struct CameraDevice;
using RequestFn = int (*)(const CameraDevice*, void*);
using FlushFn = int (*)(const CameraDevice*);
struct Ops {
    int (*initialize)(const CameraDevice*, const struct Callbacks*);
    void *configure, *register_buffers, *default_settings;
    RequestFn request;
    void *vendor_tags, *dump;
    FlushFn flush;
    void *signal_stream_flush, *is_reconfiguration_required, *reserved[6];
};
struct CameraDevice { Device common; Ops *ops; void *priv; };
#ifndef HOST_TEST
static_assert(sizeof(CameraModule) == 176);
static_assert(sizeof(Device) == 64);
static_assert(offsetof(Module, methods) == 20);
static_assert(offsetof(CameraDevice, ops) == 64);
static_assert(offsetof(Ops, request) == 16 && offsetof(Ops, flush) == 28);
static_assert(sizeof(Ops) == 64);
#endif
struct Buffer { void *stream, *buffer; int status, acquire_fence, release_fence; };
struct Request { uint32_t frame; const void *settings; Buffer *input; uint32_t count; const Buffer *buffers; uint32_t physical_count; const char **ids; const void **metadata; };
struct Result { uint32_t frame; const void *metadata; uint32_t count; const Buffer *buffers; const Buffer *input; uint32_t partial, physical_count; const char **ids; const void **physical_metadata; };
struct Notify { int type; union alignas(8) { struct { uint32_t frame; void *stream; int code; } error; unsigned char generic[32]; } message; };
struct Callbacks { void (*result)(const Callbacks*,const Result*); void (*notify)(const Callbacks*,const Notify*); void *request_buffers,*return_buffers; };
#ifndef HOST_TEST
static_assert(sizeof(Buffer)==20 && sizeof(Request)==32 && sizeof(Result)==36);
static_assert(offsetof(Notify,message)==8 && sizeof(Notify)==40);
#endif
struct State {
    Ops original, traced;
    const Callbacks *callbacks=nullptr;
    std::mutex lock, backend_lock;
    std::condition_variable idle;
    unsigned epoch=0, cancelling=0;
    bool flushing=false, closing=false;
    unsigned inflight=0;
    Ops *original_ops;
    int (*original_close)(Device*);
    CameraDevice *device;
    std::atomic<unsigned> calls{0}, active{0}, flushes{0};
};
static std::mutex global_lock;
static std::shared_ptr<State> state;
static std::shared_ptr<State> get_state(const CameraDevice*d) {
    std::lock_guard<std::mutex>l(global_lock);
    return state && state->device==d ? state : nullptr;
}
struct ScopedCall {
    std::shared_ptr<State> owner;
    State*s=nullptr;
    explicit ScopedCall(const CameraDevice*d):owner(get_state(d)) {
        if(!owner)return;
        std::lock_guard<std::mutex>l(owner->lock);
        if(owner->closing)return;
        ++owner->inflight;s=owner.get();
    }
    ~ScopedCall(){if(s){std::lock_guard<std::mutex>l(s->lock);--s->inflight;s->idle.notify_all();}}
};
static long long now_us() {
    timespec t{}; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000LL + t.tv_nsec / 1000;
}
static int traced_initialize(const CameraDevice*d,const Callbacks*cb) {
    ScopedCall call(d);State*s=call.s;if(!s)return -ENODEV;
    int rc=s->original.initialize(d,cb);
    if(!rc)s->callbacks=cb;
    return rc;
}
static int cancel_request(State*s,Request*r) {
    // Never submitted to vendor HAL: no metadata/shutter may exist for this frame.
    // Transfer the exact original buffer handles and acquire fences back as errors.
    if(!r || !s->callbacks || !s->callbacks->notify || !s->callbacks->result ||
       (r->count && !r->buffers)) return -22;
    std::vector<Buffer> outputs;
    for(unsigned i=0;i<r->count;i++) {
        Buffer b=r->buffers[i]; b.status=1; b.release_fence=b.acquire_fence; b.acquire_fence=-1;
        outputs.push_back(b);
    }
    Buffer input{};
    if(r->input){ input=*r->input; input.status=1; input.release_fence=input.acquire_fence; input.acquire_fence=-1; }
    Notify error{}; error.type=1; error.message.error.frame=r->frame; error.message.error.code=2;
    s->callbacks->notify(s->callbacks,&error);
    Result result{}; result.frame=r->frame;result.count=r->count;
    result.buffers=outputs.data();result.input=r->input?&input:nullptr;
    s->callbacks->result(s->callbacks,&result);
    LOG("cancel concurrent-flush frame=%u outputs=%u ERROR_REQUEST fences returned",r->frame,r->count);
    return 0;
}
static int finish_cancel(State*s,Request*r) {
    int rc=cancel_request(s,r);
    {std::lock_guard<std::mutex>l(s->lock); --s->cancelling;--s->active;s->idle.notify_all();}
    return rc;
}
static int traced_request(const CameraDevice*d,void*request) {
    ScopedCall call(d);State*s=call.s;if(!s)return -ENODEV;++s->calls;unsigned epoch;
    {std::lock_guard<std::mutex>l(s->lock);epoch=s->epoch;++s->active;
      if(s->flushing){++s->cancelling;epoch=~0u;}}
    if(epoch==~0u)return finish_cancel(s,(Request*)request);
    std::unique_lock<std::mutex>b(s->backend_lock);
    bool cancel;
    {std::lock_guard<std::mutex>l(s->lock);cancel=s->flushing || epoch!=s->epoch;
      if(cancel)++s->cancelling;}
    if(cancel){b.unlock();return finish_cancel(s,(Request*)request);}
    int rc=s->original.request(d,request);
    {std::lock_guard<std::mutex>l(s->lock);--s->active;s->idle.notify_all();}
    return rc;
}
static int traced_flush(const CameraDevice*d) {
    ScopedCall call(d);State*s=call.s;if(!s)return -ENODEV;auto start=now_us();
    {std::lock_guard<std::mutex>l(s->lock);++s->epoch;s->flushing=true;}
    int rc;
    rc=s->original.flush(d); // MUST run concurrently: factory flush unblocks requests waiting for frames.
    {std::unique_lock<std::mutex>l(s->lock);s->idle.wait(l,[&]{return s->active.load()==0;});s->flushing=false;}
    LOG("flush original once rc=%d duration_us=%lld",rc,now_us()-start);
    return rc;
}
static int traced_close(Device*d) {
    auto owner=get_state(reinterpret_cast<CameraDevice*>(d));
    if(!owner)return -ENODEV;
    State*s=owner.get();
    {std::unique_lock<std::mutex>l(s->lock);
      if(s->closing)return -ENODEV;
      s->closing=true;s->idle.wait(l,[&]{return s->inflight==0;});}
    LOG("CLOSE idle requests=%u",s->calls.load());
    s->device->ops=s->original_ops;d->close=s->original_close;
    int rc=s->original_close(d);
    {std::lock_guard<std::mutex>l(global_lock);if(state==owner)state.reset();}
    LOG("CLOSE completed rc=%d",rc);
    return rc;
}
static bool wrap(CameraDevice *d) {
    if (!d || !d->ops || !d->ops->request || !d->ops->flush || !d->common.close
        || d->common.version < 0x300) return false;
    std::lock_guard<std::mutex>l(global_lock);if(state)return false;
    auto owner=std::make_shared<State>();State*s=owner.get();
    s->device = d; s->original_ops = d->ops; s->original = *d->ops;
    s->original_close = d->common.close; s->traced = s->original;
    s->traced.initialize=traced_initialize; s->traced.request = traced_request; s->traced.flush = traced_flush;
    state=owner; d->ops = &s->traced; d->common.close = traced_close;
    return true;
}
#ifndef HOST_TEST
extern "C" { CameraModule HMI{}; }
static CameraModule *backend;
static ModuleMethods methods;
static int fallback_open(const Module*,const char*,Device**out){if(out)*out=nullptr;return -ENODEV;}
static int fallback_count(){return 0;}
static int fallback_info(int,void*){return -ENODEV;}
static ModuleMethods fallback_methods{fallback_open};
static int traced_open(const Module*, const char *id, Device **out) {
    int rc = backend->common.methods->open(&backend->common, id, out);
    if (!rc && out && *out && id && !strcmp(id, "2")) {
        LOG("OPEN id=2 version=%x traced=%d", (*out)->version,
            wrap(reinterpret_cast<CameraDevice*>(*out)));
    }
    return rc;
}
__attribute__((constructor)) static void initialize_trace() {
    HMI.common.tag=0x48574d54;HMI.common.module_version=0x200;
    HMI.common.id="camera";HMI.common.name="BCR rollback required";HMI.common.author="compat";
    HMI.common.methods=&fallback_methods;
    HMI.tail[0]=(void*)fallback_count;HMI.tail[1]=(void*)fallback_info;
    void*h=dlopen("/vendor/lib/hw/camera.sdm660.backend.so",RTLD_NOW|RTLD_LOCAL);
    if (!h) { LOG("INIT failed dlopen: %s", dlerror()); return; }
    backend = reinterpret_cast<CameraModule*>(dlsym(h, "HMI"));
    if (!backend || !backend->common.methods || !backend->common.methods->open) {
        LOG("INIT failed HMI"); return;
    }
    HMI = *backend;
    methods.open = traced_open; HMI.common.methods = &methods;
    LOG("INIT v2; concurrent factory flush preserved; only camera id 2 gated");
}
#else
#include <assert.h>
#include <thread>
static unsigned requests,flushes,closes,errors,results;
static bool started=false,release_flush=false;
static std::mutex testlock;static std::condition_variable testcond;
static int fake_init(const CameraDevice*,const Callbacks*){return 0;}
static int fake_request(const CameraDevice*,void*){++requests;return -17;}
static int fake_flush(const CameraDevice*) {
 ++flushes;std::unique_lock<std::mutex>l(testlock);started=true;testcond.notify_all();
 testcond.wait(l,[]{return release_flush;});return 0;
}
static int fake_close(Device*){++closes;return 7;}
static void notify(const Callbacks*,const Notify*n){assert(n->type==1 && n->message.error.code==2 && n->message.error.frame==54);++errors;}
static void result(const Callbacks*,const Result*r){assert(r->frame==54 && r->metadata==nullptr && r->count==2 && r->partial==0);assert(r->buffers[0].status==1 && r->buffers[0].release_fence==123 && r->buffers[0].acquire_fence==-1);assert(r->buffers[1].release_fence==-1);++results;}
int main(){
 Ops original{};original.initialize=fake_init;original.request=fake_request;original.flush=fake_flush;
 CameraDevice d{};d.common.version=0x300;d.common.close=fake_close;d.ops=&original;
 assert(wrap(&d));Callbacks cb{result,notify,nullptr,nullptr};assert(!d.ops->initialize(&d,&cb));
 Buffer bufs[2]{};bufs[0].acquire_fence=123;bufs[1].acquire_fence=-1;Request req{};req.frame=54;req.count=2;req.buffers=bufs;
 assert(d.ops->request(&d,&req)==-17 && requests==1);
 std::thread f([&]{assert(d.ops->flush(&d)==0);});
 {std::unique_lock<std::mutex>l(testlock);testcond.wait(l,[]{return started;});}
 assert(!d.ops->request(&d,&req));assert(requests==1 && errors==1 && results==1);
 assert(bufs[0].acquire_fence==123 && bufs[0].status==0 && req.frame==54);
 {std::lock_guard<std::mutex>l(testlock);release_flush=true;testcond.notify_all();}f.join();
 assert(d.ops->request(&d,&req)==-17 && requests==2);
 assert(d.common.close(&d.common)==7 && closes==1 && !state);
 puts("PASS: normal requests unchanged; concurrent flush request error/fences returned; post-flush resumes; original flush once; close restored");
}
#endif
