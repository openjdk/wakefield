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


#ifndef _PIPEWIRE_H
#define _PIPEWIRE_H

#include "gtk_interface.h"

#include <pipewire/pipewire.h>

#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>
#include <semaphore.h>

struct data_pw;

extern int DEBUG_SCREENCAST_ENABLE;

void debug_screencast(const char *__restrict fmt, ...);

struct ScreenProps {
    guint32 id;
    GdkRectangle bounds;


    GdkRectangle captureArea;
    gchar *captureData;
    struct data_pw* data;
    volatile int shouldCapture;
    sem_t captureDataReady;
};


#define SCREEN_SPACE_DEFAULT_ALLOCATED 2
struct ScreenSpace {
    struct ScreenProps* screens;
    int screenCount;
    int allocated;
};

void printScreen(struct ScreenProps *mon);

struct data_pw {
    struct pw_main_loop *loop;
    struct pw_stream *stream;

    struct spa_video_info format;

    struct ScreenProps* screenProps;
    int saved;
};

#endif //_PIPEWIRE_H
