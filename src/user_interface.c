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

#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "utils.h"

#ifdef CLUTTER_WINDOWING_X11
#include <clutter/x11/clutter-x11.h>
#endif

// Declaration of static functions
static gboolean controls_timeout_cb (gpointer data);
static gboolean event_cb (ClutterStage * stage, ClutterEvent * event,
    UserInterface * ui);
static void load_controls (UserInterface * ui);
static gboolean penalty_box (gpointer data);
static gchar *position_ns_to_str (gint64 nanoseconds);
static void progress_timing (UserInterface * ui);
static gboolean progress_update_text (gpointer data);
static gboolean progress_update_seekbar (gpointer data);
gboolean rotate_video (UserInterface * ui);
static void size_change (ClutterStage * stage,
    const ClutterActorBox * allocation, ClutterAllocationFlags flags,
    UserInterface * ui);
static void show_controls (UserInterface * ui, gboolean vis);
static void toggle_fullscreen (UserInterface * ui);
static void toggle_playing (UserInterface * ui);
static void update_controls_size (UserInterface * ui);
static gboolean update_volume (UserInterface * ui, gdouble volume);

/* ---------------------- static functions ----------------------- */

static gboolean
controls_timeout_cb (gpointer data)
{
  UserInterface *ui = data;

  ui->controls_timeout = -1;

  clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
  if (!ui->keep_showing_controls) {
    show_controls (ui, FALSE);
  }

  return FALSE;
}

