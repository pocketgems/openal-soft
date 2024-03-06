#ifdef ANDROID
#include "config.h"
#include "AL/alc.h"
#include <jni.h>
#include <stddef.h>
#include "stdio.h"
#include "stdlib.h"
#include <android/log.h>

// #include "alMain.h"
#include "android_openal_funcs.h"

AndroidOpenALFuncs androidOpenALFuncs;

static JavaVM *javaVM = NULL;
static JavaVM *v_javaVM = NULL;
JavaVM *alcGetJavaVM(void) {
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "alcGetJavaVM javaVM Address %p javaVM %p ",&javaVM, javaVM);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-Android ===", "alcGet_vJavaVM v_javaVM Address %p v_javaVM %p ",&v_javaVM, v_javaVM);
	return javaVM;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
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
