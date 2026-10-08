extern "C" void br_register_camera(void*, int);
// Legacy Camera::connect bridge: project implementation snapshot.
// Application -> shim and shim -> platform both use AArch64 x8 for
// non-trivial return objects. The platform call uses targetSdk in w1,
// forceSlowJpegMode in w3, and device policy in w5.
// Private layouts and addresses are build-specific. This is not a stable
// NDK API or a complete redistributable build of the final APK.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <link.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include <jni.h>

// 应用的真实 targetSdk（已确认 manifest: targetSdk=30）——w1 用它，而非旧 halVersion 语义的 1
static const int kAppTargetSdk = 30;

#define LOGI(...) __android_log_print(4, "BRBRIDGE", __VA_ARGS__)
#define LOGE(...) __android_log_print(6, "BRBRIDGE", __VA_ARGS__)

// ── 目标对象的布局（偏移均有指令级证据）──────────────────────────────
// 字段名与顺序 = Java AttributionSourceState.writeToParcel 的字节码顺序 × 原生反汇编偏移：
//   pid@8 uid@12 deviceId@16 packageName@24 attributionTag@56 token@88
//   renouncedPermissions@96  next@128（next 的类型是 AttributionSourceState[]，递归）
struct AttribLayout;
struct AttribLayout {
    void*                                     vptr;   //   0
    int32_t                                   f8;     //   8    pid
    int32_t                                   f12;    //  12    uid
    int32_t                                   f16;    //  16    deviceId
    std::optional<std::string>                s1;     //  24    packageName
    std::optional<std::string>                s2;     //  56    attributionTag
    void*                                     token;  //  88    token (IBinder)
    std::optional<std::vector<std::optional<std::string>>> v1;  // 96  renouncedPermissions
    // ★ 修正：next 在 128 处只有 24 字节空间（152-128）⇒ 必须是【普通 vector】（24B），
    //   不能包 optional（libc++ 的 optional<vector> 为 32B，会使总长变 160）。
    //   元素类型按 Java AttributionSourceState[] ⇒ 递归为 AttribLayout。
    //   元素类型与 stride 由原生 writeToParcel/readFromParcel 反汇编确认（见布局测试）。
    std::vector<AttribLayout>                 v2;     // 128   next
};
static_assert(sizeof(AttribLayout) == 152, "AttributionSourceState 大小必须是 152（反汇编证明）");
static_assert(offsetof(AttribLayout, f8)    ==   8, "偏移不符");
static_assert(offsetof(AttribLayout, f12)   ==  12, "偏移不符");
static_assert(offsetof(AttribLayout, f16)   ==  16, "偏移不符");
static_assert(offsetof(AttribLayout, s1)    ==  24, "偏移不符");
static_assert(offsetof(AttribLayout, s2)    ==  56, "偏移不符");
static_assert(offsetof(AttribLayout, token) ==  88, "偏移不符");
static_assert(offsetof(AttribLayout, v1)    ==  96, "偏移不符");
static_assert(offsetof(AttribLayout, v2)    == 128, "偏移不符");

struct CompatLayout {
    void*    vptr;      // 0
    uint8_t  rest[64];  // 8..  （getter 证明：optional@8-12、bool@16、bool@17、optional@20）
};
static_assert(offsetof(CompatLayout, rest) == 8, "偏移不符");

// ── 运行时定位 ────────────────────────────────────────────────────────
// ★ 关键修正（tombstone_23/24/25 证实）：A16 的 Camera::connect 从【x8】取隐藏返回指针(sret)。
//   If 被调函数声明为返回普通 void*，编译器会把 sret 放在 x0 ⇒ 被调方从 x8 取到垃圾并写入，
//   导致 SEGV_ACCERR(write) 落在 r-x 页（实测：str x26,[x19]，x19 = 函数指针值）。
//   ⇒ 必须让编译器按"sret 约定"生成调用：声明返回一个【非平凡】类型即可。
struct SpCam {
    void* p;
    SpCam() : p(nullptr) {}
    ~SpCam() {}
    SpCam(const SpCam& o) : p(o.p) {}
    SpCam& operator=(const SpCam& o) { p = o.p; return *this; }
};
typedef SpCam (*connect_t)(int cameraId, int halVersion, const void* compat,
                           bool hasAttribution, const void* attrib, int targetSdk);

