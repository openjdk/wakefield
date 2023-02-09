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

extern GString *restoreTokenPath;
extern GString *restoreToken;

int DEBUG_SCREENCAST_ENABLE = FALSE;

struct ScreenSpace monSpace = {0};
static struct PwLoopData pw = {0};

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


void debug_rectangle(
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

void debug_screen(struct ScreenProps* mon) {
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

static void initMonSpace() {
    monSpace.screenCount = 0;
    monSpace.allocated = SCREEN_SPACE_DEFAULT_ALLOCATED;
    monSpace.screens = calloc(
            SCREEN_SPACE_DEFAULT_ALLOCATED,
            sizeof(struct ScreenProps)
    );
}

static void doCleanup() {
    for (int i = 0; i < monSpace.screenCount; ++i) {
        struct ScreenProps *screenProps = &monSpace.screens[i];
        if (screenProps->data) {
            if (screenProps->data->stream) {
                pw_stream_disconnect(screenProps->data->stream);
                pw_stream_destroy(screenProps->data->stream);
                screenProps->data->stream = NULL;
            }
            free(screenProps->data);
            screenProps->data = NULL;
        }
    }

    if (pw.pwFd > 0) {
        close(pw.pwFd);
        pw.pwFd = -1;
    }

    portalScreenCastCleanup();

    if (pw.core) {
        pw_core_disconnect(pw.core);
        pw.core = NULL;
    }

    debug_screencast(
            "⚠⚠⚠ %s:%i STOPPING %p\n",
            __FUNCTION__, __LINE__,
            pw.loop
    );

    if (pw.loop) {
        pw_thread_loop_stop(pw.loop);
        pw_thread_loop_destroy(pw.loop);
        pw.loop = NULL;
    }

    if (monSpace.screens) {
        free(monSpace.screens);
        monSpace.screens = NULL;
    }
}

static gboolean initScreencast() {
    pw_init(NULL, NULL);

    pw.pwFd = -1;

    initMonSpace();

    if (!initXdgDesktopPortal()) {
        doCleanup();
        return FALSE;
    }
    if ((pw.pwFd = getPipewireFd()) < 0) {
        doCleanup();
        return FALSE;
    }
    return TRUE;
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
        struct spa_video_info_raw raw,
        guint32 x,
        guint32 y,
        guint32 width,
        guint32 height
) {
    debug_screencast("%s:%i ______ stride %i %i\n",
                     __FUNCTION__, __LINE__,
                     data.chunk->stride,
                     data.chunk->stride / 4
                     );
    int srcW = raw.size.width;
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

static void onStreamParamChanged(
        void *userdata,
        uint32_t id,
        const struct spa_pod *param
) {
    struct PwStreamData *data = userdata;
    uint32_t mediaType;
    uint32_t mediaSubtype;

    debug_screencast(
            "%s:%i monId#%i ===================================== id %i\n",
            __FUNCTION__, __LINE__, data->screenProps->id, id
    );

    if (param == NULL || id != SPA_PARAM_Format) {
        return;
    }

    if (spa_format_parse(param,
                         &mediaType,
                         &mediaSubtype) < 0) {
        return;
    }

    if (mediaType != SPA_MEDIA_TYPE_video ||
        mediaSubtype != SPA_MEDIA_SUBTYPE_raw) {
        return;
    }

    if (spa_format_video_raw_parse(param, &data->rawFormat) < 0) {
        return;
    }

    debug_screencast("video format:\n");
    debug_screencast("  format: %d (%s)\n",
                     data->rawFormat.format,
                     spa_debug_type_find_name(
                             spa_type_video_format,
                             data->rawFormat.format
                     ));
    debug_screencast("  size: %dx%d\n",
                     data->rawFormat.size.width,
                     data->rawFormat.size.height);
    debug_screencast("  framerate: %d/%d\n",
                     data->rawFormat.framerate.num,
                     data->rawFormat.framerate.denom);

    debug_screencast(
            "⚠⚠⚠ %s:%i monId#%i hasFormat\n",
            __FUNCTION__, __LINE__, data->screenProps->id
    );

    data->hasFormat = true;
    pw_thread_loop_signal(pw.loop, TRUE);
}

static void onStreamProcess(void *userdata) {
    struct PwStreamData *data = userdata;

    debug_screencast(
            "⚠⚠⚠ %s:%i monId#%i hasFormat %i "
            "captureDataReady %i shouldCapture %i\n",
            __FUNCTION__, __LINE__,
            data->screenProps->id,
            data->hasFormat,
            data->screenProps->captureDataReady,
            data->screenProps->shouldCapture
    );
    if (
            !data->hasFormat
            || !data->screenProps->shouldCapture
            || data->screenProps->captureDataReady
    ) {
        return;
    }

    struct pw_buffer *b;
    struct spa_buffer *buf;

    struct ScreenProps* props = data->screenProps;
    debug_screencast("%s:%i monId#%i screenProps %p\n",
                     __FUNCTION__, __LINE__,
                     data->screenProps->id,
                     props);
    debug_screen(props);
    GdkRectangle captureArea = props->captureArea;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        debug_screencast(
                "⚠⚠⚠ %s:%i out of buffers\n",
                __FUNCTION__, __LINE__
        );
        return;
    }

    //TODO buf->n_datas  check?
    buf = b->buffer;
    struct spa_data buffer = buf->datas[0];
    if (buf->datas[0].data == NULL) {
        return;
    }

    debug_screencast(
            "monId#%i got a frame of size %d/%lu offset %d stride %d "
            "flags %d FD %li captureDataReady %i\n",
            data->screenProps->id,
            buf->datas[0].chunk->size,
            b->size,
            buffer.chunk->offset, //TODO consider offset
            buffer.chunk->stride,
            buffer.chunk->flags,
            buffer.fd,
            data->screenProps->captureDataReady
    );

    //TODO format check BGRx? other?

    data->screenProps->captureData = cropTo(
            buffer,
            data->rawFormat,
            captureArea.x, captureArea.y,
            captureArea.width, captureArea.height
    );

    debug_screencast(
            "%s:%i monId#%i data->screenProps %p\n",
            __FUNCTION__, __LINE__, data->screenProps->id, data->screenProps
    );

    data->screenProps->captureDataReady = TRUE;

    debug_screencast(
            "%s:%i monId#%i data ready\n",
            __FUNCTION__, __LINE__,
            data->screenProps->id
    );
    pw_stream_queue_buffer(data->stream, b);
}

static void onStreamStateChanged(
        void *userdata,
        enum pw_stream_state old,
        enum pw_stream_state state,
        const char *error
) {
    struct PwStreamData *data = userdata;
    debug_screencast(
            "%s:%i monId#%i width %i old %i (%s) new %i (%s) err |%s|\n",
            __FUNCTION__, __LINE__,
            data->screenProps->id,
            data->screenProps->bounds.width,
            old, pw_stream_state_as_string(old),
            state, pw_stream_state_as_string(state),
            error
    );
}

static const struct pw_stream_events streamEvents = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = onStreamParamChanged,
        .process = onStreamProcess,
        .state_changed = onStreamStateChanged,
};


static bool startStream(
        struct pw_stream *stream,
        uint32_t node
) {
    char buffer[1024];
    struct spa_pod_builder builder =
            SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *param;


    param = spa_pod_builder_add_object(
            &builder,
            SPA_TYPE_OBJECT_Format,
            SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType,
            SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype,
            SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(
                    5,
                    SPA_VIDEO_FORMAT_RGB,
                    SPA_VIDEO_FORMAT_RGB,
                    SPA_VIDEO_FORMAT_RGBA,
                    SPA_VIDEO_FORMAT_RGBx,
                    SPA_VIDEO_FORMAT_BGRx
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

    debug_screencast(
            "⚠⚠⚠ %s:%i Connecting to monId#%i of stream %p\n",
            __FUNCTION__, __LINE__,  node, stream
    );

    return pw_stream_connect(
            stream,
            PW_DIRECTION_INPUT,
            node,
            PW_STREAM_FLAG_AUTOCONNECT
//            | PW_STREAM_FLAG_INACTIVE
            | PW_STREAM_FLAG_MAP_BUFFERS,
            &param,
            1
    ) >= 0;
}

/**
 * @param index of a screen
 * @return TRUE on success
 */
static gboolean connectStream(int index) {
    debug_screencast(
            "⚠⚠⚠ %s:%i @@@ using mon %i\n",
            __FUNCTION__, __LINE__,
            index
    );
    if (index >= monSpace.screenCount) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Wrong index for screen\n",
                __FUNCTION__, __LINE__
        );
        return FALSE;
    }

    struct PwStreamData *data = monSpace.screens[index].data;

    data->screenProps = &monSpace.screens[index];

    data->hasFormat = FALSE;


    data->stream = pw_stream_new(
            pw.core,
            "AWT Screen Stream",
            pw_properties_new(
                    PW_KEY_MEDIA_TYPE, "Video",
                    PW_KEY_MEDIA_CATEGORY, "Capture",
                    PW_KEY_MEDIA_ROLE, "Screen",
                    NULL
            )
    );

    if (!data->stream) {
        debug_screencast(
                "⚠⚠⚠ %s:%i monId#%i Could not create a pipewire stream\n",
                __FUNCTION__, __LINE__, data->screenProps->id
        );
        pw_thread_loop_unlock(pw.loop);
        return FALSE;
    }

    pw_stream_add_listener(
            data->stream,
            &data->streamListener,
            &streamEvents,
            data
    );

    debug_screencast(
            "%s:%i #### screenProps %p\n",
            __FUNCTION__, __LINE__,
            &data->screenProps
    );
    debug_screen(data->screenProps);

    if (!startStream(data->stream, monSpace.screens[index].id)){
        debug_screencast(
                "⚠⚠⚠ %s:%i monId#%i Could not start a pipewire stream\n",
                __FUNCTION__, __LINE__, data->screenProps->id
        );
        pw_thread_loop_unlock(pw.loop);
        return FALSE;
    }

    while (!data->hasFormat) {
        pw_thread_loop_wait(pw.loop);
    }

    debug_screencast(
            "⚠⚠⚠ %s:%i monId#%i Frame size       : %dx%d\n",
            __FUNCTION__, __LINE__,
            data->screenProps->id,
            data->rawFormat.size.width, data->rawFormat.size.height
    );

    pw_thread_loop_accept(pw.loop);

    return TRUE;
}

static gboolean checkScreen(int index, GdkRectangle requestedArea) {
    if (index >= monSpace.screenCount) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Wrong index for screen\n",
                __FUNCTION__, __LINE__
        );
        return FALSE;
    }

    struct ScreenProps * mon = &monSpace.screens[index];

    int x1 = MAX(requestedArea.x, mon->bounds.x);
    int y1 = MAX(requestedArea.y, mon->bounds.y);

    int x2 = MIN(
            requestedArea.x + requestedArea.width,
            mon->bounds.x + mon->bounds.width
    );
    int y2 = MIN(
            requestedArea.y + requestedArea.height,
            mon->bounds.y + mon->bounds.height
    );

    mon->shouldCapture = x2 > x1 && y2 > y1;

    debug_screencast(
            "%s:%i checking id %i x %i y %i w %i h %i shouldCapture %i\n",
            __FUNCTION__, __LINE__,
            mon->id,
            mon->bounds.x, mon->bounds.y,
            mon->bounds.width, mon->bounds.height,
            mon->shouldCapture
    );

    if (mon->shouldCapture) {  //intersects
        //in screen coords:
        GdkRectangle * captureArea = &(mon->captureArea);

        captureArea->x = x1 - mon->bounds.x;
        captureArea->y = y1 - mon->bounds.y;
        captureArea->width = x2 - x1;
        captureArea->height = y2 - y1;

        mon->captureArea.x = x1 - mon->bounds.x;

        debug_screencast(
                "\t\tintersection %i %i %i %i should capture %i\n",
                captureArea->x, captureArea->y,
                captureArea->width, captureArea->height,
                mon->shouldCapture
        );


        debug_screen(mon);
        return TRUE;
    } else {
        debug_screencast("%s:%i no intersection\n", __FUNCTION__, __LINE__);
        return FALSE;
    }
}


