// gcc `pkg-config --libs --cflags gio-2.0` mpris.c -o mpris

#include <gio/gio.h>
#include <stdlib.h>
#include "mpris.h"


/* ---------------------------------------------------------------------------------------------------- */

/* The object we want to export */
typedef struct _MediaPlayer2Class MediaPlayer2Class;
typedef struct _MediaPlayer2 MediaPlayer2;

struct _MediaPlayer2Class
{
  GObjectClass parent_class;
};

struct _MediaPlayer2
{
  GObject parent_instance;

  gint count;
  gchar *name;

  GDBusConnection *connection;
  GDBusNodeInfo *node_info;
  guint name_own_id;
  guint root_id;
  guint player_id;
  guint playlists_id;

  int playlist_count;

  GHashTable *player_property_changes;
  GHashTable *playlist_property_changes;
  guint property_emit_id;

  gint64 last_elapsed;
};

enum
{
  PROP_0,
  PROP_COUNT,
  PROP_NAME
};

G_DEFINE_TYPE (MediaPlayer2, my_object, G_TYPE_OBJECT);

static void
my_object_finalize (GObject * object)
{
  MediaPlayer2 *myobj = (MediaPlayer2 *) object;

  g_free (myobj->name);

  G_OBJECT_CLASS (my_object_parent_class)->finalize (object);
}

static void
my_object_init (MediaPlayer2 * object)
{
  object->count = 0;
  object->name = NULL;
}

