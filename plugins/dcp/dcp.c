/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and
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

/* Plugin tmpl version 4 */

#include "config.h"
#include <rawstudio.h>
#include <math.h> /* pow() */
#if defined (__SSE2__)
#include <emmintrin.h>
#endif /* __SSE2__ */
#define RS_TYPE_DCP (rs_dcp_type)
#define RS_DCP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP, RSDcp))
#define RS_DCP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP, RSDcpClass))
#define RS_IS_DCP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP))
#define RS_DCP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_DCP, RSDcpClass))

typedef struct _RSDcp RSDcp;
typedef struct _RSDcpClass RSDcpClass;

struct _RSDcp {
	RSFilter parent;

	gfloat exposure;
	gfloat saturation;
	gfloat hue;

	RS_xy_COORD white_xy;

	gint nknots;
	gfloat *curve_samples;

	gfloat temp1;
	gfloat temp2;

	RSSpline *baseline_exposure;
	gfloat *baseline_exposure_lut;

	gboolean has_color_matrix1;
	gboolean has_color_matrix2;
	RS_MATRIX3 color_matrix1;
	RS_MATRIX3 color_matrix2;

	gboolean has_forward_matrix1;
	gboolean has_forward_matrix2;
	RS_MATRIX3 forward_matrix1;
	RS_MATRIX3 forward_matrix2;
	RS_MATRIX3 forward_matrix;

	RSHuesatMap *looktable;

	RSHuesatMap *huesatmap;
	RSHuesatMap *huesatmap1;
	RSHuesatMap *huesatmap2;

	RS_MATRIX3 camera_to_pcs;

	RS_VECTOR3 camera_white;
	RS_MATRIX3 camera_to_prophoto;
};

struct _RSDcpClass {
	RSFilterClass parent_class;

	RSIccProfile *prophoto_profile;
};

RS_DEFINE_FILTER(rs_dcp, RSDcp)

enum {
	PROP_0,
	PROP_SETTINGS,
	PROP_PROFILE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterParam *param);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp);
static RS_xy_COORD neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral);
static RS_MATRIX3 find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix);
static void set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy);
static void precalc(RSDcp *dcp);
static void render(RSDcp *dcp, RS_IMAGE16 *image);
#if defined (__SSE2__)
static void render_SSE2(RSDcp *dcp, RS_IMAGE16 *image);
#endif
static void read_profile(RSDcp *dcp, RSDcpFile *dcp_file);
static RSIccProfile *get_icc_profile(RSFilter *filter);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_dcp_get_type(G_TYPE_MODULE(plugin));
}

static void
finalize(GObject *object)
{
	RSDcp *dcp = RS_DCP(object);

	g_free(dcp->curve_samples);
}

static void
rs_dcp_class_init(RSDcpClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	klass->prophoto_profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/prophoto.icc");

	g_object_class_install_property(object_class,
		PROP_SETTINGS, g_param_spec_object(
			"settings", "Settings", "Settings to render from",
			RS_TYPE_SETTINGS, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_PROFILE, g_param_spec_object(
			"profile", "profile", "DCP Profile",
			RS_TYPE_DCP_FILE, G_PARAM_READWRITE)
	);

	filter_class->name = "Adobe DNG camera profile filter";
	filter_class->get_image = get_image;
	filter_class->get_icc_profile = get_icc_profile;
}

static void
settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp)
{
	gboolean changed = FALSE;

	if (mask & MASK_EXPOSURE)
	{
		g_object_get(settings, "exposure", &dcp->exposure, NULL);
		changed = TRUE;
	}

	if (mask & MASK_SATURATION)
	{
		g_object_get(settings, "saturation", &dcp->saturation, NULL);
		changed = TRUE;
	}

	if (mask & MASK_HUE)
	{
		g_object_get(settings, "hue", &dcp->hue, NULL);
		dcp->hue /= 60.0;
		changed = TRUE;
	}

	if ((mask & MASK_WB) || (mask & MASK_CHANNELMIXER))
	{
		const gfloat warmth;
		gfloat tint;
		const gfloat channelmixer_red;
		const gfloat channelmixer_green;
		const gfloat channelmixer_blue;

		g_object_get(settings,
			"warmth", &warmth,
			"tint", &tint,
			"channelmixer_red", &channelmixer_red,
			"channelmixer_green", &channelmixer_green,
			"channelmixer_blue", &channelmixer_blue,
			NULL);

		RS_xy_COORD whitepoint;
		RS_VECTOR3 pre_mul;
		/* This is messy, but we're essentially converting from warmth/tint to cameraneutral */
        pre_mul.x = (1.0+warmth)*(2.0-tint)*(channelmixer_red/100.0);
        pre_mul.y = 1.0*(channelmixer_green/100.0);
        pre_mul.z = (1.0-warmth)*(2.0-tint)*(channelmixer_blue/100.0);
		RS_VECTOR3 neutral;
		neutral.x = 1.0 / CLAMP(pre_mul.x, 0.001, 100.00);
		neutral.y = 1.0 / CLAMP(pre_mul.y, 0.001, 100.00);
		neutral.z = 1.0 / CLAMP(pre_mul.z, 0.001, 100.00);
		gfloat max = vector3_max(&neutral);
		neutral.x = neutral.x / max;
		neutral.y = neutral.y / max;
		neutral.z = neutral.z / max;
		whitepoint = neutral_to_xy(dcp, &neutral);

		set_white_xy(dcp, &whitepoint);
		precalc(dcp);
		changed = TRUE;
	}

	if (mask & MASK_CURVE)
	{
		const gint nknots = rs_settings_get_curve_nknots(settings);

		if (nknots > 1)
		{
			gfloat *knots = rs_settings_get_curve_knots(settings);
			if (knots)
			{
				dcp->nknots = nknots;
				RSSpline *spline = rs_spline_new(knots, dcp->nknots, NATURAL);
				rs_spline_sample(spline, dcp->curve_samples, 65536);
				g_object_unref(spline);
				g_free(knots);
			}
		}
		else
		{
			gint i;
			for(i=0;i<65536;i++)
				dcp->curve_samples[i] = ((gfloat)i)/65536.0;
		}
		changed = TRUE;
	}

	if (changed)
		rs_filter_changed(RS_FILTER(dcp), RS_FILTER_CHANGED_PIXELDATA);
}

