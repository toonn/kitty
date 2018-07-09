//========================================================================
// GLFW 3.3 XKB - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2018 Kovid Goyal <kovid@kovidgoyal.net>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================


#include "internal.h"
#include "dbus_glfw.h"

static inline void
report_error(DBusError *err, const char *fmt, ...) {
    static char buf[1024];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    snprintf(buf + n, sizeof(buf), ". DBUS error: %s", err->message);
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s", buf);
    dbus_error_free(err);
}

GLFWbool
glfw_dbus_init(_GLFWDBUSData *dbus) {
    DBusError err;
    if (!dbus->session_conn) {
        dbus_error_init(&err);
        dbus->session_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
            report_error(&err, "Failed to connect to DBUS session bus");
            return GLFW_FALSE;
        }
    }
    return GLFW_TRUE;
}

DBusConnection*
glfw_dbus_connect_to(const char *path, const char* err_msg) {
    DBusError err;
    dbus_error_init(&err);
    DBusConnection *ans = dbus_connection_open_private(path, &err);
    if (!ans) {
        report_error(&err, err_msg);
        return NULL;
    }
    dbus_error_free(&err);
    dbus_connection_flush(ans);
    if (!dbus_bus_register(ans, &err)) {
        report_error(&err, err_msg);
        return NULL;
    }
    dbus_connection_flush(ans);
    return ans;
}

void
glfw_dbus_terminate(_GLFWDBUSData *dbus) {
    if (dbus->session_conn) {
        dbus_connection_close(dbus->session_conn);
        dbus_connection_unref(dbus->session_conn);
        dbus->session_conn = NULL;
    }
}

void
glfw_dbus_close_connection(DBusConnection *conn) {
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
}

static GLFWbool
call_void_method(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, va_list ap) {
    GLFWbool retval = GLFW_FALSE;

    if (conn) {
        DBusMessage *msg = dbus_message_new_method_call(node, path, interface, method);
        if (msg) {
            int firstarg = va_arg(ap, int);
            if ((firstarg == DBUS_TYPE_INVALID) || dbus_message_append_args_valist(msg, firstarg, ap)) {
                if (dbus_connection_send(conn, msg, NULL)) {
                    dbus_connection_flush(conn);
                    retval = GLFW_TRUE;
                }
            }

            dbus_message_unref(msg);
        }
    }

    return retval;
}

GLFWbool
glfw_dbus_call_void_method(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...) {
    GLFWbool retval;
    va_list ap;
    va_start(ap, method);
    retval = call_void_method(conn, node, path, interface, method, ap);
    va_end(ap);
    return retval;
}

static GLFWbool
call_method(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, va_list ap) {
    GLFWbool retval = GLFW_FALSE;

    if (conn) {
        DBusMessage *msg = dbus_message_new_method_call(node, path, interface, method);
        if (msg) {
            int firstarg = va_arg(ap, int);
            if ((firstarg == DBUS_TYPE_INVALID) || dbus_message_append_args_valist(msg, firstarg, ap)) {
                DBusError err;
                dbus_error_init(&err);
                DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 300, &err);
                if (reply) {
                    firstarg = va_arg(ap, int);
                    dbus_error_free(&err);
                    if ((firstarg == DBUS_TYPE_INVALID) || dbus_message_get_args_valist(reply, &err, firstarg, ap)) {
                        retval = GLFW_TRUE;
                    } else {
                        report_error(&err, "Failed to get reply args from DBUS method: %s on node: %s and interface: %s", method, node, interface);
                    }
                    dbus_message_unref(reply);
                } else {
                    report_error(&err, "Failed to call DBUS method: %s on node: %s and interface: %s", method, node, interface);
                }
            }
            dbus_message_unref(msg);
        }
    }

    return retval;
}

GLFWbool
glfw_dbus_call_method(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...) {
    GLFWbool retval;
    va_list ap;
    va_start(ap, method);
    retval = call_method(conn, node, path, interface, method, ap);
    va_end(ap);
    return retval;

}