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
#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif
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

#ifdef HAVE_X11
  gint timeout;
  gint interval;
  gint prefer_blanking;
  gint allow_exposures;
#ifdef HAVE_XTEST
  gint keycode1, keycode2;
  gint *keycode;
  gboolean have_xtest;
#endif
#endif
};

#ifdef HAVE_X11
#define XSCREENSAVER_MIN_TIMEOUT 60
#endif

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

#ifdef HAVE_X11
static void
screensaver_enable_x11 (ScreenSaver * screensaver)
{

#ifdef HAVE_XTEST
  if (screensaver->have_xtest) {
    g_source_remove_by_user_data (screensaver);
    return;
  }
#endif

  XLockDisplay (screensaver->display);
  XSetScreenSaver (screensaver->display,
      screensaver->timeout,
      screensaver->interval,
      screensaver->prefer_blanking, screensaver->allow_exposures);
  XUnlockDisplay (screensaver->display);
}

#ifdef HAVE_XTEST
static gboolean
fake_event (ScreenSaver * screensaver)
{
  if (screensaver->disabled) {
    XLockDisplay (screensaver->display);
    XTestFakeKeyEvent (screensaver->display, *screensaver->keycode,
        True, CurrentTime);
    XTestFakeKeyEvent (screensaver->display, *screensaver->keycode,
        False, CurrentTime);
    XUnlockDisplay (screensaver->display);
    /* Swap the keycode */
    if (screensaver->keycode == &screensaver->keycode1)
      screensaver->keycode = &screensaver->keycode2;
    else
      screensaver->keycode = &screensaver->keycode1;
  }

  return TRUE;
}
#endif

static void
screensaver_disable_x11 (ScreenSaver * screensaver)
{
#ifdef HAVE_XTEST
  if (screensaver->have_xtest) {
    XLockDisplay (screensaver->display);
    XGetScreenSaver (screensaver->display, &screensaver->timeout,
        &screensaver->interval,
        &screensaver->prefer_blanking, &screensaver->allow_exposures);
    XUnlockDisplay (screensaver->display);

    if (screensaver->timeout != 0) {
      g_timeout_add_seconds (screensaver->timeout / 2,
          (GSourceFunc) fake_event, screensaver);
    } else {
      g_timeout_add_seconds (XSCREENSAVER_MIN_TIMEOUT / 2,
          (GSourceFunc) fake_event, screensaver);
    }

    return;
  }
#endif

  XLockDisplay (screensaver->display);
  XGetScreenSaver (screensaver->display, &screensaver->timeout,
      &screensaver->interval,
      &screensaver->prefer_blanking, &screensaver->allow_exposures);
  XSetScreenSaver (screensaver->display, 0, 0,
      DontPreferBlanking, DontAllowExposures);
  XUnlockDisplay (screensaver->display);
}

static void
screensaver_init_x11 (ScreenSaver * screensaver)
{
#ifdef HAVE_XTEST
  int a, b, c, d;

  XLockDisplay (screensaver->display);
  screensaver->have_xtest =
      (XTestQueryExtension (screensaver->display, &a, &b, &c, &d) == True);
  if (screensaver->have_xtest) {
    screensaver->keycode1 = XKeysymToKeycode (screensaver->display, XK_Alt_L);
    if (screensaver->keycode1 == 0) {
      g_warning ("keycode1 not existant");
    }
    screensaver->keycode2 = XKeysymToKeycode (screensaver->display, XK_Alt_R);
    if (screensaver->keycode2 == 0) {
      screensaver->keycode2 = XKeysymToKeycode (screensaver->display, XK_Alt_L);
      if (screensaver->keycode2 == 0) {
        g_warning ("keycode2 not existant");
      }
    }
    screensaver->keycode = &screensaver->keycode1;
  }
  XUnlockDisplay (screensaver->display);
#endif
}

static void
screensaver_free_x11 (ScreenSaver * screensaver)
{
  g_source_remove_by_user_data (screensaver);
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

#ifdef HAVE_X11
  if (screensaver->disabled)
    screensaver_enable_x11 (screensaver);
  else
    screensaver_disable_x11 (screensaver);
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

  screensaver_init_x11 (screensaver);
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

#ifdef HAVE_X11
  screensaver_free_x11 (screensaver);
#endif

  g_free (screensaver);
}
