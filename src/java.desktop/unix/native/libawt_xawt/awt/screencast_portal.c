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

#include "stdlib.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include "screencast_pipewire.h"
#include "screencast_portal.h"


extern struct ScreenSpace monSpace;

struct XdgDesktopPortalApi *portal = NULL;

GString *tmp;
GString *tokenStr;
GString *sessionTokenStr;

GString *restoreTokenPath = NULL;
GString *restoreToken = NULL;

static volatile int initRestoreTokenComplete = FALSE;

void initRestoreToken() {
    if (initRestoreTokenComplete) {
        return;
    }
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    restoreToken = gtk->g_string_new("");
    restoreTokenPath = gtk->g_string_new(homedir);

    // ~/.screencastToken
    gtk->g_string_append(restoreTokenPath, "/.screencastToken");
    debug_screencast("%s:%i Restore token path: %s\n", __FUNCTION__, __LINE__, restoreTokenPath->str);
    initRestoreTokenComplete = TRUE;
}


void errHandle(
        GError *error,
        int lineNum
) {
    if (error) {
        fprintf(stderr, "⚠⚠⚠ Error: line %i domain %i code %i message: \"%s\"\n",
                lineNum, error->domain, error->code, error->message);
    } else {
        debug_screencast("%s:%i OK\n", lineNum);
    }
    if (error) {
        gtk->g_error_free(error);
    }
    error = NULL;
}

void loadRestoreToken() {
    struct stat st;
    if (
        stat(restoreTokenPath->str, &st) == -1
        || st.st_size != RESTORE_TOKEN_LENGTH
    ) {
        debug_screencast(
                "%s:%i ===== error reading saved restore token, wrong file length: %li expected %i\n",
                __FUNCTION__, __LINE__, st.st_size, RESTORE_TOKEN_LENGTH
        );
        return;
    }

    GError *err = NULL;
    gchar* content = NULL;
    gsize size = -1;

    if (!gtk->g_file_get_contents(restoreTokenPath->str, &content, &size, &err)) {
        debug_screencast(
                "%s:%i ===== error reading saved restore token: %s\n",
                __FUNCTION__, __LINE__, err->message
        );
        errHandle(err, __LINE__);
        return;
    }
    
    if (size == RESTORE_TOKEN_LENGTH) {
        gtk->g_string_assign(restoreToken, content);

        debug_screencast("%s:%i @@@@@ restoreToken loaded %s len %i\n",
                         __FUNCTION__, __LINE__,
                         restoreToken->str,
                         restoreToken->len
        );
    } else {
        debug_screencast(
                "%s:%i ===== error reading saved restore token, wrong file length: %i expected %i\n",
                __FUNCTION__, __LINE__, size, RESTORE_TOKEN_LENGTH
        );
    }

    gtk->g_free(content);
}

void saveRestoreToken(const gchar* token) {
    GError *err = NULL;

    if (
        gtk->g_file_set_contents_full(
                restoreTokenPath->str,
                token,
                strlen(token),
                G_FILE_SET_CONTENTS_NONE,
                0600,
                &err
        )
    ) {
        debug_screencast(
                "%s:%i restore token |%s| saved\n",
                __FUNCTION__, __LINE__, token
        );
    } else {
        debug_screencast(
                "%s:%i ===== error saving restore token: %s\n",
                __FUNCTION__, __LINE__, err->message
        );
        errHandle(err, __LINE__);
    }
}

gboolean rebuildMonData(GVariantIter *iterStreams) {
    guint32 nodeID;
    GVariant* prop = NULL;

    int monIndex = 0;

    gboolean hasFailures = FALSE;

    while (gtk->g_variant_iter_loop(
            iterStreams,
            "(u@a{sv})",
            &nodeID,
            &prop
    )) {
        debug_screencast("%s:%i ==== nodeID: %i\n", __FUNCTION__, __LINE__, nodeID);

        if (monIndex >= monSpace.allocated) {
            //TODO realloc failure?
            monSpace.screens = realloc(
                    monSpace.screens,
                    ++monSpace.allocated * sizeof(struct ScreenProps)
            );
        }

        struct ScreenProps * mon = &monSpace.screens[monIndex];
        monSpace.screenCount = monIndex + 1;

        mon->id = nodeID;

        if (
                !gtk->g_variant_lookup(
                        prop,
                        "size",
                        "(ii)",
                        &mon->bounds.width,
                        &mon->bounds.height
                )
                || !gtk->g_variant_lookup(
                        prop,
                        "position",
                        "(ii)",
                        &mon->bounds.x,
                        &mon->bounds.y
                )
        ) {
            hasFailures = TRUE;
        }

        debug_screencast("%s:%i -----------------------\n", __FUNCTION__, __LINE__);
        debug_screen(mon);
        debug_screencast("%s:%i #---------------------#\n", __FUNCTION__, __LINE__);
        
        gtk->g_variant_unref(prop);
        monIndex++;
    };

    return !hasFailures;
}

