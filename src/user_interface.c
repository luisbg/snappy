/*
 * snappy - 0.3
 *
 * Copyright (C) 2011-213 Collabora Ltd.
 * Luis de Bethencourt <luis.debethencourt@collabora.co.uk>
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

#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <cairo.h>
#include <math.h>
#include <clutter-gst/clutter-gst.h>
#include <clutter-gtk/clutter-gtk.h>

#include "user_interface.h"
#include "utils.h"

// Declaration of static functions
static gboolean controls_timeout_cb (gpointer data);
static gboolean draw_background (ClutterCanvas * canvas, cairo_t * cr,
    int surface_width, int surface_height, UserInterface * ui);
static gboolean draw_progressbar (ClutterCanvas * canvas, cairo_t * cr,
    int surface_width, int surface_height, UserInterface * ui);
static gboolean event_cb (ClutterStage * stage, ClutterEvent * event,
    UserInterface * ui);
static void hide_cursor (UserInterface * ui);
static void load_controls (UserInterface * ui);
static void new_video_size (UserInterface * ui, gfloat width, gfloat height,
    gfloat * new_width, gfloat * new_height);
static gboolean penalty_box (gpointer data);
static gchar *position_ns_to_str (UserInterface * ui, gint64 nanoseconds);
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

  hide_cursor (ui);
  if (!ui->keep_showing_controls) {
    show_controls (ui, FALSE);
  }

  return FALSE;
}


static gboolean
draw_background (ClutterCanvas * canvas, cairo_t * cr, int surface_width,
    int surface_height, UserInterface * ui)
{
  /* rounded rectangle taken from:
   *
   *   http://cairographics.org/samples/rounded_rectangle/
   *
   */
  double x, y, width, height, aspect, corner_radius, radius, degrees;
  double red, green, blue, alpha;

  x = 1.0;
  y = 1.0;
  width = surface_width - 2.0;
  height = surface_height - 2.0;
  aspect = 1.0;                 // aspect ratio
  corner_radius = height / 15.0;        // and corner curvature radius

  radius = corner_radius / aspect;
  degrees = M_PI / 180.0;

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees,
      0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees,
      90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees,
      180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  red = (double) ui->stage_bg_color.red / 256.0;
  green = (double) ui->stage_bg_color.green / 256.0;
  blue = (double) ui->stage_bg_color.blue / 256.0;
  alpha = (double) ui->stage_bg_color.alpha / 256.0;

  cairo_set_source_rgba (cr, red, green, blue, alpha);
  cairo_close_path (cr);

  cairo_fill_preserve (cr);

  red = (double) ui->border_color.red / 256.0;
  green = (double) ui->border_color.green / 256.0;
  blue = (double) ui->border_color.blue / 256.0;
  alpha = (double) ui->border_color.alpha / 256.0;

  cairo_set_source_rgba (cr, red, green, blue, alpha);
  cairo_stroke (cr);


  // We are done drawing
  return TRUE;
}


