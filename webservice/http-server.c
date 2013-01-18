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
#include <string.h>
#include <libsoup/soup.h>

#include "http-server.h"

static void
headers_ispct (const gchar *name, const gchar *value, gpointer bla)
{
  g_debug ("name: %s value: %s", name, value);
}

static void
http_handler (SoupMessage *msg,
              const gchar *path,
              DBusClient  *dbus_client)
{
  /* Currently we only deal with one key=value per request */
  gchar *data_in = NULL, *file_uri = NULL;
  guint response;

  if (msg->method == SOUP_METHOD_POST)
      data_in = g_strdup (msg->request_body->data);
  else
    {
      gchar **parsed_uri;
      gchar *tmp;

      tmp = soup_uri_to_string (soup_message_get_uri (msg), TRUE);
      parsed_uri = g_strsplit (tmp, "?", 2);
      data_in = g_strdup (parsed_uri[1]);
      g_strfreev (parsed_uri);
      g_free (tmp);
    }

#if 0
  g_debug ("got post %s", path + strlen ("/"));
  soup_message_headers_foreach (msg->request_headers, headers_ispct, NULL);
  g_debug ("data: %s", data_in);
#endif

  if (g_strcmp0 (path, "/API/ControlKey") == 0)
    {
      dbus_client_input_set_key (dbus_client, (data_in + strlen ("keyflag=")));
      response = SOUP_STATUS_OK;
    }
  else
    {
      SoupBuffer *buffer;
      GFile *gfile;
      GFileInfo *info;
      gchar *file_data;
      const gchar *mime_type;
      gsize data_size;

      if (g_strcmp0 (path, "/") == 0)
        path = "/index.html";

      file_uri = g_strconcat (DATADIR, path, NULL);

      gfile = g_file_new_for_path (file_uri);
      info = g_file_query_info (gfile,
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                0, NULL, NULL);
      if (!info)
        {
          response = SOUP_STATUS_NOT_FOUND;
          goto out;
        }

      mime_type = g_file_info_get_content_type (info);

      g_object_unref (info);
      g_object_unref (gfile);

      if (!g_file_get_contents (file_uri, &file_data, &data_size, NULL))
        {
          response = SOUP_STATUS_NOT_FOUND;
          goto out;
        }

      buffer = soup_buffer_new (SOUP_MEMORY_TAKE, file_data, data_size);

      soup_message_body_truncate (msg->response_body);
      soup_message_body_append_buffer (msg->response_body, buffer);

      soup_message_headers_set_content_type (msg->response_headers,
                                             mime_type,
                                             NULL);
      response = SOUP_STATUS_OK;
      soup_buffer_free (buffer);
    }

out:
  g_free (data_in);
  g_free (file_uri);
  soup_message_set_status (msg, response);
}

static void
server_cb (SoupServer        *server,
           SoupMessage       *msg,
           const char        *path,
           GHashTable        *query,
           SoupClientContext *client,
           DBusClient        *dbus_client)
{
#if 0
  g_debug ("%s %s HTTP/1.%d", msg->method, path, soup_message_get_http_version (msg));
#endif
  if (msg->method == SOUP_METHOD_POST ||
      msg->method == SOUP_METHOD_GET ||
      msg->method == SOUP_METHOD_HEAD)
    http_handler (msg, path, dbus_client);
  else
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
}

static gboolean
auth_cb (SoupAuthDomain *domain,
         SoupMessage    *msg,
         const gchar    *in_username,
         const gchar    *in_password,
         const gchar    *password)
{
  /* we don't really care about username but hey why make it easy */
  if (g_strcmp0 (password, in_password) == 0 &&
      g_strcmp0 (in_username, "pinpoint") == 0)
      return TRUE;

  return FALSE;
}

void
start_http_server (DBusClient  *dbus_client,
                   guint        port,
                   const gchar *password)
{
  SoupServer *server;
  SoupAuthDomain *domain;

  server = soup_server_new (SOUP_SERVER_PORT, port,
                            SOUP_SERVER_SERVER_HEADER, "pinpoint-ws",
                            NULL);

  if (!server)
    g_error ("Sorry, unable to create server");

  if (password)
    {
      domain = soup_auth_domain_basic_new (SOUP_AUTH_DOMAIN_REALM,
                                           "Authenticate",
                                           SOUP_AUTH_DOMAIN_BASIC_AUTH_CALLBACK,
                                           auth_cb,
                                           SOUP_AUTH_DOMAIN_BASIC_AUTH_DATA,
                                           (gpointer)password,
                                           SOUP_AUTH_DOMAIN_ADD_PATH, "/",
                                           NULL);

      soup_server_add_auth_domain (server, domain);
      g_object_unref (domain);
    }

  soup_server_run_async (server);

  soup_server_add_handler (server, NULL, (SoupServerCallback)server_cb,
                           dbus_client,
                           NULL);
}