static uintptr_t   g_cc_vtable   = 0;   // libcameracompatbilityinfo.so vtable(0x8008+16) 的地址
static uintptr_t   g_attr_vtable = 0;   // AttributionSourceState vtable(+16) 的地址
static connect_t   g_connect     = nullptr;
static void*       g_p4jo        = nullptr;  // parcelForJavaObject（libandroid_runtime.so）
static void*       g_rfp         = nullptr;  // AttributionSourceState::readFromParcel（libpermission.so）
static int         g_init        = -1;

static int phdr_cb(struct dl_phdr_info* info, size_t, void*) {
    const char* n = info->dlpi_name ? info->dlpi_name : "";
    if (!n || !*n) return 0;

    // 系统真库才接受（避免匹配到探针自己的副本）
    const bool sys = (strstr(n, "/system/") != nullptr) || (strstr(n, "/apex/") != nullptr);
    if (!sys) return 0;

    if (!g_cc_vtable && strstr(n, "libcameracompatbilityinfo.so")) {
        // Itanium ABI: 对象 vptr 指向 vtable 起始 + 16（跳过 offset-to-top 与 typeinfo）
        g_cc_vtable = info->dlpi_addr + 0x8008UL + 16UL;
        LOGI("found libcameracompatbilityinfo base=%p vptr=%p (typeinfo=%p)",
             (void*)info->dlpi_addr, (void*)g_cc_vtable,
             *(void**)(info->dlpi_addr + 0x8008UL + 8UL));
    }
    if (!g_attr_vtable && strstr(n, "libpermission.so")) {
        g_attr_vtable = info->dlpi_addr + 0x16360UL + 16UL;
        LOGI("found libpermission base=%p attr_vptr=%p (typeinfo=%p)",
             (void*)info->dlpi_addr, (void*)g_attr_vtable,
             *(void**)(info->dlpi_addr + 0x16360UL + 8UL));
        // readFromParcel 也在 libpermission.so（nm 实证：0x10f0c）
        if (!g_rfp) {
            g_rfp = (void*)(info->dlpi_addr + 0x10f0cUL);
            LOGI("found readFromParcel=%p", g_rfp);
        }
    }
    // parcelForJavaObject 在系统 libandroid_runtime.so（nm 实证：0x1d7708）
    // ★ 用 dl_iterate_phdr（可见进程内所有已加载库，不受链接器命名空间限制），
    //   而非 dlopen（应用命名空间拒绝访问非公开系统库）
    if (!g_p4jo && strstr(n, "libandroid_runtime.so")) {
        g_p4jo = (void*)(info->dlpi_addr + 0x1d7708UL);
        LOGI("found libandroid_runtime base=%p parcelForJavaObject=%p",
             (void*)info->dlpi_addr, g_p4jo);
    }
    if (!g_connect && strstr(n, "libcamera_client.so")) {
        g_connect = (connect_t)(info->dlpi_addr + 0x89d5cUL);
        LOGI("found libcamera_client base=%p connect=%p", (void*)info->dlpi_addr,
             (void*)g_connect);
    }
    return 0;
}

static AttribLayout g_attrib;      // 由 libc++ 默认构造 ⇒ 全部成员合法
static CompatLayout g_compat;      // 全 0 ⇒ "无兼容性覆盖"（语义由导出 getter 证明）

static int bcr_camera_id() {
    char v[PROP_VALUE_MAX] = {0};
    int n = __system_property_get("persist.camera.bcr.id", v);
    if (n <= 0) return 2;
    int id = atoi(v);
    return (id < 0) ? 2 : id;
}

