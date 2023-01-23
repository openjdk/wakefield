/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifdef HEADLESS
#error This file should not be included in headless library
#endif

#include "jni_util.h"
#include "awt_p.h"
#include "awt.h"
#include "screencast_pipewire.h"
#include <stdio.h>

#include "gtk_interface.h"
#include "gtk3_interface.h"

#include <semaphore.h>
#include <unistd.h>
#include <sys/syscall.h>

static volatile int init_complete = FALSE;
extern GString *restoreTokenPath;
extern GString *restoreToken;

int DEBUG_SCREENCAST_ENABLE = FALSE;

static int pwFd = -1;
struct ScreenSpace monSpace = {0};

pid_t getTid() {
    return syscall(__NR_gettid);
}

void debug_screencast(
        const char *__restrict fmt,
        ...
) {
    if (DEBUG_SCREENCAST_ENABLE) {
        va_list myargs;
        va_start(myargs, fmt);
        vfprintf(stderr, fmt, myargs);
        va_end(myargs);
    }
}


void printRect(
        GdkRectangle gdkRectangle,
        char *text
) {
    debug_screencast(
            "%s\n"
            "||\t               x %5i y %5i w %5i h %5i\n",
            text,
            gdkRectangle.x, gdkRectangle.y,
            gdkRectangle.width, gdkRectangle.height
    );
}

void printScreen(struct ScreenProps* mon) {
    debug_screencast(
            "Display nodeID %i \n"
            "||\tbounds         x %5i y %5i w %5i h %5i\n"
            "||\tcapture area   x %5i y %5i w %5i h %5i shouldCapture %i\n",

            mon->id,
            mon->bounds.x,          mon->bounds.y,
            mon->bounds.width,      mon->bounds.height,
            mon->captureArea.x,     mon->captureArea.y,
            mon->captureArea.width, mon->captureArea.height,
            mon->shouldCapture
    );
}

void initMonSpace() {
    monSpace.screenCount = 0;
    monSpace.allocated = SCREEN_SPACE_DEFAULT_ALLOCATED;
    monSpace.screens = calloc(SCREEN_SPACE_DEFAULT_ALLOCATED, sizeof(struct ScreenProps));
}


void initScreencast() {
    pwFd = -1;
    debug_screencast("%s:%i \n", __FUNCTION__, __LINE__);
    initMonSpace();
    initXdgDesktopPortal();
    pwFd = getPipewireFd();
}

static inline void convertBGRxToRGBA(int* in) {
    char* o = (char*) in;
    char tmp = o[0];
    o[0] = o[2];
    o[2] = tmp;

// TODO or XOR?
//    if (o[0] != o[2]) {
//        o[0] ^= o[2];
//        o[2] ^= o[0];
//        o[0] ^= o[2];
//    }
}

static gchar * cropTo(
        struct spa_data data,
        struct spa_video_info info,
        guint32 x, //TODO int?
        guint32 y,
        guint32 width,
        guint32 height
) {
    debug_screencast("%s:%i ______ stride %i %i\n", __FUNCTION__, __LINE__, data.chunk->stride, data.chunk->stride / 4);
    int srcW = info.info.raw.size.width;
    if (data.chunk->stride / 4 != srcW) {
        fprintf(stderr, "%s:%i Unexpected stride / 4: %i srcW: %i\n",
                __FUNCTION__, __LINE__, data.chunk->stride / 4, srcW);
    }


    int* d = data.data;

    int* outData = calloc(width * height, sizeof (int));

    for (guint32 j = y; j < y + height; ++j) {
        for (guint32 i = x; i < x + width; ++i) {
            int color = *(d + (j * srcW) + i);
//            convertBGRxToRGBA(&color);
            *(outData + ((j - y) * width) + (i - x)) = color;
        }
    }


    return (gchar*) outData;
}

static void on_param_changed(
        void *userdata,
        uint32_t id,
        const struct spa_pod *param
) {
    struct data_pw *data = userdata;
    debug_screencast(
            "%s:%i ===================================== %i\n",
            __FUNCTION__, __LINE__, id
    );
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param,
                         &data->format.media_type,
                         &data->format.media_subtype) < 0)
        return;

    if (data->format.media_type != SPA_MEDIA_TYPE_video ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    if (spa_format_video_raw_parse(param, &data->format.info.raw) < 0)
        return;

    debug_screencast("got video format:\n");
    debug_screencast("  format: %d (%s)\n", data->format.info.raw.format,
                     spa_debug_type_find_name(spa_type_video_format,
                                    data->format.info.raw.format));
    debug_screencast("  size: %dx%d\n", data->format.info.raw.size.width,
                     data->format.info.raw.size.height);
    debug_screencast("  framerate: %d/%d\n", data->format.info.raw.framerate.num,
                     data->format.info.raw.framerate.denom);
}