static gboolean
draw_progressbar (ClutterCanvas * canvas, cairo_t * cr, int surface_width,
    int surface_height, UserInterface * ui)
{
  double x, y, width, height, aspect, corner_radius, radius, degrees;
  double red, green, blue, alpha;
  gfloat position;
  cairo_pattern_t *pattern;

  x = 1.0;
  y = 1.0;
  width = surface_width - 2.0;
  height = surface_height - 2.0;
  aspect = 1.0;                 // aspect ratio
  corner_radius = height / 4.0; // and corner curvature radius

  radius = corner_radius / aspect;
  degrees = M_PI / 180.0;

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

  pattern = cairo_pattern_create_linear (0.0, 0.0, surface_width, 0.0);

  if (canvas == CLUTTER_CANVAS (ui->seek_canvas)) {
    // if called for seek canvas, update playback position
    position = ui->playback_position;
  } else {
    // if called for volume canvas, update volume level
    position = ui->volume;
  }

  red = (double) ui->gradient_start.red / 256.0;
  green = (double) ui->gradient_start.green / 256.0;
  blue = (double) ui->gradient_start.blue / 256.0;
  alpha = (double) ui->gradient_start.alpha / 256.0;
  cairo_pattern_add_color_stop_rgba (pattern, 0.0, red, green, blue, alpha);

  red = (double) ui->gradient_finish.red / 256.0;
  green = (double) ui->gradient_finish.green / 256.0;
  blue = (double) ui->gradient_finish.blue / 256.0;
  alpha = (double) ui->gradient_finish.alpha / 256.0;
  cairo_pattern_add_color_stop_rgba (pattern, position, red, green, blue,
      alpha);

  cairo_pattern_add_color_stop_rgba (pattern, position + 0.0001, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);
  cairo_set_source (cr, pattern);

  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees,
      0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees,
      90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees,
      180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  cairo_fill_preserve (cr);

  red = (double) ui->border_color.red / 256.0;
  green = (double) ui->border_color.green / 256.0;
  blue = (double) ui->border_color.blue / 256.0;
  alpha = (double) ui->border_color.alpha / 256.0;

  cairo_set_source_rgba (cr, red, green, blue, alpha);
  cairo_stroke (cr);

  // We are done drawing
  return TRUE;
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
        case CLUTTER_Q:
        case CLUTTER_Escape:
        {
          gtk_main_quit ();

          handled = TRUE;
          break;
        }

        case CLUTTER_f:
        case CLUTTER_F:
        case CLUTTER_F11:
        {
          // Fullscreen keys
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
        case CLUTTER_L:
        {
          // Loop
          ui->engine->loop = !ui->engine->loop;

          handled = TRUE;
          break;
        }

        case CLUTTER_8:
        {
          // Mute button
          gdouble volume = 0.0;
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

          ui->playback_position = (float) pos / ui->engine->media_duration;
          // Invalidate calls a redraw of the canvas
          clutter_content_invalidate (ui->seek_canvas);

          handled = TRUE;
          break;
        }

        case CLUTTER_r:
        case CLUTTER_R:
        {
          // rotate texture 90 degrees.
          rotate_video (ui);

          handled = TRUE;
          break;
        }

        case CLUTTER_c:
        case CLUTTER_C:
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
        case CLUTTER_V:
        {
          // toggle subtitles
          if (toggle_subtitles (ui->engine)) {
            gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
                (ui->subtitle_toggle),
                gdk_pixbuf_new_from_file (ui->subtitle_active_png, NULL), NULL);
          } else {
            gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
                (ui->subtitle_toggle),
                gdk_pixbuf_new_from_file (ui->subtitle_inactive_png, NULL),
                NULL);
          }

          handled = TRUE;
          break;
        }

        case CLUTTER_numbersign:
        case CLUTTER_underscore:
        case CLUTTER_j:
        case CLUTTER_J:
        {
          // cycle through available audio/text/video streams
          guint streamid;

          if (keyval == CLUTTER_numbersign)
            streamid = STREAM_AUDIO;
          else if (keyval == CLUTTER_j || keyval == CLUTTER_J)
            streamid = STREAM_TEXT;
          else if (keyval == CLUTTER_underscore)
            streamid = STREAM_VIDEO;

          cycle_streams (ui->engine, streamid);

          handled = TRUE;
          break;
        }

        case CLUTTER_o:
        {
          // switch display to time left of the stream
          ui->duration_str_fwd_direction = !ui->duration_str_fwd_direction;

          handled = TRUE;
          break;
        }

        case CLUTTER_KEY_bracketleft:
        case CLUTTER_KEY_bracketright:
        {
          // get current rate
          gdouble rate = ui->engine->rate;

          // change playback speed
          if (keyval == CLUTTER_KEY_bracketleft)
            engine_change_speed (ui->engine, rate - 0.1);
          else
            engine_change_speed (ui->engine, rate + 0.1);

          handled = TRUE;
          break;
        }

        case CLUTTER_less:
        {
          interface_play_next_or_prev (ui, FALSE);

          handled = TRUE;
          break;
        }

        case CLUTTER_greater:
        {
          interface_play_next_or_prev (ui, TRUE);

          handled = TRUE;
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

        } else if (actor == ui->control_seekbar) {
          gfloat x, y, dist;
          gint64 pos;

          clutter_actor_get_transformed_position (ui->control_seekbar, &x, &y);
          dist = bev->x - x;
          dist = CLAMP (dist, 0, ui->seek_width);

          if (ui->engine->media_duration == -1) {
            update_media_duration (ui->engine);
          }

          pos = ui->engine->media_duration * (dist / ui->seek_width);
          engine_seek (ui->engine, pos, FALSE);

          ui->playback_position = (float) pos / ui->engine->media_duration;
          // Invalidate calls a redraw of the canvas
          clutter_content_invalidate (ui->seek_canvas);

        } else if (actor == ui->vol_int) {
          gfloat x, y, dist;
          gdouble volume;

          clutter_actor_get_transformed_position (ui->vol_int, &x, &y);
          dist = bev->x - x;
          dist = CLAMP (dist, 0, ui->volume_width);

          volume = dist / ui->volume_width;
          g_object_set (G_OBJECT (ui->engine->player), "volume", volume, NULL);
          ui->volume = (float) volume;
          clutter_content_invalidate (ui->vol_int_canvas);

        } else if (actor == ui->texture || actor == ui->stage) {
          if (!ui->penalty_box_active) {
            penalty_box (ui);
            show_controls (ui, FALSE);
          }

        } else if (actor == ui->fullscreen_button) {
          // Fullscreen button
          toggle_fullscreen (ui);

        } else if (actor == ui->audio_stream_toggle) {
          cycle_streams (ui->engine, STREAM_AUDIO);

        } else if (actor == ui->subtitle_toggle) {
          if (toggle_subtitles (ui->engine)) {
            gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
                (ui->subtitle_toggle),
                gdk_pixbuf_new_from_file (ui->subtitle_active_png, NULL), NULL);
          } else {
            gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
                (ui->subtitle_toggle),
                gdk_pixbuf_new_from_file (ui->subtitle_inactive_png, NULL),
                NULL);
          }

        } else if (actor == ui->video_stream_toggle) {
          cycle_streams (ui->engine, STREAM_VIDEO);

        } else if (actor == ui->control_pos) {
          ui->duration_str_fwd_direction = !ui->duration_str_fwd_direction;
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

    default:
    {
      handled = FALSE;
      break;
    }
  }

  return handled;
}


static void
hide_cursor (UserInterface * ui)
{
  ClutterDeviceManager *manager = NULL;
  ClutterInputDevice *device = NULL;
  ClutterPoint point;

  manager = clutter_device_manager_get_default ();
  device =
      clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);
  clutter_input_device_get_coords (device, NULL, &point);

  if (point.x > 0 && point.y > 0 &&
      point.x < ui->stage_width && point.y < ui->stage_height) {
    clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
  }
}

