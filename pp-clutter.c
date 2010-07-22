/*
 * Pinpoint: A small-ish presentation tool
 *
 * Copyright (C) 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option0 any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Øyvind Kolås <pippin@linux.intel.com>
 *             Damien Lespiau <damien.lespiau@intel.com>
 *             Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "pinpoint.h"
#include <gio/gio.h>
#ifdef USE_CLUTTER_GST
#include <clutter-gst/clutter-gst.h>
#endif
#ifdef USE_DAX
#include <dax/dax.h>
#include "pp-super-aa.h"
#endif

#define RESTDEPTH   -9000.0
#define RESTX        4600.0
#define STARTPOS    -3000.0

static ClutterColor black = {0x00,0x00,0x00,0xff};

typedef struct _ClutterRenderer
{
  PinPointRenderer renderer;
  ClutterActor *stage;
  ClutterActor *background;
  ClutterActor *midground;
  ClutterActor *shading;
  ClutterActor *foreground;

  char *path;               /* path of the file of the GFileMonitor callback */
  float rest_y;             /* where the text can rest */
} ClutterRenderer;

typedef struct
{
  PinPointRenderer *renderer;
  ClutterActor     *background;
  ClutterActor     *text;
  float rest_y;     /* y coordinate when text is stationary unused */

  ClutterState     *state;
  ClutterActor     *json_slide;
  ClutterActor     *background2;
  ClutterScript    *script;
  ClutterActor     *midground;
  ClutterActor     *foreground;
  ClutterActor     *shading;
} ClutterPointData;

#define CLUTTER_RENDERER(renderer)  ((ClutterRenderer *) renderer)

static void     leave_slide   (ClutterRenderer  *renderer,
                               gboolean          backwards);
static void     show_slide    (ClutterRenderer  *renderer);
static void     action_slide  (ClutterRenderer  *renderer);
static void     file_changed  (GFileMonitor     *monitor,
                               GFile            *file,
                               GFile            *other_file,
                               GFileMonitorEvent event_type,
                               ClutterRenderer  *renderer);
static void     stage_resized (ClutterActor     *actor,
                               GParamSpec       *pspec,
                               ClutterRenderer  *renderer);
static gboolean key_pressed   (ClutterActor     *actor,
                               ClutterEvent     *event,
                               ClutterRenderer  *renderer);


static void
clutter_renderer_init (PinPointRenderer   *pp_renderer,
                       char               *pinpoint_file)
{
  ClutterRenderer *renderer = CLUTTER_RENDERER (pp_renderer);
  GFileMonitor *monitor;
  ClutterActor *stage;

  renderer->rest_y = STARTPOS;

  renderer->stage = stage = clutter_stage_get_default ();

  renderer->background = clutter_group_new ();
  renderer->midground = clutter_group_new ();
  renderer->foreground = clutter_group_new ();
  renderer->shading = clutter_rectangle_new_with_color (&black);
  clutter_actor_set_opacity (renderer->shading, 0x77);

  clutter_container_add_actor (CLUTTER_CONTAINER (renderer->midground),
                               renderer->shading);
  clutter_container_add (CLUTTER_CONTAINER (stage),
                         renderer->background, renderer->midground,
                         renderer->foreground, NULL);

  clutter_actor_show (stage);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &black);
  g_signal_connect (stage, "key-press-event",
                    G_CALLBACK (key_pressed), renderer);
  g_signal_connect (stage, "notify::width",
                    G_CALLBACK (stage_resized), renderer);
  g_signal_connect (stage, "notify::height",
                    G_CALLBACK (stage_resized), renderer);

  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  renderer->path = pinpoint_file;
  if (renderer->path)
    {
      monitor = g_file_monitor (g_file_new_for_commandline_arg (pinpoint_file),
                                G_FILE_MONITOR_NONE, NULL, NULL);
      g_signal_connect (monitor, "changed", G_CALLBACK (file_changed),
                                            renderer);
    }
}

static void
clutter_renderer_run (PinPointRenderer *renderer)
{
  show_slide (CLUTTER_RENDERER (renderer));
  clutter_main ();
}

static void
clutter_renderer_finalize (PinPointRenderer *pp_renderer)
{
  ClutterRenderer *renderer = CLUTTER_RENDERER (pp_renderer);

  clutter_actor_destroy (renderer->stage);
}

