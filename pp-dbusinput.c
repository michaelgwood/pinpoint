/*
 * Pinpoint - a small-ish presentation tool
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

#include <gio/gio.h>
#include "pp-dbusinput.h"

#define PP_DBUS_NAME "org.gnome.Pinpoint"

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.gnome.Pinpoint.Input'>"
"    <method name='ControlKey'>"
"      <arg name='keyflag' direction='in' type='u'/>"
"    </method>"
"  </interface>"
"</node>";

static void
_method_cb (GDBusConnection       *connection,
            const gchar           *sender,
            const gchar           *object_path,
            const gchar           *interface_name,
            const gchar           *method_name,
            GVariant              *parameters,
            GDBusMethodInvocation *invocation,
            DbusInput             *self)
{

  if (g_strcmp0 (method_name, "ControlKey") == 0)
    {
      guint keyflag;
      ClutterEvent *event;
      ClutterKeyEvent *kevent;

      g_variant_get (parameters, "(u)", &keyflag);

      event = clutter_event_new (CLUTTER_KEY_PRESS);
      kevent = (ClutterKeyEvent *)event;

      kevent->flags = 0;
      kevent->source = NULL;
      kevent->stage = CLUTTER_STAGE (self->stage);
      kevent->keyval = keyflag;
      kevent->time = time (NULL);

      /* KEY PRESS */
      clutter_event_put (event);
      /* KEY RELEASE */
      kevent->type = CLUTTER_KEY_RELEASE;
      clutter_event_put (event);

      clutter_event_free (event);
    }

  g_dbus_method_invocation_return_value (invocation, NULL);
}

static const GDBusInterfaceVTable interface_table =
{
  (GDBusInterfaceMethodCallFunc) _method_cb,
  NULL,
  NULL
};

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  DbusInput *self = user_data;
  GError *error = NULL;


  self->connection = g_bus_get_finish (result, &error);

  if (!self->connection)
    {
      g_warning ("Could not acquire bus connection: %s", error->message);
      g_error_free (error);
      return;
    }

  /* Note: Dbus object and name subject to change */
  guint id = g_dbus_connection_register_object (self->connection,
                                     "/org/Pinpoint/Input",
                                     self->introspection_data->interfaces[0],
                                     &interface_table,
                                     self,
                                     NULL,
                                     &error);
  if (id == 0)
    {
      g_warning ("Problem registering object: %s", error->message);
      g_error_free (error);
    }
}

DbusInput *
pp_dbusinput_new (ClutterStage *stage)
{
  DbusInput *self = g_slice_new0 (DbusInput);

  self->stage = stage;

  self->introspection_data =
    g_dbus_node_info_new_for_xml (introspection_xml, NULL);

  g_bus_own_name (G_BUS_TYPE_SESSION, PP_DBUS_NAME,
                  G_BUS_NAME_OWNER_FLAGS_NONE, NULL,
                  NULL, NULL, NULL, NULL);

  g_bus_get (G_BUS_TYPE_SESSION, NULL, on_bus_acquired, self);

  return self;
}

void
pp_dbusinput_free (DbusInput *self)
{
  if (self->connection)
    g_object_unref (self->connection);

  g_dbus_node_info_unref (self->introspection_data);
}