static void
rs_dcp_init(RSDcp *dcp)
{
	gint i;

	dcp->curve_samples = g_new(gfloat, 65536);

	for(i=0;i<65536;i++)
		dcp->curve_samples[i] = ((gfloat)i)/65536.0;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
//	RSDcp *dcp = RS_DCP(object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSDcp *dcp = RS_DCP(object);
	RSFilter *filter = RS_FILTER(dcp);
	RSSettings *settings;

	switch (property_id)
	{
		case PROP_SETTINGS:
			settings = g_value_get_object(value);
			g_signal_connect(settings, "settings-changed", G_CALLBACK(settings_changed), dcp);
			settings_changed(settings, MASK_ALL, dcp);
			break;
		case PROP_PROFILE:
			read_profile(dcp, g_value_get_object(value));
			rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterParam *param)
{
	RSDcp *dcp = RS_DCP(filter);
	GdkRectangle *roi;
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	RS_IMAGE16 *tmp;

	previous_response = rs_filter_get_image(filter->previous, param);

	if (!RS_IS_FILTER(filter->previous))
		return previous_response;

	input = rs_filter_response_get_image(previous_response);
	if (!input) return previous_response;
	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	output = rs_image16_copy(input, TRUE);
	g_object_unref(input);

	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	if ((roi = rs_filter_param_get_roi(param)))
		tmp = rs_image16_new_subframe(output, roi);
	else
		tmp = g_object_ref(output);

#if defined (__SSE2__)
	if (rs_detect_cpu_features() & RS_CPU_FLAG_SSE2)
		render_SSE2(dcp, tmp);
	else
#endif
		render(dcp, tmp);

	g_object_unref(tmp);

	return response;
}

/* dng_color_spec::NeutralToXY */
static RS_xy_COORD
neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral)
{
	const guint max_passes = 30;
	guint pass;
	RS_xy_COORD last;

	last = XYZ_to_xy(&XYZ_WP_D50);

	for(pass = 0; pass < max_passes; pass++)
	{
		RS_MATRIX3 xyz_to_camera = find_xyz_to_camera(dcp, &last, NULL);
		RS_MATRIX3 camera_to_xyz = matrix3_invert(&xyz_to_camera);

		RS_XYZ_VECTOR tmp = vector3_multiply_matrix(neutral, &camera_to_xyz);
		RS_xy_COORD next = XYZ_to_xy(&tmp);

		if (ABS(next.x - last.x) + ABS(next.y - last.y) < 0.0000001)
		{
			last = next;
			break;
		}

		// If we reach the limit without converging, we are most likely
		// in a two value oscillation.  So take the average of the last
		// two estimates and give up.
		if (pass == max_passes - 1)
		{
			next.x = (last.x + next.x) * 0.5;
			next.y = (last.y + next.y) * 0.5;
		}
		last = next;
	}

	return last;
}

inline void
RGBtoHSV(gfloat r, gfloat g, gfloat b, gfloat *h, gfloat *s, gfloat *v)
{
	*v = MAX(r, MAX (g, b));

	gfloat gap = *v - MIN (r, MIN (g, b));

	if (gap > 0.0f)
	{
		if (r == *v)
		{
			*h = (g - b) / gap;

			if (*h < 0.0f)
				*h += 6.0f;
		}
		else if (g == *v)
			*h = 2.0f + (b - r) / gap;
		else
			*h = 4.0f + (r - g) / gap;

		*s = gap / *v;
	}
	else
	{
		*h = 0.0f;
		*s = 0.0f;
	}
}

#if defined (__SSE2__)

