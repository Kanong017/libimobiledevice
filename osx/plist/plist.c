/*
	usbmuxd - iPhone/iPod Touch USB multiplex server daemon

Copyright (C) 2009	Hector Martin "marcan" <hector@marcansoft.com>
Copyright (C) 2009	Nikias Bassen <nikias@gmx.li>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
#include "plist.h"
//#include "utils.h"

#ifdef USBMUXD_DAEMON
# include "log.h"
# define util_error(...) usbmuxd_log(LL_ERROR, __VA_ARGS__)
#else
# define util_error(...) fprintf(stderr, __VA_ARGS__)
#endif

#include <CoreFoundation/CoreFoundation.h>

/********************************************
 *                                          *
 *          Creation & Destruction          *
 *                                          *
 ********************************************/

/**
 * Create a new root plist_t type #PLIST_DICT
 *
 * @return the created plist
 * @sa #plist_type
 */
plist_t plist_new_dict(void)
{
	return CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

/**
 * Create a new root plist_t type #PLIST_ARRAY
 *
 * @return the created plist
 * @sa #plist_type
 */
plist_t plist_new_array(void)
{
	return CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
}

/**
 * Create a new plist_t type #PLIST_STRING
 *
 * @param val the sting value, encoded in UTF8.
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_string(const char *val)
{
	return CFStringCreateWithCString(kCFAllocatorDefault, val, kCFStringEncodingUTF8);
}

/**
 * Create a new plist_t type #PLIST_BOOLEAN
 *
 * @param val the boolean value, 0 is false, other values are true.
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_bool(uint8_t val)
{
	return val ? kCFBooleanTrue : kCFBooleanFalse;
}

/**
 * Create a new plist_t type #PLIST_UINT
 *
 * @param val the unsigned integer value
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_uint(uint64_t val)
{
	return CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &val);
}

/**
 * Create a new plist_t type #PLIST_REAL
 *
 * @param val the real value
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_real(double val)
{
	return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &val);
}

/**
 * Create a new plist_t type #PLIST_DATA
 *
 * @param val the binary buffer
 * @param length the length of the buffer
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_data(const char *val, uint64_t length)
{
	return CFDataCreate(kCFAllocatorDefault, (UInt8 *)val, length);
}

/**
 * Create a new plist_t type #PLIST_DATE
 *
 * @param sec the number of seconds since 01/01/2001
 * @param usec the number of microseconds
 * @return the created item
 * @sa #plist_type
 */
plist_t plist_new_date(int32_t sec, int32_t usec)
{
//	typedef double CFTimeInterval;
//	typedef CFTimeInterval CFAbsoluteTime;
//	/* absolute time is the time interval since the reference date */
//	/* the reference date (epoch) is 00:00:00 1 January 2001. */

	CFAbsoluteTime abstime = (CFAbsoluteTime) sec + (CFAbsoluteTime) usec / 100000.0;
	abstime -= 978307200.0;

	return CFDateCreate(kCFAllocatorDefault, abstime);
}

/**
 * Destruct a plist_t node and all its children recursively
 *
 * @param plist the plist to free
 */
void plist_free(plist_t plist)
{
	if (plist)
		CFRelease(plist);
}

/**
 * Return a copy of passed node and it's children
 *
 * @param node the plist to copy
 * @return copied plist
 */
plist_t plist_copy(plist_t node)
{
	return CFPropertyListCreateDeepCopy(kCFAllocatorDefault, node, kCFPropertyListMutableContainers);
}


/********************************************
 *                                          *
 *            Array functions               *
 *                                          *
 ********************************************/

/**
 * Get size of a #PLIST_ARRAY node.
 *
 * @param node the node of type #PLIST_ARRAY
 * @return size of the #PLIST_ARRAY node
 */
uint32_t plist_array_get_size(plist_t node)
{
	return CFArrayGetCount(node);
}

/**
 * Get the nth item in a #PLIST_ARRAY node.
 *
 * @param node the node of type #PLIST_ARRAY
 * @param n the index of the item to get. Range is [0, array_size[
 * @return the nth item or NULL if node is not of type #PLIST_ARRAY
 */
plist_t plist_array_get_item(plist_t node, uint32_t n)
{
	return CFArrayGetValueAtIndex(node, n);
}

/**
 * Get the index of an item. item must be a member of a #PLIST_ARRAY node.
 *
 * @param node the node
 * @return the node index
 */
uint32_t plist_array_get_item_index(plist_t node)
{
//	return CFArrayGetFirstIndexOfValue(CFArrayRef theArray, CFRange range, const void *value)
	assert("Not Implemented" == NULL);
	return UINT32_MAX;
}

/**
 * Set the nth item in a #PLIST_ARRAY node.
 * The previous item at index n will be freed using #plist_free
 *
 * @param node the node of type #PLIST_ARRAY
 * @param item the new item at index n
 * @param n the index of the item to get. Range is [0, array_size[. Assert if n is not in range.
 */
void plist_array_set_item(plist_t node, plist_t item, uint32_t n)
{
	CFArraySetValueAtIndex((CFMutableArrayRef) node, n, item);
	CFRelease(item);
}

/**
 * Append a new item at the end of a #PLIST_ARRAY node.
 *
 * @param node the node of type #PLIST_ARRAY
 * @param item the new item
 */
void plist_array_append_item(plist_t node, plist_t item)
{
	CFArrayAppendValue((CFMutableArrayRef) node, item);
	CFRelease(item);
}

/**
 * Insert a new item at position n in a #PLIST_ARRAY node.
 *
 * @param node the node of type #PLIST_ARRAY
 * @param item the new item to insert
 * @param n The position at which the node will be stored. Range is [0, array_size[. Assert if n is not in range.
 */
void plist_array_insert_item(plist_t node, plist_t item, uint32_t n)
{
	CFArrayInsertValueAtIndex((CFMutableArrayRef) node, n, item);
	CFRelease(item);
}

/**
 * Remove an existing position in a #PLIST_ARRAY node.
 * Removed position will be freed using #plist_free
 *
 * @param node the node of type #PLIST_ARRAY
 * @param n The position to remove. Range is [0, array_size[. Assert if n is not in range.
 */
void plist_array_remove_item(plist_t node, uint32_t n)
{
	CFArrayRemoveValueAtIndex((CFMutableArrayRef) node, n);
}

/********************************************
 *                                          *
 *         Dictionary functions             *
 *                                          *
 ********************************************/

/**
 * Get size of a #PLIST_DICT node.
 *
 * @param node the node of type #PLIST_DICT
 * @return size of the #PLIST_DICT node
 */
uint32_t plist_dict_get_size(plist_t node)
{
	return CFDictionaryGetCount(node);
}

/**
 * Create iterator of a #PLIST_DICT node.
 * The allocated iterator should be freed with standard free function
 *
 * @param node the node of type #PLIST_DICT
 * @param iter iterator of the #PLIST_DICT node
 */
void plist_dict_new_iter(plist_t node, plist_dict_iter *iter)
{
	assert(CFGetTypeID(node) == CFDictionaryGetTypeID());

	struct {
		CFIndex index;
		CFIndex count;
		const void * pairs[][2];
	} *myiter;

	CFIndex count = CFDictionaryGetCount(node);

	myiter = malloc(sizeof(CFIndex) + count * 2 * sizeof(void *));
	myiter->count = count;
	myiter->index = 0;

	const void **keys = malloc(count * sizeof(void *));
	const void **values = malloc(count * sizeof(void *));

	CFDictionaryGetKeysAndValues(node, keys, values);

	for (int i = 0; i < count; i++) {
		myiter->pairs[i][0] = keys[i];
		myiter->pairs[i][1] = values[i];
	}

	free(keys);
	free(values);

	*iter = myiter;
}

/**
 * Increment iterator of a #PLIST_DICT node.
 *
 * @param node the node of type #PLIST_DICT
 * @param iter iterator of the dictionary
 * @param key a location to store the key, or NULL.
 * @param val a location to store the value, or NULL.
 */
void plist_dict_next_item(plist_t node, plist_dict_iter iter, char **key, plist_t *val)
{
	assert(CFGetTypeID(node) == CFDictionaryGetTypeID());

	struct {
		CFIndex index;
		CFIndex count;
		const void * pairs[][2];
	} * myiter;

	myiter = iter;

	if (myiter->index < myiter->count) {
		if (key) {
			CFStringRef keyRef = myiter->pairs[myiter->index][0];
			CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyRef), kCFStringEncodingUTF8);
			char *keyData = malloc(++maxSize);
			CFStringGetCString(keyRef, keyData, maxSize, kCFStringEncodingUTF8);
			*key = keyData;
		}
		if (val)
			*val = myiter->pairs[myiter->index][1];

		myiter->index += 1;
	}
	else {
		if (key) *key = NULL;
		if (val) *val = NULL;
	}
}

