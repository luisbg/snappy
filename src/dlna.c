/*
 * snappy - 0.2
 *
 * Copyright (C) 2011 Collabora Multimedia Ltd.
 * <luis.debethencourt@collabora.co.uk>
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

#ifdef ENABLE_DBUS

#include <gio/gio.h>
#include <stdlib.h>

#include "dlna.h"

const char *mpris_introspection_xml =
    "<node>"
    "  <interface name='org.mpris.MediaPlayer2'>"
    "    <method name='Raise'/>"
    "    <method name='Quit'/>"
    "    <property name='CanQuit' type='b' access='read'/>"
    "    <property name='CanRaise' type='b' access='read'/>"
    "    <property name='HasTrackList' type='b' access='read'/>"
    "    <property name='Identity' type='s' access='read'/>"
    "    <property name='DesktopEntry' type='s' access='read'/>"
    "    <property name='SupportedUriSchemes' type='as' access='read'/>"
    "    <property name='SupportedMimeTypes' type='as' access='read'/>"
    "  </interface>"
    "  <interface name='org.mpris.MediaPlayer2.Player'>"
    "    <method name='Next'/>"
    "    <method name='Previous'/>"
    "    <method name='Pause'/>"
    "    <method name='PlayPause'/>"
    "    <method name='Stop'/>"
    "    <method name='Play'/>"
    "    <method name='Seek'>"
    "      <arg direction='in' name='Offset' type='x'/>"
    "    </method>"
    "    <method name='SetPosition'>"
    "      <arg direction='in' name='TrackId' type='o'/>"
    "      <arg direction='in' name='Position' type='x'/>"
    "    </method>"
    "    <method name='OpenUri'>"
    "      <arg direction='in' name='Uri' type='s'/>"
    "    </method>"
    "    <signal name='Seeked'>"
    "      <arg name='Position' type='x'/>"
    "    </signal>"
    "    <property name='PlaybackStatus' type='s' access='read'/>"
    "    <property name='LoopStatus' type='s' access='readwrite'/>"
    "    <property name='Rate' type='d' access='readwrite'/>"
    "    <property name='Shuffle' type='b' access='readwrite'/>"
    "    <property name='Metadata' type='a{sv}' access='read'/>"
    "    <property name='Volume' type='d' access='readwrite'/>"
    "    <property name='Position' type='x' access='read'/>"
    "    <property name='MinimumRate' type='d' access='read'/>"
    "    <property name='MaximumRate' type='d' access='read'/>"
    "    <property name='CanGoNext' type='b' access='read'/>"
    "    <property name='CanGoPrevious' type='b' access='read'/>"
    "    <property name='CanPlay' type='b' access='read'/>"
    "    <property name='CanPause' type='b' access='read'/>"
    "    <property name='CanSeek' type='b' access='read'/>"
    "    <property name='CanControl' type='b' access='read'/>"
    "  </interface>"
    "  <interface name='org.mpris.MediaPlayer2.TrackList'>"
    "    <method name='GetTracksMetadata'>"
    "      <arg direction='in' name='TrackIds' type='ao'/>"
    "      <arg direction='out' name='Metadata' type='aa{sv}'/>"
    "    </method>"
    "    <method name='AddTrack'>"
    "      <arg direction='in' name='Uri' type='s'/>"
    "      <arg direction='in' name='AfterTrack' type='o'/>"
    "      <arg direction='in' name='SetAsCurrent' type='b'/>"
    "    </method>"
    "    <method name='RemoveTrack'>"
    "      <arg direction='in' name='TrackId' type='o'/>"
    "    </method>"
    "    <method name='GoTo'>"
    "      <arg direction='in' name='TrackId' type='o'/>"
    "    </method>"
    "    <signal name='TrackListReplaced'>"
    "      <arg name='Tracks' type='ao'/>"
    "      <arg name='CurrentTrack' type='o'/>"
    "    </signal>"
    "    <signal name='TrackAdded'>"
    "      <arg name='Metadata' type='a{sv}'/>"
    "      <arg name='AfterTrack' type='o'/>"
    "    </signal>"
    "    <signal name='TrackRemoved'>"
    "      <arg name='TrackId' type='o'/>"
    "    </signal>"
    "    <signal name='TrackMetadataChanged'>"
    "      <arg name='TrackId' type='o'/>"
    "      <arg name='Metadata' type='a{sv}'/>"
    "    </signal>"
    "    <property name='Tracks' type='ao' access='read'/>"
    "    <property name='CanEditTracks' type='b' access='read'/>"
    "  </interface>"
    "  <interface name='org.mpris.MediaPlayer2.Playlists'>"
    "    <method name='ActivatePlaylist'>"
    "      <arg direction='in' name='PlaylistId' type='o'/>"
    "    </method>"
    "    <method name='GetPlaylists'>"
    "      <arg direction='in' name='Index' type='u'/>"
    "      <arg direction='in' name='MaxCount' type='u'/>"
    "      <arg direction='in' name='Order' type='s'/>"
    "      <arg direction='in' name='ReverseOrder' type='b'/>"
    "      <arg direction='out' type='a(oss)'/>"
    "    </method>"
    "    <property name='PlaylistCount' type='u' access='read'/>"
    "    <property name='Orderings' type='as' access='read'/>"
    "    <property name='ActivePlaylist' type='(b(oss))' access='read'/>"
    "  </interface>" "</node>";

/* for now */
static const GDBusInterfaceVTable interface_vtable = {
  (GDBusInterfaceMethodCallFunc) handle_method_call,
  (GDBusInterfaceGetPropertyFunc) handle_get_property,
  (GDBusInterfaceSetPropertyFunc) handle_set_property
};

