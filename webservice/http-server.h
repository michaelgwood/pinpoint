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

#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <glib.h>
#include <gio/gio.h>
#include "dbus-client.h"

G_BEGIN_DECLS

void start_http_server (DBusClient  *dbus_client,
                        guint        port,
                        const gchar *password);

G_END_DECLS

#endif