/**
 * Get key associated to an item. Item must be member of a dictionary
 *
 * @param node the node
 * @param key a location to store the key.
 */
void plist_dict_get_item_key(plist_t node, char **key)
{
	assert("Not Implemented" == NULL);
}

/**
 * Get the nth item in a #PLIST_DICT node.
 *
 * @param node the node of type #PLIST_DICT
 * @param key the identifier of the item to get.
 * @return the item or NULL if node is not of type #PLIST_DICT
 */
plist_t plist_dict_get_item(plist_t node, const char* key)
{
	CFDictionaryRef retval = NULL;
	
	CFStringRef keyRef = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *)key, strlen(key), kCFStringEncodingUTF8, FALSE, kCFAllocatorNull);
	if (keyRef) {
		retval = CFDictionaryGetValue(node, keyRef);
		CFRelease(keyRef);
	}
	return retval;
}

/**
 * Set item identified by key in a #PLIST_DICT node.
 * The previous item at index n will be freed using #plist_free
 *
 * @param node the node of type #PLIST_DICT
 * @param item the new item associated to key
 * @param key the identifier of the item to get. Assert if identifier is not present.
 */
void plist_dict_set_item(plist_t node, const char* key, plist_t item)
{
	// Do not use "...NoCopy" because the key is added to the node
	CFStringRef keyRef = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8 *)key, strlen(key), kCFStringEncodingUTF8, FALSE);
	if (keyRef) {
		CFDictionarySetValue((CFMutableDictionaryRef) node, keyRef, item);
		CFRelease(keyRef);
		CFRelease(item);
	}
}