static const GDBusInterfaceVTable root_vtable = {
  (GDBusInterfaceMethodCallFunc) handle_root_method_call,
  (GDBusInterfaceGetPropertyFunc) get_root_property,
  NULL
};

static GDBusNodeInfo *introspection_data = NULL;

G_DEFINE_TYPE (SnappyMP, my_object, G_TYPE_OBJECT);

static void
my_object_init (SnappyMP * object)
{
  // g_print ("my_object_init\n");

  object->name = "snappy";
}

static void
my_object_finalize (GObject * object)
{
  SnappyMP *myobj = (SnappyMP *) object;

  g_free (myobj->name);

  G_OBJECT_CLASS (my_object_parent_class)->finalize (object);
}

static void
my_object_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  SnappyMP *myobj = (SnappyMP *) object;

  switch (prop_id) {
    case PROP_NAME:
      g_value_set_string (value, myobj->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
my_object_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  SnappyMP *myobj = (SnappyMP *) object;

  switch (prop_id) {
    case PROP_NAME:
      g_free (myobj->name);
      myobj->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
my_object_class_init (SnappyMPClass * class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = my_object_finalize;
  gobject_class->set_property = my_object_set_property;
  gobject_class->get_property = my_object_get_property;

  g_object_class_install_property (gobject_class,
      PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL, G_PARAM_READWRITE));
}

void
my_object_change_uri (SnappyMP * myobj, gchar * uri)
{
  if (myobj != NULL)
    myobj->uri = uri;

  g_object_set (G_OBJECT (myobj), "uri", uri, NULL);

  engine_open_uri (myobj->engine, uri);
  interface_load_uri (myobj->ui, uri);
  engine_play (myobj->engine);
}

static void
handle_result (GDBusMethodInvocation * invocation, gboolean ret, GError * error)
{
  if (ret) {
    g_dbus_method_invocation_return_value (invocation, NULL);
  } else {
    if (error != NULL) {
      g_print ("DLNA returning error: %s", error->message);
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    } else {
      g_print ("DLNA returning unknown error");
      g_dbus_method_invocation_return_error_literal (invocation,
          G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Unknown error");
    }
  }
}

void
handle_method_call (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * method_name,
    GVariant * parameters, GDBusMethodInvocation * invocation, SnappyMP * myobj)
{
  gboolean ret = TRUE;
  GError *error = NULL;

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0) {
    g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED,
        "Method %s.%s not supported", interface_name, method_name);
    return;
  }

  if (g_strcmp0 (method_name, "OpenUri") == 0) {
    gchar *uri;

    // g_print ("openUri.. uri: %s\n", uri);
    g_variant_get (parameters, "(s)", &uri);
    my_object_change_uri (myobj, uri);

    g_dbus_method_invocation_return_value (invocation, NULL);

  } else if (g_strcmp0 (method_name, "Next") == 0) {
    /// ToDo: next track call

    handle_result (invocation, ret, error);

  } else if (g_strcmp0 (method_name, "Play") == 0) {
    engine_play (myobj->engine);

    handle_result (invocation, ret, error);

  } else if (g_strcmp0 (method_name, "Pause") == 0) {
    change_state (myobj->engine, "Paused");

    handle_result (invocation, ret, error);
  } else if (g_strcmp0 (method_name, "Stop") == 0) {
    engine_stop (myobj->engine);

    handle_result (invocation, ret, error);

  } else if (g_strcmp0 (method_name, "Seek") == 0) {
    gint64 offset, position;
    gfloat relative;

    g_variant_get (parameters, "(x)", &offset);
    relative = offset / 100000000.0;
    position = myobj->engine->media_duration * relative;
    // g_print ("offset: %ld    relative: %f", offset, relative);
    engine_seek (myobj->engine, position, TRUE);

    handle_result (invocation, ret, error);

  }
}

GVariant *
handle_get_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name, GError ** error, gpointer user_data)
{
  GVariant *ret;
  SnappyMP *myobj = user_data;

  ret = NULL;
  if (g_strcmp0 (property_name, "Name") == 0) {
    ret = g_variant_new_string (myobj->name ? myobj->name : "snappy");
    //ret = g_variant_new_string ("snappy");

  } else if (g_strcmp0 (property_name, "PlaybackStatus") == 0) {
    ret = g_variant_new_string ("Paused");
  } else if (g_strcmp0 (property_name, "LoopStatus") == 0) {
    ret = g_variant_new_string ("Paused");
  } else if (g_strcmp0 (property_name, "Rate") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "Shuffle") == 0) {
    ret = g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "Metadata") == 0) {
    ret = g_variant_new_array (G_VARIANT_TYPE_VARDICT, NULL, 0);
  } else if (g_strcmp0 (property_name, "Volume") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "Position") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "MinimumRate") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "MaximumRate") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "CanGoNext") == 0) {
    ret = g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "CanGoPrevious") == 0) {
    ret = g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "CanPlay") == 0) {
    ret = g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "CanPause") == 0) {
    ret = g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "CanSeek") == 0) {
    ret = g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "CanControl") == 0) {
    ret = g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "Identity") == 0) {
    ret = g_variant_new_string ("snappy");

  } else if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0) {
    /* not planning to support this seriously */
    const char *fake_supported_schemes[] = {
      "file", "http", "cdda", "smb", "sftp", NULL
    };
    return g_variant_new_strv (fake_supported_schemes, -1);

  } else if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0) {
    /* nor this */
    const char *fake_supported_mimetypes[] = {
      "application/ogg", "audio/x-vorbis+ogg", "audio/x-flac", "audio/mpeg",
      "video/mpeg", "video/quicktime", "video/x-ms-asf", "video/x-msvideo",
      "video/ogg", "audio/ogg", "application/annodex", "video/annodex",
      "video/x-matroska", "audio/x-matroska", "video/x-theora+ogg",
      NULL
    };
    return g_variant_new_strv (fake_supported_mimetypes, -1);
  }

  return ret;
}

