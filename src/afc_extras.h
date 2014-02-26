//
//  afc_fts.h
//  libimobiledevice
//
//  Copyright (c) 2014 Aaron Burghardt. All rights reserved.
//

#ifndef AFC_FTS_H
#define AFC_FTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
	
	/**
	 */
struct afc_stat {
	// OS X has a struct timespec which includes fractions of seconds, but fractions are ignored here.
	// (There is no fraction from HFS.)
	
	off_t    st_size;        /**< File size in bytes */
	off_t    st_blocks;      /**< File system blocks allocated */
	int16_t  st_nlink;       /**< Number of links. */
	mode_t   st_ifmt;        /**< The file type from the mode value */
	uint32_t st_modtime;     /**< Modified time */
	uint32_t st_createtime;  /**< Creation time */
};

afc_error_t afc_stat(afc_client_t client, const char *path, struct afc_stat *st_buf);
	
	/**
	 */
struct afc_fts_entry {
	uint16_t info;                ///< A descriptor about the file entry
	char    *accpath;             ///< A path for accessing the file from the current directory.
	char    *path;                ///< The path for the file relative to the root of the traversal.  Contains the initial starting path as a prefix.
	uint16_t pathlen;             ///< strlen(path)
	char    *name;                ///< The file name.
	uint16_t namelen;             ///< strlen(name)
	int16_t  level;               ///< The depth of traversal. 0 for the root entry.
	afc_error_t afc_errno;        ///< AFC error of the last call related to the entry (either directly or if a child entry failed to intialize).
	struct afc_fts_entry *parent; ///< Weak reference to the parent entry, or NULL.
	struct afc_stat *statp;       ///< Pointer to afc_stat information for the file.
};
typedef struct afc_fts_entry *afc_fts_entry_t;


/**
 * Callback called once for each file or twice for each directory found during enumeration.
 * @param entry a structure describing the current node
 * @param context user-passed context info
 * @param stop set to true to stop enumerating
 * @return
 */
typedef afc_error_t (*afc_fts_enumerator_callback_t)(afc_fts_entry_t entry, bool *stop, void *context);
	
typedef struct afc_fts {
	afc_client_t client;
	int options;
	char *root_path;
	void *user_context;
	afc_fts_enumerator_callback_t callback;
} *afc_fts_t;
	

/*
 * The AFC interface does not have the concept of a current working directory, so the path
 * must be an absolute path.
 * @param client an open AFC connection
 * @param path the root path; must be an absolute path
 * @param options FTS_NOCHDIR required, FTS_SEEDOT supported.
 * @param callback called once for each file, twice for directories (pre-order and post-order)
 * @param context user info, which is provided to the callback
 * @return Returns a AFC_E_SUCCESS upon completion or in the event of the enumeration being stopped by the callback.
 * Otherwise, the enumeration is aborted and an AFC error returned.
 */
afc_error_t afc_fts_enumerate_path(afc_client_t client, char *path, int options, afc_fts_enumerator_callback_t callback, void *context);


	
#ifdef __cplusplus
}
#endif

#endif
