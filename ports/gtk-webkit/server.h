/*
Copyright © 2018 Atlas Engineer LLC.
Use of this file is governed by the license that can be found in LICENSE.
*/

#include <glib.h>
#include <webkit2/webkit2.h>
#include <libsoup/soup.h>

#include "autokey-dictionary.h"

#define APPNAME "Next"

typedef GVariant * (*ServerCallback) (SoupXMLRPCParams *);

// TODO: Make local?
static AutokeyDictionary *windows;
static GHashTable *server_callbacks;

static void destroy_window(GtkWidget *widget, GtkWidget *window) {
	// TODO: Call only on last window.
	gtk_main_quit();
}

static gboolean close_web_view(WebKitWebView *webView, GtkWidget *window) {
	// TODO: Call window_delete with identifier.
	gtk_widget_destroy(window);
	return true;
}

static GVariant *window_make(SoupXMLRPCParams *_params) {
	// Create an 800x600 window that will contain the browser instance
	GtkWidget *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(main_window), 800, 600);
	// TODO: Make title customizable from Lisp.
	gtk_window_set_title(GTK_WINDOW(main_window), APPNAME);
	// TODO: Deprecated?
	/* gtk_window_set_wmclass(GTK_WINDOW(main_window), APPNAME, APPNAME); */

	// Create a browser instance
	WebKitWebView *web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

	// Put the browser area into the main window
	gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(web_view));

	// Set up callbacks so that if either the main window or the browser
	// instance is closed, the program will exit
	g_signal_connect(main_window, "destroy", G_CALLBACK(destroy_window), NULL);
	g_signal_connect(web_view, "close", G_CALLBACK(close_web_view), main_window);

	// Load a web page into the browser instance
	webkit_web_view_load_uri(web_view, "https://next.atlas.engineer/");

	// Make sure that when the browser area becomes visible, it will get
	// mouse and keyboard events
	gtk_widget_grab_focus(GTK_WIDGET(web_view));

	// Make sure the main window and all its contents are visible
	gtk_widget_show_all(main_window);

	char *identifier = akd_insert_element(windows, main_window);
	return g_variant_new_string(identifier);
}

static GVariant *window_delete(SoupXMLRPCParams *params) {
	GError *error = NULL;
	GVariant *variant = soup_xmlrpc_params_parse(params, NULL, &error);
	if (error) {
		g_warning("Malformed method parameters: %s", error->message);
		return g_variant_new_boolean(FALSE);
	}
	if (!g_variant_check_format_string(variant, "av", FALSE)) {
		g_warning("Malformed parameter value: %s", g_variant_get_type_string(variant));
	}
	// Variant type string is "av", and the embedded "v"'s type string is "s".
	const char *a_key = g_variant_get_string(
		g_variant_get_variant(
			g_variant_get_child_value(variant, 0)),
		NULL);
	g_debug("Method parameter: %s", a_key);

	GtkWidget *window = akd_object_for_key(windows, a_key);
	gtk_widget_destroy(window);
	akd_remove_object_for_key(windows, a_key);
	return g_variant_new_boolean(TRUE);
}

static void server_handler(SoupServer *server, SoupMessage *msg,
	const char *path, GHashTable *query,
	SoupClientContext *context, gpointer data) {
	{
		const char *name, *value;
		SoupMessageHeadersIter iter;
		GString *pretty_message = g_string_new("XMLRPC request:\n");
		g_string_append_printf(pretty_message, "%s %s HTTP/1.%d\n", msg->method, path,
			soup_message_get_http_version(msg));
		soup_message_headers_iter_init(&iter, msg->request_headers);
		while (soup_message_headers_iter_next(&iter, &name, &value)) {
			g_string_append_printf(pretty_message, "%s: %s\n", name, value);
		}
		if (msg->request_body->length == 0) {
			g_warning("Empty HTTP request");
			return;
		}
		g_string_append_printf(pretty_message, "%s", msg->request_body->data);
		g_debug("%s", pretty_message->str);
		g_string_free(pretty_message, TRUE);
	}

	SoupXMLRPCParams *params = NULL;
	GError *error = NULL;
	char *method_name = soup_xmlrpc_parse_request(msg->request_body->data,
			msg->request_body->length,
			&params,
			&error);
	if (error) {
		g_warning("Malformed XMLRPC request: %s", error->message);
		return;
	}

	ServerCallback callback = NULL;
	gboolean found = g_hash_table_lookup_extended(server_callbacks, method_name,
			NULL, (gpointer *)&callback);
	if (!found) {
		g_warning("Unknown method: %s", method_name);
		return;
	}
	g_debug("Method name: %s", method_name);

	GVariant *operation_result = callback(params);

	soup_xmlrpc_params_free(params);

	soup_xmlrpc_message_set_response(msg, operation_result, &error);
	if (error) {
		g_warning("Failed to set XMLRPC response: %s", error->message);
	}

	g_debug("Response: %d %s", msg->status_code, msg->reason_phrase);
}

void start_server() {
	// TODO: Server logging?
	// TODO: libsoup's examples don't unref the server.  Should we?
	SoupServer *server = soup_server_new(
		/* SOUP_SERVER_SERVER_HEADER, APPNAME, */
		NULL);

	GError *error = NULL;
	soup_server_listen_all(server, 8082, 0, &error);
	if (error) {
		g_printerr("Unable to create server: %s\n", error->message);
		exit(1);
	}
	g_debug("Starting XMLRPC server");
	soup_server_add_handler(server, NULL, server_handler, NULL, NULL);

	// Register callbacks.
	server_callbacks = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(server_callbacks, "window.make", &window_make);
	g_hash_table_insert(server_callbacks, "window.delete", &window_delete);

	// Global indentifiers.
	windows = akd_init(NULL);
}

void stop_server() {
	akd_free(windows);
}