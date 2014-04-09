//
//  afc_extras.c
//  libimobiledevice
//
//  Copyright (c) 2014 Aaron Burghardt. All rights reserved.
//

#include <stdio.h>
#include "afc.h"
#include "afc_extras.h"

afc_error_t afc_stat(afc_client_t client, const char *path, struct afc_stat *st_buf)
{
	afc_error_t result;
	char **values = NULL;
	int64_t time;
	
	result = afc_get_file_info(client, path, &values);
	if (result != AFC_E_SUCCESS)
		return result;
	
	for (int i = 0; values[i] != NULL; i += 2) {
		
		if (strcmp(values[i], "st_size") == 0) {
			st_buf->st_size = atoll(values[i+1]);
		}
		else if (strcmp(values[i], "st_blocks") == 0) {
			st_buf->st_blocks = atol(values[i+1]);
		}
		else if (strcmp(values[i], "st_nlink") == 0) {
			st_buf->st_nlink = (short) atoi(values[i+1]);
		}
		else if (strcmp(values[i], "st_ifmt") == 0) {
			
			if (strcmp(values[i+1], "S_IFREG") == 0)
				st_buf->st_ifmt = S_IFREG;
			else if (strcmp(values[i+1], "S_IFDIR") == 0)
				st_buf->st_ifmt = S_IFDIR;
			else if (strcmp(values[i+1], "S_IFBLK") == 0)
				st_buf->st_ifmt = S_IFBLK;
			else if (strcmp(values[i+1], "S_IFCHR") == 0)
				st_buf->st_ifmt = S_IFCHR;
			else if (strcmp(values[i+1], "S_IFIFO") == 0)
				st_buf->st_ifmt = S_IFIFO;
			else if (strcmp(values[i+1], "S_IFLNK") == 0)
				st_buf->st_ifmt = S_IFLNK;
			else if (strcmp(values[i+1], "S_IFSOCK") == 0)
				st_buf->st_ifmt = S_IFSOCK;
		}
		else if (strcmp(values[i], "st_mtime") == 0) {
			time = atoll(values[i+1]) / 1000000000;
			st_buf->st_modtime = time;
		}
		else if (strcmp(values[i], "st_birthtime") == 0) {
			time = atoll(values[i+1]) / 1000000000;
			st_buf->st_createtime = time;
		}
		else if (strcmp(values[i], "LinkTarget") == 0) {
			// ignored for now
		}
		else
			fprintf(stderr, "unknown stat key/value: %s: %s\n", values[i], values[i+1]);
		
		free(values[i]);
		free(values[i+1]);
	}
	free(values);
	
	return AFC_E_SUCCESS;
}

static void afc_fts_entry_free(afc_ftsent_t entry)
{
	if (entry) {
		free(entry->path);
		free(entry->name);
		free(entry->statp);
		free(entry);
	}
}

static afc_ftsent_t afc_fts_entry_create(afc_fts_t fts, afc_ftsent_t parent, const char *name)
{
	afc_error_t result = AFC_E_SUCCESS;
	
	afc_ftsent_t self = calloc(1, sizeof(struct afc_ftsent));
	if (self) {
		self->name = strdup(name);
		if (self->name) {
			self->namelen = strlen(self->name);
			
			if (parent) {
				if (parent->path[parent->pathlen - 1] == '/')
					asprintf(&self->path, "%s%s", parent->path, name);
				else
					asprintf(&self->path, "%s/%s", parent->path, name);
				self->level = parent->level + 1;
			}
			else {
				self->path = strdup(self->name);
				if (self->path)
					self->pathlen = strlen(self->path);
				self->level = 0;
			}
		}
		self->accpath = (fts->options & AFC_FTS_NOCHDIR) ? self->path : self->name;
		self->parent = parent;
		self->statp = calloc(1, sizeof(struct afc_stat));
		if (self->statp)
			result = afc_stat(fts->client, self->path, self->statp);

		// verify malloc'ed objects
		if (!self->accpath || !self->path || !self->name || !self->statp) {
			afc_fts_entry_free(self);
			if (parent)
				parent->afc_errno = (result == AFC_E_SUCCESS) ? AFC_E_UNKNOWN_ERROR : result;
			return NULL;
		}
		
		// set 'info'
		if ((strcmp(name, ".") == 0 || strcmp(name, "..") == 0))
			self->info = AFC_FTS_DOT;
		
		else if (S_ISREG(self->statp->st_ifmt))
			self->info = AFC_FTS_F;
		else if (S_ISLNK(self->statp->st_ifmt))
			self->info = AFC_FTS_SL;
		else if (S_ISDIR(self->statp->st_ifmt))
			self->info = AFC_FTS_D;
	}
	if (parent)
		parent->afc_errno = result;
	return self;
}

static afc_error_t _afc_fts_enumerate_entry(afc_fts_t fts, afc_ftsent_t parent)
{
	afc_error_t result = AFC_E_SUCCESS;
	afc_ftsent_t child;
	bool stop = false;
	char **list = NULL;
	
	if (S_ISDIR(parent->statp->st_ifmt)) {
		
		parent->info = AFC_FTS_D;
		if ((result = fts->callback(parent, &stop, fts->user_context)) != AFC_E_SUCCESS)
			return result;
		if (stop) return AFC_E_SUCCESS;
		
		result = afc_read_directory(fts->client, parent->path, &list);
		if (result != AFC_E_SUCCESS) {
			afc_warn(result, "%s", parent->path);
			return result;
		}
		
		for (int i = 0; list[i] != NULL; i++) {
			
			if ((fts->options & AFC_FTS_SEEDOT) == 0 &&
				(strcmp(list[i], ".") == 0 ||
				 strcmp(list[i], "..") == 0))
				continue;
			
			// check 'result' and 'stop' here instead of the for() statement
			// to allow the loop to free all memory even if we are aborting
			if (parent->afc_errno == AFC_E_SUCCESS || stop == false) {

				child = afc_fts_entry_create(fts, parent, list[i]);
				
				if (child && parent->afc_errno == AFC_E_SUCCESS)
					result = _afc_fts_enumerate_entry(fts, child);
				afc_fts_entry_free(child);

			}
			free(list[i]);
		}
		free(list);

		parent->info = AFC_FTS_DP;
		result = fts->callback(parent, &stop, fts->user_context);
	}
	else {
		result = fts->callback(parent, &stop, fts->user_context);
		if (stop && result == AFC_E_SUCCESS)
			result = AFC_E_OP_INTERRUPTED;
		parent->afc_errno = result;
	}

	return result;
}

afc_error_t afc_fts_enumerate_path(afc_client_t client, char *path, int options, afc_fts_enumerator_callback_t callback, void *context)
{
	afc_error_t result;
	struct afc_fts fts;
	afc_ftsent_t root;
	
	memset(&fts, 0, sizeof(struct afc_fts));
	fts.client = client;
	fts.options = options;
	fts.root_path = path;
	fts.user_context = context;
	fts.callback = callback;
	
	root = afc_fts_entry_create(&fts, NULL, path);
	if (!root)
		return AFC_E_UNKNOWN_ERROR;

	result = _afc_fts_enumerate_entry(&fts, root);
	afc_fts_entry_free(root);
	
	return result;
}
