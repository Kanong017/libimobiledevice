/**
 * ideviceexec.c
 * A proxy executable for apps installed on an iDevice
 *
 * Copyright (C) 2011, Karl Krukow and Nils Durner
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <glib.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/lockdown.h>

static char debug;

/**
 * @brief Convert hex digit A to a number.
 * @param a hex digit
 * @returns decimal number
 * @source gdb/remote.c
 */
static int fromhex (int a)
{
	if (a >= '0' && a <= '9')
		return a - '0';
	else if (a >= 'a' && a <= 'f')
		return a - 'a' + 10;
	else if (a >= 'A' && a <= 'F')
		return a - 'A' + 10;
	else
		assert(0);
}

/**
 * @brief Convert a hex string to its byte representation
 * @param hex hex string
 * @param bin byte string
 * @returns number of hex characters processed
 * @source gdb/remote.c
 */
static int hex2bin (const GString *hex, GString *bin)
{
	gsize i;
	gchar *hp;

	hp = hex->str;
	for (i = 0; i < hex->len; i += 2)
	{
		if (hp[0] == 0 || hp[1] == 0)
		{
			/* Hex string is short, or of uneven length.
			 Return the count that has been converted so far. */
			return i;
		}
		g_string_append_c(bin, fromhex (hp[0]) * 16 + fromhex (hp[1]));
		hp += 2;
	}
	return i;
}

/**
 * @brief Convert number NIB to a hex digit.
 * @param nib decimal number
 * @returns hex number
 * @source gdb/remote.c
 */
static int tohex (int nib)
{
	if (nib < 10)
		return '0' + nib;
	else
		return 'a' + nib - 10;
}

/**
 * @brief Convert a byte string to a hex string
 * @param bin byte string
 * @param hex hex string
 * @returns number of bytes processed
 * @source gdb/remote.c
 */
static int bin2hex (const GString *bin, GString *hex)
{
	gsize i;
	gchar *bp;

	bp = bin->str;
	for (i = 0; i < bin->len; i++)
	{
		g_string_append_c(hex, tohex((*bp >> 4) & 0xf));
		g_string_append_c(hex, tohex(*bp++ & 0xf));
	}
	return i;
}

/**
 * @brief Send bytes to a remote gdbserver
 * @param con iDevice connection
 * @param msg bytes to send
 * @param len number of bytes to send
 * @returns number of bytes sent
 */
static uint32_t gdb_send_raw(idevice_connection_t con, const char *msg, uint32_t len)
{
	uint32_t sent;

	if (debug)
		printf("DEBUG: [gdb snd]: %s\n", msg);

	idevice_connection_send(con, msg, len, &sent);
	return sent;
}

/**
 * @brief Receive bytes from a remote gdbserver
 * @param con iDevice connection
 * @param buf receive buffer
 * @param len length of the buffer
 * @param received number of bytes received
 * @returns IDEVICE_E_SUCCESS on success
 */
static idevice_error_t gdb_receive_raw(idevice_connection_t con, char *buf, uint32_t len, uint32_t *received)
{
	idevice_error_t ret;

	ret = idevice_connection_receive(con, buf, len, received);
	if (debug && ret == IDEVICE_E_SUCCESS) {
		char *idx, *end;

		printf("DEBUG: [gdb recv(%u)]: ", *received);
		for (idx = buf, end = idx + *received; idx < end; idx++)
			putchar(*idx);
		putchar('\n');
	}

	return ret;
}

/**
 * @brief Compute and append the checksum of the payload
 * @param packet payload data without header
 */
static void gdb_append_checksum(GString *packet)
{
	gsize i;
	gchar csum, *pp;

	csum = 0;
	pp = packet->str;
	for (i = 0; i < packet->len; i++)
		csum += *pp++;

	g_string_append_c(packet, '#');
	g_string_append_c(packet, tohex((csum >> 4) & 0xf));
	g_string_append_c(packet, tohex(csum & 0xf));
}

/**
 * @brief Send an A packet to a remote gdbserver
 * @see http://developer.apple.com/library/mac/#documentation/DeveloperTools/gdb/gdb/gdb_33.html
 * @param con iDevice connection
 * @param app_path iDevice local path to the app binary
 * @param argc number of arguments (unused)
 * @param argv parameters (unused)
 * @todo pass parameters to the app
 * @returns number of bytes sent
 */
