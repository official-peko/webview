#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "os_generic.h"
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android_native_app_glue.h>
#include <android/sensor.h>
#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include "CNFGAndroid.h"

#define CNFG_IMPLEMENTATION
#define CNFG3D

#include "CNFG.h"

#define WEBVIEW_NATIVE_ACTIVITY_IMPLEMENTATION
#include "webview_native_activity.h"

float mountainangle;
float mountainoffsetx;
float mountainoffsety;

ASensorManager *sm;
const ASensor *as;
bool no_sensor_for_gyro = false;
ASensorEventQueue *aeq;
ALooper *l;

WebViewNativeActivityObject MyWebView;

const uint32_t SAMPLE_RATE = 44100;
const uint16_t SAMPLE_COUNT = 512;
uint32_t stream_offset = 0;
uint16_t audio_frequency;

unsigned long iframeno = 0;

void AndroidDisplayKeyboard(int pShow);

int lastbuttonx = 0;
int lastbuttony = 0;
int lastmotionx = 0;
int lastmotiony = 0;
int lastbid = 0;
int lastmask = 0;
int lastkey, lastkeydown;

static int keyboard_up;
uint8_t buttonstate[8];

void HandleKey(int keycode, int bDown)
{
	lastkey = keycode;
	lastkeydown = bDown;
	if (keycode == 10 && !bDown)
	{
		keyboard_up = 0;
		AndroidDisplayKeyboard(keyboard_up);
	}

	if (keycode == 4)
	{
		AndroidSendToBack(1);
	} // Handle Physical Back Button.
}

void HandleButton(int x, int y, int button, int bDown)
{
	buttonstate[button] = bDown;
	lastbid = button;
	lastbuttonx = x;
	lastbuttony = y;

	if (bDown)
	{
		keyboard_up = !keyboard_up;
		AndroidDisplayKeyboard(keyboard_up);
	}
}

void HandleMotion(int x, int y, int mask)
{
	lastmask = mask;
	lastmotionx = x;
	lastmotiony = y;
}

extern struct android_app *gapp;

int HandleDestroy()
{
	printf("Destroying\n");
	return 0;
}

volatile int suspended;

void HandleSuspend()
{
	suspended = 1;
}

void HandleResume()
{
	suspended = 0;
}

void HandleThisWindowTermination()
{
	suspended = 1;
}

short webview_width;
short webview_height;

uint32_t randomtexturedata[256 * 256];
uint32_t *webviewdata;

jobject g_attachLooper;

void SetupWebView(void *v)
{
	WebViewNativeActivityObject *wvn = (WebViewNativeActivityObject *)v;

	const struct JNINativeInterface *env = 0;
	const struct JNINativeInterface **envptr = &env;
	const struct JNIInvokeInterface **jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface *jnii = *jniiptr;

	jnii->AttachCurrentThread(jniiptr, &envptr, NULL);
	env = (*envptr);

	while (g_attachLooper == 0)
		usleep(1);
	WebViewCreate(wvn, "about:blank", g_attachLooper, webview_width, webview_height);
}

pthread_t jsthread;
void *JavscriptThread(void *v)
{
	const struct JNINativeInterface *env = 0;
	const struct JNINativeInterface **envptr = &env;
	const struct JNIInvokeInterface **jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface *jnii = *jniiptr;

	jnii->AttachCurrentThread(jniiptr, &envptr, NULL);
	env = (*envptr);

	// Create a looper on this thread...
	jclass LooperClass = env->FindClass(envptr, "android/os/Looper");
	jmethodID myLooperMethod = env->GetStaticMethodID(envptr, LooperClass, "myLooper", "()Landroid/os/Looper;");
	jobject thisLooper = env->CallStaticObjectMethod(envptr, LooperClass, myLooperMethod);
	if (!thisLooper)
	{
		jmethodID prepareMethod = env->GetStaticMethodID(envptr, LooperClass, "prepare", "()V");
		env->CallStaticVoidMethod(envptr, LooperClass, prepareMethod);
		thisLooper = env->CallStaticObjectMethod(envptr, LooperClass, myLooperMethod);
		g_attachLooper = env->NewGlobalRef(envptr, thisLooper);
	}

	return 0;
}

void SetupJSThread()
{
	pthread_create(&jsthread, 0, JavscriptThread, 0);
}

extern void *gc_alloc(int size);

bool wv_initialized = false;

void MainWVSetURL(char *url)
{
	while (!wv_initialized)
	{
	}
	MyWebView.url = url;
	RunCallbackOnUIThread((void (*)(void *))WebViewSetURL, &MyWebView);
}

void CreateMainWV(char *appname)
{
	// CNFGBGColor = 0x000040ff;
	CNFGSetupFullscreen(appname, 0);

	HandleWindowTermination = HandleThisWindowTermination;

	SetupJSThread();

	CNFGGetDimensions(&webview_width, &webview_height);
	webviewdata = gc_alloc(sizeof(uint32_t) * webview_width * webview_height);

	// Create webview and wait for its completion
	RunCallbackOnUIThread(SetupWebView, &MyWebView);
	while (!MyWebView.WebViewObject)
		usleep(1);

	int initial_width = webview_width;
	int initial_height = webview_height;

	RunCallbackOnUIThread((void (*)(void *))WebViewSetBG, &MyWebView);

	while (1)
	{
		int i;
		iframeno++;

		CNFGHandleInput();

		if (suspended)
		{
			usleep(50000);
			continue;
		}

		CNFGGetDimensions(&webview_width, &webview_height);
		MyWebView.w = webview_width;
		MyWebView.h = webview_height;

		if (initial_width != webview_width || initial_height != webview_height)
		{
			RunCallbackOnUIThread((void (*)(void *))WebviewResize, &MyWebView);

			initial_width = webview_width;
			initial_height = webview_height;
		}
		
		CNFGClearFrame();
		RunCallbackOnUIThread((void (*)(void *))WebViewRequestRenderToCanvas, &MyWebView);
		
		memset(webviewdata, 0, sizeof(uint32_t) * webview_width * webview_height);
		WebViewNativeGetPixels(&MyWebView, webviewdata, webview_width, webview_height);
		CNFGBlitImage(webviewdata, 0, 0, webview_width, webview_height);

		CNFGSwapBuffers();

		if (!wv_initialized)
		{
			wv_initialized = true;
		}
	}

	return;
}
