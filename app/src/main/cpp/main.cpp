#include <android/log.h>
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <android/looper.h>
#include <android/input.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include "platform.h"
#include "game.h"

static android_app* g_app = nullptr;
static Game* g_game = nullptr;
static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static bool g_active = false;
static int g_w = 0, g_h = 0;

bool plat_read_asset(const char* path, std::vector<uint8_t>& out) {
    if (!g_app) return false;
    AAsset* a = AAssetManager_open(g_app->activity->assetManager, path, AASSET_MODE_BUFFER);
    if (!a) return false;
    off_t n = AAsset_getLength(a);
    out.resize((size_t)n);
    size_t got = 0;
    while (got < (size_t)n) {
        int r = AAsset_read(a, out.data() + got, (size_t)n - got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    AAsset_close(a);
    return got == (size_t)n;
}

void plat_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "NatureGame", fmt, ap);
    va_end(ap);
}

std::string plat_data_dir() {
    if (g_app && g_app->activity && g_app->activity->internalDataPath) return g_app->activity->internalDataPath;
    return "/data/local/tmp";
}

double plat_time() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void setImmersive() {
    if (!g_app || !g_app->activity || !g_app->activity->vm) return;
    JavaVM* vm = g_app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
    jobject act = g_app->activity->clazz;
    jclass actCls = env->GetObjectClass(act);
    jmethodID getWindow = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
    if (getWindow) {
        jobject window = env->CallObjectMethod(act, getWindow);
        if (window) {
            jclass winCls = env->GetObjectClass(window);
            jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
            if (getDecor) {
                jobject decor = env->CallObjectMethod(window, getDecor);
                if (decor) {
                    jclass viewCls = env->GetObjectClass(decor);
                    jmethodID setVis = env->GetMethodID(viewCls, "setSystemUiVisibility", "(I)V");
                    if (setVis) env->CallVoidMethod(decor, setVis, (jint)0x1706);
                }
            }
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    vm->DetachCurrentThread();
}

static bool initEGL(ANativeWindow* win) {
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, nullptr, nullptr)) return false;
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    int depths[2] = {24, 16};
    for (int i = 0; i < 2 && n == 0; i++) {
        const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                  EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, depths[i], EGL_NONE};
        eglChooseConfig(g_dpy, attribs, &cfg, 1, &n);
    }
    if (n == 0) return false;
    EGLint fmt = 0;
    eglGetConfigAttrib(g_dpy, cfg, EGL_NATIVE_VISUAL_ID, &fmt);
    ANativeWindow_setBuffersGeometry(win, 0, 0, fmt);
    g_surf = eglCreateWindowSurface(g_dpy, cfg, win, nullptr);
    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (g_surf == EGL_NO_SURFACE || g_ctx == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) return false;
    eglSwapInterval(g_dpy, 1);
    eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w);
    eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
    return true;
}

static void termEGL() {
    if (g_dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_dpy, g_ctx);
        if (g_surf != EGL_NO_SURFACE) eglDestroySurface(g_dpy, g_surf);
        eglTerminate(g_dpy);
    }
    g_dpy = EGL_NO_DISPLAY;
    g_surf = EGL_NO_SURFACE;
    g_ctx = EGL_NO_CONTEXT;
}

static void onCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                setImmersive();
                if (initEGL(app->window)) {
                    const char* r = (const char*)glGetString(GL_RENDERER);
                    g_game->initGL(g_w, g_h, r);
                } else {
                    plat_log("EGL init failed");
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            g_game->termGL();
            termEGL();
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) {
                eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w);
                eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
                g_game->resize(g_w, g_h);
            }
            break;
        case APP_CMD_GAINED_FOCUS:
            setImmersive();
            g_active = true;
            break;
        case APP_CMD_LOST_FOCUS:
            g_active = false;
            break;
        case APP_CMD_RESUME:
            g_active = true;
            break;
        case APP_CMD_PAUSE:
            g_active = false;
            break;
        default:
            break;
    }
}

static int32_t onInput(android_app*, AInputEvent* ev) {
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t actionRaw = AMotionEvent_getAction(ev);
    int32_t action = actionRaw & AMOTION_EVENT_ACTION_MASK;
    int32_t idx = (actionRaw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            g_game->touch(0, AMotionEvent_getPointerId(ev, (size_t)idx), AMotionEvent_getX(ev, (size_t)idx), AMotionEvent_getY(ev, (size_t)idx));
            break;
        case AMOTION_EVENT_ACTION_MOVE: {
            size_t n = AMotionEvent_getPointerCount(ev);
            for (size_t i = 0; i < n; i++)
                g_game->touch(1, AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i));
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            g_game->touch(2, AMotionEvent_getPointerId(ev, (size_t)idx), AMotionEvent_getX(ev, (size_t)idx), AMotionEvent_getY(ev, (size_t)idx));
            break;
        case AMOTION_EVENT_ACTION_CANCEL: {
            size_t n = AMotionEvent_getPointerCount(ev);
            for (size_t i = 0; i < n; i++) g_game->touch(2, AMotionEvent_getPointerId(ev, i), 0, 0);
            break;
        }
        default:
            break;
    }
    return 1;
}

void android_main(android_app* app) {
    g_app = app;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    Game game;
    g_game = &game;
    while (!app->destroyRequested) {
        int events;
        android_poll_source* src;
        bool run = g_active && game.glReady();
        int timeout = run ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
            timeout = (g_active && game.glReady()) ? 0 : -1;
        }
        if (app->destroyRequested) break;
        if (g_active && game.glReady()) {
            double t0 = plat_time();
            game.frame();
            eglSwapBuffers(g_dpy, g_surf);
            int fps = game.targetFps();
            if (fps <= 30) {
                double budget = 1.0 / (double)fps;
                double left = budget - (plat_time() - t0);
                if (left > 0.001) {
                    timespec ts;
                    ts.tv_sec = 0;
                    ts.tv_nsec = (long)((left - 0.0005) * 1e9);
                    nanosleep(&ts, nullptr);
                }
            }
        }
    }
    game.termGL();
    termEGL();
    g_game = nullptr;
}
