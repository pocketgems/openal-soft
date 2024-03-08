/**
 * OpenAL cross platform audio library
 * Copyright (C) 2010 by Chris Robinson
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"
#include "stdio.h"
#include <stdlib.h>
#include <jni.h>
#include <pthread.h>
#include <sys/prctl.h>
#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"
#include "alu.h"
#include <android/log.h>

#include "android_openal_funcs.h"

#include <libv/debug_util.h>

static const ALCchar android_device[] = "Android Default";

static pthread_mutex_t deviceMutex = PTHREAD_MUTEX_INITIALIZER;

JavaVM* alcGetJavaVM(void);
static JNIEnv* env;

static jclass cAudioTrack = NULL;

static jmethodID mAudioTrack;
static jmethodID mGetMinBufferSize;
static jmethodID mPlay;
static jmethodID mPause;
static jmethodID mStop;
static jmethodID mRelease;
static jmethodID mWrite;

static int suspended = 0;
static int audioTrackPlaying = 0;
static int audioTrackWasPlaying = 0;

typedef struct
{
    pthread_t thread;
    volatile int running;
} AndroidData;

#define STREAM_MUSIC 3
#define CHANNEL_CONFIGURATION_MONO 2
#define CHANNEL_CONFIGURATION_STEREO 3
#define ENCODING_PCM_8BIT 3
#define ENCODING_PCM_16BIT 2
#define MODE_STREAM 1

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
//            __android_log_print(ANDROID_LOG_INFO, "=== WD-audiotrack-Trace ===", "%s %s ", methodName, pointers[i]);
//        }
//    }
//}


static void* thread_function(void* arg)
{
    ALCdevice* device = (ALCdevice*)arg;
    AndroidData* data = (AndroidData*)device->ExtraData;

    // Show a sensible name for the thread in debug tools
    prctl(PR_SET_NAME, (unsigned long)"OpenAL/AudioTrack", 0, 0, 0);

    JavaVM *javaVM = alcGetJavaVM();
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "thread_function javaVM Address %p JavaVM  %p ",&javaVM, javaVM);
    (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

    (*env)->PushLocalFrame(env, 2);

    int sampleRateInHz = device->Frequency;
    int channelConfig = ChannelsFromDevFmt(device->FmtChans) == 1 ? CHANNEL_CONFIGURATION_MONO : CHANNEL_CONFIGURATION_STEREO;
    int audioFormat = BytesFromDevFmt(device->FmtType) == 1 ? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;

    int bufferSizeInBytes = (*env)->CallStaticIntMethod(env, cAudioTrack,
        mGetMinBufferSize, sampleRateInHz, channelConfig, audioFormat);

    // Suggestion from Eric Wing <ewmailing@gmail.com>
    /* According to the author Martins Mozelko, I should multiply bufferSizeInBytes to tune performance.
       Say, multiply by 2.
       But making this number smaller seems to reduce latency...
       I have tried dividing by 2, 4, and 8. 8 refuses to play any sound.
       It seems that this just divides out the multiplication of NumUpdates (default=4)
       which returns it to min buffer size.
       bufferSizeInBytes is used in multiple places and
       bufferSizeInSamples is tied directly to bufferSizeInBytes though, so we need to be careful
       about what we want to change.
       I'm assuming Martins is correct and this is the indeed the place we want to change it.
       Dividing out the bufferSizeInSamples separately and skipping the multiply did not work.
       Omitting the multiply and not dividing did work, but the buffers may be unnecessarily large.
      */
    bufferSizeInBytes = bufferSizeInBytes / device->NumUpdates;

    int bufferSizeInSamples = bufferSizeInBytes / FrameSizeFromDevFmt(device->FmtChans, device->FmtType);

    jobject track = (*env)->NewObject(env, cAudioTrack, mAudioTrack,
        STREAM_MUSIC, sampleRateInHz, channelConfig, audioFormat, device->NumUpdates * bufferSizeInBytes, MODE_STREAM);

    (*env)->CallNonvirtualVoidMethod(env, track, cAudioTrack, mPlay);
    audioTrackPlaying = 1;

    jarray buffer = (*env)->NewByteArray(env, bufferSizeInBytes);

    while (data->running)
    {
        if (suspended) {
            if (audioTrackPlaying) {
                (*env)->CallNonvirtualVoidMethod(env, track, cAudioTrack, mPause);
                audioTrackPlaying = 0;
            }
            usleep(5000);
            continue;
        } else if (!audioTrackPlaying) {
            (*env)->CallNonvirtualVoidMethod(env, track, cAudioTrack, mPlay);
            audioTrackPlaying = 1;
        }

        void* pBuffer = (*env)->GetPrimitiveArrayCritical(env, buffer, NULL);

        if (pBuffer)
        {
            aluMixData(device, pBuffer, bufferSizeInSamples);
            (*env)->ReleasePrimitiveArrayCritical(env, buffer, pBuffer, 0);

            (*env)->CallNonvirtualIntMethod(env, track, cAudioTrack, mWrite, buffer, 0, bufferSizeInBytes);
        }
        else
        {
            TRACE("Failed to get pointer to array bytes");
        }
    }

    (*env)->CallNonvirtualVoidMethod(env, track, cAudioTrack, mStop);
    (*env)->CallNonvirtualVoidMethod(env, track, cAudioTrack, mRelease);
    audioTrackPlaying = 0;

    (*env)->PopLocalFrame(env, NULL);

    (*javaVM)->DetachCurrentThread(javaVM);
    return NULL;
}

