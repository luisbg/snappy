/*
 * snappy - 0.1 beta
 *
 * Copyright (C) 2011 Collabora Multimedia Ltd.
 * <sebastian.droege@collabora.co.uk>
 *
 * The screensaver code is heavily based on totem, which is
 *  Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>
 *  Copyright © 2010 Christian Persch
 *  Copyright © 2010 Carlos Garcia Campos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "screensaver.h"

#ifdef HAVE_X11
#include <clutter/x11/clutter-x11.h>
#else
/* The DBus screensaver stuff needs X11 */
#ifdef ENABLE_DBUS
#undef ENABLE_DBUS
#endif
#endif

struct _ScreenSaver
{
  ClutterStage *stage;
  gboolean disabled;

#ifdef HAVE_X11
  Window window;
  Display *display;
#endif

#ifdef ENABLE_DBUS
  GDBusProxy *gs_proxy;
  gboolean have_session_dbus;
  guint32 cookie;
#endif
};

#ifdef ENABLE_DBUS
#include <gio/gio.h>
#define GS_SERVICE   "org.gnome.SessionManager"
#define GS_PATH      "/org/gnome/SessionManager"
#define GS_INTERFACE "org.gnome.SessionManager"
/* From org.gnome.SessionManager.xml */
#define GS_NO_IDLE_FLAG 8
#define REASON "Playing a movie"

static void
on_inhibit_cb (GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);
  ScreenSaver *screensaver = (ScreenSaver *) user_data;
  GVariant *value;
  GError *error = NULL;

  value = g_dbus_proxy_call_finish (proxy, res, &error);
  if (!value) {
    g_warning ("Problem inhibiting the screensaver: %s", error->message);
    g_error_free (error);
    return;
  }

  /* save the cookie */
  if (g_variant_is_of_type (value, G_VARIANT_TYPE ("(u)")))
    g_variant_get (value, "(u)", &screensaver->cookie);
  else
    screensaver->cookie = 0;

  g_variant_unref (value);
}

static void
on_uninhibit_cb (GObject * source_object,
    GAsyncResult * res, gpointer user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);
  ScreenSaver *screensaver = (ScreenSaver *) user_data;
  GVariant *value;
  GError *error = NULL;

  value = g_dbus_proxy_call_finish (proxy, res, &error);
  if (!value) {
    g_warning ("Problem uninhibiting the screensaver: %s", error->message);
    g_error_free (error);
    return;
  }

  /* clear the cookie */
  screensaver->cookie = 0;
  g_variant_unref (value);
}

static void
screensaver_inhibit_dbus (ScreenSaver * screensaver, gboolean inhibit)
{
  if (!screensaver->gs_proxy)
    return;

  if (inhibit) {
    guint xid;

    xid = screensaver->window;

    g_dbus_proxy_call (screensaver->gs_proxy,
        "Inhibit",
        g_variant_new ("(susu)",
            g_get_application_name (),
            xid,
            REASON,
            GS_NO_IDLE_FLAG),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, on_inhibit_cb, screensaver);
  } else {
    if (screensaver->cookie > 0) {
      g_dbus_proxy_call (screensaver->gs_proxy,
          "Uninhibit",
          g_variant_new ("(u)", screensaver->cookie),
          G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, on_uninhibit_cb,
          screensaver);
    }
  }
}

static void
screensaver_enable_dbus (ScreenSaver * screensaver)
{
  screensaver_inhibit_dbus (screensaver, FALSE);
}

static void
screensaver_disable_dbus (ScreenSaver * screensaver)
{
  screensaver_inhibit_dbus (screensaver, TRUE);
}

static void
screensaver_dbus_proxy_new_cb (GObject * source,
    GAsyncResult * result, gpointer user_data)
{
  ScreenSaver *screensaver = (ScreenSaver *) user_data;

  screensaver->gs_proxy = g_dbus_proxy_new_for_bus_finish (result, NULL);
  if (!screensaver->gs_proxy)
    return;

  if (screensaver->disabled)
    screensaver_disable_dbus (screensaver);
}

static void
screensaver_init_dbus (ScreenSaver * screensaver)
{
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
      NULL,
      GS_SERVICE,
      GS_PATH, GS_INTERFACE, NULL, screensaver_dbus_proxy_new_cb, screensaver);
}

static void
screensaver_free_dbus (ScreenSaver * screensaver)
{
  if (screensaver->gs_proxy)
    g_object_unref (screensaver->gs_proxy);
}

#endif

void
screensaver_enable (ScreenSaver * screensaver, gboolean enable)
{
  screensaver->disabled = !enable;

#ifdef ENABLE_DBUS
  if (screensaver->disabled)
    screensaver_enable_dbus (screensaver);
  else
    screensaver_disable_dbus (screensaver);
#endif
}

ScreenSaver *
screensaver_new (ClutterStage * stage)
{
  ScreenSaver *screensaver;

  screensaver = g_new0 (ScreenSaver, 1);

  screensaver->disabled = FALSE;
  screensaver->stage = stage;

#if HAVE_X11
  screensaver->display = clutter_x11_get_default_display ();
  screensaver->window = clutter_x11_get_stage_window (stage);
#endif

#ifdef ENABLE_DBUS
  screensaver_init_dbus (screensaver);
#endif

  return screensaver;
}

void
screensaver_free (ScreenSaver * screensaver)
{
#ifdef ENABLE_DBUS
  screensaver_free_dbus (screensaver);
#endif

  g_free (screensaver);
}