gboolean
handle_set_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name,
    GVariant * value, GError ** error, gpointer user_data)
{
  SnappyMP *myobj = user_data;

  if (g_strcmp0 (property_name, "Name") == 0) {
    g_object_set (myobj, "name", g_variant_get_string (value, NULL), NULL);

  } else if (g_strcmp0 (property_name, "Volume") == 0) {
    gdouble level;

    level = g_variant_get_double (value);
    engine_volume (myobj->engine, level);
  }

  return TRUE;
}

void
handle_root_method_call (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *method_name,
    GVariant * parameters, GDBusMethodInvocation * invocation, SnappyMP * mp)
{
  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0) {
    g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED,
        "Method %s.%s not supported", interface_name, method_name);
    return;
  }

  if (g_strcmp0 (method_name, "Raise") == 0) {
    g_dbus_method_invocation_return_value (invocation, NULL);
  } else if (g_strcmp0 (method_name, "Quit") == 0) {
    g_dbus_method_invocation_return_value (invocation, NULL);
  } else {
    g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED,
        "Method %s.%s not supported", interface_name, method_name);
  }
}

GVariant *
get_root_property (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *property_name, GError ** error, SnappyMP * mp)
{
  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0) {
    g_set_error (error,
        G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED,
        "Property %s.%s not supported", interface_name, property_name);
    return NULL;
  }

  if (g_strcmp0 (property_name, "CanQuit") == 0) {
    return g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "CanRaise") == 0) {
    return g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "HasTrackList") == 0) {
    return g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "Identity") == 0) {
    return g_variant_new_string ("snappy");
  } else if (g_strcmp0 (property_name, "DesktopEntry") == 0) {
    return g_variant_new_string ("snappy");
  } else if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0) {
    /* not planning to support this seriously */
    const char *fake_supported_schemes[] = {
      "file", "http", "cdda", "smb", "sftp", NULL
    };
    return g_variant_new_strv (fake_supported_schemes, -1);
  } else if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0) {
    /* nor this */
    const char *fake_supported_mimetypes[] = {
      "video/mpeg", "video/quicktime", "video/x-ms-asf", "video/x-msvideo",
      "video/ogg", "audio/ogg", "application/annodex", "video/annodex",
      "application/ogg", "audio/x-vorbis+ogg", "audio/x-flac", "audio/mpeg",
      "video/x-matroska", "audio/x-matroska", "video/x-theora+ogg",
      NULL
    };
    return g_variant_new_strv (fake_supported_mimetypes, -1);
  }

  g_set_error (error,
      G_DBUS_ERROR,
      G_DBUS_ERROR_NOT_SUPPORTED,
      "Property %s.%s not supported", interface_name, property_name);
  return NULL;
}