static void on_process(void *userdata) {
    struct data_pw *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;

    struct ScreenProps* props = data->screenProps;
    debug_screencast("%s:%i screenProps %p\n", __FUNCTION__, __LINE__, props);
    printScreen(props);
    GdkRectangle *capture = &props->captureArea;
    debug_screencast("%s:%i shortcut saved %i shouldCapture %i id %i %i %i %i %i\n", __FUNCTION__, __LINE__,
                     data->saved, data->screenProps->shouldCapture, data->screenProps->id,
                     capture->x, capture->y,
                     capture->width, capture->height);
    if (data->saved || !data->screenProps->shouldCapture) {
        return;
    }

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    //TODO buf->n_datas  check?
    buf = b->buffer;
    struct spa_data buffer = buf->datas[0];
    if (buf->datas[0].data == NULL)
        return;

    debug_screencast("got a frame of size %d/%lu of %d st %d fl %d FD %li SAVED %i %i\n",
                     buf->datas[0].chunk->size,
                     b->size,
                     buffer.chunk->offset, //TODO consider offset
           buffer.chunk->stride,
                     buffer.chunk->flags,
                     buffer.fd,
                     data->saved,
                     data->screenProps->id
    );


    if (!data->saved) {
        //TODO format check BGRx? other?
        GdkRectangle capture = data->screenProps->captureArea;

        data->screenProps->captureData = cropTo(
                buffer,
                data->format,
                capture.x, capture.y,
                capture.width, capture.height
        );

        debug_screencast(
                "%s:%i data->screenProps %p\n",
                __FUNCTION__, __LINE__, data->screenProps
        );


        debug_screencast("%s:%i data ready\n", __FUNCTION__, __LINE__);
        sem_post(&data->screenProps->captureDataReady);

        data->saved = TRUE;
        debug_screencast("%s:%i Got image\n", __FUNCTION__, __LINE__);
    }

    pw_stream_queue_buffer(data->stream, b);
    debug_screencast("⚠⚠⚠ %s:%i disabling stream %p\n", __FUNCTION__, __LINE__, data->stream);
    pw_stream_set_active(data->stream, false);
}

static void on_state_changed(
        void *userdata,
        enum pw_stream_state old,
        enum pw_stream_state state,
        const char *error
) {
    struct data_pw *data = userdata;
    debug_screencast(
            "%s:%i width %i old %i (%s) new %i (%s) err %s on %p\n",
            __FUNCTION__, __LINE__,
            data->screenProps->bounds.width,
            old, pw_stream_state_as_string(old),
            state, pw_stream_state_as_string(state),
            error, getTid()
    );
}

static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = on_param_changed,
        .process = on_process,
        .state_changed = on_state_changed,
};

static struct pw_main_loop *theLoop;;

void connectStream(struct pw_main_loop *loop, int index) {

    //TODO realloc? leak?
    //TODO currently freed in on_state_changed?
    struct data_pw *data = (struct data_pw*) malloc(sizeof (struct data_pw));
    memset(data, 0, sizeof (struct data_pw));

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    data->loop = loop;

    data->stream = pw_stream_new_simple(
            pw_main_loop_get_loop(data->loop),
            "AWT-video-capture",
            pw_properties_new(
                    PW_KEY_MEDIA_TYPE, "Video",
                    PW_KEY_MEDIA_CATEGORY, "Capture",
                    PW_KEY_MEDIA_ROLE, "Screen",
                    NULL),
            &stream_events,
            data);

    debug_screencast("%s:%i /// stream %p\n", __FUNCTION__, __LINE__, data->stream);

    params[0] = spa_pod_builder_add_object(
            &b,
            SPA_TYPE_OBJECT_Format,
            SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType,
            SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype,
            SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(
                    7,
                    SPA_VIDEO_FORMAT_RGB,
                    SPA_VIDEO_FORMAT_RGB,
                    SPA_VIDEO_FORMAT_RGBA,
                    SPA_VIDEO_FORMAT_RGBx,
                    SPA_VIDEO_FORMAT_BGRx,
                    SPA_VIDEO_FORMAT_YUY2,
                    SPA_VIDEO_FORMAT_I420
            ),
            SPA_FORMAT_VIDEO_size,
            SPA_POD_CHOICE_RANGE_Rectangle(
                    &SPA_RECTANGLE(320, 240),
                    &SPA_RECTANGLE(1, 1),
                    &SPA_RECTANGLE(8192, 8192)
            ),
            SPA_FORMAT_VIDEO_framerate,
            SPA_POD_CHOICE_RANGE_Fraction(
                    &SPA_FRACTION(25, 1),
                    &SPA_FRACTION(0, 1),
                    &SPA_FRACTION(1000, 1)
            )
    );

    data->screenProps = &monSpace.screens[index];

    debug_screencast("%s:%i #### screenProps %p\n", __FUNCTION__, __LINE__, &data->screenProps);
    printScreen(data->screenProps);

    monSpace.screens[index].data = data;

    pw_stream_connect(
            data->stream,
            PW_DIRECTION_INPUT,
            monSpace.screens[index].id,
            PW_STREAM_FLAG_AUTOCONNECT
            | PW_STREAM_FLAG_INACTIVE
            | PW_STREAM_FLAG_MAP_BUFFERS,
            params, 1
    );
}