static void
load_controls (UserInterface * ui)
{
  // Check icon files exist
  gchar *icon_files[9];
  gchar *duration_str = NULL;
  gint c;

  ClutterContent *canvas;
  ClutterLayoutManager *controls_layout = NULL;
  ClutterLayoutManager *bottom_box_layout = NULL;
  ClutterLayoutManager *volume_box_layout = NULL;
  ClutterLayoutManager *right_box_layout = NULL;
  ClutterActor *middle_box = NULL;
  ClutterActor *bottom_box = NULL;
  ClutterActor *vol_int_box = NULL;
  ClutterActor *right_box = NULL;
  GError *error = NULL;

  ui->play_png = g_build_filename (ui->data_dir, "media-actions-start.svg",
      NULL);
  ui->pause_png = g_build_filename (ui->data_dir, "media-actions-pause.svg",
      NULL);
  ui->volume_low_png = g_build_filename (ui->data_dir,
      "audio-volume-low.svg", NULL);
  ui->volume_high_png = g_build_filename (ui->data_dir,
      "audio-volume-high.svg", NULL);
  ui->fullscreen_svg = g_build_filename (ui->data_dir, "fullscreen.svg", NULL);
  ui->subtitle_active_png = g_build_filename (ui->data_dir,
      "subtitles-active.svg", NULL);
  ui->subtitle_inactive_png = g_build_filename (ui->data_dir,
      "subtitles-inactive.svg", NULL);
  ui->video_stream_toggle_png = g_build_filename (ui->data_dir,
      "video-stream-toggle.png", NULL);
  ui->audio_stream_toggle_png = g_build_filename (ui->data_dir,
      "audio-stream-toggle.png", NULL);

  icon_files[0] = ui->play_png;
  icon_files[1] = ui->pause_png;
  icon_files[2] = ui->volume_low_png;
  icon_files[3] = ui->volume_high_png;
  icon_files[4] = ui->fullscreen_svg;
  icon_files[5] = ui->subtitle_active_png;
  icon_files[6] = ui->subtitle_inactive_png;
  icon_files[7] = ui->video_stream_toggle_png;
  icon_files[8] = ui->audio_stream_toggle_png;

  for (c = 0; c < 9; c++) {
    if (!g_file_test (icon_files[c], G_FILE_TEST_EXISTS)) {
      g_print ("Icon file doesn't exist, are you sure you have "
          " installed snappy correctly?\nThis file needed is: %s\n",
          icon_files[c]);
    }
  }

  // Controls layout management
  controls_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  ui->control_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (ui->control_box, controls_layout);

  // Controls rectangular background with curved edges (drawn in cairo)
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas),
      ui->media_width * CONTROLS_WIDTH_RATIO,
      ui->media_height * CONTROLS_HEIGHT_RATIO);

  ui->control_bg = clutter_actor_new ();
  clutter_actor_set_content (ui->control_bg, canvas);

  // The actor now owns the canvas
  g_object_unref (canvas);
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_background), ui);
  // Invalidate the canvas, so that we can draw before the main loop starts
  clutter_content_invalidate (canvas);

  clutter_actor_add_constraint (ui->control_bg,
      clutter_bind_constraint_new (ui->control_box, CLUTTER_BIND_SIZE, 0));

  clutter_actor_add_child (CLUTTER_ACTOR (ui->control_box), ui->control_bg);
  clutter_actor_add_constraint (ui->control_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (ui->control_box,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0.95));

  // Main Box
  ui->main_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (ui->main_box_layout),
      CLUTTER_ORIENTATION_VERTICAL);

  ui->main_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (ui->main_box, ui->main_box_layout);

  clutter_actor_add_child (CLUTTER_ACTOR (ui->control_box),
      CLUTTER_ACTOR (ui->main_box));

  // Controls title
  ui->control_title = clutter_text_new_full ("Clear Sans Bold 32px",
      cut_long_filename (ui->filename, ui->title_length), &ui->text_color);
  if (strcmp (ui->filename, "") == 0) {
    clutter_text_set_text (CLUTTER_TEXT (ui->control_title),
        "Drag and drop a file here to play it");
  }
  clutter_text_set_max_length (CLUTTER_TEXT (ui->control_title),
      ui->title_length);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->main_box_layout), ui->control_title, TRUE,   /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_CENTER,     /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Info Box
  ui->info_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (ui->info_box_layout),
      CLUTTER_ORIENTATION_HORIZONTAL);

  ui->info_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (ui->info_box, ui->info_box_layout);

  // Controls play toggle
  ui->control_play_toggle = gtk_clutter_texture_new ();
  if (strcmp (ui->filename, "") == 0) {
    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->control_play_toggle), gdk_pixbuf_new_from_file (ui->play_png,
            NULL), &error);
  } else {
    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->control_play_toggle), gdk_pixbuf_new_from_file (ui->pause_png,
            NULL), &error);
  }
  if (!ui->control_play_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  g_assert (ui->control_bg && ui->control_play_toggle);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->info_box_layout), ui->control_play_toggle, FALSE,    /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,      /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Position, volume and streams box
  ui->pos_n_vol_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (ui->pos_n_vol_layout),
      CLUTTER_ORIENTATION_VERTICAL);

  ui->pos_n_vol_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (ui->pos_n_vol_box, ui->pos_n_vol_layout);

  // Seek progress bar
  ui->seek_canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (ui->seek_canvas),
      ui->media_width * CONTROLS_WIDTH_RATIO,
      (ui->media_height * CONTROLS_HEIGHT_RATIO) / 5);
  ui->control_seekbar = clutter_actor_new ();
  clutter_actor_set_content (ui->control_seekbar, ui->seek_canvas);

  g_signal_connect (ui->seek_canvas, "draw", G_CALLBACK (draw_progressbar), ui);
  clutter_content_invalidate (ui->seek_canvas);

  // Add seek box to Position and Volume Layout
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->pos_n_vol_layout), ui->control_seekbar, TRUE,        /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_END,        /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Controls middle box
  ui->middle_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT
      (ui->middle_box_layout), CLUTTER_ORIENTATION_HORIZONTAL);

  middle_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (middle_box, ui->middle_box_layout);

  // Controls volume box
  volume_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (volume_box_layout),
      CLUTTER_ORIENTATION_HORIZONTAL);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (volume_box_layout), 5);
  ui->volume_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (ui->volume_box, volume_box_layout);

  clutter_actor_add_child (middle_box, ui->volume_box);

  // Controls volume low
  ui->volume_low = gtk_clutter_texture_new ();
  gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE (ui->volume_low),
      gdk_pixbuf_new_from_file (ui->volume_low_png, NULL), &error);
  if (!ui->volume_low && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_actor_add_child (ui->volume_box, ui->volume_low);

  // Controls volume intensity
  vol_int_box = clutter_actor_new ();

  ui->vol_int_canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (ui->vol_int_canvas),
      ui->media_width * CONTROLS_WIDTH_RATIO,
      (ui->media_height * CONTROLS_HEIGHT_RATIO) / 5);
  ui->vol_int = clutter_actor_new ();
  clutter_actor_set_content (ui->vol_int, ui->vol_int_canvas);

  g_signal_connect (ui->vol_int_canvas, "draw", G_CALLBACK (draw_progressbar),
      ui);
  clutter_content_invalidate (ui->vol_int_canvas);

  clutter_actor_add_child (vol_int_box, ui->vol_int);
  clutter_actor_add_child (ui->volume_box, vol_int_box);

  // Controls volume high
  ui->volume_high = gtk_clutter_texture_new ();
  gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE (ui->volume_high),
      gdk_pixbuf_new_from_file (ui->volume_high_png, NULL), &error);
  if (!ui->volume_high && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_actor_add_child (ui->volume_box, ui->volume_high);

  // Controls position text
  duration_str = g_strdup_printf ("   0:00:00 | %s", ui->duration_str);
  ui->control_pos = clutter_text_new_full ("Clear Sans 22px", duration_str,
      &ui->text_color);
  clutter_actor_add_child (middle_box, ui->control_pos);

  // Add middle box (volume and text position) to Position and Volume Layout
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->pos_n_vol_layout), middle_box, TRUE, /* expand */
      TRUE,                     /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,      /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  if (FALSE) {                  // hide this buttons (TODO: optional Flag)
    // Controls bottom box
    bottom_box_layout = clutter_box_layout_new ();
    clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (bottom_box_layout),
        CLUTTER_ORIENTATION_HORIZONTAL);
    clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (bottom_box_layout), 5);
    bottom_box = clutter_actor_new ();
    clutter_actor_set_layout_manager (bottom_box, bottom_box_layout);

    // Controls video stream toggle
    ui->video_stream_toggle = gtk_clutter_texture_new ();
    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->video_stream_toggle),
        gdk_pixbuf_new_from_file (ui->video_stream_toggle_png, NULL), &error);
    if (!ui->video_stream_toggle && error)
      g_debug ("Clutter error: %s", error->message);
    if (error) {
      g_error_free (error);
      error = NULL;
    }
    clutter_actor_add_child (bottom_box, ui->video_stream_toggle);

    // Controls audio stream toggle
    ui->audio_stream_toggle = gtk_clutter_texture_new ();
    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->audio_stream_toggle),
        gdk_pixbuf_new_from_file (ui->audio_stream_toggle_png, NULL), &error);
    if (!ui->audio_stream_toggle && error)
      g_debug ("Clutter error: %s", error->message);
    if (error) {
      g_error_free (error);
      error = NULL;
    }
    clutter_actor_add_child (bottom_box, ui->audio_stream_toggle);

    // Add bottom box (different streams) to Position and Volume Layout
    clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->pos_n_vol_layout), bottom_box, TRUE,       /* expand */
        FALSE,                  /* x-fill */
        FALSE,                  /* y-fill */
        CLUTTER_BOX_ALIGNMENT_END,      /* x-align */
        CLUTTER_BOX_ALIGNMENT_END);     /* y-align */
  }

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->info_box_layout), ui->pos_n_vol_box, FALSE,  /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,      /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Controls right box for subtitles and fullscreen
  right_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (right_box_layout),
      CLUTTER_ORIENTATION_HORIZONTAL);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (right_box_layout), 10);
  right_box = clutter_actor_new ();
  clutter_actor_set_layout_manager (right_box, right_box_layout);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->info_box_layout), right_box, FALSE,  /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_CENTER,     /* x-align */
      CLUTTER_BOX_ALIGNMENT_START);     /* y-align */

  // Controls subtitle toggle
  ui->subtitle_toggle = gtk_clutter_texture_new ();
  gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
      (ui->subtitle_toggle),
      gdk_pixbuf_new_from_file (ui->subtitle_active_png, NULL), &error);
  if (!ui->subtitle_toggle && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_actor_hide (ui->subtitle_toggle);
  clutter_actor_add_child (right_box, ui->subtitle_toggle);

  // Controls fullscreen
  ui->fullscreen_button = gtk_clutter_texture_new ();
  gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
      (ui->fullscreen_button),
      gdk_pixbuf_new_from_file (ui->fullscreen_svg, NULL), &error);
  if (!ui->fullscreen_button && error)
    g_debug ("Clutter error: %s", error->message);
  if (error) {
    g_error_free (error);
    error = NULL;
  }
  clutter_actor_add_child (right_box, ui->fullscreen_button);

  // Add Info Box to Main Box Layout
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (ui->main_box_layout), ui->info_box, FALSE,       /* expand */
      FALSE,                    /* x-fill */
      FALSE,                    /* y-fill */
      CLUTTER_BOX_ALIGNMENT_CENTER,     /* x-align */
      CLUTTER_BOX_ALIGNMENT_CENTER);    /* y-align */

  clutter_actor_set_child_below_sibling (ui->control_box, ui->control_bg,
      ui->main_box);

  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);
}