static void
send_property_change (GObject * obj,
    GParamSpec * pspec, GDBusConnection * connection)
{

  GVariantBuilder *builder;
  GVariantBuilder *invalidated_builder;
  SnappyMP *myobj = (SnappyMP *) obj;

  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

  if (g_strcmp0 (pspec->name, "name") == 0)
    g_variant_builder_add (builder,
        "{sv}", "Name", g_variant_new_string (myobj->name ? myobj->name : ""));

  g_dbus_connection_emit_signal (connection,
      NULL,
      "org/mpris/MediaPlayer2",
      "org.freedesktop.DBus.Properties",
      "PropertiesChanged",
      g_variant_new ("(sa{sv}as)",
          "org.mpris.MediaPlayer2", builder, invalidated_builder), NULL);
}

static void
on_name_acquired (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
  // g_print ("DLNA MediaPlayer name acquired.\n");
}

static void
on_name_lost (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
  exit (1);
}

gboolean
load_dlna (SnappyMP * mp)
{
  guint owner_id, player_id, root_id;
  GError *error = NULL;
  GDBusInterfaceInfo *ifaceinfo;
  GDBusConnection *connection;

  g_type_init ();

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  /* Build the introspection data structures from the XML */
  introspection_data =
      g_dbus_node_info_new_for_xml (mpris_introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  /* register media player interface */
  ifaceinfo =
      g_dbus_node_info_lookup_interface (introspection_data,
      MPRIS_PLAYER_INTERFACE);
  mp->player_id =
      g_dbus_connection_register_object (connection, MPRIS_OBJECT_NAME,
      ifaceinfo, &interface_vtable, mp, NULL, &error);

  /* register root interface */
  ifaceinfo =
      g_dbus_node_info_lookup_interface (introspection_data,
      MPRIS_ROOT_INTERFACE);
  mp->root_id =
      g_dbus_connection_register_object (connection, MPRIS_OBJECT_NAME,
      ifaceinfo, &root_vtable, NULL, NULL, &error);
  if (error != NULL) {
    g_warning ("unable to register MPRIS root interface: %s", error->message);
    g_error_free (error);
  }

  mp->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
      "org.mpris.MediaPlayer2.snappy",
      G_BUS_NAME_OWNER_FLAGS_NONE,
      NULL,
      (GBusNameAcquiredCallback) on_name_acquired,
      (GBusNameLostCallback) on_name_lost, g_object_ref (mp), g_object_unref);
  g_assert (mp->owner_id > 0);

  mp->name = "snappy";

  return TRUE;
}

gboolean
close_dlna (SnappyMP * mp)
{
  g_bus_unown_name (mp->owner_id);

  g_dbus_node_info_unref (introspection_data);

  g_object_unref (mp);

  return TRUE;
}

#endif
