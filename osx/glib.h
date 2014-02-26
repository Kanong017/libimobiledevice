//	G-lib replacement for OS X
//
//  Created by Aaron Burghardt 7/25/2013.
//  Copyright 2013 Aaron Burghardt. All rights reserved.
//

#ifndef GLIB_H
#define GLIB_H

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!0)
#endif

typedef char gchar;
typedef unsigned int guint;
typedef signed long gssize;
typedef unsigned long gsize;
typedef int gboolean;
typedef char gunichar;

#include <gstring.h>

#endif