inline void
RGBtoHSV_SSE(__m128 *c0, __m128 *c1, __m128 *c2)
{

	__m128 zero_ps = _mm_set_ps(0.0f, 0.0f, 0.0f, 0.0f);
	__m128 ones_ps = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
	// Any number > 1
	__m128 add_v = _mm_set_ps(10.0f, 10.0f, 10.0f, 10.0f);

	__m128 r = *c0;
	__m128 g = *c1;
	__m128 b = *c2;

	__m128 h, v;
	v = _mm_max_ps(b,_mm_max_ps(r,g));

	__m128 m = _mm_min_ps(b,_mm_min_ps(r,g));
	__m128 gap = _mm_sub_ps(v,m);
	__m128 v_mask = _mm_cmpeq_ps(gap, zero_ps);
	v = _mm_add_ps(v, _mm_and_ps(add_v, v_mask));

	h = _mm_xor_ps(r,r);

	/* Set gap to one where sat = 0, this will avoid divisions by zero, these values will not be used */
	ones_ps = _mm_and_ps(ones_ps, v_mask);
	gap = _mm_or_ps(gap, ones_ps);
	/*  gap_inv = 1.0 / gap */
	__m128 gap_inv = _mm_rcp_ps(gap);

	/* if r == v */
	/* h = (g - b) / gap; */
	__m128 mask = _mm_cmpeq_ps(r, v);
	__m128 val = _mm_mul_ps(gap_inv, _mm_sub_ps(g, b));

	/* fill h */
	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	/* if g == v */
	/* h = 2.0f + (b - r) / gap; */
	__m128 two_ps = _mm_set_ps(2.0f, 2.0f, 2.0f, 2.0f);
	mask = _mm_cmpeq_ps(g, v);
	val = _mm_sub_ps(b, r);
	val = _mm_mul_ps(val, gap_inv);
	val = _mm_add_ps(val, two_ps);

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	/* If (b == v) */
	/* h = 4.0f + (r - g) / gap; */
	__m128 four_ps = _mm_add_ps(two_ps, two_ps);
	mask = _mm_cmpeq_ps(b, v);
	val = _mm_add_ps(four_ps, _mm_mul_ps(gap_inv, _mm_sub_ps(r, g)));

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	__m128 s;
	/* Fill s, if gap > 0 */
	v = _mm_sub_ps(v, add_v);
	val = _mm_mul_ps(gap,_mm_rcp_ps(v));
	s = _mm_andnot_ps(v_mask, val );

	/* Check if h < 0 */
	__m128 six_ps = _mm_set_ps(6.0f, 6.0f, 6.0f, 6.0f);
	mask = _mm_cmplt_ps(h, zero_ps);
	h = _mm_add_ps(h, _mm_and_ps(mask, six_ps));


	*c0 = h;
	*c1 = s;
	*c2 = v;
}
#endif

inline void
HSVtoRGB(gfloat h, gfloat s, gfloat v, gfloat *r, gfloat *g, gfloat *b)
{
	if (s > 0.0f)
	{

		if (h < 0.0f)
			h += 6.0f;

		if (h >= 6.0f)
			h -= 6.0f;

		gint i = (gint) h;
		gfloat f = h - (gint) i;

		gfloat p = v * (1.0f - s);

#define q   (v * (1.0f - s * f))
#define t   (v * (1.0f - s * (1.0f - f)))

		switch (i)
		{
			case 0: *r = v; *g = t; *b = p; break;
			case 1: *r = q; *g = v; *b = p; break;
			case 2: *r = p; *g = v; *b = t; break;
			case 3: *r = p; *g = q; *b = v; break;
			case 4: *r = t; *g = p; *b = v; break;
			case 5: *r = v; *g = p; *b = q; break;
		}

#undef q
#undef t

	}
	else
	{
		*r = v;
		*g = v;
		*b = v;
	}
}

#define _F(x) (x / 65535.0)
#define _S(x) CLAMP(((gint) (x * 65535.0)), 0, 65535)