static gboolean
event_cb (ClutterStage * stage, ClutterEvent * event, UserInterface * ui)
{
  gboolean handled = FALSE;

  switch (event->type) {
    case CLUTTER_KEY_PRESS:
    {
      /* Clutter key codes based on */
      /* http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h */
      guint keyval = clutter_event_get_key_symbol (event);
      switch (keyval) {
        case CLUTTER_q:
        case CLUTTER_Escape:
        {
          clutter_main_quit ();

          handled = TRUE;
          break;
        }

        case CLUTTER_f:
        {
          // Fullscreen button
          toggle_fullscreen (ui);

          handled = TRUE;
          break;
        }

        case CLUTTER_space:
        {
          // Spacebar
          toggle_playing (ui);

          handled = TRUE;
          break;
        }

        case CLUTTER_l:
        {
          // Loop
          ui->engine->loop = !ui->engine->loop;

          handled = TRUE;
          break;
        }

        case CLUTTER_8:
        {
          // Mute button
          gdouble volume;
          gboolean muteval;

          g_object_get (G_OBJECT (ui->engine->player), "mute", &muteval, NULL);
          g_object_set (G_OBJECT (ui->engine->player), "mute", !muteval, NULL);
          update_volume (ui, volume);

          handled = TRUE;
          break;
        }

        case CLUTTER_9:
        case CLUTTER_0:
        {
          gdouble volume;
          g_object_get (G_OBJECT (ui->engine->player), "volume", &volume, NULL);
          // Volume Down
          if (keyval == CLUTTER_9 && volume > 0.0) {
            volume -= 0.05;
            if (volume < 0.01)
              volume = 0;
            g_object_set (G_OBJECT (ui->engine->player), "volume",
                volume, NULL);

            // Volume Up
          } else if (keyval == CLUTTER_0 && volume < 1.0) {
            volume += 0.05;
            if (volume > 1)
              volume = 1;
            g_object_set (G_OBJECT (ui->engine->player), "volume",
                volume, NULL);
          }

          update_volume (ui, volume);

          handled = TRUE;
          break;
        }

        case CLUTTER_Up:
        case CLUTTER_Down:
        case CLUTTER_Left:
        case CLUTTER_Right:
        case CLUTTER_Page_Up:
        case CLUTTER_Page_Down:
        {
          gint64 pos, second;
          gfloat progress;

          pos = query_position (ui->engine);
          second = ui->engine->second;

          if (keyval == CLUTTER_Up) {
            // Seek 1 minute foward
            pos += 60 * second;

          } else if (keyval == CLUTTER_Down) {
            // Seek 1 minute back
            pos -= 60 * second;

          } else if (keyval == CLUTTER_Right) {
            // Seek 10 seconds foward
            pos += 10 * second;

          } else if (keyval == CLUTTER_Left) {
            // Seek 10 seconds back
            pos -= 10 * second;

          } else if (keyval == CLUTTER_Page_Up) {
            // Seek 10 minutes foward
            pos += 600 * second;

          } else if (keyval == CLUTTER_Page_Down) {
            // Seek 10 minutes back
            pos -= 600 * second;
          }

          /* clamp the timestamp to be within the media */
          pos = CLAMP (pos, 0, ui->engine->media_duration);
          engine_seek (ui->engine, pos, FALSE);

          handled = TRUE;
          break;
        }

        case CLUTTER_r:
        {
          // rotate texture 90 degrees.
          rotate_video (ui);

          handled = TRUE;
          break;
        }

        case CLUTTER_c:
        {
          // show or hide controls
          penalty_box (ui);
          ui->keep_showing_controls = !ui->controls_showing;
          show_controls (ui, !ui->controls_showing);

          handled = TRUE;
          break;
        }

        case CLUTTER_period:
        {
          // frame step forward
          frame_stepping (ui->engine, TRUE);

          handled = TRUE;
          break;
        }

        case CLUTTER_comma:
        {
          // frame step backward
          frame_stepping (ui->engine, FALSE);

          handled = TRUE;
          break;
        }

        case CLUTTER_v:
        {
          // toggle subtitles
          toggle_subtitles (ui->engine);

          handled = TRUE;
          break;
        }

        case CLUTTER_numbersign:
        case CLUTTER_underscore:
        case CLUTTER_j:
        {
          // cycle through available audio/text/video streams
          guint streamid;

          if (keyval == CLUTTER_numbersign)
            streamid = STREAM_AUDIO;
          else if (keyval == CLUTTER_j)
            streamid = STREAM_TEXT;
          else if (keyval == CLUTTER_underscore)
            streamid = STREAM_VIDEO;

          cycle_streams (ui->engine, streamid);

          handled = TRUE;
          break;
        }

        case CLUTTER_less:
        {
          interface_play_next_or_prev (ui, FALSE);
          break;
        }

        case CLUTTER_greater:
        {
          interface_play_next_or_prev (ui, TRUE);
          break;
        }

        default:
        {
          handled = FALSE;
          break;
        }
      }

      break;
    }

    case CLUTTER_BUTTON_PRESS:
    {
      if (ui->controls_showing) {
        ClutterActor *actor;
        ClutterButtonEvent *bev = (ClutterButtonEvent *) event;

        actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL,
            bev->x, bev->y);
        if (actor == ui->control_play_toggle) {
          toggle_playing (ui);

        } else if (actor == ui->control_seek1 ||
            actor == ui->control_seek2 || actor == ui->control_seekbar) {
          gfloat x, y, dist;
          gint64 progress;

          clutter_actor_get_transformed_position (ui->control_seekbar, &x, &y);
          dist = bev->x - x;
          dist = CLAMP (dist, 0, ui->seek_width);

          if (ui->engine->media_duration == -1) {
            update_media_duration (ui->engine);
          }

          progress = ui->engine->media_duration * (dist / ui->seek_width);
          engine_seek (ui->engine, progress, FALSE);
          clutter_actor_set_size (ui->control_seekbar, dist, ui->seek_height);

        } else if (actor == ui->vol_int || actor == ui->vol_int_bg) {
          gfloat x, y, dist;
          gdouble volume;

          clutter_actor_get_transformed_position (ui->vol_int_bg, &x, &y);
          dist = bev->x - x;
          dist = CLAMP (dist, 0, ui->volume_width);

          volume = dist / ui->volume_width;
          g_object_set (G_OBJECT (ui->engine->player), "volume", volume, NULL);
          clutter_actor_set_size (ui->vol_int, dist, ui->volume_height);

        } else if (actor == ui->control_bg || actor == ui->control_title
            || actor == ui->control_pos) {
          ui->keep_showing_controls = !ui->keep_showing_controls;

          if (ui->keep_showing_controls) {
            clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
          } else {
            penalty_box (ui);
            show_controls (ui, FALSE);
          }

        } else if (actor == ui->texture || actor == ui->stage) {
          if (!ui->penalty_box_active) {
            penalty_box (ui);
            show_controls (ui, FALSE);
          }

        } else if (actor == ui->audio_stream_toggle) {
          cycle_streams (ui->engine, STREAM_AUDIO);

        } else if (actor == ui->subtitle_toggle) {
          toggle_subtitles (ui->engine);

        } else if (actor == ui->video_stream_toggle) {
          cycle_streams (ui->engine, STREAM_VIDEO);
        }
      }

      handled = TRUE;
      break;
    }

    case CLUTTER_MOTION:
    {
      if (!ui->penalty_box_active)
        show_controls (ui, TRUE);

      handled = TRUE;
      break;
    }
  }

  return handled;
}

