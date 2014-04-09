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
#include <sys/param.h>

/** AFC stat structure
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
	char     st_linktarget[MAXPATHLEN]; /**< The target of a symbolic link */
};

afc_error_t afc_stat(afc_client_t client, const char *path, struct afc_stat *st_buf);
	
#pragma mark - AFC FTS Entry
	
/** AFC FTS Entry Info
 */
typedef enum afc_ftsent_info {
	AFC_FTS_D		=  1,		/**< preorder directory */
	AFC_FTS_DC		=  2,		/**< directory that causes cycles */
	AFC_FTS_DEFAULT	=  3,		/**< none of the above */
	AFC_FTS_DNR		=  4,		/**< unreadable directory */
	AFC_FTS_DOT		=  5,		/**< dot or dot-dot */
	AFC_FTS_DP		=  6,		/**< postorder directory */
	AFC_FTS_ERR		=  7,		/**< error; errno is set */
	AFC_FTS_F		=  8,		/**< regular file */
	AFC_FTS_INIT	=  9,		/**< initialized only */
	AFC_FTS_NS		= 10,		/**< stat(2) failed */
	AFC_FTS_NSOK	= 11,		/**< no stat(2) requested */
	AFC_FTS_SL		= 12,		/**< symbolic link */
	AFC_FTS_SLNONE	= 13,		/**< symbolic link without target */
	AFC_FTS_W		= 14		/**< whiteout object */
} afc_ftsent_info_t;

/** AFC FTS Entry Internal Flags
 * @discussion Private flags for FTSENT structure
 */
//typedef enum afc_ftsent_flags {
//	AFC_FTS_DONTCHDIR   = 0x01,      /**< don't chdir .. to the parent */
//	AFC_FTS_SYMFOLLOW   = 0x02,      /**< followed a symlink to get here */
//	AFC_FTS_ISW         = 0x04       /**< this is a whiteout object */
//} afc_ftsent_flags_t;

/** AFC FTSENT Instructions
 */
//typedef enum afc_ftsent_instr {
//	AFC_FTS_AGAIN	= 1,	/**< read node again */
//	AFC_FTS_FOLLOW	= 2,	/**< follow symbolic link */
//	AFC_FTS_NOINSTR	= 3,	/**< no instructions */
//	AFC_FTS_SKIP	= 4 	/**< discard node */
//} afc_ftsent_instr_t;	/* fts_set() instructions */

/** AFC FTS Entry
 */
struct afc_ftsent {
	afc_ftsent_info_t info;       ///< A descriptor about the file entry
	char    *accpath;             ///< A path for accessing the file from the current directory.
	char    *path;                ///< The path for the file relative to the root of the traversal.  Contains the initial starting path as a prefix.
	uint16_t pathlen;             ///< strlen(path)
	char    *name;                ///< The file name.
	uint16_t namelen;             ///< strlen(name)
	int16_t  level;               ///< The depth of traversal. 0 for the root entry.
//	afc_ftsent_flags_t flags;     ///< AFC internal flags (not implemented)
//	afc_ftsent_instr_t instr;     ///< entry instructions (not implemented)
	afc_error_t afc_errno;        ///< AFC error of the last call related to the entry (either directly or if a child entry failed to intialize).
	struct afc_ftsent *parent; ///< Weak reference to the parent entry, or NULL.
	struct afc_stat *statp;       ///< Pointer to afc_stat information for the file.
};
typedef struct afc_ftsent *afc_ftsent_t;

#pragma mark - AFC FTS

/**
 * Callback called once for each file or twice for each directory found during enumeration.
 * @param entry a structure describing the current node
 * @param context user-passed context info
 * @param stop set to true to stop enumerating
 * @return
 */
typedef afc_error_t (*afc_fts_enumerator_callback_t)(afc_ftsent_t entry, bool *stop, void *context);
	

/** AFC FTS Options
 */
typedef enum afc_fts_options {
	AFC_FTS_NOCHDIR	= 0x004,		/* don't change directories */
	AFC_FTS_SEEDOT	= 0x020		/* return dot and dot-dot */
} afc_fts_options_t;

struct afc_fts {
	afc_client_t client;
	afc_fts_options_t options;
	char *root_path;
	void *user_context;
	afc_fts_enumerator_callback_t callback;
};
typedef struct afc_fts * afc_fts_t;
	
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