static void
huesat_map(RSHuesatMap *map, gfloat *h, gfloat *s, gfloat *v)
{
	g_assert(RS_IS_HUESAT_MAP(map));

	gfloat hScale = (map->hue_divisions < 2) ? 0.0f : (map->hue_divisions * (1.0f / 6.0f));
	gfloat sScale = (gfloat) (map->sat_divisions - 1);
    gfloat vScale = (gfloat) (map->val_divisions - 1);

	gint maxHueIndex0 = map->hue_divisions - 1;
    gint maxSatIndex0 = map->sat_divisions - 2;
    gint maxValIndex0 = map->val_divisions - 2;

    const RS_VECTOR3 *tableBase = map->deltas;

    gint hueStep = map->sat_divisions;
    gint valStep = map->hue_divisions * hueStep;

	gfloat hueShift;
	gfloat satScale;
	gfloat valScale;

	if (map->val_divisions < 2)
	{
		gfloat hScaled = *h * hScale;
		gfloat sScaled = *s * sScale;

		gint hIndex0 = (gint) hScaled;
		gint sIndex0 = (gint) sScaled;

		sIndex0 = MIN(sIndex0, maxSatIndex0);

		gint hIndex1 = hIndex0 + 1;

		if (hIndex0 >= maxHueIndex0)
		{
			hIndex0 = maxHueIndex0;
			hIndex1 = 0;
		}

		gfloat hFract1 = hScaled - (gfloat) hIndex0;
		gfloat sFract1 = sScaled - (gfloat) sIndex0;

		gfloat hFract0 = 1.0f - hFract1;
		gfloat sFract0 = 1.0f - sFract1;

		const RS_VECTOR3 *entry00 = tableBase + hIndex0 * hueStep + sIndex0;

		const RS_VECTOR3 *entry01 = entry00 + (hIndex1 - hIndex0) * hueStep;
		gfloat hueShift0 = hFract0 * entry00->fHueShift +
		hFract1 * entry01->fHueShift;

		gfloat satScale0 = hFract0 * entry00->fSatScale +
		hFract1 * entry01->fSatScale;

		gfloat valScale0 = hFract0 * entry00->fValScale +
		hFract1 * entry01->fValScale;

		entry00++;
		entry01++;

		gfloat hueShift1 = hFract0 * entry00->fHueShift +
		hFract1 * entry01->fHueShift;

		gfloat satScale1 = hFract0 * entry00->fSatScale +
		hFract1 * entry01->fSatScale;

		gfloat valScale1 = hFract0 * entry00->fValScale +
		hFract1 * entry01->fValScale;

		hueShift = sFract0 * hueShift0 + sFract1 * hueShift1;
		satScale = sFract0 * satScale0 + sFract1 * satScale1;
		valScale = sFract0 * valScale0 + sFract1 * valScale1;
	}
	else
	{
		gfloat hScaled = *h * hScale;
		gfloat sScaled = *s * sScale;
		gfloat vScaled = *v * vScale;

		gint hIndex0 = (gint) hScaled;
		gint sIndex0 = (gint) sScaled;
		gint vIndex0 = (gint) vScaled;

		sIndex0 = MIN(sIndex0, maxSatIndex0);
		vIndex0 = MIN(vIndex0, maxValIndex0);

		gint hIndex1 = hIndex0 + 1;

		if (hIndex0 >= maxHueIndex0)
		{
			hIndex0 = maxHueIndex0;
			hIndex1 = 0;
		}

		gfloat hFract1 = hScaled - (gfloat) hIndex0;
		gfloat sFract1 = sScaled - (gfloat) sIndex0;
		gfloat vFract1 = vScaled - (gfloat) vIndex0;

		gfloat hFract0 = 1.0f - hFract1;
		gfloat sFract0 = 1.0f - sFract1;
		gfloat vFract0 = 1.0f - vFract1;

		const RS_VECTOR3 *entry00 = tableBase + vIndex0 * valStep + hIndex0 * hueStep + sIndex0;
		const RS_VECTOR3 *entry01 = entry00 + (hIndex1 - hIndex0) * hueStep;

		const RS_VECTOR3 *entry10 = entry00 + valStep;
		const RS_VECTOR3 *entry11 = entry01 + valStep;

		gfloat hueShift0 = vFract0 * (hFract0 * entry00->fHueShift +
			hFract1 * entry01->fHueShift) +
			vFract1 * (hFract0 * entry10->fHueShift +
			hFract1 * entry11->fHueShift);

		gfloat satScale0 = vFract0 * (hFract0 * entry00->fSatScale +
			hFract1 * entry01->fSatScale) +
			vFract1 * (hFract0 * entry10->fSatScale +
			hFract1 * entry11->fSatScale);

		gfloat valScale0 = vFract0 * (hFract0 * entry00->fValScale +
			hFract1 * entry01->fValScale) +
			vFract1 * (hFract0 * entry10->fValScale +
			hFract1 * entry11->fValScale);

		entry00++;
		entry01++;
		entry10++;
		entry11++;

		gfloat hueShift1 = vFract0 * (hFract0 * entry00->fHueShift +
			hFract1 * entry01->fHueShift) +
			vFract1 * (hFract0 * entry10->fHueShift +
			hFract1 * entry11->fHueShift);

		gfloat satScale1 = vFract0 * (hFract0 * entry00->fSatScale +
			hFract1 * entry01->fSatScale) +
			vFract1 * (hFract0 * entry10->fSatScale +
			hFract1 * entry11->fSatScale);

		gfloat valScale1 = vFract0 * (hFract0 * entry00->fValScale +
			hFract1 * entry01->fValScale) +
			vFract1 * (hFract0 * entry10->fValScale +
			hFract1 * entry11->fValScale);

		hueShift = sFract0 * hueShift0 + sFract1 * hueShift1;
		satScale = sFract0 * satScale0 + sFract1 * satScale1;
		valScale = sFract0 * valScale0 + sFract1 * valScale1;
	}

	hueShift *= (6.0f / 360.0f);

	*h += hueShift;
	*s = MIN(*s * satScale, 1.0);
	*v = MIN(*v * valScale, 1.0);
}

/* RefBaselineRGBTone() */
void
rgb_tone(gfloat *_r, gfloat *_g, gfloat *_b, const gfloat * const tone_lut)
{
	gfloat r = *_r;
	gfloat g = *_g;
	gfloat b = *_b;
		gfloat rr;
		gfloat gg;
		gfloat bb;

		#define RGBTone(r, g, b, rr, gg, bb)\
			{\
			\
/*			DNG_ASSERT (r >= g && g >= b && r > b, "Logic Error RGBTone");*/\
			\
			rr = tone_lut[_S(r)];\
			bb = tone_lut[_S(b)];\
			\
			gg = bb + ((rr - bb) * (g - b) / (r - b));\
			\
			}

		if (r >= g)
			{

			if (g > b)
				{

				// Case 1: r >= g > b

				RGBTone (r, g, b, rr, gg, bb);

				}

			else if (b > r)
				{

				// Case 2: b > r >= g

				RGBTone (b, r, g, bb, rr, gg);

				}

			else if (b > g)
				{

				// Case 3: r >= b > g

				RGBTone (r, b, g, rr, bb, gg);

				}

			else
				{

				// Case 4: r >= g == b

//				DNG_ASSERT (r >= g && g == b, "Logic Error 2");

				rr = tone_lut[_S(r)];
				gg = tone_lut[_S(b)];
//				rr = table.Interpolate (r);
//				gg = table.Interpolate (g);
				bb = gg;

				}

			}

		else
			{

			if (r >= b)
				{

				// Case 5: g > r >= b

				RGBTone (g, r, b, gg, rr, bb);

				}

			else if (b > g)
				{

				// Case 6: b > g > r

				RGBTone (b, g, r, bb, gg, rr);

				}

			else
				{

				// Case 7: g >= b > r

				RGBTone (g, b, r, gg, bb, rr);

				}

			}

		#undef RGBTone

		*_r = rr;
		*_g = gg;
		*_b = bb;

}

#if defined (__SSE2__)

inline __m128
sse_matrix3_mul(float* mul, __m128 a, __m128 b, __m128 c)
{

	__m128 v = _mm_set_ps(mul[0], mul[0], mul[0], mul[0]);
	__m128 acc = _mm_mul_ps(a, v);

	v = _mm_set_ps(mul[1], mul[1], mul[1], mul[1]);
	acc = _mm_add_ps(acc, _mm_mul_ps(b, v));

	v = _mm_set_ps(mul[2], mul[2], mul[2], mul[2]);
	acc = _mm_add_ps(acc, _mm_mul_ps(c, v));

	return acc;
}