static void
load_controls (UserInterface * ui)
{
  // Check icon files exist
  gchar *vid_panel_png = NULL;
  gchar *icon_files[8];
  gchar *duration_str = NULL;
  gint c;
  ClutterColor control_color1 = { 0x12, 0x12, 0x12, 0xff };
  ClutterColor control_color2 = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterLayoutManager *controls_layout = NULL;
  ClutterLayoutManager *main_box_layout = NULL;
  ClutterLayoutManager *info_box_layout = NULL;
  ClutterLayoutManager *middle_box_layout = NULL;
  ClutterLayoutManager *bottom_box_layout = NULL;
  ClutterLayoutManager *volume_box_layout = NULL;
  ClutterLayoutManager *seek_box_layout = NULL;
  ClutterLayoutManager *vol_int_box_layout = NULL;
  ClutterActor *seek_box = NULL;
  ClutterActor *middle_box = NULL;
  ClutterActor *bottom_box = NULL;
  ClutterActor *vol_int_box = NULL;
  GError *error = NULL;

  vid_panel_png = g_build_filename (ui->data_dir, "vid-panel.png", NULL);
  ui->play_png = g_build_filename (ui->data_dir, "media-actions-start.png",
      NULL);
  ui->pause_png = g_build_filename (ui->data_dir, "media-actions-pause.png",
      NULL);
  ui->volume_low_png = g_build_filename (ui->data_dir,
      "audio-volume-low.png", NULL);
  ui->volume_high_png = g_build_filename (ui->data_dir,
      "audio-volume-high.png", NULL);
  ui->subtitle_toggle_png = g_build_filename (ui->data_dir,
      "subtitle-toggle.png", NULL);
  ui->video_stream_toggle_png = g_build_filename (ui->data_dir,
      "video-stream-toggle.png", NULL);
  ui->audio_stream_toggle_png = g_build_filename (ui->data_dir,
      "audio-stream-toggle.png", NULL);

  icon_files[0] = vid_panel_png;
  icon_files[1] = ui->play_png;
  icon_files[2] = ui->pause_png;
  icon_files[3] = ui->volume_low_png;
  icon_files[4] = ui->volume_high_png;
  icon_files[5] = ui->subtitle_toggle_png;
  icon_files[6] = ui->video_stream_toggle_png;
  icon_files[7] = ui->audio_stream_toggle_png;

  for (c = 0; c < 8; c++) {
    if (!g_file_test (icon_files[c], G_FILE_TEST_EXISTS)) {
      g_print ("Icon file doesn't exist, are you sure you have "
          " installed snappy correctly?\nThis file needed is: %s\n",
          icon_files[c]);
    }
  }

  // Controls layout management
  controls_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  ui->control_box = clutter_box_new (controls_layout);

  // Controls background
  ui->control_bg = clutter_texture_new_from_file (vid_panel_png, &error);
  if (!ui->control_bg && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_actor_add_constraint (ui->control_bg,
      clutter_bind_constraint_new (ui->control_box, CLUTTER_BIND_SIZE, 0));

  g_free (vid_panel_png);
  clutter_container_add_actor (CLUTTER_CONTAINER (ui->control_box),
      ui->control_bg);
  clutter_actor_add_constraint (ui->control_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (ui->control_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0.95));

  // Main Box
  main_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (main_box_layout), FALSE);
  ui->main_box = clutter_box_new (main_box_layout);

  clutter_container_add_actor (CLUTTER_CONTAINER (ui->control_box),
      ui->main_box);
  clutter_actor_add_constraint (ui->main_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0.03));
  clutter_actor_add_constraint (ui->main_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0.03));

  // Controls play toggle
  ui->control_play_toggle =
      clutter_texture_new_from_file (ui->pause_png, &error);
  if (!ui->control_play_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  g_assert (ui->control_bg && ui->control_play_toggle);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (main_box_layout),
      ui->control_play_toggle, FALSE,        /* expand */
      FALSE,                            /* x-fill */
      FALSE,                            /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,      /* x-align */
      CLUTTER_BOX_ALIGNMENT_CENTER);    /* y-align */

  // Controls title
  info_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (info_box_layout), TRUE);

  ui->info_box = clutter_box_new (info_box_layout);

  ui->control_title = clutter_text_new_full ("Sans 32px",
      cut_long_filename (ui->filename, ui->title_length), &control_color1);
  clutter_text_set_max_length (CLUTTER_TEXT (ui->control_title),
      ui->title_length);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (info_box_layout),
      ui->control_title, TRUE,  /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_CENTER,     /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Controls seek
  seek_box_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  seek_box = clutter_box_new (seek_box_layout);

  // background box rectangle shows as the border
  ui->control_seek1 = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box), ui->control_seek1);
  clutter_actor_add_constraint (ui->control_seek1,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0));
  clutter_actor_add_constraint (ui->control_seek1,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0));

  // smaller background rectangle inside seek1 to create a border
  ui->control_seek2 = clutter_rectangle_new_with_color (&control_color2);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box), ui->control_seek2);
  clutter_actor_set_position (ui->control_seek2, SEEK_BORDER, SEEK_BORDER);

  // progress rectangle
  ui->control_seekbar = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box),
      ui->control_seekbar);
  clutter_actor_set_position (ui->control_seekbar, SEEK_BORDER, SEEK_BORDER);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (info_box_layout),
      seek_box, TRUE,           /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_END,        /* x-align */
      CLUTTER_BOX_ALIGNMENT_CENTER);    /* y-align */

  // Controls middle box
  middle_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (middle_box_layout),
      FALSE);
  middle_box = clutter_box_new (middle_box_layout);

  // Controls volume box
  volume_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (volume_box_layout),
      FALSE);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (volume_box_layout), 5);
  ui->volume_box = clutter_box_new (volume_box_layout);

  clutter_box_pack (CLUTTER_BOX (middle_box), ui->volume_box,
      "x-align", CLUTTER_BOX_ALIGNMENT_END, "expand", FALSE, NULL);

  // Controls volume low
  ui->volume_low = clutter_texture_new_from_file (ui->volume_low_png, &error);
  if (!ui->volume_low && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_box_pack (CLUTTER_BOX (ui->volume_box), ui->volume_low, "x-align",
      CLUTTER_BOX_ALIGNMENT_START, NULL);

  // Controls volume intensity
  vol_int_box_layout =
      clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  vol_int_box = clutter_box_new (vol_int_box_layout);

  ui->vol_int_bg = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (vol_int_box), ui->vol_int_bg);
  clutter_actor_set_position (ui->vol_int_bg, 0, 0);

  ui->vol_int = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (vol_int_box), ui->vol_int);

  clutter_box_pack (CLUTTER_BOX (ui->volume_box), vol_int_box,
      "x-fill", FALSE,
      "y-fill", FALSE, "y-align", CLUTTER_BOX_ALIGNMENT_CENTER, NULL);

  // Controls volume high
  ui->volume_high = clutter_texture_new_from_file (ui->volume_high_png, &error);
  if (!ui->volume_high && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_box_pack (CLUTTER_BOX (ui->volume_box), ui->volume_high, "x-align",
      CLUTTER_BOX_ALIGNMENT_END, NULL);

  // Controls position text
  duration_str = g_strdup_printf ("   0:00:00/%s", ui->duration_str);
  ui->control_pos = clutter_text_new_full ("Sans 22px", duration_str,
      &control_color1);
  clutter_box_pack (CLUTTER_BOX (middle_box), ui->control_pos,
      "x-align", CLUTTER_BOX_ALIGNMENT_END, "expand", FALSE, NULL);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (info_box_layout),
      middle_box, TRUE,         /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_END,     /* x-align */
      CLUTTER_BOX_ALIGNMENT_END);    /* y-align */

  // Controls bottom box
  bottom_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (bottom_box_layout),
      FALSE);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (bottom_box_layout), 5);
  bottom_box = clutter_box_new (bottom_box_layout);

  // Controls video stream toggle
  ui->video_stream_toggle = clutter_texture_new_from_file
      (ui->video_stream_toggle_png, &error);
  if (!ui->video_stream_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_box_pack (CLUTTER_BOX (bottom_box), ui->video_stream_toggle,
      "x-align", CLUTTER_BOX_ALIGNMENT_START, NULL);

  // Controls audio stream toggle
  ui->audio_stream_toggle = clutter_texture_new_from_file
      (ui->audio_stream_toggle_png, &error);
  if (!ui->audio_stream_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_box_pack (CLUTTER_BOX (bottom_box), ui->audio_stream_toggle,
      "x-align", CLUTTER_BOX_ALIGNMENT_END, NULL);

  // Controls subtitle toggle
  ui->subtitle_toggle = clutter_texture_new_from_file (ui->subtitle_toggle_png,
      &error);
  if (!ui->subtitle_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_box_pack (CLUTTER_BOX (bottom_box), ui->subtitle_toggle, "x-align",
      CLUTTER_BOX_ALIGNMENT_END, NULL);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (info_box_layout),
      bottom_box, TRUE,         /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_END,        /* x-align */
      CLUTTER_BOX_ALIGNMENT_END);       /* y-align */

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (main_box_layout),
      ui->info_box, FALSE,      /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_END,        /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  clutter_actor_lower_bottom (ui->control_bg);

  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);
}