static void onCoreError(
        void *data,
        uint32_t id,
        int seq,
        int res,
        const char *message
) {
    debug_screencast(
            "⚠⚠⚠ %s:%i pipewire error: id %u, seq: %d, res: %d (%s): %s\n",
            __FUNCTION__, __LINE__,
            id, seq, res, strerror(res), message
    );
}

static const struct pw_core_events coreEvents = {
        PW_VERSION_CORE_EVENTS,
        .error = onCoreError,
};

/**
 *
 * @param requestedArea requested screenshot area
 * @return TRUE on success
 */
static gboolean doLoop(GdkRectangle requestedArea) {
    pw.loop = pw_thread_loop_new("AWT Pipewire Thread", NULL);

    if (!pw.loop) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Could not create a loop\n",
                __FUNCTION__, __LINE__
        );
        doCleanup();
        return FALSE;
    }

    pw.context = pw_context_new(
            pw_thread_loop_get_loop(pw.loop),
            NULL,
            0
    );

    if (!pw.context) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Could not create a pipewire context\n",
                __FUNCTION__, __LINE__
        );
        doCleanup();
        return FALSE;
    }

    if (pw_thread_loop_start(pw.loop) != 0) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Could not start pipewire thread loop\n",
                __FUNCTION__, __LINE__
        );
        doCleanup();
        return FALSE;
    }

    pw_thread_loop_lock(pw.loop);

    pw.core = pw_context_connect_fd(
            pw.context,
            pw.pwFd,
            NULL,
            0
    );

    if (!pw.core) {
        debug_screencast(
                "⚠⚠⚠ %s:%i Could not create pipewire core\n",
                __FUNCTION__, __LINE__
        );
        pw_thread_loop_unlock(pw.loop);
        doCleanup();
        return FALSE;
    }

    pw_core_add_listener(pw.core, &pw.coreListener, &coreEvents, NULL);

    for (int i = 0; i < monSpace.screenCount; ++i) {
        debug_screencast(
                "⚠⚠⚠ %s:%i @@@ adding mon %i\n",
                __FUNCTION__, __LINE__,
                i
        );
        struct PwStreamData *data =
                (struct PwStreamData*) malloc(sizeof (struct PwStreamData));

        memset(data, 0, sizeof (struct PwStreamData));
        monSpace.screens[i].data = data;

        if (checkScreen(i, requestedArea)) {
            if (!connectStream(i)){
                doCleanup();
                return FALSE;
            }
        }
        debug_screencast(
                "⚠⚠⚠ %s:%i @@@ mon processed %i\n",
                __FUNCTION__, __LINE__,
                i
        );
    }

    pw_thread_loop_unlock(pw.loop);

    return TRUE;
}

