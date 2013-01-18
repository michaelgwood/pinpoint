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

#ifndef __DBUS_CLIENT_H__
#define __DBUS_CLIENT_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _DBusClient DBusClient;

struct _DBusClient
{
  GDBusConnection *connection;
  GDBusProxy *input;
};

DBusClient *dbus_client_new (void);

void dbus_client_free (DBusClient *dbus_client);

void dbus_client_input_set_key (DBusClient *dbus_client, const gchar *keyval);

G_END_DECLS

#endif