static void bridge_init() {
    if (g_init >= 0) return;
    g_init = 0;
    dl_iterate_phdr(phdr_cb, nullptr);
    if (!g_connect)       { LOGE("FATAL: 未能定位 A16 Camera::connect"); g_init = -1; return; }
    if (!g_cc_vtable)     { LOGE("FATAL: 未能定位 CameraCompatibilityInfo vtable"); g_init = -1; return; }
    if (!g_attr_vtable)   { LOGE("FATAL: 未能定位 AttributionSourceState vtable"); g_init = -1; return; }
    g_compat.vptr = (void*)g_cc_vtable;
    g_attrib.vptr = (void*)g_attr_vtable;
    LOGI("bridge ready: connect=%p compat=%p attrib=%p size(compat=%zu attrib=%zu)",
         (void*)g_connect, g_compat.vptr, g_attrib.vptr,
         sizeof(CompatLayout), sizeof(AttribLayout));
}

__attribute__((constructor)) static void br_ctor() { bridge_init(); }

// ── 路线 a：从应用自身 Context 取得【框架构造的】attribution 与 token ───────────
// 严格复刻框架 Camera.cameraInit 的路径：
//   ActivityThread.currentApplication()
//     → Context.getAttributionSource()
//     → AttributionSource.asScopedParcelState().getParcel()
//     → libandroid_runtime 的 parcelForJavaObject() → 原生 Parcel*
//     → 真库导出的 AttributionSourceState::readFromParcel() 填入 g_attrib
// 不做任何身份自填；任何一步失败都【明确报错并中止】（不自动退回自填方案）。
typedef void* (*parcelForJavaObject_t)(JNIEnv*, jobject);
typedef int   (*attrReadFromParcel_t)(void* self, const void* parcel);

// ★ 从【系统库】显式解析符号：本进程里同名库被我们的转发 shim 占用 SONAME，
//   RTLD_DEFAULT 会在本命名空间内找不到系统库的符号（已由 parcelForJavaObject 的失败实证）。
static void* sysdlsym(const char* soname, const char* sym) {
    char path[256];
    snprintf(path, sizeof(path), "/system/lib64/%s", soname);
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        LOGE("sysdlsym: dlopen(%s) 失败: %s", path, dlerror());
        return nullptr;
    }
    dlerror();
    void* p = dlsym(h, sym);
    if (!p) { LOGE("sysdlsym: dlsym(%s, %s) 失败: %s", soname, sym, dlerror()); return nullptr; }
    LOGI("sysdlsym: %s -> %s = %p", soname, sym, p);
    return p;
}

static int g_attr_ready = -1;      // -1 未尝试, 0 失败, 1 成功

// ★ 保存并检查 JNI 异常：区分「签名错误 / 方法缺失 / 隐藏 API 限制 / 前置调用残留异常」
//   —— GetMethodID 返回 null 本身不能证明方法不存在。
static int jni_check(JNIEnv* env, const char* where) {
    if (!env->ExceptionCheck()) return 0;
    LOGE("routeA/JNI 异常 @%s:", where);
    env->ExceptionDescribe();         // 打印到 logcat（含具体异常类型与消息）
    env->ExceptionClear();
    return 1;
}