static void
render_SSE2(RSDcp *dcp, RS_IMAGE16 *image)
{
	gint x, y;
	__m128 h, s, v;
	__m128i p1,p2;
	__m128 p1f, p2f, p3f, p4f;
	__m128 r, g, b, r2, g2, b2;
	__m128i zero;

	printf("Using SSE2\n");
	int xfer[4] __attribute__ ((aligned (16)));

	const gfloat exposure_comp = pow(2.0, dcp->exposure);
	const gfloat saturation = dcp->saturation;
	const gfloat hue = dcp->hue;
	gfloat r_coeffs[3] = {dcp->camera_to_prophoto.coeff[0][0], dcp->camera_to_prophoto.coeff[0][1], dcp->camera_to_prophoto.coeff[0][2]};
	gfloat g_coeffs[3] = {dcp->camera_to_prophoto.coeff[1][0], dcp->camera_to_prophoto.coeff[1][1], dcp->camera_to_prophoto.coeff[1][2]};
	gfloat b_coeffs[3] = {dcp->camera_to_prophoto.coeff[2][0], dcp->camera_to_prophoto.coeff[2][1], dcp->camera_to_prophoto.coeff[2][2]};

	for(y = 0 ; y < image->h; y++)
	{
		for(x=0; x < image->w; x+=4)
		{
			__m128i* pixel = (__m128i*)GET_PIXEL(image, x, y);

			zero = _mm_xor_si128(zero,zero);

			/* Convert to float */
			p1 = _mm_load_si128(pixel);
			p2 = _mm_load_si128(pixel + 1);

			/* Unpack to R G B x */
			p2f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p1, zero));
			p4f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p2, zero));
			p1f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p1, zero));
			p3f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p2, zero));

			/* Normalize to 0 to 1 range */
			__m128 rgb_div = _mm_set_ps(1.0/65535.0, 1.0/65535.0, 1.0/65535.0, 1.0/65535.0);
			p1f = _mm_mul_ps(p1f, rgb_div);
			p2f = _mm_mul_ps(p2f, rgb_div);
			p3f = _mm_mul_ps(p3f, rgb_div);
			p4f = _mm_mul_ps(p4f, rgb_div);

			/* Restric to camera white */
			__m128 min_cam = _mm_set_ps(0.0f, dcp->camera_white.z, dcp->camera_white.y, dcp->camera_white.x);
			p1f = _mm_min_ps(p1f, min_cam);
			p2f = _mm_min_ps(p2f, min_cam);
			p3f = _mm_min_ps(p3f, min_cam);
			p4f = _mm_min_ps(p4f, min_cam);

			/* Convert to planar */
			__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
			__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
			__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
			__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
			r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
			g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
			b = _mm_movelh_ps(b1b0, b3b2);

			/* Convert to Prophoto */
			r2 = sse_matrix3_mul(r_coeffs, r, g, b);
			g2 = sse_matrix3_mul(g_coeffs, r, g, b);
			b2 = sse_matrix3_mul(b_coeffs, r, g, b);

			/* Set min/max before HSV conversion */
			__m128 min_val = _mm_set_ps(1e-15, 1e-15, 1e-15, 1e-15);
			__m128 max_val = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
			r = _mm_max_ps(_mm_min_ps(r2, max_val), min_val);
			g = _mm_max_ps(_mm_min_ps(g2, max_val), min_val);
			b = _mm_max_ps(_mm_min_ps(b2, max_val), min_val);

			RGBtoHSV_SSE(&r, &g, &b);
			h = r; s = g; v = b;

			if (dcp->huesatmap)
			{
				gfloat* h_p = (gfloat*)&h;
				gfloat* s_p = (gfloat*)&s;
				gfloat* v_p = (gfloat*)&v;

				huesat_map(dcp->huesatmap, &h_p[0], &s_p[0], &v_p[0]);
				huesat_map(dcp->huesatmap, &h_p[1], &s_p[1], &v_p[1]);
				huesat_map(dcp->huesatmap, &h_p[2], &s_p[2], &v_p[2]);
				huesat_map(dcp->huesatmap, &h_p[3], &s_p[3], &v_p[3]);
			}

			/* Exposure */
			__m128 exp = _mm_set_ps(exposure_comp, exposure_comp, exposure_comp, exposure_comp);
			v = _mm_min_ps(max_val, _mm_mul_ps(v, exp));


			/* Saturation */
			__m128 sat = _mm_set_ps(saturation, saturation, saturation, saturation);
			s = _mm_min_ps(max_val, _mm_mul_ps(s, sat));

			/* Hue */
			__m128 hue_add = _mm_set_ps(hue, hue, hue, hue);
			__m128 six_ps = _mm_set_ps(6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15);
			__m128 zero_ps = _mm_set_ps(0.0f, 0.0f, 0.0f, 0.0f);
			h = _mm_add_ps(h, hue_add);

			/* Check if hue > 6 or < 0*/
			__m128 h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			__m128 h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			__m128 six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			__m128 six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);

			/* Convert v to lookup values */

			/* TODO: Use 8 bit fraction as interpolation, for interpolating
			 * a more precise lookup using linear interpolation. Maybe use less than
			 * 16 bits for lookup for speed, 10 bits with interpolation should be enough */
			__m128 v_mul = _mm_set_ps(65535.0, 65535.0, 65535.0, 65535.0);
			v = _mm_mul_ps(v, v_mul);
			__m128i lookup = _mm_cvtps_epi32(v);
			gfloat* v_p = (gfloat*)&v;
			_mm_store_si128((__m128i*)&xfer[0], lookup);

			v_p[0] = dcp->curve_samples[xfer[0]];
			v_p[1] = dcp->curve_samples[xfer[1]];
			v_p[2] = dcp->curve_samples[xfer[2]];
			v_p[3] = dcp->curve_samples[xfer[3]];


			if (dcp->looktable) {
				gfloat* h_p = (gfloat*)&h;
				gfloat* s_p = (gfloat*)&s;
				huesat_map(dcp->looktable, &h_p[0], &s_p[0], &v_p[0]);
				huesat_map(dcp->looktable, &h_p[1], &s_p[1], &v_p[1]);
				huesat_map(dcp->looktable, &h_p[2], &s_p[2], &v_p[2]);
				huesat_map(dcp->looktable, &h_p[3], &s_p[3], &v_p[3]);
			}


			/* Back to RGB */

			/* ensure that hue is within range */
			h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);

			/* s always slightly > 0 */
			s = _mm_max_ps(s, min_val);


			/* Convert get the fraction of h
			 * h_fraction = h - (float)(int)h */
			__m128 half_ps = _mm_set_ps(0.5f, 0.5f, 0.5f, 0.5f);
			__m128 h_fraction = _mm_sub_ps(h,_mm_cvtepi32_ps(_mm_cvtps_epi32(_mm_sub_ps(h, half_ps))));
			__m128 ones_ps = _mm_add_ps(half_ps, half_ps);

			/* p = v * (1.0f - s)  */
			__m128 p = _mm_mul_ps(v,  _mm_sub_ps(ones_ps, s));
			/* q = (v * (1.0f - s * f)) */
			__m128 q = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, h_fraction)));
			/* t = (v * (1.0f - s * (1.0f - f))) */
			__m128 t = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, _mm_sub_ps(ones_ps, h_fraction))));

			/* h < 1  (case 0)*/
			/* case 0: *r = v; *g = t; *b = p; break; */
			__m128 h_threshold = _mm_add_ps(ones_ps, ones_ps);
			__m128 out_mask = _mm_cmplt_ps(h, ones_ps);
			r = _mm_and_ps(v, out_mask);
			g = _mm_and_ps(t, out_mask);
			b = _mm_and_ps(p, out_mask);

			/* h < 2 (case 1) */
			/* case 1: *r = q; *g = v; *b = p; break; */
			__m128 m = _mm_cmplt_ps(h, h_threshold);
			h_threshold = _mm_add_ps(h_threshold, ones_ps);
			m = _mm_andnot_ps(out_mask, m);
			r = _mm_or_ps(r, _mm_and_ps(q, m));
			g = _mm_or_ps(g, _mm_and_ps(v, m));
			b = _mm_or_ps(b, _mm_and_ps(p, m));
			out_mask = _mm_or_ps(out_mask, m);

			/* h < 3 (case 2)*/
			/* case 2: *r = p; *g = v; *b = t; break; */
			m = _mm_cmplt_ps(h, h_threshold);
			h_threshold = _mm_add_ps(h_threshold, ones_ps);
			m = _mm_andnot_ps(out_mask, m);
			r = _mm_or_ps(r, _mm_and_ps(p, m));
			g = _mm_or_ps(g, _mm_and_ps(v, m));
			b = _mm_or_ps(b, _mm_and_ps(t, m));
			out_mask = _mm_or_ps(out_mask, m);

			/* h < 4 (case 3)*/
			/* case 3: *r = p; *g = q; *b = v; break; */
			m = _mm_cmplt_ps(h, h_threshold);
			h_threshold = _mm_add_ps(h_threshold, ones_ps);
			m = _mm_andnot_ps(out_mask, m);
			r = _mm_or_ps(r, _mm_and_ps(p, m));
			g = _mm_or_ps(g, _mm_and_ps(q, m));
			b = _mm_or_ps(b, _mm_and_ps(v, m));
			out_mask = _mm_or_ps(out_mask, m);

			/* h < 5 (case 4)*/
			/* case 4: *r = t; *g = p; *b = v; break; */
			m = _mm_cmplt_ps(h, h_threshold);
			m = _mm_andnot_ps(out_mask, m);
			r = _mm_or_ps(r, _mm_and_ps(t, m));
			g = _mm_or_ps(g, _mm_and_ps(p, m));
			b = _mm_or_ps(b, _mm_and_ps(v, m));
			out_mask = _mm_or_ps(out_mask, m);


			/* Remainder (case 5) */
			/* case 5: *r = v; *g = p; *b = q; break; */
			__m128 all_ones = _mm_cmpeq_ps(h,h);
			m = _mm_xor_ps(out_mask, all_ones);
			r = _mm_or_ps(r, _mm_and_ps(v, m));
			g = _mm_or_ps(g, _mm_and_ps(p, m));
			b = _mm_or_ps(b, _mm_and_ps(q, m));


			__m128 rgb_mul = _mm_set_ps(65535.0, 65535.0, 65535.0, 65535.0);
			r = _mm_mul_ps(r, rgb_mul);
			g = _mm_mul_ps(g, rgb_mul);
			b = _mm_mul_ps(b, rgb_mul);

			__m128i r_i = _mm_cvtps_epi32(r);
			__m128i g_i = _mm_cvtps_epi32(g);
			__m128i b_i = _mm_cvtps_epi32(b);

			__m128i sub_32 = _mm_set_epi32(32768, 32768, 32768, 32768);
			__m128i signxor = _mm_set_epi32(0x80008000, 0x80008000, 0x80008000, 0x80008000);

			/* Subtract 32768 to avoid saturation */
			r_i = _mm_sub_epi32(r_i, sub_32);
			g_i = _mm_sub_epi32(g_i, sub_32);
			b_i = _mm_sub_epi32(b_i, sub_32);

			/* 32 bit signed -> 16 bit signed conversion, all in lower 64 bit */
			r_i = _mm_packs_epi32(r_i, r_i);
			g_i = _mm_packs_epi32(g_i, g_i);
			b_i = _mm_packs_epi32(b_i, b_i);

			/* Interleave*/
			__m128i rg_i = _mm_unpacklo_epi16(r_i, g_i);
			__m128i bb_i = _mm_unpacklo_epi16(b_i, b_i);
			p1 = _mm_unpacklo_epi32(rg_i, bb_i);
			p2 = _mm_unpackhi_epi32(rg_i, bb_i);

			/* Convert sign back */
			p1 = _mm_xor_si128(p1, signxor);
			p2 = _mm_xor_si128(p2, signxor);

			/* Store processed pixel */
			_mm_store_si128(pixel, p1);
			_mm_store_si128(pixel + 1, p2);
		}
	}
}
#endif