/**
 * Insert a new item at position n in a #PLIST_DICT node.
 *
 * @param node the node of type #PLIST_DICT
 * @param item the new item to insert
 * @param key The identifier of the item to insert. Assert if identifier already present.
 */
void plist_dict_insert_item(plist_t node, const char* key, plist_t item)
{
	// Do not use "...NoCopy" because the key is added to the node
	CFStringRef keyRef = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8 *)key, strlen(key), kCFStringEncodingUTF8, FALSE);
	if (keyRef) {
		assert(CFDictionaryGetValue(node, keyRef) == NULL);
		CFDictionarySetValue((CFMutableDictionaryRef) node, keyRef, item);
		CFRelease(keyRef);
		CFRelease(item);
	}
}

/**
 * Remove an existing position in a #PLIST_DICT node.
 * Removed position will be freed using #plist_free
 *
 * @param node the node of type #PLIST_DICT
 * @param key The identifier of the item to remove. Assert if identifier is not present.
 */
void plist_dict_remove_item(plist_t node, const char* key)
{
	CFStringRef keyRef = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *)key, strlen(key), kCFStringEncodingUTF8, FALSE, kCFAllocatorNull);
	if (keyRef) {
		CFDictionaryRemoveValue((CFMutableDictionaryRef) node, keyRef);
		CFRelease(keyRef);
	}
}


/********************************************
 *                                          *
 *                Getters                   *
 *                                          *
 ********************************************/

/**
 * Get the parent of a node
 *
 * @param node the parent (NULL if node is root)
 */
plist_t plist_get_parent(plist_t node)
{
	assert("Not Implemented" == NULL);
	return NULL;
}

/**
 * Get the #plist_type of a node.
 *
 * @param node the node
 * @return the type of the node
 */
plist_type plist_get_node_type(plist_t node)
{
	if (!node)
		return PLIST_NONE;
	
	CFTypeID type = CFGetTypeID(node);

	if (type == CFArrayGetTypeID())
		return PLIST_ARRAY;

	else if (type == CFDictionaryGetTypeID())
		return PLIST_DICT;

	else if (type == CFStringGetTypeID())
		return PLIST_STRING;

	else if (type == CFDataGetTypeID())
		return PLIST_DATA;

	else if (type == CFBooleanGetTypeID())
		return PLIST_BOOLEAN;

	else if (type == CFDateGetTypeID())
		return PLIST_DATE;

	else if (type == CFNumberGetTypeID()) {

		CFTypeID numType = CFNumberGetType(node);

		switch (numType) {
			case kCFNumberFloat32Type:
			case kCFNumberFloat64Type:
			case kCFNumberFloatType:
			case kCFNumberDoubleType:
				return PLIST_REAL;
				break;
			case kCFNumberSInt8Type:
			case kCFNumberSInt16Type:
			case kCFNumberSInt32Type:
			case kCFNumberSInt64Type:
			case kCFNumberCharType:
			case kCFNumberShortType:
			case kCFNumberIntType:
				return PLIST_UINT;
				break;
			default:
				return PLIST_NONE;
				break;
		}
	}
	return PLIST_NONE;
}