static void
new_video_size (UserInterface * ui, gfloat width, gfloat height,
    gfloat * new_width, gfloat * new_height)
{
  gfloat tmp_width, tmp_height;
  gfloat media_width, media_height;
  gfloat stage_ar, media_ar;

  media_width = clutter_actor_get_width (ui->texture);
  media_height = clutter_actor_get_height (ui->texture);

  stage_ar = width / height;
  tmp_width = width;
  tmp_height = height;

  if (media_height > 0.0f && media_width > 0.0f) {
    media_ar = media_width / media_height;

    /* calculate new width and height
     * note: when we're done, new_width/new_height should equal media_ar */
    if (media_ar > stage_ar) {
      /* media has wider aspect than stage so use new width as stage width and
       * scale down height */
      tmp_height = width / media_ar;
    } else {
      tmp_width = height * media_ar;
    }
  } else {
    g_debug ("Warning: not considering texture dimensions %fx%f", media_width,
        media_height);
  }

  *new_width = tmp_width;
  *new_height = tmp_height;
}

static gboolean
penalty_box (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;

  if (ui->penalty_box_active) {
    ui->penalty_box_active = FALSE;
  } else {
    ui->penalty_box_active = TRUE;
    g_timeout_add (PENALTY_TIME, penalty_box, ui);
  }

  return FALSE;
}