static void
render(RSDcp *dcp, RS_IMAGE16 *image)
{
	gint x, y;
	gfloat h, s, v;
	gfloat r, g, b;
	RS_VECTOR3 pix;

	const gfloat exposure_comp = pow(2.0, dcp->exposure);

	for(y = 0 ; y < image->h; y++)
	{
		for(x=0; x < image->w; x++)
		{
			gushort *pixel = GET_PIXEL(image, x, y);

			/* Convert to float */
			r = _F(pixel[R]);
			g = _F(pixel[G]);
			b = _F(pixel[B]);

			r = MIN(dcp->camera_white.x, r);
			g = MIN(dcp->camera_white.y, g);
			b = MIN(dcp->camera_white.z, b);

			pix.R = r;
			pix.G = g;
			pix.B = b;
			pix = vector3_multiply_matrix(&pix, &dcp->camera_to_prophoto);

			r = CLAMP(pix.R, 0.0, 1.0);
			g = CLAMP(pix.G, 0.0, 1.0);
			b = CLAMP(pix.B, 0.0, 1.0);

			/* To HSV */
			RGBtoHSV(r, g, b, &h, &s, &v);

			if (dcp->huesatmap)
				huesat_map(dcp->huesatmap, &h, &s, &v);

			v = MIN(v * exposure_comp, 1.0);

			/* Saturation */
			s *= dcp->saturation;
			s = MIN(s, 1.0);

			/* Hue */
			h += dcp->hue;

			/* Curve */
			v = dcp->curve_samples[_S(v)];

			if (dcp->looktable)
				huesat_map(dcp->looktable, &h, &s, &v);

			/* Back to RGB */
			HSVtoRGB(h, s, v, &r, &g, &b);

			/* Save as gushort */
			pixel[R] = _S(r);
			pixel[G] = _S(g);
			pixel[B] = _S(b);
		}
	}
}