static ALCenum android_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
//    CallBackTrace("android_open_playback");

    JavaVM *javaVM = alcGetJavaVM();

    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback JavaVM Address  %p JavaVM %p ",&javaVM, javaVM);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback Env Address %p Env %p ",&env, env);

    if (javaVM) (*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_4);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback AFTER JavaVM Address  %p JavaVM %p ",&javaVM, javaVM);

    if (javaVM)
    {
        (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);
    }
    else // This is to avoid crash on android devices getting JavaVM as null
    {
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "javaVM NULL");
        return ALC_TRUE;
    }

    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback AttachCurrentThread DONE");

    AndroidData* data;
    int channels;
    int bytes;

    if (!cAudioTrack)
    {
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback cAudioTrack not NULL");

        /* Cache AudioTrack class and it's method id's
         * And do this only once!
         */

        cAudioTrack = (*env)->FindClass(env, "android/media/AudioTrack");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback cAudioTrack %c",cAudioTrack);
        if (!cAudioTrack)
        {
            __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback cAudioTrack Not found");
            TRACE("android.media.AudioTrack class is not found. Are you running at least 1.5 version?");
            return ALC_FALSE;
        }

        cAudioTrack = (*env)->NewGlobalRef(env, cAudioTrack);
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback cAudioTrack NewGlobalRef %p",cAudioTrack);

        mAudioTrack = (*env)->GetMethodID(env, cAudioTrack, "<init>", "(IIIIII)V");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mAudioTrack %p",mAudioTrack);

        mGetMinBufferSize = (*env)->GetStaticMethodID(env, cAudioTrack, "getMinBufferSize", "(III)I");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mGetMinBufferSize %d",mGetMinBufferSize);

        mPlay = (*env)->GetMethodID(env, cAudioTrack, "play", "()V");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mPlay %s",mPlay);

        mPause = (*env)->GetMethodID(env, cAudioTrack, "pause", "()V");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mPause %s",mPause);

        mStop = (*env)->GetMethodID(env, cAudioTrack, "stop", "()V");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mStop %s",mStop);

        mRelease = (*env)->GetMethodID(env, cAudioTrack, "release", "()V");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mRelease %s",mRelease);

        mWrite = (*env)->GetMethodID(env, cAudioTrack, "write", "([BII)I");
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback mWrite %s",mWrite);
    }


    if (!deviceName)
    {
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback before deviceName %s",deviceName);
        deviceName = android_device;
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback after deviceName %s",deviceName);
    }
    else if (strcmp(deviceName, android_device) != 0)
    {
        __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback else return ALC_FALSE");
        return ALC_FALSE;
    }

    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback before data");
    data = (AndroidData*)calloc(1, sizeof(*data));
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback after data %p",data);
    device->DeviceName = strdup(deviceName);
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback after device %p",device);
    device->ExtraData = data;
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "android_open_playback last return ALC_NO_ERROR");
    return ALC_NO_ERROR;
}