static gboolean
penalty_box (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;

  if (ui->penalty_box_active) {
    ui->penalty_box_active = FALSE;
  } else {
    g_timeout_add (PENALTY_TIME, penalty_box, ui);
    ui->penalty_box_active = TRUE;
  }

  return ui->penalty_box_active;
}

static gchar *
position_ns_to_str (gint64 nanoseconds)
{
  gint64 seconds;
  gint hours, minutes;

  seconds = nanoseconds / GST_SECOND;
  hours = seconds / SEC_IN_HOUR;
  seconds = seconds - (hours * SEC_IN_HOUR);
  minutes = seconds / SEC_IN_MIN;
  seconds = seconds - (minutes * SEC_IN_MIN);

  return g_strdup_printf ("%d:%02d:%02ld", hours, minutes, seconds);
}

static void
progress_timing (UserInterface * ui)
{
  gint64 duration_ms;
  gint64 timeout_ms;

  if (ui->progress_id != -1)
    g_source_remove (ui->progress_id);

  duration_ms = ui->engine->media_duration / GST_MSECOND;
  if (duration_ms > 0) {
    timeout_ms = MAX (250, duration_ms / ui->seek_width);
    ui->progress_id = g_timeout_add (timeout_ms, progress_update_seekbar, ui);
  }
}

