/*
 * Pinpoint-ws - a small-ish webservice for a small-ish presentation tool
 *
 * Copyright © 2013 Intel Corporation.
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
 * along with this program; if not, see <://www.gnu.org/licenses>
 *
 * Written by: Michael Wood <michael.g.wood@intel.com>
 */

#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include "dbus-client.h"

#define NAME "org.gnome.Pinpoint"
#define PATH "/org/Pinpoint/Input"
#define INTERFACE "org.gnome.Pinpoint.Input"

static void
dbus_input_proxy_new (DBusClient  *dbus_client)
{
  GError *error=NULL;
  GDBusProxy *proxy;

  if (dbus_client->input)
    return;

  dbus_client->input = g_dbus_proxy_new_sync (dbus_client->connection,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              NULL,
                                              NAME,
                                              PATH,
                                              INTERFACE,
                                              NULL,
                                              &error);
  if (error)
    {
      g_warning ("Could not create dbus input proxy: %s", error->message);
      g_error_free (error);
    }
}

void
dbus_client_input_set_key (DBusClient  *dbus_client,
                           const gchar *s_keyval)
{
  GError *error=NULL;
  guint keyval;

  if (!dbus_client->input)
    dbus_input_proxy_new (dbus_client);


  keyval = g_ascii_strtoll (s_keyval, NULL, 0);

  if (keyval == 0)
    return;

  g_dbus_proxy_call_sync (dbus_client->input,
                          "ControlKey",
                          g_variant_new ("(u)", keyval),
                          0,
                          -1,
                          NULL,
                          &error);
  if (error)
    {
      g_warning ("Problem calling ControlKey: %s", error->message);
      g_error_free (error);
    }
}


DBusClient *
dbus_client_new (void)
{
  DBusClient *dbus_client;
  GError *error = NULL;

  dbus_client = g_new0 (DBusClient, 1);

  dbus_client->connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                            NULL,
                                            &error);
  if (error)
    {
      g_warning ("Failed to connect to dbus %s\n", error->message );
      g_error_free (error);
    }
  else
    {
      dbus_input_proxy_new (dbus_client);
    }
  return dbus_client;
}


void
dbus_client_free (DBusClient *dbus_client)
{
  g_object_unref (dbus_client->connection);
  g_object_unref (dbus_client->input);
  g_free (dbus_client);
}