//void checkVersion(GDBusProxy *proxy) {
//    GError *error = NULL;
//    GVariant *retVersion = gtk->g_dbus_proxy_call_sync(proxy, "org.freedesktop.DBus.Properties.Get",
//                                                     gtk->g_variant_new("(ss)",
//                                                                      "org.freedesktop.portal.ScreenCast",
//                                                                      "version"),
//                                                     G_DBUS_CALL_FLAGS_NONE,
//                                                     -1, NULL, NULL);
//
//    errHandle(error, __LINE__);
//
//    if (retVersion) {
//        GVariant *varVersion = NULL;
//        gtk->g_variant_get(retVersion, "(v)", &varVersion);
//
//        guint32 version = gtk->g_variant_get_uint32(varVersion);
//        debug_screencast("%s:%i version %d\n", __FUNCTION__, __LINE__, version);
//        if (version < 4) {
//            fprintf(stderr, "%s:%i version %i < 4, session restore is not available\n", __FUNCTION__, __LINE__, version);
//            //TODO check for backend availability
//        }
//    }
//}

/**
 * @return TRUE on success
 */
gboolean initXdgDesktopPortal() {
    portal = calloc(1, sizeof(*portal));

    if (!portal) {
        fprintf(stderr, "⚠⚠⚠ %s:%i Unable to allocate memory\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    GError* err = NULL;

    portal->connection = gtk->g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);

    if (err) {
        errHandle(err, __LINE__);
        return FALSE;
    }

    const gchar * name = gtk->g_dbus_connection_get_unique_name(portal->connection);
    if (!name) {
        fprintf(stderr, "%s:%i Failed to get unique connection name\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    debug_screencast("%s:%i unique connection name %s\n", __FUNCTION__, __LINE__, name);

    GString * nameStr = gtk->g_string_new(name);

    gtk->g_string_erase(nameStr, 0, 1); //remove leading colon ":"
    gtk->g_string_replace(nameStr, ".", "_", 0);

    portal->senderName = nameStr->str;

    gtk->g_string_free(nameStr, FALSE);

    debug_screencast("%s:%i portal->senderName %s\n", __FUNCTION__, __LINE__, portal->senderName);

    portal->screenCastProxy = gtk->g_dbus_proxy_new_sync(
            portal->connection,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.ScreenCast",
            NULL,
            &err
    );

    if (err) {
        debug_screencast(
                "%s:%i Failed to get ScreenCast portal: %s",
                __FUNCTION__, __LINE__, err->message
        );
        errHandle(err, __LINE__);
        return FALSE;
    }

    return TRUE;
}

static void updateRequestPath(
        gchar **path,
        gchar **token
) {
    static uint64_t counter = 0;
    ++counter;

    GString *tokenStr = gtk->g_string_new(NULL);
    gtk->g_string_printf(
            tokenStr,
            PORTAL_TOKEN_TEMPLATE,
            counter
    );

    *token = tokenStr->str;
    gtk->g_string_free(tokenStr, FALSE);

    debug_screencast("⚠⚠⚠ %s:%i token %s\n", __FUNCTION__, __LINE__, *token);

    GString *pathStr = gtk->g_string_new(NULL);

    gtk->g_string_printf(
            pathStr,
            PORTAL_REQUEST_TEMPLATE,
            portal->senderName,
            counter
    );

    *path = pathStr->str;
    gtk->g_string_free(pathStr, FALSE);

    debug_screencast("⚠⚠⚠ %s:%i path %s\n", __FUNCTION__, __LINE__, *path);
}

static void updateSessionToken(
        gchar **token
) {
    static uint64_t counter = 0;
    counter++;

    GString *tokenStr = gtk->g_string_new(NULL);

    gtk->g_string_printf(
            tokenStr,
            PORTAL_TOKEN_TEMPLATE,
            counter
    );

    *token = tokenStr->str;
    gtk->g_string_free(tokenStr, FALSE);

    debug_screencast("⚠⚠⚠ %s:%i token %s\n", __FUNCTION__, __LINE__, *token);
}

static void registerScreenCastCallback(
        const char *path,
        struct DBusCallbackHelper *helper,
        GDBusSignalCallback callback
) {
        helper->id = gtk->g_dbus_connection_signal_subscribe(
            portal->connection,
            "org.freedesktop.portal.Desktop",
            "org.freedesktop.portal.Request",
            "Response",
            path,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
            callback,
            helper,
            NULL
    );
}

static void unregisterScreenCastCallback(
        struct DBusCallbackHelper *helper
) {
    if (helper->id) {
        gtk->g_dbus_connection_signal_unsubscribe(
                portal->connection,
                helper->id
        );
    }
}

static void callbackScreenCastCreateSession(
        GDBusConnection *connection,
        const char *senderName,
        const char *objectPath,
        const char *interfaceName,
        const char *signalName,
        GVariant *parameters,
        void *data
) {
    struct DBusCallbackHelper *helper = data;
    uint32_t status;
    GVariant *result = NULL;

    gtk->g_variant_get(
            parameters,
            "(u@a{sv})",
            &status,
            &result
    );

    if (status != 0) {
        debug_screencast("Failed to create ScreenCast: %u\n", status);
    } else {
        gtk->g_variant_lookup(result, "session_handle", "s", helper->data);
    }

    helper->isDone = TRUE;
}

gboolean portalScreenCastCreateSession() {
    GError *err = NULL;

    gchar *requestPath = NULL;
    gchar *requestToken = NULL;
    gchar *sessionToken = NULL;

    struct DBusCallbackHelper helper = {
            .id = 0,
            .data = &portal->screenCastSessionHandle
    };

    updateRequestPath(
            &requestPath,
            &requestToken
    );
    updateSessionToken(&sessionToken);

    portal->screenCastSessionHandle = NULL;

    registerScreenCastCallback(
            requestPath,
            &helper,
            callbackScreenCastCreateSession
    );

    GVariantBuilder builder;

    gtk->g_variant_builder_init(
            &builder,
            G_VARIANT_TYPE_VARDICT
    );

    gtk->g_variant_builder_add(
            &builder,
            "{sv}",
            "handle_token",
            gtk->g_variant_new_string(requestToken)
    );

    gtk->g_variant_builder_add(
            &builder,
            "{sv}",
            "session_handle_token",
            gtk->g_variant_new_string(sessionToken)
    );

    GVariant *response = gtk->g_dbus_proxy_call_sync(
            portal->screenCastProxy,
            "CreateSession",
            gtk->g_variant_new("(a{sv})", &builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &err
    );

    if (err) {
        debug_screencast("Failed to create ScreenCast session: %s\n", err->message);
        errHandle(err, __LINE__);
    } else {
        while (!helper.isDone) {
            gtk->g_main_context_iteration(NULL, TRUE);
        }

        debug_screencast(
                "⚠⚠⚠ %s:%i session_handle %s\n",
                __FUNCTION__, __LINE__, portal->screenCastSessionHandle
        );
    }

    unregisterScreenCastCallback(&helper);
    gtk->g_variant_unref(response);

    free(sessionToken);
    free(requestPath);
    free(requestToken);

    return portal->screenCastSessionHandle != NULL;
}

static void callbackScreenCastSelectSources(
        GDBusConnection *connection,
        const char *senderName,
        const char *objectPath,
        const char *interfaceName,
        const char *signalName,
        GVariant *parameters,
        void *data
) {
    struct DBusCallbackHelper *helper = data;

    helper->data = (void *) 0;

    uint32_t status;
    GVariant* result = NULL;

    gtk->g_variant_get(parameters, "(u@a{sv})", &status, &result);

    if (status != 0) {
        debug_screencast("Failed select sources: %u\n", status);
    } else {
        helper->data = (void *) 1;
    }

    helper->isDone = TRUE;

    gtk->g_variant_unref(result);
}

gboolean portalScreenCastSelectSources() {
    GError* err = NULL;

    gchar *requestPath = NULL;
    gchar *requestToken = NULL;

    struct DBusCallbackHelper helper = {0};

    updateRequestPath(
            &requestPath,
            &requestToken
    );

    registerScreenCastCallback(
            requestPath,
            &helper,
            callbackScreenCastSelectSources
    );

    GVariantBuilder builder;

    gtk->g_variant_builder_init(
            &builder,
            G_VARIANT_TYPE_VARDICT
    );

    gtk->g_variant_builder_add(
            &builder,
            "{sv}", "handle_token",
            gtk->g_variant_new_string(requestToken)
    );

    gtk->g_variant_builder_add(
            &builder,
            "{sv}", "multiple",
            gtk->g_variant_new_boolean(TRUE));

    // 1: MONITOR
    // 2: WINDOW
    // 4: VIRTUAL
    gtk->g_variant_builder_add(
            &builder, "{sv}", "types",
            gtk->g_variant_new_uint32(1)
    );

    // 0: Do not persist (default)
    // 1: Permissions persist as long as the application is running
    // 2: Permissions persist until explicitly revoked
    gtk->g_variant_builder_add(
            &builder,
            "{sv}",
            "persist_mode",
            gtk->g_variant_new_uint32(2)
    );

    loadRestoreToken();

    if (restoreToken->len > 0) {
        gtk->g_variant_builder_add(
                &builder,
                "{sv}",
                "restore_token",
                gtk->g_variant_new_string(restoreToken->str)
        );
    }

    GVariant *response = gtk->g_dbus_proxy_call_sync(
            portal->screenCastProxy,
            "SelectSources",
            gtk->g_variant_new("(oa{sv})", portal->screenCastSessionHandle, &builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &err
    );

    if (err) {
        debug_screencast("Failed to call SelectSources: %s\n", err->message);
        errHandle(err, __LINE__);
    } else {
        while (!helper.isDone) {
            gtk->g_main_context_iteration(NULL, TRUE);
        }
    }

    unregisterScreenCastCallback(&helper);
    gtk->g_variant_unref(response);

    free(requestPath);
    free(requestToken);

    return helper.data != NULL;
}

static void callbackScreenCastStart(
        GDBusConnection *connection,
        const char *senderName,
        const char *objectPath,
        const char *interfaceName,
        const char *signalName,
        GVariant *parameters,
        void *data
) {
    struct DBusCallbackHelper *helper = data;

    uint32_t status;
    GVariant* result = NULL;

    gtk->g_variant_get(parameters, "(u@a{sv})", &status, &result);

    if (status != 0) {
        debug_screencast("Failed to start screencast: %u\n", status);
        helper->data = (void *) 0; //TODO let java know about failure
        helper->isDone = TRUE;
        return;
    }

    GVariant *restoreTokenVar = gtk->g_variant_lookup_value(
            result,
            "restore_token",
            G_VARIANT_TYPE_STRING
    );

    if (restoreTokenVar) {
        gsize len;
        const gchar * tkn = gtk->g_variant_get_string(restoreTokenVar, &len);

        debug_screencast(
                "%s:%i restore_token %s strcmp %i\n",
                __FUNCTION__, __LINE__, tkn, strcmp(tkn, restoreToken->str)
        );

        if (strcmp(tkn, restoreToken->str) != 0) {
            saveRestoreToken(tkn);
        }

        gtk->g_variant_unref(restoreTokenVar);
    }

    GVariant *streams = gtk->g_variant_lookup_value(
            result,
            "streams",
            G_VARIANT_TYPE_ARRAY
    );

    GVariantIter iter;
    gtk->g_variant_iter_init(
            &iter,
            streams
    );

    size_t count = gtk->g_variant_iter_n_children(&iter);

    debug_screencast("⚠⚠⚠ %s:%i count %i\n", __FUNCTION__, __LINE__, count);

    helper->data = (rebuildMonData(&iter))
                   ? (void *) 1
                   : (void *) 0;

    helper->isDone = TRUE;
    
    gtk->g_variant_unref(streams);
}

gboolean portalScreenCastStart() {
    GError *err = NULL;

    gchar *requestPath = NULL;
    gchar *requestToken = NULL;

    struct DBusCallbackHelper helper = { 0 };

    updateRequestPath(
            &requestPath,
            &requestToken
    );

    registerScreenCastCallback(
            requestPath,
            &helper,
            callbackScreenCastStart
    );

    GVariantBuilder builder;

    gtk->g_variant_builder_init(
            &builder,
            G_VARIANT_TYPE_VARDICT
    );

    gtk->g_variant_builder_add(
            &builder,
            "{sv}",
            "handle_token",
            gtk->g_variant_new_string(requestToken)
    );

    GVariant *response = gtk->g_dbus_proxy_call_sync(
            portal->screenCastProxy,
            "Start",
            gtk->g_variant_new("(osa{sv})", portal->screenCastSessionHandle, "", &builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &err
    );

    if (err) {
        debug_screencast("Failed to call Start on session: %s\n", err->message);
        errHandle(err, __LINE__);
    } else {
        while (!helper.isDone) {
            gtk->g_main_context_iteration(NULL, TRUE);
        }
    }

    unregisterScreenCastCallback(&helper);
    gtk->g_variant_unref(response);

    free(requestPath);
    free(requestToken);

    return helper.data != NULL;
}

int portalScreenCastOpenPipewireRemote() {
    GError* err = NULL;
    GUnixFDList* fdList = NULL;

    GVariantBuilder builder;

    gtk->g_variant_builder_init(
            &builder, G_VARIANT_TYPE_VARDICT
    );

    GVariant *response = gtk->g_dbus_proxy_call_with_unix_fd_list_sync(
            portal->screenCastProxy,
            "OpenPipeWireRemote",
            gtk->g_variant_new("(oa{sv})", portal->screenCastSessionHandle, &builder),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &fdList,
            NULL,
            &err
    );

    if (err) {
        debug_screencast(
                "Failed to call OpenPipeWireRemote on session: %s\n",
                err->message
        );
        errHandle(err, __LINE__);
        return -1;
    }

    gint32 index;
    gtk->g_variant_get(
            response,
            "(h)",
            &index,
            &err
    );

    gtk->g_variant_unref(response);

    if (err) {
        debug_screencast("Failed to get pipewire fd index: %s\n", err->message);
        errHandle(err, __LINE__);
        return -1;
    }

    int fd = gtk->g_unix_fd_list_get(
            fdList,
            index,
            &err
    );

    gtk->g_object_unref(fdList);

    if (err) {
        debug_screencast("Failed to get pipewire fd: %s\n", err->message);
        errHandle(err, __LINE__);
        return -1;
    }

    return fd;
}

void portalScreenCastCleanup() {
    if (portal->screenCastSessionHandle) {
        gtk->g_dbus_connection_call_sync(
                portal->connection,
                "org.freedesktop.portal.Desktop",
                portal->screenCastSessionHandle,
                "org.freedesktop.portal.Session",
                "Close",
                NULL,
                NULL,
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                NULL
        );

        gtk->g_free(portal->screenCastSessionHandle);
        portal->screenCastSessionHandle = NULL;
    }

    // TODO reuse connection?
    if (!portal) {
        return;
    }
    if (portal->connection) {
        gtk->g_object_unref(portal->connection);
        portal->connection = NULL;
    }

    if (portal->screenCastProxy) {
        gtk->g_object_unref(portal->screenCastProxy);
        portal->screenCastProxy = NULL;
    }

    if (portal->senderName) {
        free(portal->senderName);
        portal->senderName = NULL;
    }

    free(portal);
    portal = NULL;
}

void load_pipewire() { //TODO check if pipewire available
//    void * pipewire_handle = dlopen("libpipewire-0.3.so.0", RTLD_LAZY | RTLD_LOCAL);
//
//    debug_screencast("%s:%i pipewire_handle %p\n", __FUNCTION__, __LINE__,pipewire_handle);
}


int getPipewireFd() {
    if (!portalScreenCastCreateSession())  {
        debug_screencast("Failed to create ScreenCast session\n");
        return -1;
    }

    debug_screencast(
            "⚠⚠⚠ %s:%i Got session handle: %s\n",
            __FUNCTION__, __LINE__,
            portal->screenCastSessionHandle
    );

    if (!portalScreenCastSelectSources()) {
        debug_screencast("Failed to select source\n");
        return -1;
    }

    debug_screencast("⚠⚠⚠ %s:%i\n", __FUNCTION__, __LINE__);

    if (!portalScreenCastStart()) {
        debug_screencast("Failed to get pipewire node\n");
        return -1;
    }

    debug_screencast(
            "⚠⚠⚠ %s:%i --- portalScreenCastStart\n",
            __FUNCTION__, __LINE__
    );

    int pipewireFd = portalScreenCastOpenPipewireRemote();
    if (pipewireFd < 0) {
        debug_screencast("Failed to get pipewire fd\n");
    }

    debug_screencast("%s:%i pwFd %i\n", __FUNCTION__, __LINE__, pipewireFd);
    return pipewireFd;
}