static gboolean
progress_update_text (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;
  GstEngine *engine = ui->engine;

  if (ui->controls_showing && !engine->queries_blocked) {
    if (engine->media_duration != -1) {
      gchar *duration_str;
      gint64 pos;

      if (ui->media_duration != engine->media_duration) {
        ui->duration_str = position_ns_to_str (engine->media_duration);
        progress_timing (ui);
      }

      pos = query_position (engine);
      duration_str = g_strdup_printf ("   %s/%s", position_ns_to_str (pos),
          ui->duration_str);
      clutter_text_set_text (CLUTTER_TEXT (ui->control_pos), duration_str);
    }
  }

  return TRUE;
}

static gboolean
progress_update_seekbar (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;
  GstEngine *engine = ui->engine;

  if (ui->controls_showing && !engine->queries_blocked) {
    if (engine->media_duration != -1) {
      gint64 pos;
      gfloat progress = 0.0;

      pos = query_position (engine);
      progress = (float) pos / engine->media_duration;

      clutter_actor_set_size (ui->control_seekbar, progress * ui->seek_width,
          ui->seek_height);
    }
  }

  return TRUE;
}

gboolean
rotate_video (UserInterface * ui)
{
  gfloat *x = 0, *y = 0, *z = 0;
  gfloat vid_width, vid_height;
  gfloat x_center, y_center;
  gdouble angle;

  angle = clutter_actor_get_rotation (ui->texture, CLUTTER_Z_AXIS, x, y, z);
  angle += 90;
  if (angle == 360)
    angle = 0;
  clutter_actor_set_z_rotation_from_gravity (ui->texture, angle,
      CLUTTER_GRAVITY_CENTER);

  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);

  return TRUE;
}

static void
size_change (ClutterStage * stage,
    const ClutterActorBox * allocation,
    ClutterAllocationFlags flags, UserInterface * ui)
{
  gfloat stage_width, stage_height;
  gfloat new_width, new_height;
  gfloat media_width, media_height;
  gfloat stage_ar, media_ar;

  media_width = clutter_actor_get_width (ui->texture);
  media_height = clutter_actor_get_height (ui->texture);

  stage_width = clutter_actor_get_width (ui->stage);
  stage_height = clutter_actor_get_height (ui->stage);

  ui->stage_width = stage_width;
  ui->stage_height = stage_height;

  stage_ar = stage_width / stage_height;

  new_width = stage_width;
  new_height = stage_height;

  if (media_height > 0.0f && media_width > 0.0f) {
    media_ar = media_width / media_height;

    /* calculate new width and height
     * note: when we're done, new_width/new_height should equal media_ar */
    if (media_ar > stage_ar) {
      /* media has wider aspect than stage so use new width as stage width and
       * scale down height */
      new_height = stage_width / media_ar;
    } else {
      new_width = stage_height * media_ar;
    }
  } else {
    g_debug ("Warning: not considering texture dimensions %fx%f", media_width,
        media_height);
  }

  clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), new_width, new_height);

  update_controls_size (ui);
  progress_timing (ui);
}