static void android_close_playback(ALCdevice *device)
{
    AndroidData* data = (AndroidData*)device->ExtraData;
    if (data != NULL)
    {
        free(data);
        device->ExtraData = NULL;
    }
}

static ALCboolean android_reset_playback(ALCdevice *device)
{
    return ALC_TRUE;
}

static ALCboolean android_start_playback(ALCdevice *device)
{
    AndroidData* data = (AndroidData*)device->ExtraData;

    SetDefaultChannelOrder(device);

    data->running = 1;
    pthread_create(&data->thread, NULL, thread_function, device);

    return ALC_TRUE;
}


static void android_stop_playback(ALCdevice *device)
{
    AndroidData* data = (AndroidData*)device->ExtraData;

    if (data->running)
    {
        data->running = 0;
        pthread_join(data->thread, NULL);
    }
    suspended = 0;
}

static ALCenum android_open_capture(ALCdevice *pDevice, const ALCchar *deviceName)
{
    (void)pDevice;
    (void)deviceName;
    return ALC_NO_ERROR;
}

static void android_close_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static void android_start_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static void android_stop_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static ALCenum android_capture_samples(ALCdevice *pDevice, ALCvoid *pBuffer, ALCuint lSamples)
{
    (void)pDevice;
    (void)pBuffer;
    (void)lSamples;
    return ALC_NO_ERROR;
}

static ALCuint android_available_samples(ALCdevice *pDevice)
{
    (void)pDevice;
    return 0;
}

void android_device_lock(ALCdevice *pDevice)
{
    pthread_mutex_lock(&(deviceMutex));
}

void android_device_unlock(ALCdevice *pDevice)
{
    pthread_mutex_unlock(&(deviceMutex));
}

ALint64 android_get_latency(ALCdevice *pDevice)
{
    TRACE("android_get_latency device=%p", pDevice);
    return 0;
}

static const BackendFuncs android_funcs = {
    android_open_playback,
    android_close_playback,
    android_reset_playback,
    android_start_playback,
    android_stop_playback,

    android_open_capture,
    android_close_capture,
    android_start_capture,
    android_stop_capture,

    android_capture_samples,
    android_available_samples,

    android_device_lock,
    android_device_unlock,

    android_get_latency
};

static void alc_audiotrack_suspend()
{
    suspended = 1;
    audioTrackWasPlaying = audioTrackPlaying;
}

static void alc_audiotrack_resume()
{
    suspended = 0;
    if (audioTrackWasPlaying)
    {
        while (!audioTrackPlaying)
        {
            sched_yield();
        }
        audioTrackWasPlaying = 0;
    }
}


ALCboolean alc_audiotrack_init(BackendFuncs *func_list)
{
    *func_list = android_funcs;

    if (androidOpenALFuncs.alc_android_suspend == NULL
		&& androidOpenALFuncs.alc_android_set_java_vm == NULL) {
        androidOpenALFuncs.alc_android_suspend = alc_audiotrack_suspend;
        androidOpenALFuncs.alc_android_resume = alc_audiotrack_resume;
    }

    return ALC_TRUE;
}

void alc_audiotrack_deinit(void)
{
//    CallBackTrace("alc_audiotrack_deinit");
    /* release cached AudioTrack class */
    JavaVM *javaVM = alcGetJavaVM();
    __android_log_print(ANDROID_LOG_INFO, "=== WD-AudioTrack ===", "alc_audiotrack_deinit javaVM Address  %p JavaVM %p ",&javaVM, javaVM);
    (*env)->DeleteGlobalRef(env, cAudioTrack);
    (*javaVM)->DetachCurrentThread(javaVM);
    cAudioTrack = NULL;
}

void alc_audiotrack_probe(enum DevProbe type)
{
    switch(type) {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(android_device);
            break;

        case CAPTURE_DEVICE_PROBE:
            // No supported
            break;
    }
}