/**
 * Get the value of a #PLIST_KEY node.
 * This function does nothing if node is not of type #PLIST_KEY
 *
 * @param node the node
 * @param val a pointer to a C-string. This function allocates the memory,
 *            caller is responsible for freeing it.
 */
void plist_get_key_val(plist_t node, char **val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Get the value of a #PLIST_STRING node.
 * This function does nothing if node is not of type #PLIST_STRING
 *
 * @param node the node
 * @param val a pointer to a C-string. This function allocates the memory,
 *            caller is responsible for freeing it. Data is UTF-8 encoded.
 */
void plist_get_string_val(plist_t node, char **val)
{
	CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(node), kCFStringEncodingUTF8);
	*val = malloc(length);
	CFStringGetCString(node, *val, length, kCFStringEncodingUTF8);
}

/**
 * Get the value of a #PLIST_BOOLEAN node.
 * This function does nothing if node is not of type #PLIST_BOOLEAN
 *
 * @param node the node
 * @param val a pointer to a uint8_t variable.
 */
void plist_get_bool_val(plist_t node, uint8_t * val)
{
	*val = (node == kCFBooleanTrue) ? 1 : 0;
}

/**
 * Get the value of a #PLIST_UINT node.
 * This function does nothing if node is not of type #PLIST_UINT
 *
 * @param node the node
 * @param val a pointer to a uint64_t variable.
 */
void plist_get_uint_val(plist_t node, uint64_t * val)
{
	CFNumberGetValue(node, kCFNumberLongLongType, val);
}

/**
 * Get the value of a #PLIST_REAL node.
 * This function does nothing if node is not of type #PLIST_REAL
 *
 * @param node the node
 * @param val a pointer to a double variable.
 */
void plist_get_real_val(plist_t node, double *val)
{
	CFNumberGetValue(node, kCFNumberDoubleType, val);
}

/**
 * Get the value of a #PLIST_DATA node.
 * This function does nothing if node is not of type #PLIST_DATA
 *
 * @param node the node
 * @param val a pointer to an unallocated char buffer. This function allocates the memory,
 *            caller is responsible for freeing it.
 * @param length the length of the buffer
 */
void plist_get_data_val(plist_t node, char **val, uint64_t * length)
{
	*length = CFDataGetLength(node);
	*val = malloc(*length);
	CFDataGetBytes(node, CFRangeMake(0, *length), (UInt8 *) *val);
}

/**
 * Get the value of a #PLIST_DATE node.
 * This function does nothing if node is not of type #PLIST_DATE
 *
 * @param node the node
 * @param sec a pointer to an int32_t variable. Represents the number of seconds since 01/01/2001.
 * @param usec a pointer to an int32_t variable. Represents the number of microseconds
 */
void plist_get_date_val(plist_t node, int32_t * sec, int32_t * usec)
{
	assert(CFGetTypeID(node) == CFDateGetTypeID());

	CFAbsoluteTime abstime = CFDateGetAbsoluteTime((CFDateRef) node);
	abstime += 978307200.0;

	*sec = (int32_t) abstime;
	abstime = fabs(abstime);
	*usec = (int32_t) ((abstime - floor(abstime)) * 1000000);
}


/********************************************
 *                                          *
 *                Setters                   *
 *                                          *
 ********************************************/

/**
 * Forces type of node. Changing type of structured nodes is only allowed if node is empty.
 * Reset value of node
{
}
 * @param node the node
 * @param type the key value
 */
void plist_set_type(plist_t node, plist_type type)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_KEY
 *
 * @param node the node
 * @param val the key value
 */