static int acquire_attribution_from_context() {
    if (g_attr_ready >= 0) return g_attr_ready;
    g_attr_ready = 0;

    JavaVM* vm = nullptr; jsize nvm = 0;
    if (JNI_GetCreatedJavaVMs(&vm, 1, &nvm) != JNI_OK || !vm || nvm < 1) {
        LOGE("routeA: JNI_GetCreatedJavaVMs 失败"); return 0;
    }
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
            LOGE("routeA: AttachCurrentThread 失败"); return 0;
        }
    }

    // 1) ActivityThread.currentApplication()
    jclass at = env->FindClass("android/app/ActivityThread");
    if (jni_check(env, "FindClass(ActivityThread)") || !at) {
        LOGE("routeA: 取 ActivityThread 失败（见上方异常）"); return 0;
    }
    jmethodID ca = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
    if (jni_check(env, "GetStaticMethodID currentApplication") || !ca) {
        LOGE("routeA: currentApplication 方法ID 为空"); return 0;
    }
    jobject app = env->CallStaticObjectMethod(at, ca);
    if (jni_check(env, "currentApplication()") || !app) {
        LOGE("routeA: currentApplication() 返回空"); return 0;
    }

    // 2) Context.getAttributionSource()  —— blacklist，但 JNI 查找不受反射黑名单限制
    jclass ctx = env->FindClass("android/content/Context");
    if (jni_check(env, "FindClass(Context)") || !ctx) { LOGE("routeA: 取 Context 失败"); return 0; }
    jmethodID gas = env->GetMethodID(ctx, "getAttributionSource", "()Landroid/content/AttributionSource;");
    if (jni_check(env, "GetMethodID getAttributionSource") || !gas) {
        LOGE("routeA: getAttributionSource 方法ID 为空"); return 0;
    }
    jobject as = env->CallObjectMethod(app, gas);
    if (jni_check(env, "getAttributionSource()") || !as) {
        LOGE("routeA: getAttributionSource() 返回空"); return 0;
    }

    // 3) ★ a″：纯框架序列化路径 —— Parcel.obtain() → AttributionSource.writeToParcel(whitelist)
    jclass parcel_cls = env->FindClass("android/os/Parcel");
    if (jni_check(env, "FindClass(Parcel)") || !parcel_cls) { LOGE("routeA: 取 Parcel 类失败"); return 0; }
    jmethodID obtain = env->GetStaticMethodID(parcel_cls, "obtain", "()Landroid/os/Parcel;");
    if (jni_check(env, "GetStaticMethodID Parcel.obtain") || !obtain) {
        LOGE("routeA: Parcel.obtain 方法ID 为空"); return 0;
    }
    jobject parcel = env->CallStaticObjectMethod(parcel_cls, obtain);
    if (jni_check(env, "Parcel.obtain()") || !parcel) { LOGE("routeA: Parcel.obtain() 返回空"); return 0; }

    jclass as_cls = env->GetObjectClass(as);
    if (jni_check(env, "GetObjectClass(AttributionSource)") || !as_cls) {
        LOGE("routeA: 取 AttributionSource 类失败"); return 0;
    }
    // ★ 优先取 asState() 得到 AttributionSourceState（其 writeToParcel 布局与原生
    //   readFromParcel 严格对应）；失败再退回对 AttributionSource 本身写 Parcel。
    jmethodID as_state = env->GetMethodID(as_cls, "asState", "()Landroid/content/AttributionSourceState;");
    jobject src_obj = as;
    if (!jni_check(env, "GetMethodID asState") && as_state) {
        jobject st = env->CallObjectMethod(as, as_state);
        if (!jni_check(env, "asState()") && st) {
            src_obj = st;
            LOGI("routeA: 已取 asState() → AttributionSourceState");
        } else {
            LOGE("routeA: asState() 失败，退回 AttributionSource.writeToParcel");
        }
    } else {
        LOGE("routeA: 无 asState()，退回 AttributionSource.writeToParcel");
    }

    jclass src_cls = (src_obj == as) ? as_cls : env->GetObjectClass(src_obj);
    jmethodID wtp = env->GetMethodID(src_cls, "writeToParcel", "(Landroid/os/Parcel;I)V");
    if (jni_check(env, "GetMethodID writeToParcel") || !wtp) {
        LOGE("routeA: writeToParcel 方法ID 为空"); return 0;
    }
    env->CallVoidMethod(src_obj, wtp, parcel, (jint)0);
    if (jni_check(env, "writeToParcel()")) { LOGE("routeA: writeToParcel 抛异常"); return 0; }

    // ★ 关键：把 Parcel 读位置复位到 0，否则 readFromParcel 从末尾读 ⇒ 返回 -61 (ENODATA)
    jmethodID sdp = env->GetMethodID(parcel_cls, "setDataPosition", "(I)V");
    if (!jni_check(env, "GetMethodID setDataPosition") && sdp) {
        env->CallVoidMethod(parcel, sdp, (jint)0);
        jni_check(env, "setDataPosition(0)");
        LOGI("routeA: 已 setDataPosition(0)");
    } else {
        LOGE("routeA: 无 setDataPosition ⇒ 可能读位置错误");
    }

    // 4) Java Parcel → 原生 Parcel*（框架自己的转换函数）
    void* p4jo = g_p4jo;
    if (!p4jo) { LOGE("routeA: g_p4jo 未解析（系统 libandroid_runtime.so 未在进程内？）"); return 0; }
    if (!p4jo) { LOGE("routeA: 未找到 parcelForJavaObject: %s", dlerror()); return 0; }
    void* nparcel = ((parcelForJavaObject_t)p4jo)(env, parcel);
    if (!nparcel) { LOGE("routeA: parcelForJavaObject 返回空"); return 0; }

    // 5) 用真库导出的 readFromParcel 填入原生对象（含 token 与 next 归属链）
    void* rfp = g_rfp;
    if (!rfp) { LOGE("routeA: g_rfp 未解析（libpermission.so 未在进程内？）"); return 0; }
    if (!rfp) { LOGE("routeA: 未找到 AttributionSourceState::readFromParcel: %s", dlerror()); return 0; }
    int rc = ((attrReadFromParcel_t)rfp)(&g_attrib, nparcel);
    if (rc != 0) { LOGE("routeA: readFromParcel 返回错误 %d", rc); return 0; }

    // 6) 回收 Java Parcel
    jmethodID recycle = env->GetMethodID(parcel_cls, "recycle", "()V");
    if (!jni_check(env, "GetMethodID Parcel.recycle") && recycle) {
        env->CallVoidMethod(parcel, recycle);
        jni_check(env, "Parcel.recycle()");
    }

    LOGI("routeA(a″): 已用框架 Parcel 序列化路径填充 attribution（含 token 与 next 链）");
    g_attr_ready = 1;
    return 1;
}

