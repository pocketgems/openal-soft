#ifdef ANDROID
#include "config.h"
#include "AL/alc.h"
#include <jni.h>
#include <stddef.h>
#include "stdio.h"
#include "stdlib.h"
#include <android/log.h>
#include <libv/debug_util.h>

// #include "alMain.h"
#include "android_openal_funcs.h"

// The max backtrace size for allocations bigger than LARGE_ALLOCATION
//#define MAX_BT_SIZE 40 // NOTE : 40 is hardcoded in debug_util.c get_backtrace_pcs_skip()
//
//#define MAX_BT_SIZE_SMALL_ALLOC 40
//
//typedef backtrace_pointer_t bt_pointer_t;
//
//typedef struct {
//    uint8_t type; // Needs to be 'm'
//    uint8_t flags;
//    uint32_t pointer;
//    uint32_t allocationSize;
//    uint32_t threadId;
//    uint32_t packetSize;
//    // Followed by packetSize / 4 function addresses
//} memtrace_header;
//
//void CallBackTrace(char *methodName)
//{
//    char buf[sizeof(memtrace_header) + MAX_BT_SIZE * sizeof(bt_pointer_t)];
//
//    bt_pointer_t *pointers = (void*)(buf + sizeof(memtrace_header));
//
//    int pointerCount = MAX_BT_SIZE;
//
//    get_backtrace_pcs(pointers, &pointerCount);
//
//    if(pointerCount>0)
//    {
//        for (int i = 0; i < pointerCount; ++i) {
//            __android_log_print(ANDROID_LOG_INFO, "=== WD-android-Trace ===", "%s %s ", methodName, pointers[i]);
//        }
//    }
//}

AndroidOpenALFuncs androidOpenALFuncs;

static JavaVM *javaVM = NULL;
static JavaVM *v_javaVM = NULL;
JavaVM *alcGetJavaVM(void) {
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "alcGetJavaVM javaVM Address %p javaVM %p ",&javaVM, javaVM);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "alcGet_vJavaVM v_javaVM Address %p v_javaVM %p ",&v_javaVM, v_javaVM);
	return javaVM;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
//    CallBackTrace("android.c - JNI_OnLoad");
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "JNI_OnLoad vm Address : %p  vm : %p ",&vm, vm);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "JNI_OnLoad javaVM Address : %p  javaVM : %p ",&javaVM, javaVM);
	// BackendFuncs func_list;
	if (androidOpenALFuncs.alc_android_set_java_vm) {
        __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "JNI_OnLoad alc_android_set_java_vm is not NULL");
		androidOpenALFuncs.alc_android_set_java_vm(vm);
	}
	javaVM = vm;
    v_javaVM = vm;
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "javaVM assigned  javaVM address : %p  javaVM : %p ",&javaVM, javaVM);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "v_javaVM assigned  v_javaVM address : %p  v_javaVM : %p ",&v_javaVM, v_javaVM);
	return JNI_VERSION_1_4;
}

void JNI_OnUnload (JavaVM *vm, void *reserved)
{
//    CallBackTrace("android.c - JNI_OnUnload");
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "JNI_OnUnload vm address : %p vm : %p ",&vm, vm);
	if (androidOpenALFuncs.alc_android_set_java_vm) {
		androidOpenALFuncs.alc_android_set_java_vm(NULL);
	}
}

ALC_API void ALC_APIENTRY alcSuspend(void) {
	if (androidOpenALFuncs.alc_android_suspend) {
		androidOpenALFuncs.alc_android_suspend();
	}
}

ALC_API void ALC_APIENTRY alcResume(void) {
	if (androidOpenALFuncs.alc_android_resume) {
		androidOpenALFuncs.alc_android_resume();
	}
}

#endif