void plist_set_key_val(plist_t node, const char *val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_STRING
 *
 * @param node the node
 * @param val the string value
 */
void plist_set_string_val(plist_t node, const char *val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_BOOLEAN
 *
 * @param node the node
 * @param val the boolean value
 */
void plist_set_bool_val(plist_t node, uint8_t val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_UINT
 *
 * @param node the node
 * @param val the unsigned integer value
 */
void plist_set_uint_val(plist_t node, uint64_t val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_REAL
 *
 * @param node the node
 * @param val the real value
 */
void plist_set_real_val(plist_t node, double val)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_DATA
 *
 * @param node the node
 * @param val the binary buffer
 * @param length the length of the buffer
 */
void plist_set_data_val(plist_t node, const char *val, uint64_t length)
{
	assert("Not Implemented" == NULL);
}

/**
 * Set the value of a node.
 * Forces type of node to #PLIST_DATE
 *
 * @param node the node
 * @param sec the number of seconds since 01/01/2001
 * @param usec the number of microseconds
 */
void plist_set_date_val(plist_t node, int32_t sec, int32_t usec)
{
	assert("Not Implemented" == NULL);
}


/********************************************
 *                                          *
 *            Import & Export               *
 *                                          *
 ********************************************/

/**
 * Export the #plist_t structure to XML format.
 *
 * @param plist the root node to export
 * @param plist_xml a pointer to a C-string. This function allocates the memory,
 *            caller is responsible for freeing it. Data is UTF-8 encoded.
 * @param length a pointer to an uint32_t variable. Represents the length of the allocated buffer.
 */
void plist_to_xml(plist_t plist, char **plist_xml, uint32_t * length)
{
	UInt8 *data;
	CFIndex dataLen;

	*plist_xml = NULL;
	*length = 0;

	CFDataRef xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, plist);
	if (xmlData) {
		dataLen = CFDataGetLength(xmlData);
		data = malloc(dataLen + 1);
		if (data) {
			CFDataGetBytes(xmlData, CFRangeMake(0, dataLen), data);
			data[dataLen] = 0;
			*plist_xml = (char *)data;
			*length = dataLen;
		}
		CFRelease(xmlData);
	}
}

/**
 * Export the #plist_t structure to binary format.
 *
 * @param plist the root node to export
 * @param plist_bin a pointer to a char* buffer. This function allocates the memory,
 *            caller is responsible for freeing it.
 * @param length a pointer to an uint32_t variable. Represents the length of the allocated buffer.
 */
void plist_to_bin(plist_t plist, char **plist_bin, uint32_t * length)
{
	*plist_bin = NULL;
	*length = 0;

	CFDataRef data = CFPropertyListCreateData(kCFAllocatorDefault, plist, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	if (data) {
		*length = (uint32_t) CFDataGetLength(data);
		*plist_bin = malloc(*length);
		if (*plist_bin)
			CFDataGetBytes(data, CFRangeMake(0, *length), (UInt8 *) *plist_bin);
		else
			*length = 0;
		
		CFRelease(data);
	}
}

/**
 * Import the #plist_t structure from XML format.
 *
 * @param plist_xml a pointer to the xml buffer.
 * @param length length of the buffer to read.
 * @param plist a pointer to the imported plist.
 */
void plist_from_xml(const char *plist_xml, uint32_t length, plist_t * plist)
{
	*plist = NULL;
	
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, (UInt8 *)plist_xml, length);
	if (data) {
		*plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, data, kCFPropertyListMutableContainers, NULL);
		CFRelease(data);
	}
}

/**
 * Import the #plist_t structure from binary format.
 *
 * @param plist_bin a pointer to the xml buffer.
 * @param length length of the buffer to read.
 * @param plist a pointer to the imported plist.
 */
void plist_from_bin(const char *plist_bin, uint32_t length, plist_t * plist)
{
	*plist = NULL;
	
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, (UInt8 *)plist_bin, length);
	if (data) {
		*plist = CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListMutableContainers, NULL, NULL);
		CFRelease(data);
	}
}


/********************************************
 *                                          *
 *                 Utils                    *
 *                                          *
 ********************************************/

/**
 * Get a node from its path. Each path element depends on the associated father node type.
 * For Dictionaries, var args are casted to const char*, for arrays, var args are caster to uint32_t
 * Search is breath first order.
 *
 * @param plist the node to access result from.
 * @param length length of the path to access
 * @return the value to access.
 */
plist_t plist_access_path(plist_t plist, uint32_t length, ...)
{
	assert("Not Implemented" == NULL);
	return NULL;
}

/**
 * Variadic version of #plist_access_path.
 *
 * @param plist the node to access result from.
 * @param length length of the path to access
 * @param v list of array's index and dic'st key
 * @return the value to access.
 */
plist_t plist_access_pathv(plist_t plist, uint32_t length, va_list v)
{
	assert("Not Implemented" == NULL);
	return NULL;
}

/**
 * Compare two node values
 *
 * @param node_l left node to compare
 * @param node_r rigth node to compare
 * @return TRUE is type and value match, FALSE otherwise.
 */
char plist_compare_node_value(plist_t node_l, plist_t node_r)
{
	assert("Not Implemented" == NULL);
	return 0;
}

/*@}*/
