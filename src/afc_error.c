//
//  afc_error.c
//  libimobiledevice
//
//  Copyright (c) 2014 Aaron Burghardt. All rights reserved.
//

#include <err.h>
#include <stdio.h>
#include <sys/errno.h>
#include "afc.h"

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

afc_error_t posix_err_to_afc_error(int err)
{
	switch (err) {
		case EPERM:      return AFC_E_PERM_DENIED;
		case ENOMEM:     return AFC_E_NO_MEM;
		case ENOENT:     return AFC_E_OBJECT_NOT_FOUND;
		case EIO:        return AFC_E_IO_ERROR;
		case EACCES:     return AFC_E_PERM_DENIED;
		case EEXIST:     return AFC_E_OBJECT_EXISTS;
		case EISDIR:     return AFC_E_OBJECT_IS_DIR;
		case ENOTDIR:    return AFC_E_OBJECT_EXISTS;
	}
	return AFC_E_UNKNOWN_ERROR;
}

void afc_warn(afc_error_t err, const char *fmt, ...)
{
	va_list ap;
	
	char *newfmt;
	asprintf(&newfmt, "%s: %s (%d)", fmt, afc_strerror(err), err);
	
	va_start(ap, fmt);
	
	vwarnx(newfmt, ap);
	
	va_end(ap);
	
	free(newfmt);
}