static uint32_t gdb_send_a_packet(idevice_connection_t con, const char *app_path, int argc, char **argv)
{
	GString *payload, *packet;
	char args[10], len[10];
	uint32_t ret, payload_len;

	/* argv array for the app */
	payload = g_string_new(app_path);

	// TODO: support parameters

	if (debug)
		printf("DEBUG: app argv[]: %s\n", payload->str);

	/* hex encode argv string */
	packet = g_string_new("");
	bin2hex(payload, packet);
	g_string_free(payload, TRUE);
	payload_len = packet->len;

	/* prepend number of arguments */
	sprintf(args, "%u", argc - 1);
	g_string_prepend(packet, ",");
	g_string_prepend(packet, args);

	/* prepend length of hex encoded string */
	sprintf(len, "%u", (unsigned int) payload_len);
	g_string_prepend(packet, ",");
	g_string_prepend(packet, len);

	/* packet type */
	g_string_prepend(packet, "A");

	/* packet trailer with checksum */
	gdb_append_checksum(packet);

	/* packet header start */
	g_string_prepend(packet, "$");

	/* send packet */
	ret = gdb_send_raw(con, packet->str, packet->len);
	g_string_free(packet, TRUE);

	return ret;
}

/**
 * @brief Read and check gdbserver response
 * @param con iDevice connection
 * @return gtrue if the response indicates success
 * @information Only works in gdb ACK mode
 */
static gboolean gdb_check_ok(idevice_connection_t con)
{
	char buf[6];
	uint32_t len;

	return gdb_receive_raw(con, buf, 1, &len) == IDEVICE_E_SUCCESS && len == 1 &&
	buf[0] == '+' && gdb_receive_raw(con, buf, 6, &len) == IDEVICE_E_SUCCESS &&
	len == 6 && (memcmp(buf, "$OK#", 4) == 0) && gdb_send_raw(con, "+", 1) == 1;
}

/**
 * @brief Retrieve the iDevice-local path to the app binary
 * @param idevice iDevice handle
 * @param lockd lockdownd session handle
 * @param app bundle id or app name
 * @returns app binary path, to be freed by caller
 */
static char *get_app_path(idevice_t idevice, lockdownd_client_t lockd,
						  const char *app) {
	lockdownd_service_descriptor_t port;
	instproxy_client_t ipc;
	plist_t client_opts;
	instproxy_error_t err;
	plist_t apps;
	plist_t path_p;
	plist_t exec_p;
	char *path_str;
	char *exec_str;
	uint32_t i;
	plist_t app_found;
	char *ret;

	port = 0;
	ipc = NULL;
	apps = NULL;

	client_opts = instproxy_client_options_new();
	instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);

	if ((lockdownd_start_service(lockd, "com.apple.mobile.installation_proxy",
								 &port) != LOCKDOWN_E_SUCCESS) || !port) {
		fprintf(stderr,
				"ERROR: Could not start com.apple.mobile.installation_proxy!\n");
		instproxy_client_options_free(client_opts);
		return NULL;
	}

	if (instproxy_client_new(idevice, port, &ipc) != INSTPROXY_E_SUCCESS) {
		fprintf(stderr, "ERROR: Could not connect to installation_proxy!\n");
		instproxy_client_options_free(client_opts);
		return NULL;
	}

	err = instproxy_browse(ipc, client_opts, &apps);
	instproxy_client_options_free(client_opts);
	if (err != INSTPROXY_E_SUCCESS) {
		if (ipc)
			instproxy_client_free(ipc);

		fprintf(stderr,
				"ERROR: Unable to browse applications. Error code %d\n", err);
		return NULL;
	}

	app_found = NULL;
	for (i = 0; i < plist_array_get_size(apps); i++) {
		plist_t app_info;
		plist_t idp;
		char *appid_str;
		char *name_str;

		appid_str = NULL;
		name_str = NULL;
		app_info = plist_array_get_item(apps, i);
		idp = plist_dict_get_item(app_info, "CFBundleIdentifier");
		plist_t namep = plist_dict_get_item(app_info, "CFBundleDisplayName");

		if (idp) {
			plist_get_string_val(idp, &appid_str);
		}

		if (namep) {
			plist_get_string_val(namep, &name_str);
		}

		if (debug)
			printf("DEBUG: found app %s (%s)\n", appid_str, name_str);

		if (appid_str && strcmp(app, appid_str) == 0) {
			if (app_found) {
				fprintf(stderr, "ERROR: ambigous bundle ID: %s\n",
						app);
				return NULL;
			} else {
				app_found = app_info;
			}

		}

		if (name_str && strcmp(app, name_str) == 0) {
			if (app_found) {
				fprintf(stderr, "ERROR: ambigous app name: %s\n",
						app);
				return NULL;
			}
			else {
				app_found = app_info;
			}
		}

		free(appid_str);
		free(name_str);
	}

	if (!app_found) {
		fprintf(stderr, "ERROR: No app found with name or bundle id: %s\n", app);
		return NULL;
	}

	path_p = plist_dict_get_item(app_found, "Path");
	if (path_p)
		plist_get_string_val(path_p, &path_str);
	else
		path_str = NULL;

	exec_p = plist_dict_get_item(app_found, "CFBundleExecutable");
	if (exec_p)
		plist_get_string_val(exec_p, &exec_str);
	else
		exec_str = NULL;

	if (debug) {
		char *xml;
		uint32_t len;

		xml = NULL;
		len = 0;

		plist_to_xml(app_found, &xml, &len);
		if (xml) {
			printf("DEBUG: app found:\n%s", xml);
			free(xml);
		}
	}

	if (!path_str) {
		fprintf(stderr, "ERROR: app path not found\n");
		return NULL;
	}

	if (!exec_str) {
		fprintf(stderr, "ERROR: bundle executable not found\n");
		return NULL;
	}

	plist_free(apps);
	if (ipc) {
		instproxy_client_free(ipc);
	}

	asprintf(&ret, "%s/%s", path_str, exec_str);
	return ret;
}