static gchar *
position_ns_to_str (UserInterface * ui, gint64 nanoseconds)
{
  gint64 seconds;
  gint hours, minutes;

  if (!ui->duration_str_fwd_direction)
    nanoseconds = ui->engine->media_duration - nanoseconds;

  seconds = nanoseconds / GST_SECOND;
  hours = seconds / SEC_IN_HOUR;
  seconds = seconds - (hours * SEC_IN_HOUR);
  minutes = seconds / SEC_IN_MIN;
  seconds = seconds - (minutes * SEC_IN_MIN);

  if (hours >= 1)
    return g_strdup_printf ("%d:%02d:%02ld", hours, minutes, seconds);
  else
    return g_strdup_printf ("%02d:%02ld", minutes, seconds);
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
        progress_timing (ui);
      }

      pos = query_position (engine);

      duration_str = g_strdup_printf ("   %s | %s",
          position_ns_to_str (ui, pos), ui->duration_str);
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
      gfloat pos;

      pos = (float) query_position (engine) / engine->media_duration;
      ui->playback_position = pos;

      // Invalidate calls a redraw of the canvas
      clutter_content_invalidate (ui->seek_canvas);
    }
  }

  return TRUE;
}

gboolean
rotate_video (UserInterface * ui)
{
  gdouble angle;

  angle = clutter_actor_get_rotation_angle (ui->texture, CLUTTER_Z_AXIS);
  angle += 90;
  if (angle == 360)
    angle = 0;
  clutter_actor_set_rotation_angle (ui->texture, CLUTTER_Z_AXIS, angle);

  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);

  return TRUE;
}

static void
size_change (ClutterStage * stage,
    const ClutterActorBox * allocation,
    ClutterAllocationFlags flags, UserInterface * ui)
{
  gfloat video_width, video_height;

  ui->stage_width = clutter_actor_get_width (ui->stage);
  ui->stage_height = clutter_actor_get_height (ui->stage);

  new_video_size (ui, ui->stage_width, ui->stage_height, &video_width,
      &video_height);
  clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), video_width,
      video_height);

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

    if (ui->subtitles_available) {
      clutter_actor_show (ui->subtitle_toggle);
    } else {
      clutter_actor_hide (ui->subtitle_toggle);
    }

    update_controls_size (ui);
    progress_update_seekbar (ui);
    progress_update_text (ui);
    clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));

    clutter_actor_set_easing_mode (CLUTTER_ACTOR (ui->control_box),
        CLUTTER_EASE_OUT_QUINT);
    clutter_actor_set_easing_duration (CLUTTER_ACTOR (ui->control_box),
        CTL_FADE_DURATION);
    clutter_actor_set_opacity (CLUTTER_ACTOR (ui->control_box), 0xff);

    if (ui->controls_timeout == -1) {
      ui->controls_timeout = g_timeout_add_seconds (CTL_SHOW_SEC,
          controls_timeout_cb, ui);
    }
  }

  else if (vis == FALSE && ui->controls_showing == TRUE) {
    ui->controls_showing = FALSE;

    hide_cursor (ui);

    clutter_actor_set_easing_mode (CLUTTER_ACTOR (ui->control_box),
        CLUTTER_EASE_OUT_QUINT);
    clutter_actor_set_easing_duration (CLUTTER_ACTOR (ui->control_box),
        CTL_FADE_DURATION);
    clutter_actor_set_opacity (CLUTTER_ACTOR (ui->control_box), 0);
  }
}

