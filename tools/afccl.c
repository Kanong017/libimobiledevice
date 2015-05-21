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

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "afc_extras.h"

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

afc_error_t posix_err_to_afc_error(int err);

static inline bool str_is_equal(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

static inline bool str_has_prefix(const char *str, const char *prefix)
{
	return strstr(str, prefix) == str;
}

static int do_cmd(int cmd, int argc, const char *argv[]);

static char *build_absolute_path(const char *inpath)
{
	char *path;

	if (inpath[0] == '/')
		path = strdup(inpath);
	else {
		if (asprintf(&path, "%s/%s", str_is_equal(cwd, "/") ? "" : cwd, inpath) == -1) {
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
		if (token[0] == 0 || str_is_equal(token, ".")) {
			continue;
		}
		else if (str_is_equal(token, "..")) {
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

	if (argc == 3 && str_is_equal("-s", argv[0])) {
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

static void infolist_free(char **infolist)
{
	for (int j = 0; infolist[j] != NULL; j++)
		free(infolist[j]);

	free(infolist);
}

static char * infolist_get_value(char **infolist, char *property)
{
	char *value = NULL;

	for (int j = 0; infolist[j] != NULL; j += 2)
	{
		if (str_is_equal(property, infolist[j])) {
			value = infolist[j+1];
			break;
		}
	}
	return value;
}


static afc_error_t is_directory(char *path, bool *is_dir)
{
	afc_error_t result;
	char **infolist = NULL;
	char *value;

	result = afc_get_file_info(afc, path, &infolist);

	if (result != AFC_E_SUCCESS) {
		if (result != AFC_E_OBJECT_NOT_FOUND)
			afc_warn(result, "%s: stat failed: %s", __func__, path);
		return result;
	}

	if ((value = infolist_get_value(infolist, "st_ifmt")))
		*is_dir = str_is_equal(value, "S_IFDIR");

	infolist_free(infolist);

	return result;
}

static afc_error_t remove_path(char *path, bool recurse)
{
	afc_error_t result;
	char **infolist = NULL;
	bool is_dir;

	result = afc_get_file_info(afc, path, &infolist);

	if (result != AFC_E_SUCCESS) {
		if (result != AFC_E_OBJECT_NOT_FOUND)
			afc_warn(result, "%s: stat failed: %s", __func__, path);
		return result;
	}

	if (recurse && is_directory(path, &is_dir) == AFC_E_SUCCESS && is_dir) {
		char **list = NULL;

		result = afc_read_directory(afc, path, &list);
		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", path);
			return result;
		}

		for (int i = 0; list[i] && result == AFC_E_SUCCESS; i++) {
			if (str_is_equal(list[i], "."))
				; // NOP
			else if (str_is_equal(list[i], ".."))
				; // NOP
			else {
				char *subdir_path;
				if (asprintf(&subdir_path, "%s/%s", path, list[i]) > 0) {
					result = remove_path(subdir_path, recurse);
					free(subdir_path);
				}
				else {
					warn("%s: %s", __func__, "asprintf");
				}
			}
			free(list[i]);
		}
		free(list);
	}

	if (recurse)
		printf("removing: %s\n", path);

	result = afc_remove_path(afc, path);

	if (result != AFC_E_SUCCESS)
		afc_warn(result, "%s", path);

	return result;
}


static afc_error_t cmd_rm(int argc, const char *argv[])
{
	afc_error_t result;
	char *path = NULL;
	bool recurse = false;

	if (argc < 1) {
		warnx("usage: rm [-r] <file> ...");
		return AFC_E_INVALID_ARG;
	}

	if (argc > 1 && (recurse = str_is_equal("-r", argv[0]))) {
		argc--;
		argv++;
	}

	for (int i = 0; i < argc; i++) {

		path = build_absolute_path(argv[i]);
		if (!path)
			return AFC_E_INTERNAL_ERROR;

		result = remove_path(path, recurse);
		free(path);
		if (result == AFC_E_OBJECT_NOT_FOUND)
			afc_warn(result, "%s", argv[i]);
		return  result;
	}

	return AFC_E_SUCCESS;
}

static afc_error_t cmd_mv(int argc, const char *argv[])
{
	afc_error_t result;
	const char *filename;
	char *source_path, *target_path;
	char **infolist;
	bool target_is_dir;

	if (argc != 2) {
		warnx("usage: mv <source> <target>");
		return -1;
	}
	source_path = build_absolute_path(argv[0]);
	target_path = build_absolute_path(argv[1]);

	if (source_path && target_path) {

		result = afc_get_file_info(afc, target_path, &infolist);
		if (result == AFC_E_SUCCESS) {
			for (int i = 0; infolist[i] != NULL; i++) {
				if (str_is_equal(infolist[i], "st_ifmt"))
					target_is_dir = str_is_equal(infolist[i+1], "S_IFDIR");
				free(infolist[i]);
			}
			free(infolist);

			if (target_is_dir) {
				filename = basename((char *) argv[0]);
				target_path = realloc(target_path, strlen(filename) + 2);
				strcat(target_path, "/");
				strcat(target_path, filename);
				result = AFC_E_SUCCESS;
			}
		}
		else if (result == AFC_E_OBJECT_NOT_FOUND) {
			result = AFC_E_SUCCESS;
		}

		if (result == AFC_E_SUCCESS) {
			result = afc_rename_path(afc, source_path, target_path);
			if (result != AFC_E_SUCCESS)
				afc_warn(result, "rename %s", argv[0]);
		}
	}
	else {
		result = AFC_E_INTERNAL_ERROR;
	}
	free(source_path);
	free(target_path);

	return result;
}

static afc_error_t cmd_stat(int argc, const char *argv[])
{
	afc_error_t result;
	struct afc_stat st_buf;
	char *path;
	char *ifmt;
	time_t atime;
	char *modtime, *createtime;
	
	if (argc < 1) {
		warnx("usage: stat <file> ...");
		return -1;
	}

	for (int i = 0; i < argc; i++) {

		path = build_absolute_path(argv[0]);
		result = afc_stat(afc, path, &st_buf);
		free(path);

		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", argv[i]);
			return result;
		}
		if      (S_ISREG(st_buf.st_ifmt))  ifmt = "S_IFREG";
		else if (S_ISDIR(st_buf.st_ifmt))  ifmt = "S_IFDIR";
		else if (S_ISLNK(st_buf.st_ifmt))  ifmt = "S_IFLNK";
		else if (S_ISBLK(st_buf.st_ifmt))  ifmt = "S_IFBLK";
		else if (S_ISCHR(st_buf.st_ifmt))  ifmt = "S_IFCHR";
		else if (S_ISFIFO(st_buf.st_ifmt)) ifmt = "S_IFIFO";
		else if (S_ISSOCK(st_buf.st_ifmt)) ifmt = "S_IFSOCK";
		
		// time_t may be 32 or 64 bits
		atime = (time_t) st_buf.st_modtime;
		modtime = strdup(ctime(&atime));
		atime = (time_t) st_buf.st_createtime;
		createtime = strdup(ctime(&atime));
		
		printf("%s:\n", argv[i]);
		printf("%14s %lld\n",  "st_size",      st_buf.st_size);
		printf("%14s %lld\n",  "st_blocks",    st_buf.st_blocks);
		printf("%14s %hd\n",   "st_nlink",     st_buf.st_nlink);
		printf("%14s %s\n",    "st_ifmt",      ifmt);
		printf("%14s %u - %s", "st_mtime",     st_buf.st_modtime, modtime);
		printf("%14s %u - %s", "st_birthtime", st_buf.st_createtime, createtime);
		putc('\n', stdout);
		
		free(modtime);
		free(createtime);
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
	warnx("%s not implemented", __func__);
	return AFC_E_INVALID_ARG;
}

static int str_to_cmd(const char *str)
{
	if (str_is_equal(str, "-"))
		return CMD_INTERACTIVE;
	else if (str_is_equal(str, "cd"))
		return CMD_CD;
	else if (str_is_equal(str, "pwd"))
		return CMD_PWD;
	else if (str_is_equal(str, "ls"))
		return CMD_LS;
	else if (str_is_equal(str, "mkdir"))
		return CMD_MKDIR;
	else if (str_is_equal(str, "ln"))
		return CMD_LN;
	else if (str_is_equal(str, "rm"))
		return CMD_RM;
	else if (str_is_equal(str, "mv"))
		return CMD_MV;
	else if (str_is_equal(str, "cp"))
		return CMD_CP;
	else if (str_is_equal(str, "cat"))
		return CMD_CAT;
	else if (str_is_equal(str, "stat"))
		return CMD_STAT;
	else if (str_is_equal(str, "quit"))
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
    printf("  -a, --appid APPID\tconnect via house_arrest to the app with bundle ID APPID\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
}

int main(int argc, const char **argv)
{
	char *errmsg = "";
	idevice_t device = NULL;
	lockdownd_client_t client = NULL;
	lockdownd_service_descriptor_t service = NULL;
	house_arrest_client_t hac = NULL;
	const char *service_name = "com.apple.afc";
    const char *appid = NULL;
	char *device_name = NULL;
	int result = 0;
	char* udid = NULL;
	int cmd = CMD_INTERACTIVE;
	const char *cmdstr = NULL;
	int i;

	cwd = strdup("/");

	/* parse cmdline args */
	for (i = 1; i < argc; i++) {
		if (str_is_equal(argv[i], "-d") || str_is_equal(argv[i], "--debug")) {
			idevice_set_debug_level(1);
			continue;
		}
		else if (str_is_equal(argv[i], "-u") || str_is_equal(argv[i], "--udid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) != 40)) {
				print_usage(argc, argv);
				exit(EXIT_FAILURE);
			}
			udid = strdup(argv[i]);
			continue;
		}
		else if (str_is_equal(argv[i], "-2") || str_is_equal(argv[i], "--afc2")) {
			service_name = "com.apple.afc2";
			continue;
		}
        else if (str_is_equal(argv[i], "-a") || str_is_equal(argv[i], "--appid")) {
            if (++i >=  argc) {
                print_usage(argc, argv);
                exit(EXIT_FAILURE);
            }
            appid = argv[i];
        }
		else if (str_is_equal(argv[i], "-h") || str_is_equal(argv[i], "--help")) {
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
		asprintf(&errmsg, "ERROR: Connecting to lockdownd service failed!");
		goto bail;
	}

	result = lockdownd_get_device_name(client, &device_name);
	if ((result != LOCKDOWN_E_SUCCESS) || !device_name) {
		asprintf(&errmsg, "ERROR: Could not get device name!");
		goto bail;
	}

    if (appid) {
        result = lockdownd_start_service(client, "com.apple.mobile.house_arrest", &service);
        if (result != LOCKDOWN_E_SUCCESS || !service || !service->port) {
			asprintf(&errmsg, "error starting house arrest service: (%d) %s", result, afc_strerror(result));
			goto bail;
        }
        if (client) {
            lockdownd_client_free(client);
            client = NULL;
        }
        
        if (house_arrest_client_new(device, service, &hac) != HOUSE_ARREST_E_SUCCESS) {
            asprintf(&errmsg, "could not connect to house_arrest service!\n");
			goto bail;
        }
        
        if (service) {
            lockdownd_service_descriptor_free(service);
            service = NULL;
        }
        
        result = house_arrest_send_command(hac, "VendDocuments", appid);
//		result = house_arrest_send_command(hac, "VendContainer", appid);
        if (result != HOUSE_ARREST_E_SUCCESS) {
            asprintf(&errmsg, "error %d when trying to get VendContainer\n", result);
			goto bail;
        }
        
        plist_t dict = NULL;
        if (house_arrest_get_result(hac, &dict) != HOUSE_ARREST_E_SUCCESS) {
            if (house_arrest_get_result(hac, &dict) != HOUSE_ARREST_E_SUCCESS) {
                asprintf(&errmsg, "hmmm....\n");
				goto bail;
            }
        }
        
        plist_t node = plist_dict_get_item(dict, "Error");
        if (node) {
            char *str = NULL;
            plist_get_string_val(node, &str);
            asprintf(&errmsg, "Error: %s\n", str);
            if (str) free(str);
            plist_free(dict);
            dict = NULL;
			goto bail;
		}
        node = plist_dict_get_item(dict, "Status");
        if (node) {
            char *str = NULL;
            plist_get_string_val(node, &str);
            if (str && (strcmp(str, "Complete") != 0)) {
                printf("Warning: Status is not 'Complete' but '%s'\n", str);
            }
            if (str) free(str);
        }
        if (dict) {
            plist_free(dict);
        }
        
        afc_error_t ae = afc_client_new_from_house_arrest_client(hac, &afc);
        if (ae != AFC_E_SUCCESS) {
            printf("afc error %d\n", ae);
        }

    }
    else {
        result = lockdownd_start_service(client, service_name, &service);
        if (result != LOCKDOWN_E_SUCCESS || !service || !service->port) {
            asprintf(&errmsg, "error starting AFC service: (%d) %s", result, afc_strerror(result));
			goto bail;
        }

        /* Connect to AFC */
        result = afc_client_new(device, service, &afc);
        lockdownd_client_free(client);
        idevice_free(device);
        if (result != AFC_E_SUCCESS) {
            errx(EXIT_FAILURE, "AFC connection failed (%d) %s", result, afc_strerror(result));
        }
	}
	result = do_cmd(cmd, argc, argv);

	if (hac)
		house_arrest_client_free(hac);

	afc_client_free(afc);

	exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    
bail:
    if (hac)
		house_arrest_client_free(hac);

	if (service)
		lockdownd_service_descriptor_free(service);

    if (client)
		lockdownd_client_free(client);

    if (device)
		idevice_free(device);

	errx(EXIT_FAILURE, "%s", errmsg);
}