static void
show_controls (UserInterface * ui, gboolean vis)
{
  gboolean cursor;

  if (vis == TRUE && ui->controls_showing == TRUE) {
    // ToDo: add 3 more seconds to the controls hiding delay
    g_object_get (G_OBJECT (ui->stage), "cursor-visible", &cursor, NULL);
    if (!cursor)
      clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));
    if (ui->controls_timeout == -1) {
      ui->controls_timeout = g_timeout_add_seconds (CTL_SHOW_SEC,
          controls_timeout_cb, ui);
    }
  }

  else if (vis == TRUE && ui->controls_showing == FALSE) {
    ui->controls_showing = TRUE;

    progress_update_seekbar (ui);
    progress_update_text (ui);
    clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));
    clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT,
        CTL_FADE_DURATION, "opacity", 0xa0, NULL);

    if (ui->controls_timeout == -1) {
      ui->controls_timeout = g_timeout_add_seconds (CTL_SHOW_SEC,
          controls_timeout_cb, ui);
    }
  }

  else if (vis == FALSE && ui->controls_showing == TRUE) {
    ui->controls_showing = FALSE;

    clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
    clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT,
        CTL_FADE_DURATION, "opacity", 0, NULL);
  }
}

#ifdef CLUTTER_WINDOWING_X11
static void
toggle_fullscreen_x11 (ClutterStage * stage, gboolean fullscreen)
{
  static gboolean is_fullscreen = FALSE;
  static float old_width, old_height;

  struct
  {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
  } MWMHints = {
  2, 0, 0, 0, 0};

  Display *xdisplay = clutter_x11_get_default_display ();
  int xscreen = clutter_x11_get_default_screen ();
  Atom wm_hints = XInternAtom (xdisplay, "_MOTIF_WM_HINTS", True);
  Window xwindow = clutter_x11_get_stage_window (stage);
  // XRRScreenResources *xresources = XRRGetScreenResources (xdisplay, xwindow);
  // g_print ("outputs: %d\n", xresources->ncrtc);

  if (fullscreen) {
    int full_width = DisplayWidth (xdisplay, xscreen);
    int full_height = DisplayHeight (xdisplay, xscreen) + 5;
    /* avoid being detected as fullscreen, workaround for some
       windowmanagers  */
    clutter_actor_get_size (CLUTTER_ACTOR (stage), &old_width, &old_height);

    if (wm_hints != None)
      XChangeProperty (xdisplay, xwindow, wm_hints, wm_hints, 32,
          PropModeReplace, (guchar *) & MWMHints,
          sizeof (MWMHints) / sizeof (long));
    clutter_actor_set_size (CLUTTER_ACTOR (stage), full_width, full_height);
    XMoveResizeWindow (xdisplay, xwindow, 0, 0, full_width, full_height);
  } else {
    MWMHints.decorations = 7;
    if (wm_hints != None)
      XChangeProperty (xdisplay, xwindow, wm_hints, wm_hints, 32,
          PropModeReplace, (guchar *) & MWMHints,
          sizeof (MWMHints) / sizeof (long));
    clutter_stage_set_fullscreen (stage, FALSE);
    clutter_actor_set_size (CLUTTER_ACTOR (stage), old_width, old_height);
  }
}
#endif

static void
toggle_fullscreen (UserInterface * ui)
{
#ifdef CLUTTER_WINDOWING_X11
  if (ui->fullscreen) {
    toggle_fullscreen_x11 (CLUTTER_STAGE (ui->stage), FALSE);
    ui->fullscreen = FALSE;
  } else {
    toggle_fullscreen_x11 (CLUTTER_STAGE (ui->stage), TRUE);
    ui->fullscreen = TRUE;
  }

#else
  if (ui->fullscreen) {
    clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), FALSE);
    ui->fullscreen = FALSE;
  } else {
    clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), TRUE);
    ui->fullscreen = TRUE;
  }
#endif
}

static void
toggle_playing (UserInterface * ui)
{
  GstEngine *engine = ui->engine;

  if (engine->playing) {
    change_state (engine, "Paused");
    engine->playing = FALSE;

    clutter_texture_set_from_file (CLUTTER_TEXTURE (ui->control_play_toggle),
        ui->play_png, NULL);
  } else {
    change_state (engine, "Playing");
    engine->playing = TRUE;

    clutter_texture_set_from_file (CLUTTER_TEXTURE (ui->control_play_toggle),
        ui->pause_png, NULL);
  }
}


