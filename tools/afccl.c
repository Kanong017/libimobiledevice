/*
 * afccl.c
 * AFC command line utility
 *
 * Copyright (C) 2013 Aaron Burghardt
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libimobiledevice/afc.h>
#include <readline/readline.h>
#include <readline/history.h>

#define CMD_INTERACTIVE 0
#define CMD_CD 1
#define CMD_PWD 2
#define CMD_LS 3
#define CMD_MKDIR 4
#define CMD_LN 5
#define CMD_RM 6
#define CMD_MV 7
#define CMD_CP 8
#define CMD_CAT 9
#define CMD_STAT 10
#define CMD_QUIT 98
#define CMD_UNKNOWN 99


afc_client_t afc = NULL;
char *cwd;

static int do_cmd(int cmd, int argc, const char *argv[]);

char *afc_strerror(afc_error_t err)
{
	switch (err) {
		case AFC_E_SUCCESS:
			return "AFC success";
		case AFC_E_OP_HEADER_INVALID:
			return "op header invalid";
		case AFC_E_NO_RESOURCES:
			return "no resources";
		case AFC_E_READ_ERROR:
			return "read error";
		case AFC_E_WRITE_ERROR:
			return "write error";
		case AFC_E_UNKNOWN_PACKET_TYPE:
			return "unknown packet type";
		case AFC_E_INVALID_ARG:
			return "invalid argument";
		case AFC_E_OBJECT_NOT_FOUND:
			return "object not found";
		case AFC_E_OBJECT_IS_DIR:
			return "object is a directory";
		case AFC_E_PERM_DENIED:
			return "permission denied";
		case AFC_E_SERVICE_NOT_CONNECTED:
			return "service not connected";
		case AFC_E_OP_TIMEOUT:
			return "op timeout";
		case AFC_E_TOO_MUCH_DATA:
			return "too much data";
		case AFC_E_END_OF_DATA:
			return "end of data";
		case AFC_E_OP_NOT_SUPPORTED:
			return "op not supported";
		case AFC_E_OBJECT_EXISTS:
			return "object exists";
		case AFC_E_OBJECT_BUSY:
			return "object busy";
		case AFC_E_NO_SPACE_LEFT:
			return "no space available";
		case AFC_E_OP_WOULD_BLOCK:
			return "op would block";
		case AFC_E_IO_ERROR:
			return "I/O error";
		case AFC_E_OP_INTERRUPTED:
			return "op interrupted";
		case AFC_E_OP_IN_PROGRESS:
			return "op in progress";
		case AFC_E_INTERNAL_ERROR:
			return "internal error";
		case AFC_E_MUX_ERROR:
			return "usbmuxd error";
		case AFC_E_NO_MEM:
			return "out of memory";
		case AFC_E_NOT_ENOUGH_DATA:
			return "not enough data";
		case AFC_E_DIR_NOT_EMPTY:
			return "directory not empty";

		case AFC_E_UNKNOWN_ERROR:
		default:
			break;
	}
	return "unknown error";
}

void afc_warn(afc_error_t err, const char *fmt, ...)
{
//	void vwarn(const char *fmt, va_list args);
	va_list ap;

	char *newfmt;
	asprintf(&newfmt, "%s: %s (%d)", fmt, afc_strerror(err), err);

	va_start(ap, fmt);

	vwarnx(newfmt, ap);

	va_end(ap);

	free(newfmt);
}

static char *build_absolute_path(const char *inpath)
{
	char *path;

	if (inpath[0] == '/')
		path = strdup(inpath);
	else {
		if (asprintf(&path, "%s/%s", (strcmp(cwd, "/")) ? cwd : "", inpath) == -1) {
			warn("asprintf");
			return NULL;
		}
	}
	for (int i = strlen(path); i > 1 && path[i] == '/'; i--)
		path[i] = 0;

	return path;
}

static char *cleanse_path(char *inpath)
{
	char *newpath;

	// Remove "." and ".."
	newpath = malloc(strlen(inpath));
	memcpy(newpath, "/", 2);

	char *token;
	char *remainder = inpath;
	while ((token = strsep(&remainder, "/"))) {
		if (token[0] == 0 || !strcmp(token, ".")) {
			continue;
		}
		else if (!strcmp(token, "..")) {
			char *lastslash = strrchr(newpath, '/');
			if (lastslash > newpath)
				*lastslash = 0;
			else
				newpath[1] = 0;
		}
		else {
			if (strlen(newpath) > 1)
				strcat(newpath, "/");
			strcat(newpath, token);
		}
	}
	free(inpath);

	return newpath;
}


static int cmd_cd(const char *path)
{
	afc_error_t result;
	char *fullpath, *cleanpath;
	char **infolist;

	if (!path) {
		free(cwd);
		cwd = strdup("/");
		return 0;
	}

	if ((fullpath = build_absolute_path(path)) == NULL)
		return AFC_E_INTERNAL_ERROR;

	cleanpath = cleanse_path(fullpath);

	result = afc_get_file_info(afc, cleanpath, &infolist);
	if (result != AFC_E_SUCCESS) {
		afc_warn(result, "%s", path);
		free(cleanpath);
		return result;
	}
	for (int i = 0; infolist[i] != NULL; i++)
		free(infolist[i]);
	free(infolist);

	free(cwd);
	cwd = cleanpath;

	return 0;
}

static int cmd_pwd(void)
{
	printf("%s\n", cwd);
	return 0;
}

static afc_error_t cmd_ls(int argc, const char *argv[])
{
	afc_error_t result;
	char *path;
	char **list = NULL;

	if (argc == 0) {
		path = strdup(cwd);
	}
	else if (argc == 1) {
		path = build_absolute_path(argv[0]);
		if (!path)
			return AFC_E_INTERNAL_ERROR;
	}
	else {
		warnx("usage: ls [<dir>]");
		return AFC_E_INVALID_ARG;
	}

	result = afc_read_directory(afc, path, &list);
	if (result != AFC_E_SUCCESS) {
		afc_warn(result, "%s", argc == 1 ? argv[0] : cwd);
		return result;
	}

	for (int i = 0; list[i]; i++) {
		printf("%s\n", list[i]);
		free(list[i]);
	}
	free(list);

	return AFC_E_SUCCESS;
}

static afc_error_t cmd_mkdir(int argc, const char *argv[])
{
	afc_error_t result;
	char *path;

	if (argc != 1) {
		warnx("usage: mkdir <dir>");
		return AFC_E_INVALID_ARG;
	}

	if ((path = build_absolute_path(argv[0])) == NULL)
		return AFC_E_INTERNAL_ERROR;

	result = afc_make_directory(afc, path);
	if (result != AFC_E_SUCCESS)
		afc_warn(result, "%s", path);
	free(path);

	return result;
}

static afc_error_t cmd_ln(int argc, const char *argv[])
{
	afc_error_t result;
	int type = AFC_HARDLINK;
	char *source_path = NULL;
	char *target_path = NULL;

	if (argc == 3 && !strcmp("-s", argv[0])) {
		type = AFC_SYMLINK;
		argc--;
		argv++;
	}
	if (argc != 2) {
		warnx("usage: ln [-s] <source> <target>");
		return AFC_E_INVALID_ARG;
	}

	if (type == AFC_HARDLINK)
		source_path = build_absolute_path(argv[0]);
	else
		source_path = strdup(argv[0]);

	target_path = build_absolute_path(argv[1]);

	if (!source_path || !target_path) {
		free(source_path);
		free(target_path);
		return AFC_E_INTERNAL_ERROR;
	}

//	warnx("%s: %s -> %s\n", type == AFC_HARDLINK ? "hard link" : "sym link", source_path, target_path);

	result = afc_make_link(afc, type, source_path, target_path);
	if (result == AFC_E_OBJECT_NOT_FOUND)
		afc_warn(result, "%s", argv[0]);
	else if (result != AFC_E_SUCCESS)
		afc_warn(result, "afc_make_link");

	free(source_path);
	free(target_path);

	return result;
}

static afc_error_t cmd_rm(int argc, const char *argv[])
{
	int result;
	int retval = AFC_E_SUCCESS;
	char *path = NULL;

	for (int i = 0; i < argc; i++) {

		path = build_absolute_path(argv[i]);
		if (!path)
			return AFC_E_INTERNAL_ERROR;

		result = afc_remove_path(afc, path);
		free(path);

		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", argv[i]);
			retval = result;
		}
	}

	return retval;
}

static afc_error_t cmd_mv(int argc, const char *argv[])
{
	afc_error_t result;
	char *source_path, *target_path;

	if (argc != 2) {
		warnx("usage: mv <source> <target>");
		return -1;
	}
	source_path = build_absolute_path(argv[0]);
	target_path = build_absolute_path(argv[1]);
	if (!source_path || !target_path) {
		free(source_path);
		free(target_path);
		return AFC_E_INTERNAL_ERROR;
	}

	result = afc_rename_path(afc, source_path, target_path);
	if (result != AFC_E_SUCCESS)
		afc_warn(result, "rename %s", argv[0]);
	free(source_path);
	free(target_path);

	return result;
}

static afc_error_t cmd_stat(int argc, const char *argv[])
{
	afc_error_t result;
	char **infolist = NULL;
	char *path;

	if (argc < 1) {
		warnx("usage: stat <file> ...");
		return -1;
	}

	for (int i = 0; i < argc; i++) {

		path = build_absolute_path(argv[0]);
		result = afc_get_file_info(afc, path, &infolist);
		free(path);

		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", argv[i]);
			return result;
		}

		printf("%s:\n", argv[i]);

		for (int j = 0; infolist[j] != NULL; j++) {
			printf("%14s ", infolist[j]);
			free(infolist[j]);
			printf("%s\n", infolist[++j]);
			free(infolist[j]);
		}
		free(infolist);

		putc('\n', stdout);
	}

	return AFC_E_SUCCESS;
}

static afc_error_t cmd_cat(int argc, const char *argv[])
{
	afc_error_t result;
	char *path;
	uint64_t handle, size;
	char **infolist;
	uint32_t readsize;
	char buffer[0x1000];

	if (argc != 1) {
		warnx("usage: cat <file>");
		return -1;
	}

	if ((path = build_absolute_path(argv[0])) == NULL)
		return AFC_E_INTERNAL_ERROR;

	result = afc_get_file_info(afc, path, &infolist);
	if (result != AFC_E_SUCCESS) {
		afc_warn(result, "%s", argv[0]);
		free(path);
		return result;
	}
	size = atoll(infolist[1]);
	for (int i = 0; infolist[i] != NULL; i++)
		free(infolist[i]);
	free(infolist);

	result = afc_file_open(afc, path, AFC_FOPEN_RDONLY, &handle);
	if (result != AFC_E_SUCCESS) {
		afc_warn(result, "%s", argv[0]);
		free(path);
		return result;
	}

	for (int i = 0; i < size; ) {
		result = afc_file_read(afc, handle, buffer, sizeof(buffer), &readsize);
		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", argv[0]);
			afc_file_close(afc, handle);
			free(path);
			return result;
		}
		fwrite(buffer, 1, readsize, stdout);
		i += readsize;
	}

	afc_file_close(afc, handle);
	free(path);
	
	return AFC_E_SUCCESS;
}

static afc_error_t cmd_cp(int argc, const char *argv[])
{
	return AFC_E_INVALID_ARG;
}

static int str_to_cmd(const char *str)
{
	if (!strcmp(str, "-"))
		return CMD_INTERACTIVE;
	else if (!strcmp(str, "cd"))
		return CMD_CD;
	else if (!strcmp(str, "pwd"))
		return CMD_PWD;
	else if (!strcmp(str, "ls"))
		return CMD_LS;
	else if (!strcmp(str, "mkdir"))
		return CMD_MKDIR;
	else if (!strcmp(str, "ln"))
		return CMD_LN;
	else if (!strcmp(str, "rm"))
		return CMD_RM;
	else if (!strcmp(str, "mv"))
		return CMD_MV;
	else if (!strcmp(str, "cp"))
		return CMD_CP;
	else if (!strcmp(str, "cat"))
		return CMD_CAT;
	else if (!strcmp(str, "stat"))
		return CMD_STAT;
	else if (!strcmp(str, "quit"))
		return CMD_QUIT;

	return CMD_UNKNOWN;
}

#define PARSER_IDLE     0
#define PARSER_IN_QUOTE 1
#define PARSER_IN_WORD  2

static int tokenize_command_line(char *input, int *out_argc, char **out_argv[])
{

	int argc = 0;
	char **argv, *pout;
	const char *p, *pmax;
	int state = PARSER_IDLE;

	if (!input || strlen(input) == 0) {
		*out_argc = 0;
		*out_argv = NULL;
		return 0;
	}

	p = pout = input;
	pmax = input + strlen(input);

	argv = malloc(sizeof(char *));

	while (p < pmax) {

		switch (state) {

			case PARSER_IDLE:
				if (isspace(*p)) {
					p++;
					continue;
				}
				state = PARSER_IN_WORD;
				argv[argc++] = pout;
				argv = realloc(argv, (argc + 1) * sizeof(char *));
				argv[argc] = NULL;
				break;

			case PARSER_IN_WORD:
				if (isspace(*p)) {
					*pout = 0;
					pout++;
					p++;
					state = PARSER_IDLE;
				}
				else {
					if (*p == '"') {
						state = PARSER_IN_QUOTE;
						p++;
						continue;
					}
					if (*p == '\\')
						p++;
					*pout = *p;
					pout++;
					p++;
					if (p >= pmax) {
						state = PARSER_IDLE;
						*pout = 0;
					}
				}
				break;

			case PARSER_IN_QUOTE:
				if (*p == '"') {
					state = PARSER_IN_WORD;
					p++;
					continue;
				}
				if (*p == '\\' && *(p + 1) == '"')
					p++;
				*pout = *p;
				p++;
				pout++;
				break;

			default:
				break;
		}
	}

	if (state == PARSER_IN_QUOTE) {
		free(argv);
		warnx("quote mismatch");
		return -1;
	}

	*out_argc = argc;
	*out_argv = argv;
	return 0;
}

char *load_history(void)
{
	char *homedir, *path = NULL;

	homedir = getenv("HOME");
	if (homedir) {
		if (asprintf(&path, "%s/%s", homedir, ".afccl") == -1) {
			warn("%s: %s", __func__, "asprintf");
		}
		read_history(path);
	}
	else {
		warnx("HOME environment variable not found");
	}
	return path;
}

void append_history(char *path, char *cmd)
{
	if (!path || !cmd) return;

	add_history(cmd);
	write_history(path);
}

static int cmd_loop(int in_argc, const char *in_argv[])
{
	int argc;
	char *input, **argv;
	int quit = 0;
	char *history_path;

	history_path = load_history();

	while (!quit) {

		input = readline("> ");
		append_history(history_path, input);

		if ((tokenize_command_line(input, &argc, &argv) == -1) || argc < 1) {
			free(input);
			continue;
		}

		int cmd = str_to_cmd(argv[0]);
		if (cmd == CMD_UNKNOWN || CMD_INTERACTIVE) {
			warnx("'%s': unknown command", argv[0]);
			free(argv);
			free(input);
			continue;
		}

		do_cmd(cmd, --argc, (const char **) ++argv);

		free(--argv);
		free(input);
	}

	return AFC_E_SUCCESS;
}

static int do_cmd(int cmd, int argc, const char *argv[])
{
	switch (cmd) {
		case CMD_INTERACTIVE:
		case CMD_UNKNOWN:
			return cmd_loop(argc, argv);
			break;
		case CMD_CD:
			return cmd_cd(argv[0]);
			break;
		case CMD_PWD:
			return cmd_pwd();
			break;
		case CMD_LS:
			return cmd_ls(argc, argv);
			break;
		case CMD_MKDIR:
			return cmd_mkdir(argc, argv);
			break;
		case CMD_LN:
			return cmd_ln(argc, argv);
			break;
		case CMD_RM:
			return cmd_rm(argc, argv);
			break;
		case CMD_MV:
			return cmd_mv(argc, argv);
			break;
		case CMD_CP:
			return cmd_cp(argc, argv);
			break;
		case CMD_CAT:
			return cmd_cat(argc, argv);
			break;
		case CMD_STAT:
			return cmd_stat(argc, argv);
			break;
		case CMD_QUIT:
			exit(EXIT_SUCCESS);
			break;
		default:
			return -1;
			break;
	}
}

static void print_usage(int argc, const char **argv)
{
	char *name = NULL;

	// TODO: document commands and options
	// 		see usage for idevicebackup2 for a complex example with commands that take options

	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS] [<cmd> [CMDOPTIONS]]\n", (name ? name + 1: argv[0]));
	printf("AFC command line utility.\n\n");
	printf("  -d, --debug\t\tenable communication debugging\n");
	printf("  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
	printf("  -2, --afc2\t\tconnect to afc2 service\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
}

int main(int argc, const char **argv)
{
	idevice_t device = NULL;
	lockdownd_client_t client = NULL;
	lockdownd_service_descriptor_t service = NULL;
	const char *service_name = "com.apple.afc";
	char *device_name = NULL;
	int result = 0;
	char* udid = NULL;
	int cmd = CMD_INTERACTIVE;
	const char *cmdstr = NULL;
	int i;

	cwd = strdup("/");

	/* parse cmdline args */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			idevice_set_debug_level(1);
			continue;
		}
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) != 40)) {
				print_usage(argc, argv);
				exit(EXIT_FAILURE);
			}
			udid = strdup(argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-2") || !strcmp(argv[i], "--afc2")) {
			service_name = "com.apple.afc2";
			continue;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage(argc, argv);
			exit(EXIT_SUCCESS);
		}
		else if ((cmd = str_to_cmd(argv[i])) != CMD_UNKNOWN) {
			cmdstr = argv[i];
			i++;
			break;
		}
	}
	argc -= i;
	argv += i;

	/* Connect to device */
	if (udid) {
		result = idevice_new(&device, udid);
		if (result != IDEVICE_E_SUCCESS)
			errx(EXIT_FAILURE, "No device found with udid %s, is it plugged in?", udid);
	}
	else {
		result = idevice_new(&device, NULL);
		if (result != IDEVICE_E_SUCCESS)
			errx(EXIT_FAILURE, "No device found, is it plugged in?");
		idevice_get_udid(device, &udid);
	}

	/* Connect to lockdownd */
	result = lockdownd_client_new_with_handshake(device, &client, "afccl");
	if (result != LOCKDOWN_E_SUCCESS) {
		idevice_free(device);
		errx(EXIT_FAILURE, "ERROR: Connecting to lockdownd service failed!");
	}

	result = lockdownd_get_device_name(client, &device_name);
	if ((result != LOCKDOWN_E_SUCCESS) || !device_name) {
		lockdownd_client_free(client);
		idevice_free(device);
		errx(EXIT_FAILURE, "ERROR: Could not get device name!");
	}

	result = lockdownd_start_service(client, service_name, &service);
	if (result != LOCKDOWN_E_SUCCESS || !service || !service->port) {
		lockdownd_client_free(client);
		idevice_free(device);
		errx(EXIT_FAILURE, "error starting AFC service: (%d) %s", result, afc_strerror(result));
	}

	/* Connect to AFC */
	result = afc_client_new(device, service, &afc);
	lockdownd_client_free(client);
	idevice_free(device);
	if (result != AFC_E_SUCCESS) {
		errx(EXIT_FAILURE, "AFC connection failed (%d) %s", result, afc_strerror(result));
	}

	result = do_cmd(cmd, argc, argv);

	afc_client_free(afc);

	exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
