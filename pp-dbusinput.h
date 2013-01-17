/*
 * PinPoint - a small-ish presentation tool
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>
 *
 * Written by: Michael Wood <michael.g.wood@intel.com>
 *
 * Based on media-explorer's pp-dbusinput-plugin.c by
 * Michael Wood <michael.g.wood@intel.com>
 * Allows you to push key codes into pinpoint
 */

#include <glib-object.h>
#include <gmodule.h>
#include <clutter/clutter.h>

typedef struct _DbusInput
{
  ClutterStage *stage;

  GDBusConnection *connection;
  GDBusNodeInfo *introspection_data;
} DbusInput;


DbusInput *pp_dbusinput_new (ClutterStage *stage);

void pp_dbusinput_free (DbusInput *self);