static gboolean
clutter_renderer_make_point (PinPointRenderer *pp_renderer,
                             PinPointPoint    *point)
{
  ClutterRenderer *renderer = CLUTTER_RENDERER (pp_renderer);
  ClutterPointData *data = point->data;
  ClutterColor color;
  gboolean ret;

  switch (point->bg_type)
    {
    case PP_BG_COLOR:
      {
        ret = clutter_color_from_string (&color, point->bg);
        if (ret)
          data->background = g_object_new (CLUTTER_TYPE_RECTANGLE,
                                           "color", &color,
                                           "width", 100.0,
                                           "height", 100.0,
                                           NULL);
      }
      break;
    case PP_BG_IMAGE:
      data->background = g_object_new (CLUTTER_TYPE_TEXTURE,
                                       "filename", point->bg,
                                       "load-data-async", TRUE,
                                       NULL);
      ret = TRUE;
      break;
    case PP_BG_VIDEO:
#ifdef USE_CLUTTER_GST
      data->background = clutter_gst_video_texture_new ();
      clutter_media_set_filename (CLUTTER_MEDIA (data->background), point->bg);
      /* should pre-roll the video and set the size */
      clutter_actor_set_size (data->background, 400, 300);
      ret = TRUE;
#endif
      break;
    case PP_BG_SVG:
#ifdef USE_DAX
      {
        ClutterActor *aa, *svg;
        GError *error = NULL;

        aa = pp_super_aa_new ();
        pp_super_aa_set_resolution (PP_SUPER_AA (aa), 2, 2);
        svg = dax_actor_new_from_file (point->bg, &error);
        mx_offscreen_set_pick_child (MX_OFFSCREEN (aa), TRUE);
        clutter_container_add_actor (CLUTTER_CONTAINER (aa),
                                     svg);

        data->background = aa;

        if (data->background == NULL)
          {
            g_warning ("Could not open SVG file %s: %s",
                       point->bg, error->message);
            g_clear_error (&error);
          }
        ret = data->background != NULL;
      }
#endif
      break;
    default:
      g_assert_not_reached();
    }

  if (data->background)
    {
      clutter_container_add_actor (CLUTTER_CONTAINER (renderer->background),
                                   data->background);
      clutter_actor_set_opacity (data->background, 0);
    }

  clutter_color_from_string (&color, point->text_color);
  data->text = g_object_new (CLUTTER_TYPE_TEXT,
                             "font-name", point->font,
                             "text", point->text,
                             "line-alignment", point->text_align,
                             "color", &color,
                             NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (renderer->foreground),
                               data->text);

  clutter_actor_set_position (data->text, RESTX, renderer->rest_y);
  data->rest_y = renderer->rest_y;
  renderer->rest_y += clutter_actor_get_height (data->text);
  clutter_actor_set_depth (data->text, RESTDEPTH);

  return ret;
}

static void *
clutter_renderer_allocate_data (PinPointRenderer *renderer)
{
  ClutterPointData *data = g_slice_new0 (ClutterPointData);
  data->renderer = renderer;
  return data;
}

static void
clutter_renderer_free_data (PinPointRenderer *renderer,
                            void             *datap)
{
  ClutterPointData *data = datap;

  if (data->background)
    clutter_actor_destroy (data->background);
  if (data->text)
    clutter_actor_destroy (data->text);
  g_slice_free (ClutterPointData, data);
}


static gboolean
key_pressed (ClutterActor    *actor,
             ClutterEvent    *event,
             ClutterRenderer *renderer)
{
  if (event) /* There is no event for the first triggering */
  switch (clutter_event_get_key_symbol (event))
    {
      case CLUTTER_Left:
      case CLUTTER_Up:
      case CLUTTER_BackSpace:
      case CLUTTER_Prior:
        if (pp_slidep && pp_slidep->prev)
          {
            leave_slide (renderer, TRUE);
            pp_slidep = pp_slidep->prev;
            show_slide (renderer);
          }
        break;
      case CLUTTER_Right:
      case CLUTTER_space:
      case CLUTTER_Next:
        if (pp_slidep && pp_slidep->next)
          {
            leave_slide (renderer, FALSE);
            pp_slidep = pp_slidep->next;
            show_slide (renderer);
          }
        break;
      case CLUTTER_Escape:
        clutter_main_quit ();
        break;
      case CLUTTER_F11:
        clutter_stage_set_fullscreen (CLUTTER_STAGE (renderer->stage),
            !clutter_stage_get_fullscreen (CLUTTER_STAGE (renderer->stage)));
      case CLUTTER_Return:
        action_slide (renderer);
        break;
    }
  return TRUE;
}