static void
toggle_fullscreen (UserInterface * ui)
{
  gfloat new_width, new_height;

  if (!ui->fullscreen) {
    ui->windowed_width = clutter_actor_get_width (ui->stage);
    ui->windowed_height = clutter_actor_get_height (ui->stage);

    new_video_size (ui, ui->screen_width, ui->screen_height, &new_width,
        &new_height);
    clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), new_width, new_height);
    gtk_window_fullscreen (GTK_WINDOW (ui->window));

    ui->fullscreen = TRUE;

  } else {
    gtk_window_unfullscreen (GTK_WINDOW (ui->window));

    new_video_size (ui, ui->windowed_width, ui->windowed_height, &new_width,
        &new_height);
    clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), new_width, new_height);

    ui->fullscreen = FALSE;
  }
}

static void
toggle_playing (UserInterface * ui)
{
  GstEngine *engine = ui->engine;

  if (engine->playing) {
    change_state (engine, "Paused");
    engine->playing = FALSE;

    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->control_play_toggle), gdk_pixbuf_new_from_file (ui->play_png,
            NULL), NULL);

  } else {
    change_state (engine, "Playing");
    engine->playing = TRUE;

    gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
        (ui->control_play_toggle), gdk_pixbuf_new_from_file (ui->pause_png,
            NULL), NULL);
  }
}

static void
update_controls_size (UserInterface * ui)
{
  gchar *font_name;
  gfloat ctl_width, ctl_height;
  gfloat icon_size;
  gfloat control_box_width, control_box_height;
  gfloat main_box_width, main_box_height;
  gfloat main_box_horiz_pos, main_box_vert_pos;

  // g_print ("Updating controls size for stage: %ux%u\n", ui->stage_width,
  //     ui->stage_height);

  ctl_width = ui->stage_width * CONTROLS_WIDTH_RATIO;
  ctl_height = ui->stage_height * CONTROLS_HEIGHT_RATIO;

  if (ctl_width / ctl_height > CONTROLS_ASPECT_RATIO) {
    ctl_width = ctl_height * CONTROLS_ASPECT_RATIO;
  } else {
    ctl_height = ctl_width / CONTROLS_ASPECT_RATIO;
  }

  if (ctl_width > CONTROLS_MAX_WIDTH) {
    ctl_width = CONTROLS_MAX_WIDTH;
    ctl_height = ctl_width / CONTROLS_ASPECT_RATIO;
  } else if (ctl_width < CONTROLS_MIN_WIDTH) {
    ctl_width = CONTROLS_MIN_WIDTH;
    ctl_height = ctl_width / CONTROLS_ASPECT_RATIO;
  }

  icon_size = ctl_height * PLAY_TOGGLE_RATIO;

  if (ui->subtitles_available) {
    control_box_width = ctl_width + (icon_size * 2.0f);
  } else {
    control_box_width = ctl_width + (icon_size);
  }

  control_box_height = ctl_height * 0.85;
  clutter_actor_set_size (CLUTTER_ACTOR (ui->control_box),
      control_box_width, control_box_height);

  clutter_actor_set_size (ui->control_play_toggle, icon_size, icon_size);

  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (ui->info_box_layout),
      ctl_width * 0.04f);

  font_name =
      g_strdup_printf ("Clear Sans Bold %dpx",
      (gint) (ctl_width * TITLE_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_title), font_name);

  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (ui->main_box_layout),
      ctl_height * 0.10f);

  ui->seek_width = 12 +         // accomodate volume_box spacing
      (ctl_width * MAIN_BOX_W - icon_size) * SEEK_WIDTH_RATIO - 4.0f;
  ui->seek_height = ctl_height * MAIN_BOX_H * SEEK_HEIGHT_RATIO - 4.0f;

  clutter_actor_set_size (ui->control_seekbar,
      ui->seek_width + 4.0f, ui->seek_height + 4.0f);

  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (ui->pos_n_vol_layout),
      ctl_height * 0.16f);

  progress_update_seekbar (ui);

  font_name =
      g_strdup_printf ("Clear Sans %dpx", (gint) (ctl_height * POS_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_pos), font_name);

  ui->volume_width =
      (ctl_width * MAIN_BOX_W - icon_size -
      clutter_actor_get_width (CLUTTER_ACTOR (ui->control_pos))) *
      VOLUME_WIDTH_RATIO;
  ui->volume_height = ctl_height * MAIN_BOX_H * VOLUME_HEIGHT_RATIO;
  clutter_actor_set_size (ui->vol_int, ui->volume_width, ui->volume_height);

  icon_size = ctl_height * VOLUME_ICON_RATIO;
  clutter_actor_set_size (ui->volume_low, icon_size, icon_size);
  clutter_actor_set_size (ui->volume_high, icon_size, icon_size);
  clutter_actor_set_size (ui->fullscreen_button, icon_size, icon_size);
  clutter_actor_set_size (ui->subtitle_toggle, icon_size, icon_size);

  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (ui->middle_box_layout),
      ctl_width * 0.04f);

  if (FALSE) {                  // hide this buttons (TODO: optional Flag)
    clutter_actor_set_size (ui->video_stream_toggle, icon_size, icon_size);
    clutter_actor_set_size (ui->audio_stream_toggle, icon_size, icon_size);
  }

  clutter_actor_get_size (CLUTTER_ACTOR (ui->main_box),
      &main_box_width, &main_box_height);
  main_box_horiz_pos = (control_box_width - main_box_width) / 2;
  main_box_vert_pos = (control_box_height - main_box_height) / 2;

  clutter_actor_set_position (CLUTTER_ACTOR (ui->main_box),
      main_box_horiz_pos, main_box_vert_pos);

  update_volume (ui, -1);
}

