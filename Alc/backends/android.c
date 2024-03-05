#ifdef ANDROID
#include "config.h"
#include "AL/alc.h"
#include <jni.h>
#include <stddef.h>
#include "stdio.h"
#include <android/log.h>

// #include "alMain.h"
#include "android_openal_funcs.h"

AndroidOpenALFuncs androidOpenALFuncs;

static JavaVM *javaVM = NULL;
JavaVM *alcGetJavaVM(void) {
    __android_log_print(ANDROID_LOG_INFO, "======= WARDRAGON =======", "alcGetJavaVM called  %p  %p ",&javaVM, javaVM);
	return javaVM;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_INFO, "======= WARDRAGON =======", "JNI_OnLoad called  %p  %p ",&vm, vm);
	// BackendFuncs func_list;
	if (androidOpenALFuncs.alc_android_set_java_vm) {
		androidOpenALFuncs.alc_android_set_java_vm(vm);
	}
	javaVM = vm;
	return JNI_VERSION_1_4;
}

void JNI_OnUnload (JavaVM *vm, void *reserved)
{
    __android_log_print(ANDROID_LOG_INFO, "======= WARDRAGON =======", "JNI_OnUnload called  %p  %p ",&vm, vm);
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
