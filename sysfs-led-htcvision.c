/** @file sysfs-led-htcvision.c
 *
 * mce-plugin-libhybris - Libhybris plugin for Mode Control Entity
 * <p>
 * Copyright (C) 2013-2017 Jolla Ltd.
 * <p>
 * @author Simo Piiroinen <simo.piiroinen@jollamobile.com>
 *
 * mce-plugin-libhybris is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * mce-plugin-libhybris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mce-plugin-libhybris; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* ========================================================================= *
 * Amber/green led control: Htcvision backend
 *
 * Two channels (amber and green), both of which:
 * - must have 'brightness' control file
 * - must have 'max_brightness' control file
 * - must have 'blink' enable/disable control file
 *
 * Assumptions built into code:
 * - while there are two channels, kernel and/or hw only allows one of them
 *   to be active -> Map rgb form request from mce to amber/green and try
 *   to minimize color error.
 * ========================================================================= */

#include "sysfs-led-htcvision.h"

#include "sysfs-led-util.h"

#include <stdio.h>

#include <glib.h>

/* ========================================================================= *
 * PROTOTYPES
 * ========================================================================= */

typedef struct
{
  const char *max;    // R
  const char *val;    // W
  const char *blink;  // W
} led_paths_htcvision_t;

typedef struct
{
  int maxval;
  int fd_val;
  int fd_blink;
} led_channel_htcvision_t;

/* ------------------------------------------------------------------------- *
 * ONE_CHANNEL
 * ------------------------------------------------------------------------- */

static void        led_channel_htcvision_init        (led_channel_htcvision_t *self);
static void        led_channel_htcvision_close       (led_channel_htcvision_t *self);
static bool        led_channel_htcvision_probe       (led_channel_htcvision_t *self, const led_paths_htcvision_t *path);
static void        led_channel_htcvision_set_value   (const led_channel_htcvision_t *self, int value);
static void        led_channel_htcvision_set_blink   (const led_channel_htcvision_t *self, int blink);

/* ------------------------------------------------------------------------- *
 * ALL_CHANNELS
 * ------------------------------------------------------------------------- */

static void        led_control_htcvision_map_color   (int r, int g, int b, int *amber, int *green);
static void        led_control_htcvision_blink_cb    (void *data, int on_ms, int off_ms);
static void        led_control_htcvision_value_cb    (void *data, int r, int g, int b);
static void        led_control_htcvision_close_cb    (void *data);

bool               led_control_htcvision_probe       (led_control_t *self);

/* ========================================================================= *
 * ONE_CHANNEL
 * ========================================================================= */

static void
led_channel_htcvision_init(led_channel_htcvision_t *self)
{
  self->maxval   = -1;
  self->fd_val   = -1;
  self->fd_blink = -1;
}

static void
led_channel_htcvision_close(led_channel_htcvision_t *self)
{
  led_util_close_file(&self->fd_val);
  led_util_close_file(&self->fd_blink);
}

static bool
led_channel_htcvision_probe(led_channel_htcvision_t *self,
                            const led_paths_htcvision_t *path)
{
  bool res = false;

  led_channel_htcvision_close(self);

  if( (self->maxval = led_util_read_number(path->max)) <= 0 )
  {
    goto cleanup;
  }

  if( !led_util_open_file(&self->fd_val,   path->val)  ||
      !led_util_open_file(&self->fd_blink, path->blink) )
  {
    goto cleanup;
  }

  res = true;

cleanup:

  if( !res ) led_channel_htcvision_close(self);

  return res;
}

static void
led_channel_htcvision_set_value(const led_channel_htcvision_t *self,
                                int value)
{
  if( self->fd_val != -1 )
  {
    dprintf(self->fd_val, "%d", led_util_scale_value(value, self->maxval));
  }
}

static void
led_channel_htcvision_set_blink(const led_channel_htcvision_t *self, int blink)
{
  if( self->fd_blink != -1 )
  {
    dprintf(self->fd_val, "%d", blink);
  }
}

/* ========================================================================= *
 * ALL_CHANNELS
 * ========================================================================= */

static void
led_control_htcvision_map_color(int r, int g, int b,
                                int *amber, int *green)
{
  /* Only "amber" or "green" color can be used.
   *
   * Assume amber = r:ff g:7f b:00
   *        green = r:00 g:ff b:00
   *
   * Try to choose the one with smaller delta to the
   * requested rgb color by using the 'r':'g' ratio.
   *
   * The 'b' is used for intensity preservation only.
   */

  if( r * 3 < g * 4)
  {
    *amber = 0;
    *green = (g > b) ? g : b;
  }
  else
  {
    *amber = (r > b) ? r : b;
    *green = 0;
  }
}

static void
led_control_htcvision_blink_cb(void *data, int on_ms, int off_ms)
{
  const led_channel_htcvision_t *channel = data;

  int blink = (on_ms && off_ms);

  led_channel_htcvision_set_blink(channel + 0, blink);
  led_channel_htcvision_set_blink(channel + 1, blink);
}

static void
led_control_htcvision_value_cb(void *data, int r, int g, int b)
{
  const led_channel_htcvision_t *channel = data;

  int amber = 0;
  int green = 0;
  led_control_htcvision_map_color(r, g, b, &amber, &green);

  led_channel_htcvision_set_value(channel + 0, amber);
  led_channel_htcvision_set_value(channel + 1, green);
}

static void
led_control_htcvision_close_cb(void *data)
{
  led_channel_htcvision_t *channel = data;
  led_channel_htcvision_close(channel + 0);
  led_channel_htcvision_close(channel + 1);
}

bool
led_control_htcvision_probe(led_control_t *self)
{
  /** Sysfs control paths for Amber/Green leds */
  static const led_paths_htcvision_t paths[][3] =
  {
    // htc vision, htc ace
    {
      {
        .max   = "/sys/class/leds/amber/max_brightness",
        .val   = "/sys/class/leds/amber/brightness",
        .blink = "/sys/class/leds/amber/blink",
      },
      {
        .max   = "/sys/class/leds/green/max_brightness",
        .val   = "/sys/class/leds/green/brightness",
        .blink = "/sys/class/leds/green/blink",
      },
    },
  };

  static led_channel_htcvision_t channel[2];

  bool res = false;

  led_channel_htcvision_init(channel+0);
  led_channel_htcvision_init(channel+1);

  self->name   = "htcvision";
  self->data   = channel;
  self->enable = 0;
  self->blink  = led_control_htcvision_blink_cb;
  self->value  = led_control_htcvision_value_cb;
  self->close  = led_control_htcvision_close_cb;

  /* TODO: check if breathing can be left enabled */
  self->can_breathe = true;

  for( size_t i = 0; i < G_N_ELEMENTS(paths) ; ++i )
  {
    if( led_channel_htcvision_probe(&channel[0], &paths[i][0]) &&
        led_channel_htcvision_probe(&channel[1], &paths[i][1]) )
    {
      res = true;
      break;
    }
  }

  if( !res )
  {
    led_control_close(self);
  }

  return res;
}