static gboolean
update_volume (UserInterface * ui, gdouble volume)
{
  if (volume == -1)
    g_object_get (G_OBJECT (ui->engine->player), "volume", &volume, NULL);

  ui->volume = (float) volume;
  clutter_content_invalidate (ui->vol_int_canvas);

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

  ui->subtitle_active_png = NULL;
  ui->subtitle_inactive_png = NULL;
  ui->subtitles_available = FALSE;
  ui->video_stream_toggle_png = NULL;
  ui->audio_stream_toggle_png = NULL;

  ui->duration_str = NULL;

  ui->stage = NULL;
  ui->texture = NULL;

  ui->control_box = NULL;
  ui->control_bg = NULL;
  ui->control_title = NULL;
  ui->control_play_toggle = NULL;

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

  ui->main_box_layout = NULL;
  ui->info_box_layout = NULL;
  ui->pos_n_vol_layout = NULL;
  ui->middle_box_layout = NULL;

  ui->engine = NULL;
  ui->screensaver = NULL;

  ui->playback_position = 0.0;

  ClutterColor stage_bg_color = { 0x00, 0x00, 0x00, 0xda };
  ui->stage_bg_color = stage_bg_color;
  ClutterColor text_color = { 0xff, 0xff, 0xff, 0xff };
  ui->text_color = text_color;
  ClutterColor border_color = { 0xff, 0xff, 0xff, 0x26 };
  ui->border_color = border_color;
  ClutterColor gradient_start = { 0x3e, 0xb4, 0x8a, 0xff };
  ui->gradient_start = gradient_start;
  ClutterColor gradient_finish = { 0x3e, 0xb4, 0x8a, 0xff };
  ui->gradient_finish = gradient_finish;
}

gboolean
interface_is_it_last (UserInterface * ui)
{
  GList *element;

  element = g_list_find (ui->uri_list, ui->engine->uri);
  element = g_list_next (element);

  return (element == NULL);
}

gboolean
interface_load_uri (UserInterface * ui, gchar * uri)
{
  ui->fileuri = uri;

  ui->filename = g_path_get_basename (ui->fileuri);

  if (ui->stage != NULL) {
    gtk_window_set_title (GTK_WINDOW (ui->window), ui->filename);
    clutter_text_set_text (CLUTTER_TEXT (ui->control_title), ui->filename);
  }

  ui->duration_str = position_ns_to_str (ui, ui->engine->media_duration);
  ui->media_width = ui->engine->media_width;
  ui->media_height = ui->engine->media_height;
  ui->windowed_width = ui->media_width;
  ui->windowed_height = ui->media_height;

  clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), ui->media_width,
      ui->media_height);
  size_change (CLUTTER_STAGE (ui->stage), NULL, 0, ui);

  if (!ui->fullscreen) {
    ui->stage_width = ui->media_width;
    ui->stage_height = ui->media_height;

    gtk_widget_set_size_request (ui->clutter_widget, ui->stage_width / 2,
        ui->stage_height / 2);
    clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->stage_width,
        ui->stage_height);

    gtk_window_resize (GTK_WINDOW (ui->window), ui->stage_width,
        ui->stage_height);
  }

  if (!ui->penalty_box_active)
    show_controls (ui, TRUE);

  gtk_clutter_texture_set_from_pixbuf (GTK_CLUTTER_TEXTURE
      (ui->control_play_toggle), gdk_pixbuf_new_from_file (ui->pause_png,
          NULL), NULL);

  return TRUE;
}

void
interface_on_drop_cb (GtkWidget * widget,
    GdkDragContext * context,
    gint x,
    gint y,
    GtkSelectionData * data, guint info, guint _time, UserInterface * ui)
{
  char **list;

  list =
      g_uri_list_extract_uris ((const gchar *)
      gtk_selection_data_get_data (data));

  engine_open_uri (ui->engine, list[0]);
  interface_load_uri (ui, list[0]);
  engine_play (ui->engine);

  if (!CLUTTER_ACTOR_IS_VISIBLE (ui->texture)) {
    clutter_actor_show (ui->texture);
  }
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
  } else {
    engine_seek (ui->engine, 0, TRUE);
    toggle_playing (ui);
  }
}