static void do_connect(void* sret, int cameraId, int halVersion) {
    bridge_init();
    memset(sret, 0, 8);
    if (g_init != 0) { LOGE("connect aborted: init failed"); return; }
    // ★ 路线 a：必须拿到应用 Context 的真实 attribution（含框架 token）。
    //   失败则【中止并报错】——绝不用自填身份回退。
    if (!acquire_attribution_from_context()) {
        LOGE("connect aborted: 路线 a 未能取得应用 Context 的真实 attribution（不自填身份回退）");
        return;
    }
    LOGI("bridge connect(cameraId=%d, targetSdk(w1)=%d, forceSlowJpegMode=false, "
         "devicePolicy(w5)=%d) [诊断试验：仅改 w5，w1 未修正]",
         cameraId, halVersion, 0);
    // ★ 诊断试验：第 6 参（服务端 device policy）由 30 改为 0。
    //   依据：Camera.getDevicePolicyFromContext() 在 deviceId==0 的默认路径直接 return 0
    //   （smali 实证）；服务端按无符号 <2 判定。w1 仍为 1（targetSdk 尚未修正，单列第二项）。
    SpCam r = g_connect(cameraId, halVersion, &g_compat, false, &g_attrib, /*w5 devicePolicy*/ 0);
    *(void**)sret = r.p;          // 交给调用方（应用期望的 sp<Camera> 槽）
    LOGI("bridge connect returned Camera*=%p", r.p);
}