static gboolean isAllDataReady() {
    for (int i = 0; i < monSpace.screenCount; ++i) {
        if (!monSpace.screens[i].shouldCapture) {
            continue;
        }
        if (!monSpace.screens[i].captureDataReady ) {
            return FALSE;
        }
    }
    return TRUE;
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
        jintArray pixelArray,
        jboolean screencastDebug
) {
    DEBUG_SCREENCAST_ENABLE = screencastDebug;

    GdkRectangle requestedArea = { jx, jy, jwidth, jheight};

    debug_screencast(
            "%s:%i taking screenshot at x: %5i y %5i w %5i h %5i\n",
            __FUNCTION__, __LINE__, jx, jy, jwidth, jheight
    );

    initRestoreToken();

    initScreencast();

    if (doLoop(requestedArea)) {
        while (!isAllDataReady()) {
            pw_thread_loop_wait(pw.loop);
        }

        debug_screencast(
            "\n%s:%i data ready$$\n",
            __FUNCTION__, __LINE__
        );

        for (int i = 0; i < monSpace.screenCount; ++i) {
            struct ScreenProps * screenProps = &monSpace.screens[i];

            if (screenProps->shouldCapture) {
                debug_screencast(
                        "%s:%i @@@ getting data mon %i\n",
                        __FUNCTION__, __LINE__, i
                );

                GdkRectangle captureArea  = screenProps->captureArea;

                debug_rectangle(
                    requestedArea,
                    "requestedArea"
                );
                debug_rectangle(
                    screenProps->bounds,
                    "screen bounds"
                );
                debug_rectangle(
                    captureArea,
                    "in-screen coords capture area"
                );

                debug_screencast(
                    "%s:%i screenProps->captureData %p\n",
                    __FUNCTION__, __LINE__,
                    screenProps->captureData
                );


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
                            ((jint *) screenProps->captureData)
                                + (captureArea.width * y)
                    );
                }

                free(screenProps->captureData);
                screenProps->captureData = NULL;
                screenProps->shouldCapture = FALSE;

                pw_stream_set_active(screenProps->data->stream, FALSE);
                pw_stream_disconnect(screenProps->data->stream);
            }
        }
        doCleanup();
    }
}