static void
update_controls_size (UserInterface * ui)
{
  gchar *font_name;
  gfloat ctl_width, ctl_height, text_width;
  gfloat icon_size;

  // g_print ("Updating controls size for stage: %ux%u\n", ui->stage_width,
  //     ui->stage_height);

  ctl_width = ui->stage_width * CONTROLS_WIDTH_RATIO;
  ctl_height = ui->stage_height * CONTROLS_HEIGHT_RATIO;

  if (ctl_width / ctl_height > CONTROLS_ASPECT_RATIO) {
    ctl_width = ctl_height * CONTROLS_ASPECT_RATIO;
  } else {
    ctl_height = ctl_width / CONTROLS_ASPECT_RATIO;
  }

  clutter_actor_set_size (ui->control_box,
      ctl_width + ((ctl_width / BG_W) * SHADOW_RIGHT),
      ctl_height + ((ctl_height / BG_H) * SHADOW_BOTTOM));

  icon_size = ctl_height * PLAY_TOGGLE_RATIO;
  clutter_actor_set_size (ui->control_play_toggle, icon_size, icon_size);
  clutter_actor_set_size (ui->info_box, (ctl_width * MAIN_BOX_W - icon_size),
      ctl_height * MAIN_BOX_H);

  font_name = g_strdup_printf ("Sans %dpx", (gint) (ctl_width * TITLE_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_title), font_name);
  text_width = clutter_actor_get_width (CLUTTER_ACTOR (ui->control_title));

  ui->seek_width = 12 +         // accomodate volume_box spacing
      (ctl_width * MAIN_BOX_W - icon_size) * SEEK_WIDTH_RATIO -
      2.0f * SEEK_BORDER;
  ui->seek_height =
      ctl_height * MAIN_BOX_H * SEEK_HEIGHT_RATIO - 2.0f * SEEK_BORDER;

  clutter_actor_set_size (ui->control_seek1,
      ui->seek_width + 2.0f * SEEK_BORDER,
      ui->seek_height + 2.0f * SEEK_BORDER);

  clutter_actor_set_size (ui->control_seek2, ui->seek_width, ui->seek_height);

  progress_update_seekbar (ui);

  font_name = g_strdup_printf ("Sans %dpx", (gint) (ctl_height * POS_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_pos), font_name);
  text_width = clutter_actor_get_width (CLUTTER_ACTOR (ui->control_pos));

  ui->volume_width =
      (ctl_width * MAIN_BOX_W - icon_size -
      clutter_actor_get_width (CLUTTER_ACTOR (ui->control_pos))) *
      VOLUME_WIDTH_RATIO;
  ui->volume_height = ctl_height * MAIN_BOX_H * VOLUME_HEIGHT_RATIO;
  clutter_actor_set_size (ui->vol_int_bg, ui->volume_width, ui->volume_height);

  icon_size = ctl_height * VOLUME_ICON_RATIO;
  clutter_actor_set_size (ui->volume_low, icon_size, icon_size);
  clutter_actor_set_size (ui->volume_high, icon_size * 1.2f, icon_size);        /* originally 120x100 */
  clutter_actor_set_size (ui->subtitle_toggle, icon_size, icon_size);
  clutter_actor_set_size (ui->video_stream_toggle, icon_size, icon_size);
  clutter_actor_set_size (ui->audio_stream_toggle, icon_size, icon_size);

  update_volume (ui, -1);
}

static gboolean
update_volume (UserInterface * ui, gdouble volume)
{
  if (volume == -1)
    g_object_get (G_OBJECT (ui->engine->player), "volume", &volume, NULL);

  clutter_actor_set_size (ui->vol_int, volume * ui->volume_width,
      ui->volume_height);

  return TRUE;
}

/* -------------------- non-static functions --------------------- */

void
interface_init (UserInterface * ui)
{
  ui->filename = NULL;
  ui->fileuri = NULL;

  ui->play_png = NULL;
  ui->pause_png = NULL;

  ui->volume_low_png = NULL;
  ui->volume_high_png = NULL;

  ui->subtitle_toggle_png = NULL;
  ui->video_stream_toggle_png = NULL;
  ui->audio_stream_toggle_png = NULL;

  ui->duration_str = NULL;

  ui->stage = NULL;
  ui->texture = NULL;

  ui->control_box = NULL;
  ui->control_bg = NULL;
  ui->control_title = NULL;
  ui->control_play_toggle = NULL;

  ui->control_seek1 = NULL;
  ui->control_seek2 = NULL;
  ui->control_seekbar = NULL;
  ui->control_pos = NULL;

  ui->volume_box = NULL;
  ui->volume_low = NULL;
  ui->volume_high = NULL;
  ui->vol_int = NULL;
  ui->vol_int_bg = NULL;
  ui->volume_point = NULL;

  ui->info_box = NULL;
  ui->main_box = NULL;

  ui->engine = NULL;
  ui->screensaver = NULL;
}