#undef _F
#undef _S

/* dng_color_spec::FindXYZtoCamera */
static RS_MATRIX3
find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix)
{
	gfloat temp = 5000.0;

	rs_color_whitepoint_to_temp(white_xy, &temp, NULL);

	gfloat alpha = 0.0;

	if (temp <=  dcp->temp1)
		alpha = 1.0;
	else if (temp >=  dcp->temp2)
		alpha = 0.0;
	else if ((dcp->temp2 > 0.0) && (dcp->temp1 > 0.0) && (temp > 0.0))
	{
		gdouble invT = 1.0 / temp;
		alpha = (invT - (1.0 / dcp->temp2)) / ((1.0 / dcp->temp1) - (1.0 / dcp->temp2));
	}

	RS_MATRIX3 color_matrix;

	matrix3_interpolate(&dcp->color_matrix1, &dcp->color_matrix2, alpha, &color_matrix);

	if (forward_matrix)
	{
		if (dcp->has_forward_matrix1 && dcp->has_forward_matrix2)
			matrix3_interpolate(&dcp->forward_matrix1, &dcp->forward_matrix2, 1.0-alpha, forward_matrix);
		else if (dcp->has_forward_matrix1)
			*forward_matrix = dcp->forward_matrix1;
		else if (dcp->has_forward_matrix2)
			*forward_matrix = dcp->forward_matrix2;
	}

	return color_matrix;
}

/* Verified to behave like dng_camera_profile::NormalizeForwardMatrix */
static void
normalize_forward_matrix(RS_MATRIX3 *matrix)
{
	RS_MATRIX3 tmp;
	RS_VECTOR3 camera_one = {{1.0}, {1.0}, {1.0} };

	RS_MATRIX3 pcs_to_xyz_dia = vector3_as_diagonal(&XYZ_WP_D50);
	RS_VECTOR3 xyz = vector3_multiply_matrix(&camera_one, matrix);
	RS_MATRIX3 xyz_as_dia = vector3_as_diagonal(&xyz);
	RS_MATRIX3 xyz_as_dia_inv = matrix3_invert(&xyz_as_dia);

	matrix3_multiply(&pcs_to_xyz_dia, &xyz_as_dia_inv, &tmp);
	matrix3_multiply(&tmp, matrix, matrix);
}