static void leave_slide (ClutterRenderer *renderer,
                         gboolean         backwards)
{
  PinPointPoint *point = pp_slidep->data;
  ClutterPointData *data = point->data;

  if (!point->transition)
    {
      clutter_actor_animate (data->text,
                             CLUTTER_LINEAR, 2000,
                             "depth", RESTDEPTH,
                             "scale-x", 1.0,
                             "scale-y", 1.0,
                             "x", RESTX,
                             "y", data->rest_y,
                             NULL);
      if (data->background)
        {
          clutter_actor_animate (data->background,
                                 CLUTTER_LINEAR, 1000,
                                 "opacity", 0x0,
                                 NULL);
#ifdef USE_CLUTTER_GST
          if (CLUTTER_GST_IS_VIDEO_TEXTURE (data->background))
            {
              clutter_media_set_playing (CLUTTER_MEDIA (data->background), FALSE);
            }
#endif
#ifdef USE_DAX
          if (DAX_IS_ACTOR (data->background))
            {
              dax_actor_set_playing (DAX_ACTOR (data->background), FALSE);
            }
#endif
        }
    }
  else
    {
      if (data->script)
        {
          if (backwards)
            clutter_state_set_state (data->state, "pre");
          else 
            clutter_state_set_state (data->state, "post");
        }
    }
}

static void state_completed (ClutterState *state, gpointer user_data)
{
  PinPointPoint    *point = user_data;
  ClutterPointData *data = point->data;
  const gchar *new_state = clutter_state_get_state (state);

  if (new_state == g_intern_static_string ("post") ||
      new_state == g_intern_static_string ("pre"))
    {
      clutter_actor_hide (data->json_slide);
      if (data->background2)
        {
          clutter_actor_reparent (data->background, CLUTTER_RENDERER (data->renderer)->background);
          clutter_actor_reparent (data->text, CLUTTER_RENDERER (data->renderer)->foreground);

          g_object_set (data->text,
                        "depth", RESTDEPTH,
                        "scale-x", 1.0,
                        "scale-y", 1.0,
                        "x", RESTX,
                        "y", data->rest_y,
                        NULL);
          clutter_actor_set_opacity (data->background, 0);
        }
    }
}

static gboolean in_stage_resize = FALSE;


static void
action_slide (ClutterRenderer *renderer)
{
  PinPointPoint *point;
  ClutterPointData *data;

  if (!pp_slidep)
    return;
 
  point = pp_slidep->data;
  data = point->data;

  if (data->state)
    clutter_state_set_state (data->state, "action");
}

static gchar *pp_lookup_transition (const gchar *transition)
{
  gchar *ret = NULL;
  int i;
  gchar *dirs[] ={ "", "./transitions/", PKGDATADIR, NULL};
  for (i = 0; dirs[i]; i++)
    {
      gchar *path = g_strdup_printf ("%s%s.json", dirs[i], transition);
      if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;
      g_free (path);
    }
  return NULL;
}