gboolean
interface_load_uri (UserInterface * ui, gchar * uri)
{
  ui->fileuri = uri;

  ui->filename = g_path_get_basename (ui->fileuri);

  if (ui->stage != NULL) {
    clutter_stage_set_title (CLUTTER_STAGE (ui->stage), ui->filename);
    clutter_text_set_text (CLUTTER_TEXT (ui->control_title), ui->filename);
  }

  ui->duration_str = position_ns_to_str (ui->engine->media_duration);
  ui->media_width = ui->engine->media_width;
  ui->media_height = ui->engine->media_height;

  clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), ui->media_width,
      ui->media_height);
  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);

  if (!ui->fullscreen)
    clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->media_width,
        ui->media_height);

  if (!ui->penalty_box_active)
    show_controls (ui, TRUE);

  return TRUE;
}

void
interface_play_next_or_prev (UserInterface * ui, gboolean next)
{
  GList *element;
  gchar *uri;

  element = g_list_find (ui->uri_list, ui->engine->uri);
  if (next)
    element = g_list_next (element);
  else
    element = g_list_previous (element);

  if (element != NULL) {
    uri = element->data;

    engine_open_uri (ui->engine, uri);
    interface_load_uri (ui, uri);
    engine_play (ui->engine);
  }
}

void
interface_start (UserInterface * ui, gchar * uri)
{
  ClutterColor stage_color = { 0x00, 0x00, 0x00, 0x00 };

  g_print ("Loading ui!\n");

  // Init UserInterface structure variables
  ui->fileuri = uri;
  ui->filename = g_path_get_basename (ui->fileuri);

  ui->media_width = ui->engine->media_width;
  ui->media_height = ui->engine->media_height;

  ui->stage_width = ui->media_width;
  ui->stage_height = ui->media_height;
  ui->stage = clutter_stage_get_default ();

  ui->controls_showing = FALSE;
  ui->keep_showing_controls = FALSE;
  ui->penalty_box_active = FALSE;
  ui->controls_timeout = -1;

  ui->seek_width = ui->stage_width / SEEK_WIDTH_RATIO;
  ui->seek_height = ui->stage_height / SEEK_HEIGHT_RATIO;

  ui->progress_id = -1;
  ui->title_length = TITLE_LENGTH;
  ui->media_duration = -1;
  ui->duration_str = position_ns_to_str (ui->engine->media_duration);

  clutter_stage_set_color (CLUTTER_STAGE (ui->stage), &stage_color);
  clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->media_width,
      ui->media_height);
  clutter_stage_set_title (CLUTTER_STAGE (ui->stage), ui->filename);

  clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->stage_width,
      ui->stage_height);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (ui->stage), TRUE);

  if (ui->fullscreen) {
    clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), TRUE);
  }

  // Controls
  load_controls (ui);

  // Add video texture and control UI to stage
  clutter_container_add (CLUTTER_CONTAINER (ui->stage), ui->texture, NULL);
  if (!ui->hide) {
    clutter_container_add (CLUTTER_CONTAINER (ui->stage), ui->control_box,
        NULL);
  }
  clutter_actor_add_constraint (ui->texture,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (ui->texture,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
  clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT,
      G_TIME_SPAN_MILLISECOND, "opacity", 0, NULL);

  g_signal_connect (CLUTTER_STAGE (ui->stage), "allocation-changed",
      G_CALLBACK (size_change), ui);
  g_signal_connect (CLUTTER_STAGE (ui->stage), "event", G_CALLBACK (event_cb),
      ui);

  progress_timing (ui);

  ui->screensaver = screensaver_new (CLUTTER_STAGE (ui->stage));
  screensaver_enable (ui->screensaver, FALSE);

  g_timeout_add (G_TIME_SPAN_MILLISECOND, progress_update_text, ui);

  if (!ui->blind)
    clutter_actor_show (ui->stage);
}

gboolean
interface_update_controls (UserInterface * ui)
{
  progress_update_text (ui);
  progress_update_seekbar (ui);
  update_volume (ui, -1);

  return TRUE;
}