/**
 * @brief Print help screen
 * @param argc number of arguments
 * @param argv arguments
 */
static void print_usage(int argc, char **argv)
{
	char *name = NULL;

	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS] <app bundle or name> [parameter]*\n", (name ? name + 1: argv[0]));
	printf("Executes an app on a development iDevice\n");
	printf("  -d, --debug\t\tenable output of debug information\n");
	printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
}

/**
 * @brief Proxy for apps installed on an iDevice
 * @param argc number of arguments
 * @param argv arguments
 * @returns exit code of the app, -1 on errors
 * @see http://developer.apple.com/library/mac/#documentation/DeveloperTools/gdb/gdb/gdb_33.html
 * @see http://sourceware.org/gdb/talks/esc-west-1999/protocol.html
 */
int main(int argc, char **argv)
{
	int arg_idx;
	char **arg;
	char *app;
	char *uuid;
	char *app_path;
	idevice_t device;
	lockdownd_client_t lockd;
	lockdownd_service_descriptor_t service;
	idevice_connection_t con;
	char buf[1000];
	GString *in_queue;
	uint32_t received;
	enum {FIN_NO, FIN_RETRY, FIN_EXIT, FIN_TERM} fin;
	int rc;
	int retries;

	app = NULL;
	uuid = NULL;

	signal(SIGPIPE, SIG_IGN);

	/* parse cmdline args */
	arg = argv + 1;
	for (arg_idx = 1; arg_idx < argc; arg_idx++, arg++) {
		char *param;

		param = *arg;

		if (!strcmp(param, "-d") || !strcmp(param, "--debug")) {
			idevice_set_debug_level(1);
			debug = 1;
		}
		else if (!strcmp(param, "-u") || !strcmp(param, "--uuid")) {
			arg_idx++;
			arg++;
			if (!arg || (strlen(param) != 40)) {
				print_usage(argc, argv);
				return 0;
			}
			uuid = param;
		}
		else if (!strcmp(param, "-h") || !strcmp(param, "--help")) {
			print_usage(argc, argv);
			return 0;
		}
		else {
			app = param;
			break;
		}
	}

	/* verify options */
	if (!app) {
		puts("No app name or id specified\n");
		print_usage(argc, argv);
	}

	/* connect to device */
	if (!uuid) {
		if (idevice_new(&device, uuid) != IDEVICE_E_SUCCESS) {
			printf("No device found with uuid %s, is it plugged in?\n", uuid);
			return -1;
		}
	}
	else
	{
		if (idevice_new(&device, NULL) != IDEVICE_E_SUCCESS) {
			printf("No device found, is it plugged in?\n");
			return -1;
		}
	}

	lockd = NULL;
	for(retries = 1; retries < 4; retries++) {
		if (lockdownd_client_new_with_handshake(device, &lockd, "ideviceexec") != LOCKDOWN_E_SUCCESS) {
			if (debug)
				printf("DEBUG: connection to lockdownd failed, retrying in %i seconds\n", retries);

			sleep(retries);
		}
		else
			break;
	}
	if (!lockd) {
		printf("ERROR: could not establish a lockdownd connection\n");
		idevice_free(device);
		return -1;
	}

	app_path = get_app_path(device, lockd, app);
	if (!app_path)
		return -1;

	if (debug)
		printf("DEBUG: starting debug server\n");
	if (lockdownd_start_service(lockd, "com.apple.debugserver", &service) != IDEVICE_E_SUCCESS) {
		printf("ERROR: could not start debug server. Check that the device is enabled for development.\n");
		return -1;
	}

	if (debug)
		printf("DEBUG: debug server started, connecting\n");

	if (idevice_connect(device, service->port, &con) != IDEVICE_E_SUCCESS) {
		printf("ERROR: connection failed");
		return -1;
	}

	/* set argv[], this implicitely loads the app */
	if (gdb_send_a_packet(con, app_path, argc - arg_idx, arg) < 5) {
		printf("ERROR: could not send load command\n");
		return -1;
	}

	if (!gdb_check_ok(con)) {
		printf("ERROR: could not load app\n");
		return -1;
	}

	/* query success */
	if (gdb_send_raw(con, "$qLaunchSuccess#a5", 18) != 18) {
		printf("ERROR: could not query launch success\n");
		return -1;
	}

	if (!gdb_check_ok(con)) {
		printf("ERROR: app could not be launched\n");
		return -1;
	}

	/* run app and query for return code */
	gdb_send_raw(con, "$c#63", 5);
	gdb_receive_raw(con, buf, 1, &received);

	/* wait for termination */
	in_queue = g_string_new("");
	do {

		/* receive remote messages */
		received = 0;
		gdb_receive_raw(con, buf, sizeof(buf), &received);
		if (received == 0) {
			fin = FIN_NO;
			continue;
		}

		g_string_append_len(in_queue, buf, received);

		if (in_queue->len > 4) {
			do {
				char *in, *idx, *end, *pkg_end;
				GString *console_output, gdb_output;

				/* find end of message */
				pkg_end = NULL;
				in = in_queue->str + 1;
				idx = in;
				for (end = in + in_queue->len; idx <= end; idx++) {
					if (*idx == '#' && idx + 2 <= end) {
						pkg_end = idx + 2;
					}
				}

				if (!pkg_end) {
					fin = FIN_RETRY;
					continue;
				}

				switch(*in) {
					case 'W':
						/* normal exit */
						rc = fromhex(in[1]) * 16 + fromhex(in[2]);
						fin = FIN_EXIT;
						break;

					case 'X':
						/* terminated with signal */
						rc = fromhex(in[1]) * 16 + fromhex(in[2]);
						fin = FIN_TERM;
						break;

					case 'O':
						/* output */
						console_output = g_string_new("");
						bzero(&gdb_output, sizeof(gdb_output));
						gdb_output.str = in + 1;
						gdb_output.len = pkg_end - in - 3;

						hex2bin(&gdb_output, console_output);
						g_string_append_c(console_output, '\0');
						printf("%s", console_output->str);
						g_string_free(console_output, TRUE);

					default:
						/* remove message from input buffer */
						fin = FIN_NO;
						g_string_erase(in_queue, 0, pkg_end - in_queue->str + 1);
//						g_string_erase(in_queue, 0, in_queue->str - pkg_end); // orig

						if (debug)
							printf("DEBUG: read full packet, discarded, rest: %u\n", (unsigned int) in_queue->len);

						/* query termination status */
						if (gdb_send_raw(con, "+$?#3f", 6) != 6) {
							puts("ERROR: could not resume app execution\n");
							return -1;
						}

						if (gdb_receive_raw(con, buf, 1, &received) != IDEVICE_E_SUCCESS || received != 1) {
							puts("ERROR: could not confirm app resumption\n");
							return -1;
						}

						break;

				}
			} while (in_queue->len > 0 && fin == FIN_NO);
		}

	} while (fin == FIN_NO || fin == FIN_RETRY);

	if (debug) {
		if (fin == FIN_EXIT)
			printf("DEBUG: app finished with return code %i\n", rc);
		else if (fin == FIN_TERM) {
			printf("DEBUG: app got terminated with signal %i\n", rc);
		}
	}

	if (fin == FIN_TERM)
		rc = 0;

	return rc;
}