static void
my_object_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  MediaPlayer2 *myobj = (MediaPlayer2 *) object;

  switch (prop_id) {
    case PROP_COUNT:
      g_value_set_int (value, myobj->count);
      break;

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
  MediaPlayer2 *myobj = (MediaPlayer2 *) object;

  switch (prop_id) {
    case PROP_COUNT:
      myobj->count = g_value_get_int (value);
      break;

    case PROP_NAME:
      g_free (myobj->name);
      myobj->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
my_object_class_init (MediaPlayer2Class * class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = my_object_finalize;
  gobject_class->set_property = my_object_set_property;
  gobject_class->get_property = my_object_get_property;

  g_object_class_install_property (gobject_class,
      PROP_COUNT,
      g_param_spec_int ("count",
          "Count", "Count", 0, 99999, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL, G_PARAM_READWRITE));
}

/* A method that we want to export */
void
my_object_change_count (MediaPlayer2 * myobj, gint change)
{
  myobj->count = 2 * myobj->count + change;

  g_object_notify (G_OBJECT (myobj), "count");
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

static void
handle_result (GDBusMethodInvocation * invocation, gboolean ret, GError * error)
{
  if (ret) {
    g_dbus_method_invocation_return_value (invocation, NULL);
  } else {
    if (error != NULL) {
      g_print ("returning error: %s", error->message);
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    } else {
      g_print ("returning unknown error");
      g_dbus_method_invocation_return_error_literal (invocation,
          G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Unknown error");
    }
  }
}

static void
handle_method_call (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * method_name,
    GVariant * parameters,
    GDBusMethodInvocation * invocation, gpointer user_data)
{
  MediaPlayer2 *myobj = user_data;
  gboolean ret = TRUE;
  GError *error = NULL;

  g_print ("handle_method_call\n");

  if (g_strcmp0 (method_name, "ChangeCount") == 0) {
    gint change;
    g_variant_get (parameters, "(i)", &change);

    my_object_change_count (myobj, change);

    g_dbus_method_invocation_return_value (invocation, NULL);
  } else if (g_strcmp0 (method_name, "Next") == 0) {
    g_print ("next\n");
    /// ToDo: next track call
    handle_result (invocation, ret, error);
  } else if (g_strcmp0 (method_name, "OpenUri") == 0) {
    const gchar *uri;

    g_variant_get (parameters, "(&s)", &uri);
    g_print ("set uri: %s\n", uri);
    handle_result (invocation, ret, error);
  }
}

static GVariant *
handle_get_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name, GError ** error, gpointer user_data)
{
  GVariant *ret;
  MediaPlayer2 *myobj = user_data;

  g_print ("handle_get_property: %s\n", property_name);

  ret = NULL;
  if (g_strcmp0 (property_name, "Count") == 0) {
    ret = g_variant_new_int32 (myobj->count);
  } else if (g_strcmp0 (property_name, "Name") == 0) {
    //ret = g_variant_new_string (myobj->name ? myobj->name : "");
    ret = g_variant_new_string ("snappy");
  } else if (g_strcmp0 (property_name, "Volume") == 0) {
    ret = g_variant_new_double (0);
  } else if (g_strcmp0 (property_name, "PlaybackStatus") == 0) {
    ret = g_variant_new_string ("Paused");
  } else if (g_strcmp0 (property_name, "Metadata") == 0) {
    const char *strv[] = { "", NULL };
    ret = g_variant_new_strv (strv, -1);
  } else if (g_strcmp0 (property_name, "Position") == 0) {
    ret = g_variant_new_double (0);
  }
  return ret;
}

static gboolean
handle_set_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name,
    GVariant * value, GError ** error, gpointer user_data)
{
  MediaPlayer2 *myobj = user_data;

  g_print ("handle_set_property\n");

  if (g_strcmp0 (property_name, "Count") == 0) {
    g_object_set (myobj, "count", g_variant_get_int32 (value), NULL);
  } else if (g_strcmp0 (property_name, "Name") == 0) {
    g_object_set (myobj, "name", g_variant_get_string (value, NULL), NULL);
  }

  return TRUE;
}

static void
handle_root_method_call (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *method_name,
    GVariant * parameters,
    GDBusMethodInvocation * invocation, MediaPlayer2 * mp)
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

static GVariant *
get_root_property (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *property_name, GError ** error, MediaPlayer2 * mp)
{
  g_print ("get_root_property: %s\n", property_name);

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0) {
    g_set_error (error,
        G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED,
        "Property %s.%s not supported", interface_name, property_name);
    return NULL;
  }

  if (g_strcmp0 (property_name, "CanQuit") == 0) {
    return g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "CanRaise") == 0) {
    return g_variant_new_boolean (TRUE);
  } else if (g_strcmp0 (property_name, "HasTrackList") == 0) {
    return g_variant_new_boolean (FALSE);
  } else if (g_strcmp0 (property_name, "Identity") == 0) {
    return g_variant_new_string ("snappy");
  } else if (g_strcmp0 (property_name, "DesktopEntry") == 0) {
    GVariant *v = NULL;
    char *path;
    return v;
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

static void
send_property_change (GObject * obj,
    GParamSpec * pspec, GDBusConnection * connection)
{
  GVariantBuilder *builder;
  GVariantBuilder *invalidated_builder;
  MediaPlayer2 *myobj = (MediaPlayer2 *) obj;

  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

  if (g_strcmp0 (pspec->name, "count") == 0)
    g_variant_builder_add (builder,
        "{sv}", "Count", g_variant_new_int32 (myobj->count));
  else if (g_strcmp0 (pspec->name, "name") == 0)
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
on_bus_acquired (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
  MediaPlayer2 *myobj = user_data;
  guint registration_id;

  g_print ("on bus acquired\n");

  g_signal_connect (myobj, "notify",
      G_CALLBACK (send_property_change), connection);
  registration_id = g_dbus_connection_register_object (connection, "/org/mpris/MediaPlayer2", introspection_data->interfaces[0], &interface_vtable, myobj, NULL,        /* user_data_free_func */
      NULL);                    /* GError** */
  // g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
}

static void
on_name_lost (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint owner_id, player_id, root_id;
  GMainLoop *loop;
  MediaPlayer2 *myobj;

  GError *error = NULL;
  GDBusInterfaceInfo *ifaceinfo;
  GDBusConnection *connection;

  g_type_init ();

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  /* Build the introspection data structures from the XML */
  introspection_data =
      g_dbus_node_info_new_for_xml (mpris_introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  myobj = g_object_new (my_object_get_type (), NULL);

  /* register root interface */
  ifaceinfo =
      g_dbus_node_info_lookup_interface (introspection_data,
      MPRIS_ROOT_INTERFACE);
  root_id =
      g_dbus_connection_register_object (connection, MPRIS_OBJECT_NAME,
      ifaceinfo, &root_vtable, NULL, NULL, &error);
  if (error != NULL) {
    g_warning ("unable to register MPRIS root interface: %s", error->message);
    g_error_free (error);
  }

  /* register player interface */
  ifaceinfo =
      g_dbus_node_info_lookup_interface (introspection_data,
      MPRIS_PLAYER_INTERFACE);
  player_id =
      g_dbus_connection_register_object (connection, MPRIS_OBJECT_NAME,
      ifaceinfo, &interface_vtable, NULL, NULL, &error);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
      "org.mpris.MediaPlayer2.snappy",
      G_BUS_NAME_OWNER_FLAGS_NONE,
      on_bus_acquired, on_name_acquired, on_name_lost, myobj, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_unref (introspection_data);

  g_object_unref (myobj);

  return 0;
}