void findScreens(GdkRectangle props) {
    debug_screencast("%s:%i searching x %i y %i w %i h %i\n", __FUNCTION__, __LINE__,
                     props.x, props.y,
                     props.width, props.height
    );

    for (int i = 0; i < monSpace.screenCount; ++i) {
        struct ScreenProps * mon = &monSpace.screens[i];


        //TODO optimize
        int x1 = MAX(props.x, mon->bounds.x);
        int y1 = MAX(props.y, mon->bounds.y);

        int x2 = MIN(props.x + props.width,  mon->bounds.x + mon->bounds.width);
        int y2 = MIN(props.y + props.height, mon->bounds.y + mon->bounds.height);

        mon->shouldCapture = x2 > x1 && y2 > y1;

        debug_screencast("%s:%i checking id %i x %i y %i w %i h %i shouldCapture %i\n", __FUNCTION__, __LINE__,
                         mon->id,
                         mon->bounds.x, mon->bounds.y,
                         mon->bounds.width, mon->bounds.height,
                         mon->shouldCapture
        );

        if (mon->shouldCapture) {  //intersects
            mon->data->saved = false;
            //in screen coords:
            GdkRectangle * captureArea = &(mon->captureArea);

            captureArea->x = x1 - mon->bounds.x;
            captureArea->y = y1 - mon->bounds.y;
            captureArea->width = x2 - x1;
            captureArea->height = y2 - y1;

            mon->captureArea.x = x1 - mon->bounds.x;

            debug_screencast("\t\tintersection %i %i %i %i should capture %i\n",
                             captureArea->x, captureArea->y,
                             captureArea->width, captureArea->height,
                             mon->shouldCapture);


            printScreen(mon);
        } else {
            debug_screencast("%s:%i OUCH no intersection\n", __FUNCTION__, __LINE__);
        }
    }
}



/*
 * Class:     sun_awt_screencast_ScreencastHelper
 * Method:    getRGBPixelsImpl
 * Signature: (IIII[I)V
 */
