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

#ifndef __USER_INTERFACE_H__
#define __USER_INTERFACE_H__

#include "gst_engine.h"
#include "screensaver.h"

#define CTL_SHOW_SEC 3
#define CTL_FADE_DURATION G_TIME_SPAN_MILLISECOND / 4

#define CTL_BORDER 0
#define SHADOW_CORRECT 15
#define CTL_SPACING 0

#define CONTROLS_WIDTH_RATIO 0.8f
#define CONTROLS_HEIGHT_RATIO 0.25f
#define CONTROLS_ASPECT_RATIO 4.0f

#define BG_W 986.0f
#define BG_H 162.0f
#define SHADOW_RIGHT 18.0f
#define SHADOW_BOTTOM 11.0f

#define MAIN_BOX_W 0.95f
#define MAIN_BOX_H 0.8f

#define PLAY_TOGGLE_RATIO 0.9f
#define TITLE_RATIO 0.03f
#define SEEK_WIDTH_RATIO 0.9f
#define SEEK_HEIGHT_RATIO 0.15f
#define SEEK_BORDER 2.0f
#define POS_RATIO 0.1f
#define VOLUME_ICON_RATIO 0.2f
#define VOLUME_WIDTH_RATIO 0.65f
#define VOLUME_HEIGHT_RATIO 0.05f

#define TITLE_LENGTH 40

#define SEC_IN_HOUR 3600
#define SEC_IN_MIN 60

#define PENALTY_TIME G_TIME_SPAN_MILLISECOND / 2

G_BEGIN_DECLS

enum
{
  STREAM_AUDIO,
  STREAM_TEXT,
  STREAM_VIDEO
};

typedef struct _UserInterface UserInterface;

struct _UserInterface
{
  gboolean controls_showing, keep_showing_controls;
  gboolean blind, fullscreen, hide, penalty_box_active, tags;

  gint title_length, controls_timeout, progress_id;
  guint media_width, media_height;
  guint stage_width, stage_height;
  gint64 media_duration;
  gfloat seek_width, seek_height;
  gfloat volume_width, volume_height;

  gchar *filename, *fileuri;
  gchar *play_png, *pause_png;
  gchar *segment_png;
  gchar *volume_low_png, *volume_high_png;
  gchar *subtitle_toggle_png;
  gchar *video_stream_toggle_png, *audio_stream_toggle_png;
  gchar *data_dir;
  gchar *duration_str;

  GList *uri_list;

  ClutterColor stage_color, control_color1, control_color2;

  ClutterActor *stage;
  ClutterActor *texture;
  ClutterActor *control_box;
  ClutterActor *control_bg, *control_title, *control_play_toggle;
  ClutterActor *control_seek1, *control_seek2, *control_seekbar;
  ClutterActor *control_pos;
  ClutterActor *volume_box;
  ClutterActor *volume_low, *volume_high;
  ClutterActor *subtitle_toggle;
  ClutterActor *video_stream_toggle, *audio_stream_toggle;
  ClutterActor *vol_int, *vol_int_bg, *volume_point;
  ClutterActor *info_box;
  ClutterActor *main_box;

  GstEngine *engine;
  ScreenSaver *screensaver;
};

// Declaration of non-static functions
void interface_init (UserInterface * ui);
gboolean interface_load_uri (UserInterface * ui, gchar * uri);
void interface_play_next_or_prev (UserInterface * ui, gboolean next);
void interface_start (UserInterface * ui, gchar * uri);
gboolean interface_update_controls (UserInterface * ui);

G_END_DECLS
#endif /* __USER_INTERFACE_H__ */
