/*
 * snappy - 0.1 beta
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

#ifndef __MPRIS_H__
#define __MPRIS_H__

#include <gio/gio.h>
#include <stdlib.h>

#define MPRIS_BUS_NAME_PREFIX "org.mpris.MediaPlayer2"
#define MPRIS_OBJECT_NAME "/org/mpris/MediaPlayer2"

#define MPRIS_ROOT_INTERFACE "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define MPRIS_TRACKLIST_INTERFACE "org.mpris.MediaPlayer2.TrackList"
#define MPRIS_PLAYLISTS_INTERFACE "org.mpris.MediaPlayer2.Playlists"

G_BEGIN_DECLS

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

void handle_method_call (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * method_name,
    GVariant * parameters,
    GDBusMethodInvocation * invocation, gpointer user_data);

GVariant * handle_get_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name, GError ** error, gpointer user_data);

gboolean handle_set_property (GDBusConnection * connection,
    const gchar * sender,
    const gchar * object_path,
    const gchar * interface_name,
    const gchar * property_name,
    GVariant * value, GError ** error, gpointer user_data);

void handle_root_method_call (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *method_name,
    GVariant * parameters,
    GDBusMethodInvocation * invocation, MediaPlayer2 * mp);

GVariant * get_root_property (GDBusConnection * connection,
    const char *sender,
    const char *object_path,
    const char *interface_name,
    const char *property_name, GError ** error, MediaPlayer2 * mp);

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
	"  </interface>"
	"</node>";

G_END_DECLS
#endif /* __MPRIS_H__ */