JNIEXPORT void JNICALL Java_sun_awt_screencast_ScreencastHelper_getRGBPixelsImpl(
        JNIEnv *env,
        jclass cls,
        jint jx,
        jint jy,
        jint jwidth,
        jint jheight,
        jintArray pixelArray
) {
    debug_screencast(
            "%s:%i taking screenshot at x: %5i y %5i w %5i h %5i\n",
            __FUNCTION__, __LINE__, jx, jy, jwidth, jheight
    );

    GdkRectangle requestedArea = { jx, jy, jwidth, jheight};
    findScreens(requestedArea);

    for (int i = 0; i < monSpace.screenCount; ++i) {
        struct ScreenProps * props = &monSpace.screens[i];

        if (props->shouldCapture) {
            debug_screencast("%s:%i @@@ resuming mon %i stream %p \n", __FUNCTION__, __LINE__, i, props->data->stream );
            sem_init(&props->captureDataReady, 0, 0);
            int ret = pw_stream_set_active(props->data->stream, true);
            debug_screencast("⚠⚠⚠ pw_stream_set_active %i\n", ret);
        }
    }

    debug_screencast("%s:%i data ready$$\n", __FUNCTION__, __LINE__);
    AWT_LOCK();
    for (int i = 0; i < monSpace.screenCount; ++i) {
        struct ScreenProps * screenProps = &monSpace.screens[i];

        if (screenProps->shouldCapture) {
            debug_screencast("%s:%i @@@ getting data mon %i stream %p \n", __FUNCTION__, __LINE__, i, screenProps->data->stream );

            debug_screencast("%s:%i waiting for data$$ on %p\n", __FUNCTION__, __LINE__, getTid());
            sem_wait(&screenProps->captureDataReady);
////TODO hangs sometimes?
//            {
//                int s;
//                struct timespec ts;
//
//                if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
//                    debug_screencast("⚠⚠⚠ clock_gettime failed\n");
//                }
//
//                ts.tv_sec += 5;
//
//                while (
//                    (s = sem_timedwait(&screenProps->captureDataReady, &ts)) == -1
//                    && errno == EINTR
//                ) {
//                    continue;
//                }
//
//                if (s == -1) {
//                    debug_screencast("⚠⚠⚠\n");
//                    debug_screencast("⚠⚠⚠\n");
//                    debug_screencast("⚠⚠⚠ data time out\n");
//                    debug_screencast("⚠⚠⚠\n");
//                    debug_screencast("⚠⚠⚠\n");
//                    if (errno == ETIMEDOUT) {
//                        printf("sem_timedwait() timed out\n");
//                    } else {
//                        perror("sem_timedwait");
//                    }
//
//                }
//            }
            sem_destroy(&screenProps->captureDataReady);

            GdkRectangle captureArea  = screenProps->captureArea;

            printRect(requestedArea, "requestedArea");
            printRect(screenProps->bounds, "screen bounds");
            printRect(captureArea, "in-screen coords capture area");

            debug_screencast("%s:%i screenProps->captureData %p\n", __FUNCTION__, __LINE__, screenProps->captureData);


            for (int y = 0; y < captureArea.height; y++) {
                jsize preY = (requestedArea.y > screenProps->bounds.y)
                        ? 0
                        : screenProps->bounds.y - requestedArea.y;
                jsize preX = (requestedArea.x > screenProps->bounds.x)
                        ? 0
                        : screenProps->bounds.x - requestedArea.x;
                jsize start = jwidth * (preY + y) + preX;

                jsize len = captureArea.width;


                (*env)->SetIntArrayRegion(
                        env, pixelArray,
                        start, len,
                        ((jint *) screenProps->captureData) + (captureArea.width * y)
                );
            }

            free(screenProps->captureData);
            screenProps->captureData = NULL;

            screenProps->shouldCapture = false;

            pw_stream_set_active(screenProps->data->stream, false);
            pw_stream_disconnect(screenProps->data->stream);
        }
    }

    AWT_UNLOCK();
}

/*
 * Class:     sun_awt_screencast_ScreencastHelper
 * Method:    initScreencast
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_sun_awt_screencast_ScreencastHelper_initScreencast(
        JNIEnv * env, jclass class,
        jboolean screencastDebug
) {

    DEBUG_SCREENCAST_ENABLE = screencastDebug;

    debug_screencast("⚠⚠⚠ %s:%i init_complete %i\n", __FUNCTION__, __LINE__, init_complete);
    if (!init_complete) {
        initRestoreToken();

        debug_screencast(
            "%s:%i using restore token location: %s\n",
            __FUNCTION__, __LINE__, restoreTokenPath->str
        );

        initScreencast();

        debug_screencast("%s:%i %i\n", __FUNCTION__, __LINE__, pwFd);

        pw_init(NULL, NULL);
        init_complete = TRUE;

        theLoop = pw_main_loop_new(NULL);

        for (int i = 0; i < monSpace.screenCount; ++i) {
            debug_screencast("%s:%i @@@ adding mon %i\n", __FUNCTION__, __LINE__, i );
            connectStream(theLoop, i);
        }
    }

    debug_screencast("%s:%i new loop %p\n", __FUNCTION__, __LINE__, theLoop);
}


/*
 * Class:     sun_awt_screencast_ScreencastHelper
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_awt_screencast_ScreencastHelper_start(
        JNIEnv * env,
        jclass class
) {

    debug_screencast("⚠⚠⚠ %s:%i pw_main_loop_run %p\n", __FUNCTION__, __LINE__, theLoop);
    pw_main_loop_run(theLoop);
    debug_screencast("⚠⚠⚠ %s:%i EXITED pw_main_loop_run\n", __FUNCTION__, __LINE__);
}

/*
 * Class:     sun_awt_screencast_ScreencastHelper
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_awt_screencast_ScreencastHelper_stop(
        JNIEnv * env,   
        jclass class
) {
    debug_screencast("⚠⚠⚠ %s:%i STOPPING %p\n", __FUNCTION__, __LINE__, theLoop);
    if (theLoop) {
        for (int i = 0; i < monSpace.screenCount; ++i) {
            struct ScreenProps *screenProps = &monSpace.screens[i];
            pw_stream_destroy(screenProps->data->stream);
        }
        close(pwFd);
        pwFd = -1;

        portalScreenCastCleanup();

        pw_main_loop_quit(theLoop);
        theLoop = NULL;
        init_complete = FALSE;
    }
}
