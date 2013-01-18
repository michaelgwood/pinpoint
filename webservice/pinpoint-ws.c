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
#include "http-server.h"


int main (int argc, char **argv)
{
  GOptionContext *context;

  GMainLoop *main_loop;
  guint opt_port;
  const gchar *opt_auth = NULL;
  GError *error=NULL;
  DBusClient *dbus_client;

  g_type_init ();

  opt_port = 9999;

  GOptionEntry entries[] =
    {
        { "http-port", 'p', 0, G_OPTION_ARG_INT, &opt_port,
          "http port to listen on", NULL},
        { "password", 'a', 0, G_OPTION_ARG_STRING, &opt_auth,
          "Authentication password to use (username pinpoint)", NULL },
        { NULL }
    };

  context = g_option_context_new ("- A webservice for PinPoint");

  g_option_context_add_main_entries (context, entries, "pinpoint-ws");

  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_warning ("Failed to parse options: %s", error->message);


  dbus_client = dbus_client_new ();
  if (!dbus_client)
    goto cleanup;


  start_http_server (dbus_client, opt_port, opt_auth);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (main_loop);

cleanup:

  if (context)
    g_option_context_free (context);
  if (dbus_client)
    dbus_client_free (dbus_client);

  g_clear_error (&error);

  return EXIT_SUCCESS;
}