static void
show_slide (ClutterRenderer *renderer)
{
  PinPointPoint *point;
  ClutterPointData *data;

  if (!pp_slidep)
    return;
 
  point = pp_slidep->data;
  data = point->data;

      if (point->stage_color)
        {
          ClutterColor color;

          clutter_color_from_string (&color, point->stage_color);
          clutter_stage_set_color (CLUTTER_STAGE (renderer->stage), &color);
        }

      if (data->background)
        {
          float bg_x, bg_y, bg_width, bg_height, bg_scale;
          if (CLUTTER_IS_RECTANGLE (data->background))
            {
              clutter_actor_get_size (renderer->stage, &bg_width, &bg_height);
              clutter_actor_set_size (data->background, bg_width, bg_height);
            }
          else
            {
              clutter_actor_get_size (data->background, &bg_width, &bg_height);
            }

          pp_get_background_position_scale (point,
                                            clutter_actor_get_width (renderer->stage),
                                            clutter_actor_get_height (renderer->stage),
                                            bg_width, bg_height,
                                            &bg_x, &bg_y, &bg_scale);

          clutter_actor_set_scale (data->background, bg_scale, bg_scale);
          clutter_actor_set_position (data->background, bg_x, bg_y);

          clutter_actor_animate (data->background,
                                       CLUTTER_LINEAR, 1000,
                                       "opacity", 0xff,
                                       NULL);

#ifdef USE_CLUTTER_GST
         if (CLUTTER_GST_IS_VIDEO_TEXTURE (data->background))
           {
             clutter_media_set_progress (CLUTTER_MEDIA (data->background), 0.0);
             clutter_media_set_playing (CLUTTER_MEDIA (data->background), TRUE);
           }
         else
#endif
#ifdef USE_DAX
        if (DAX_IS_ACTOR (data->background))
          {
            dax_actor_set_playing (DAX_ACTOR (data->background), TRUE);
          }
        else
#endif
          {
          }
       }

  if (!point->transition)
    {
       clutter_actor_animate (renderer->foreground,
                              CLUTTER_LINEAR, 500,
                              "opacity", 255,
                              NULL);
      clutter_actor_animate (renderer->midground,
                             CLUTTER_LINEAR, 500,
                             "opacity", 255,
                             NULL);


      {
       float text_x, text_y, text_width, text_height, text_scale;
       float shading_x, shading_y, shading_width, shading_height;

       clutter_actor_get_size (data->text, &text_width, &text_height);
       pp_get_text_position_scale (point,
                                   clutter_actor_get_width (renderer->stage),
                                   clutter_actor_get_height (renderer->stage),
                                   text_width, text_height,
                                   &text_x, &text_y,
                                   &text_scale);

       clutter_actor_animate (data->text,
                              CLUTTER_EASE_OUT_QUINT, 1000,
                              "depth", 0.0,
                              "scale-x", text_scale,
                              "scale-y", text_scale,
                              "x", text_x,
                              "y", text_y,
                              NULL);

       pp_get_shading_position_size (clutter_actor_get_width (renderer->stage),
                                     clutter_actor_get_height (renderer->stage),
                                     text_x, text_y,
                                     text_width, text_height,
                                     text_scale,
                                     &shading_x, &shading_y,
                                     &shading_width, &shading_height);

       if (clutter_actor_get_width (data->text) > 1.0)
         {
           ClutterColor color;
           clutter_color_from_string (&color, point->shading_color);

           clutter_actor_animate (renderer->shading,
                  CLUTTER_LINEAR, 500,
                  "x", shading_x,
                  "y", shading_y,
                  "opacity", (int)(point->shading_opacity*255),
                  "color", &color,
                  "width", shading_width,
                  "height", shading_height,
                  NULL);
         }
       else /* no text, fade out shading */
         clutter_actor_animate (renderer->shading,
                CLUTTER_LINEAR, 500,
                "opacity", 0,
                "x", shading_x,
                "y", shading_y,
                "width", shading_width,
                "height", shading_height,
                NULL);
      }
    }
  else
    {
      GError *error = NULL;
      /* fade out global group of texts when using a custom .json template */
      clutter_actor_animate (renderer->foreground,
                             CLUTTER_LINEAR, 500,
                             "opacity", 0,
                             NULL);
      clutter_actor_animate (renderer->midground,
                             CLUTTER_LINEAR, 500,
                             "opacity", 0,
                             NULL);
      if (!data->script)
        {
          gchar *path = pp_lookup_transition (point->transition);
          data->script = clutter_script_new ();
          clutter_script_load_from_file (data->script, path, &error);
          g_free (path);
          data->foreground = CLUTTER_ACTOR (clutter_script_get_object (data->script, "foreground"));
          data->midground = CLUTTER_ACTOR (clutter_script_get_object (data->script, "midground"));
          data->background2 = CLUTTER_ACTOR (clutter_script_get_object (data->script, "background"));
          data->state = CLUTTER_STATE (clutter_script_get_object (data->script, "state"));
          data->json_slide = CLUTTER_ACTOR (clutter_script_get_object (data->script, "actor"));
          clutter_container_add_actor (CLUTTER_CONTAINER (renderer->stage), data->json_slide);
          g_signal_connect (data->state, "completed", G_CALLBACK (state_completed), point);
          clutter_state_warp_to_state (data->state, "pre");


        }
          clutter_actor_set_size (data->json_slide, clutter_actor_get_width (renderer->stage),
                                                    clutter_actor_get_height (renderer->stage));

          clutter_actor_set_size (data->foreground, clutter_actor_get_width (renderer->stage),
                                                    clutter_actor_get_height (renderer->stage));

          clutter_actor_set_size (data->background2, clutter_actor_get_width (renderer->stage),
                                                    clutter_actor_get_height (renderer->stage));
      if (!data->json_slide)
        {
          g_warning ("failed to load transition %s %s\n", point->transition, error?error->message:"");
          return;
        }
      if (data->background2)
        {
          clutter_actor_reparent (data->background, data->background2);
          clutter_actor_set_opacity (data->background, 255);
        }
      if (data->foreground)
        {
          clutter_actor_reparent (data->text, data->foreground);
        }

      {
       float text_x, text_y, text_width, text_height, text_scale;

       clutter_actor_get_size (data->text, &text_width, &text_height);
       pp_get_text_position_scale (point,
                                   clutter_actor_get_width (renderer->stage),
                                   clutter_actor_get_height (renderer->stage),
                                   text_width, text_height,
                                   &text_x, &text_y,
                                   &text_scale);
       g_object_set (data->text,
                     "depth", 0.0,
                     "scale-x", text_scale,
                     "scale-y", text_scale,
                     "x", text_x,
                     "y", text_y,
                     NULL);


       if (clutter_actor_get_width (data->text) > 1.0)
         {
           ClutterColor color;
           float shading_x, shading_y, shading_width, shading_height;
           clutter_color_from_string (&color, point->shading_color);


           pp_get_shading_position_size (clutter_actor_get_width (renderer->stage),
                                         clutter_actor_get_height (renderer->stage),
                                         text_x, text_y,
                                         text_width, text_height,
                                         text_scale,
                                         &shading_x, &shading_y,
                                         &shading_width, &shading_height);
           if (!data->shading)
             {
               data->shading = clutter_rectangle_new_with_color (&black);
               clutter_container_add_actor (CLUTTER_CONTAINER (data->midground), data->shading);
               clutter_actor_set_size (data->midground, clutter_actor_get_width (renderer->stage),
                                                        clutter_actor_get_height (renderer->stage));
             }
           g_object_set (data->shading,
                  "depth", -0.01,
                  "x", shading_x,
                  "y", shading_y,
                  "opacity", (int)(point->shading_opacity*255),
                  "color", &color,
                  "width", shading_width,
                  "height", shading_height,
                  NULL);
         }
       else /* no text, fade out shading */
         g_object_set (data->shading, "opacity", 0, NULL);
       if (data->foreground)
         {
           clutter_actor_reparent (data->text, data->foreground);
         }
      }

      clutter_actor_raise_top (data->json_slide);
      clutter_actor_show (data->json_slide);
      clutter_state_set_state (data->state, "show");
    }

  if (point->command)
    {
      g_print ("running: %s\n", point->command);
      system (point->command);
    }
}


static void
stage_resized (ClutterActor    *actor,
               GParamSpec      *pspec,
               ClutterRenderer *renderer)
{
  show_slide (renderer); /* redisplay the current slide */
}

static void
file_changed (GFileMonitor      *monitor,
              GFile             *file,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              ClutterRenderer   *renderer)
{
  char   *text = NULL;
  if (!g_file_get_contents (renderer->path, &text, NULL, NULL))
    g_error ("failed to load slides from %s\n", renderer->path);
  pp_parse_slides (PINPOINT_RENDERER (renderer), text);
  g_free (text);
  show_slide(renderer);
}

static ClutterRenderer clutter_renderer_vtable =
{
  .renderer =
    {
      .init = clutter_renderer_init,
      .run = clutter_renderer_run,
      .finalize = clutter_renderer_finalize,
      .make_point = clutter_renderer_make_point,
      .allocate_data = clutter_renderer_allocate_data,
      .free_data = clutter_renderer_free_data
    }
};

PinPointRenderer *pp_clutter_renderer (void)
{
  return (void*)&clutter_renderer_vtable;
}
