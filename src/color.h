/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _COLOR_H_
#define _COLOR_H_

#include "x86_cpu.h"

/* luminance weight, notice that these is used for linear data */

#define RLUM (0.3086)
#define GLUM (0.6094)
#define BLUM (0.0820)

#define GAMMA 2.2 /* this is ONLY used to render the histogram */

#define _CLAMP(in, max) if (in>max) in=max

#define _CLAMP65535(a) a = MAX(MIN(65535,a),0)

#define _CLAMP65535_TRIPLET(a, b, c) \
a = MAX(MIN(65535,a),0);b = MAX(MIN(65535,b),0);c = MAX(MIN(65535,c),0)

#define _CLAMP255(a) a = MAX(MIN(255,a),0)

#define COLOR_BLACK(c) do { c.red=0; c.green=0; c.blue=0; } while (0)

enum {
	R=0,
	G=1,
	B=2,
	G2=3
};

#endif /* _COLOR_H_ */