// ── 导出的 6 个旧符号（sret 约定：x8 = 返回对象地址）─────────────────
// ★ 修正后的实现：返回非平凡类型 SpCam ⇒ 编译器按 sret(x8) 生成“返回给应用”的约定
static SpCam do_connect(int cameraId, int targetSdk) {
    bridge_init();
    SpCam out;                                    // p = nullptr
    if (g_init != 0) { LOGE("connect aborted: init failed"); return out; }
    // ★ 路线 a：必须拿到应用 Context 的真实 attribution（含框架 token）。
    //   失败则【中止并报错】——绝不用自填身份回退。
    if (!acquire_attribution_from_context()) {
        LOGE("connect aborted: 路线 a 未能取得应用 Context 的真实 attribution（不自填身份回退）");
        return out;
    }
    // ★ 修正：不再把 cameraId 强制改成 0 —— 那会用【普通相机】冒充【扫描头】，
    //   连接成功也不能证明取得了扫描头图像。此处严格保留调用方请求的 ID。
    LOGI("bridge connect(cameraId=%d [调用方原值，未改写], w1(targetSdk)=%d, "
         "w3=forceSlowJpegMode false, w5(devicePolicy)=0)", cameraId, targetSdk);
    // 边界二（shim → A16 真库）：g_connect 亦声明为返回 SpCam ⇒ 传出槽同样在 x8
    SpCam r = g_connect(cameraId, targetSdk, &g_compat, false, &g_attrib, /*w5 devicePolicy*/ 0);
    br_register_camera(r.p, cameraId);
    LOGI("bridge connect returned Camera*=%p", r.p);
    return r;
}

// ── 边界一（应用 → shim）：六个导出【全部】使用非平凡返回类型 ─────────────
// 已由原始汇编实证：应用以 x8 传返回槽（connectEv/connectEi/connectEii 三个调用点均为
// `add x8, sp, #16` 后 `blr`，参数从 w0 起）。声明为非平凡返回类型即让编译器按此约定接收。
extern "C" SpCam _ZN7android6Camera7connectEv() {
    return do_connect(bcr_camera_id(), kAppTargetSdk);
}
extern "C" SpCam _ZN7android6Camera7connectEi(int cameraId) {
    return do_connect(cameraId, kAppTargetSdk);
}
// ★ 修正：第 2 个参数是【旧 API 的 halVersion】，与 A16 connect 的 w1(=targetSdk) 语义不同
//   ⇒ w1 一律传 kAppTargetSdk；调用方给的 halVersion 仅记入日志。
extern "C" SpCam _ZN7android6Camera7connectEii(int cameraId, int halVersion) {
    LOGI("connectEii 调用方 halVersion=%d（不用于 w1）", halVersion);
    return do_connect(cameraId, kAppTargetSdk);
}
extern "C" SpCam _ZN7android6Camera7connectEiRKNS_8String16Ei(int cameraId,
                                                              const void* clientName, int uidOrPid) {
    (void)clientName; (void)uidOrPid;
    return do_connect(cameraId, kAppTargetSdk);
}
extern "C" SpCam _ZN7android6Camera7connectEiRKNS_8String16Eii(int cameraId,
                                                               const void* clientName, int uid, int pid) {
    (void)clientName; (void)uid; (void)pid;
    return do_connect(cameraId, kAppTargetSdk);
}
// connectLegacy 返回 status_t(int) 且以 sp<Camera>& 出参 ⇒ 无 sret，按普通返回处理
// ★ 同样修正：halVersion 不用于 w1，w1 = kAppTargetSdk
extern "C" int _ZN7android6Camera13connectLegacyEiiRKNS_8String16EiRNS_2spIS0_EE(
        int cameraId, int halVersion, const void* clientName, int uid, void* outSp) {
    (void)clientName; (void)uid;
    LOGI("connectLegacy 调用方 halVersion=%d（不用于 w1）", halVersion);
    SpCam r = do_connect(cameraId, kAppTargetSdk);
    if (outSp) *(void**)outSp = r.p;
    return r.p ? 0 : -1;
}