/* dng_color_spec::SetWhiteXY */
static void
set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy)
{
	RS_MATRIX3 color_matrix;
	RS_MATRIX3 forward_matrix;

	dcp->white_xy = *xy;

	color_matrix = find_xyz_to_camera(dcp, xy, &forward_matrix);

	RS_XYZ_VECTOR white = xy_to_XYZ(xy);

	dcp->camera_white = vector3_multiply_matrix(&white, &color_matrix);

	gfloat white_scale = 1.0 / vector3_max(&dcp->camera_white);

	dcp->camera_white.x = CLAMP(0.001, white_scale * dcp->camera_white.x, 1.0);
	dcp->camera_white.y = CLAMP(0.001, white_scale * dcp->camera_white.y, 1.0);
	dcp->camera_white.z = CLAMP(0.001, white_scale * dcp->camera_white.z, 1.0);

	if (dcp->has_forward_matrix1 || dcp->has_forward_matrix2)
	{
		/* verified by DNG SDK */
		RS_MATRIX3 refCameraWhite_diagonal = vector3_as_diagonal(&dcp->camera_white);

		RS_MATRIX3 refCameraWhite_diagonal_inv = matrix3_invert(&refCameraWhite_diagonal); /* D */
		matrix3_multiply(&forward_matrix, &refCameraWhite_diagonal_inv, &dcp->camera_to_pcs);
	}
	else
	{
		/* FIXME: test this */
		RS_xy_COORD PCStoXY = {0.3457, 0.3585};
		RS_MATRIX3 map = rs_calculate_map_white_matrix(&PCStoXY, xy); /* or &white?! */
		RS_MATRIX3 pcs_to_camera;
		matrix3_multiply(&color_matrix, &map, &pcs_to_camera);
		RS_VECTOR3 tmp = vector3_multiply_matrix(&XYZ_WP_D50, &pcs_to_camera);
		gfloat scale = vector3_max(&tmp);
		matrix3_scale(&pcs_to_camera, 1.0 / scale, &pcs_to_camera);
		dcp->camera_to_pcs = matrix3_invert(&pcs_to_camera);
	}

}

static void
precalc(RSDcp *dcp)
{
	const static RS_MATRIX3 xyz_to_prophoto = {{
		{  1.3459433, -0.2556075, -0.0511118 },
		{ -0.5445989,  1.5081673,  0.0205351 },
		{  0.0000000,  0.0000000,  1.2118128 }
	}};

	/* Camera to ProPhoto */
	matrix3_multiply(&xyz_to_prophoto, &dcp->camera_to_pcs, &dcp->camera_to_prophoto); /* verified by SDK */
}

static void
read_profile(RSDcp *dcp, RSDcpFile *dcp_file)
{
	/* ColorMatrix */
	dcp->has_color_matrix1 = rs_dcp_file_get_color_matrix1(dcp_file, &dcp->color_matrix1);
	dcp->has_color_matrix2 = rs_dcp_file_get_color_matrix2(dcp_file, &dcp->color_matrix2);

	/* CalibrationIlluminant */
	dcp->temp1 = rs_dcp_file_get_illuminant1(dcp_file);
	dcp->temp2 = rs_dcp_file_get_illuminant2(dcp_file);

	/* ProfileToneCurve */
	dcp->baseline_exposure = rs_dcp_file_get_tonecurve(dcp_file);
	if (dcp->baseline_exposure)
		dcp->baseline_exposure_lut = rs_spline_sample(dcp->baseline_exposure, NULL, 65536);
	/* FIXME: Free these at some point! */

	/* ForwardMatrix */
	dcp->has_forward_matrix1 = rs_dcp_file_get_forward_matrix1(dcp_file, &dcp->forward_matrix1);
	dcp->has_forward_matrix2 = rs_dcp_file_get_forward_matrix2(dcp_file, &dcp->forward_matrix2);
	if (dcp->has_forward_matrix1)
		normalize_forward_matrix(&dcp->forward_matrix1);
	if (dcp->has_forward_matrix2)
		normalize_forward_matrix(&dcp->forward_matrix2);

	dcp->looktable = rs_dcp_file_get_looktable(dcp_file);

	dcp->huesatmap1 = rs_dcp_file_get_huesatmap1(dcp_file);
	dcp->huesatmap2 = rs_dcp_file_get_huesatmap2(dcp_file);
	dcp->huesatmap = dcp->huesatmap2; /* FIXME: Interpolate this! */
}

static RSIccProfile *
get_icc_profile(RSFilter *filter)
{
	/* We discard all earlier profiles before returning our own ProPhoto profile */
	return g_object_ref(RS_DCP_GET_CLASS(filter)->prophoto_profile);
}

/*
+ 0xc621 ColorMatrix1 (9 * SRATIONAL)
+ 0xc622 ColorMatrix2 (9 * SRATIONAL)
+ 0xc725 ReductionMatrix1 (9 * SRATIONAL)
+ 0xc726 ReductionMatrix2 (9 * SRATIONAL)
+ 0xc65a CalibrationIlluminant1 (1 * SHORT)
+ 0xc65b CalibrationIlluminant2 (1 * SHORT)
• 0xc6f4 ProfileCalibrationSignature (ASCII or BYTE)
• 0xc6f8 ProfileName (ASCII or BYTE)
• 0xc6f9 ProfileHueSatMapDims (3 * LONG)
• 0xc6fa ProfileHueSatMapData1 (FLOAT)
• 0xc6fb ProfileHueSatMapData2 (FLOAT)
• 0xc6fc ProfileToneCurve (FLOAT)
• 0xc6fd ProfileEmbedPolicy (LONG)
• 0xc6fe ProfileCopyright (ASCII or BYTE)
+ 0xc714 ForwardMatrix1 (SRATIONAL)
+ 0xc715 ForwardMatrix2 (SRATIONAL)
• 0xc725 ProfileLookTableDims (3 * LONG)
• 0xc726 ProfileLookTableData
*/