void
interface_start (UserInterface * ui, gchar * uri)
{
  GtkSettings *gtk_settings;
  GdkScreen *screen;

  g_print ("Loading ui!\n");

  // Init UserInterface structure variables
  if (uri) {
    ui->fileuri = uri;

    ui->filename = g_path_get_basename (ui->fileuri);

    ui->media_width = ui->engine->media_width;
    ui->media_height = ui->engine->media_height;

    // Get screen size
    screen = gdk_screen_get_default ();
    ui->screen_width = gdk_screen_get_width (screen);
    ui->screen_height = gdk_screen_get_height (screen);

    if (ui->media_width < ui->screen_width &&
        ui->media_height < ui->screen_height) {
      ui->stage_width = ui->media_width;
      ui->stage_height = ui->media_height;

    } else {
      // Media bigger than the screen
      gfloat aspect_ratio;

      aspect_ratio = (float) ui->media_width / ui->media_height;
      // Scale down to screen width proportionally
      ui->stage_width = ui->screen_width;
      ui->stage_height = ui->screen_width / aspect_ratio;

      if (ui->stage_height > ui->screen_height) {
        // Stage height still too big, scale down to screen height
        ui->stage_width = ui->screen_height * aspect_ratio;
        ui->stage_height = ui->screen_height;
      }
    }

  } else {
    ui->filename = "";

    ui->media_width = DEFAULT_WIDTH;
    ui->media_height = DEFAULT_HEIGHT;

    ui->stage_width = DEFAULT_WIDTH;
    ui->stage_height = DEFAULT_HEIGHT;
  }

  /* Create the window and some child widgets: */
  ui->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (strcmp (ui->filename, "") != 0)
    gtk_window_set_title (GTK_WINDOW (ui->window), ui->filename);
  else
    gtk_window_set_title (GTK_WINDOW (ui->window), "snappy");
  g_signal_connect (ui->window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_settings = gtk_settings_get_default ();
  g_object_set (G_OBJECT (gtk_settings), "gtk-application-prefer-dark-theme",
      TRUE, NULL);

  ui->box = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (ui->box),
      GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_hexpand (ui->box, TRUE);
  gtk_widget_set_vexpand (ui->box, TRUE);
  gtk_container_add (GTK_CONTAINER (ui->window), ui->box);

  /* Create the clutter widget: */
  ui->clutter_widget = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (ui->box), ui->clutter_widget);

  /* Get the stage */
  ui->stage =
      gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (ui->clutter_widget));
  clutter_actor_set_background_color (CLUTTER_ACTOR (ui->stage),
      &ui->stage_bg_color);
  /* Set the size of the widget,
   * because we should not set the size of its stage when using GtkClutterEmbed.
   */
  gtk_widget_set_size_request (ui->clutter_widget, ui->stage_width / 2,
      ui->stage_height / 2);
  gtk_window_resize (GTK_WINDOW (ui->window), ui->stage_width,
      ui->stage_height);

  ui->controls_showing = FALSE;
  ui->keep_showing_controls = FALSE;
  ui->penalty_box_active = FALSE;
  ui->duration_str_fwd_direction = TRUE;
  ui->controls_timeout = -1;

  ui->seek_width = ui->stage_width / SEEK_WIDTH_RATIO;
  ui->seek_height = ui->stage_height / SEEK_HEIGHT_RATIO;

  ui->progress_id = -1;
  ui->title_length = TITLE_LENGTH;
  ui->media_duration = -1;
  ui->duration_str = position_ns_to_str (ui, ui->engine->media_duration);

  clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->stage_width,
      ui->stage_height);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (ui->stage), TRUE);

  if (ui->fullscreen) {
    gtk_window_fullscreen (GTK_WINDOW (ui->window));
  }
  // Controls
  load_controls (ui);

  // Add video texture and control UI to stage
  clutter_actor_add_child (ui->stage, ui->texture);
  if (!ui->hide) {
    clutter_actor_add_child (ui->stage, CLUTTER_ACTOR (ui->control_box));
  }
  clutter_actor_add_constraint (ui->texture,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (ui->texture,
      clutter_align_constraint_new (ui->stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  clutter_actor_set_pivot_point (ui->texture, 0.5, 0.5);

  clutter_actor_set_easing_mode (CLUTTER_ACTOR (ui->control_box),
      CLUTTER_EASE_OUT_QUINT);
  clutter_actor_set_easing_duration (CLUTTER_ACTOR (ui->control_box),
      G_TIME_SPAN_MILLISECOND);
  clutter_actor_set_opacity (CLUTTER_ACTOR (ui->control_box), 0);

  /* Connect a signal handler to mouse clicks and key presses on the stage */
  g_signal_connect (CLUTTER_STAGE (ui->stage), "allocation-changed",
      G_CALLBACK (size_change), ui);
  g_signal_connect (CLUTTER_STAGE (ui->stage), "event", G_CALLBACK (event_cb),
      ui);

  progress_timing (ui);

  ui->screensaver = screensaver_new (CLUTTER_STAGE (ui->stage));
  screensaver_enable (ui->screensaver, FALSE);

  g_timeout_add (G_TIME_SPAN_MILLISECOND, progress_update_text, ui);

  if (!ui->blind) {
    /* Show the window */
    gtk_widget_show_all (ui->window);
  }

  if (!ui->fileuri) {
    clutter_actor_hide (ui->texture);
  }

  gtk_drag_dest_set (GTK_WIDGET (ui->box), GTK_DEST_DEFAULT_ALL,
      drop_target_table, G_N_ELEMENTS (drop_target_table),
      GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (ui->box), "drag_data_received",
      G_CALLBACK (interface_on_drop_cb), ui);
}

gboolean
interface_update_controls (UserInterface * ui)
{
  progress_update_text (ui);
  progress_update_seekbar (ui);
  update_volume (ui, -1);

  return TRUE;
}
