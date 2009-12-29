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

#include <rawstudio.h>
#include <lensfun.h>
#if defined (__SSE2__)
#include <emmintrin.h>
#endif /* __SSE2__ */
#include <rs-lens.h>
#include "rs-lensfun-select.h"

#define RS_TYPE_LENSFUN (rs_lensfun_type)
#define RS_LENSFUN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LENSFUN, RSLensfun))
#define RS_LENSFUN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LENSFUN, RSLensfunClass))
#define RS_IS_LENSFUN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LENSFUN))

typedef struct _RSLensfun RSLensfun;
typedef struct _RSLensfunClass RSLensfunClass;

struct _RSLensfun {
	RSFilter parent;

	lfDatabase *ldb;

	gchar *make;
	gchar *model;
	RSLens *lens;
	gchar *lens_make;
	gchar *lens_model;
	gfloat focal;
	gfloat aperture;
	gfloat tca_kr;
	gfloat tca_kb;
	gfloat vignetting_k1;
	gfloat vignetting_k2;
	gfloat vignetting_k3;

	lfLens *selected_lens;
	const lfCamera *selected_camera;

	gboolean DIRTY;
};

struct _RSLensfunClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_lensfun, RSLensfun)

enum {
	PROP_0,
	PROP_MAKE,
	PROP_MODEL,
	PROP_LENS,
	PROP_LENS_MAKE,
	PROP_LENS_MODEL,
	PROP_FOCAL,
	PROP_APERTURE,
	PROP_TCA_KR,
	PROP_TCA_KB,
	PROP_VIGNETTING_K1,
	PROP_VIGNETTING_K2,
	PROP_VIGNETTING_K3,
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static void inline rs_image16_nearest_full(RS_IMAGE16 *in, gushort *out, gfloat *pos);
static void inline rs_image16_bilinear_full(RS_IMAGE16 *in, gushort *out, gfloat *pos);
#if defined (__SSE2__)
static void inline rs_image16_bilinear_full_sse2(RS_IMAGE16 *in, gushort *out, gfloat *pos);
#endif
static RSFilterClass *rs_lensfun_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_lensfun_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_lensfun_class_init(RSLensfunClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_lensfun_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_MAKE, g_param_spec_string(
			"make", "make", "The make of the camera (ie. \"Canon\")",
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_MODEL, g_param_spec_string(
			"model", "model", "The model of the camera (ie. \"Canon EOS 20D\")",
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_LENS, g_param_spec_object(
			"lens", "lens", "A RSLens object describing the lens",
			RS_TYPE_LENS, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_LENS_MAKE, g_param_spec_string(
			"lens_make", "lens_make", "The make of the lens (ie. \"Canon\")",
			NULL, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_LENS_MODEL, g_param_spec_string(
			"lens_model", "lens_model", "The model of the lens (ie. \"Canon\")",
			NULL, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_FOCAL, g_param_spec_float(
			"focal", "focal", "focal",
			0.0, G_MAXFLOAT, 50.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_APERTURE, g_param_spec_float(
			"aperture", "aperture", "aperture",
			1.0, G_MAXFLOAT, 5.6, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_TCA_KR, g_param_spec_float(
			"tca_kr", "tca_kr", "tca_kr",
			-1, 1, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_TCA_KB, g_param_spec_float(
			"tca_kb", "tca_kb", "tca_kb",
			-1, 1, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_VIGNETTING_K1, g_param_spec_float(
			"vignetting_k1", "vignetting_k1", "vignetting_k1",
			-1, 2, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_VIGNETTING_K2, g_param_spec_float(
			"vignetting_k2", "vignetting_k2", "vignetting_k2",
			-1, 2, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_VIGNETTING_K3, g_param_spec_float(
			"vignetting_k3", "vignetting_k3", "vignetting_k3",
			-1, 2, 0.0, G_PARAM_READWRITE)
	);

	filter_class->name = "Lensfun filter";
	filter_class->get_image = get_image;
}

static void
rs_lensfun_init(RSLensfun *lensfun)
{
	lensfun->make = NULL;
	lensfun->model = NULL;
	lensfun->lens = NULL;
	lensfun->lens_make = NULL;
	lensfun->lens_model = NULL;
	lensfun->focal = 50.0; /* Well... */
	lensfun->aperture = 5.6;
	lensfun->tca_kr = 0.0;
	lensfun->tca_kb = 0.0;
	lensfun->vignetting_k1 = 0.0;
	lensfun->vignetting_k2 = 0.0;
	lensfun->vignetting_k3 = 0.0;

	/* Initialize Lensfun database */
	lensfun->ldb = lf_db_new ();
	lf_db_load (lensfun->ldb);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSLensfun *lensfun = RS_LENSFUN(object);

	switch (property_id)
	{
		case PROP_MAKE:
			g_value_set_string(value, lensfun->make);
			break;
		case PROP_MODEL:
			g_value_set_string(value, lensfun->model);
			break;
		case PROP_LENS:
			g_value_set_object(value, lensfun->lens);
			break;
		case PROP_LENS_MAKE:
			g_value_set_string(value, lensfun->lens_make);
			break;
		case PROP_LENS_MODEL:
			g_value_set_string(value, lensfun->lens_model);
			break;
		case PROP_FOCAL:
			g_value_set_float(value, lensfun->focal);
			break;
		case PROP_APERTURE:
			g_value_set_float(value, lensfun->aperture);
			break;
		case PROP_TCA_KR:
			g_value_set_float(value, lensfun->tca_kr);
			break;
		case PROP_TCA_KB:
			g_value_set_float(value, lensfun->tca_kb);
			break;
		case PROP_VIGNETTING_K1:
			g_value_set_float(value, lensfun->vignetting_k1);
			break;
		case PROP_VIGNETTING_K2:
			g_value_set_float(value, lensfun->vignetting_k2);
			break;
		case PROP_VIGNETTING_K3:
			g_value_set_float(value, lensfun->vignetting_k3);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSLensfun *lensfun = RS_LENSFUN(object);

	switch (property_id)
	{
		case PROP_MAKE:
			g_free(lensfun->make);
			lensfun->make = g_value_dup_string(value);
			lensfun->DIRTY = TRUE;
			break;
		case PROP_MODEL:
			g_free(lensfun->model);
			lensfun->model = g_value_dup_string(value);
			lensfun->DIRTY = TRUE;
			break;
		case PROP_LENS:
			if (lensfun->lens)
				g_object_unref(lensfun->lens);
			lensfun->lens = g_value_dup_object(value);
			lensfun->DIRTY = TRUE;
			break;
		case PROP_FOCAL:
			lensfun->focal = g_value_get_float(value);
			break;
		case PROP_APERTURE:
			lensfun->aperture = g_value_get_float(value);
			break;
		case PROP_TCA_KR:
			lensfun->tca_kr = g_value_get_float(value);
			rs_filter_changed(RS_FILTER(lensfun), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_TCA_KB:
			lensfun->tca_kb = g_value_get_float(value);
			rs_filter_changed(RS_FILTER(lensfun), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_VIGNETTING_K1:
			lensfun->vignetting_k1 = g_value_get_float(value);
			rs_filter_changed(RS_FILTER(lensfun), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_VIGNETTING_K2:
			/* FIXME: only have one vignetting input */
			lensfun->vignetting_k1 = g_value_get_float(value);
			lensfun->vignetting_k2 = g_value_get_float(value);
			rs_filter_changed(RS_FILTER(lensfun), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_VIGNETTING_K3:
			lensfun->vignetting_k3 = g_value_get_float(value);
			rs_filter_changed(RS_FILTER(lensfun), RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

typedef struct {
	gint start_y;
	gint end_y;
	lfModifier *mod;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	GThread *threadid;
	gint effective_flags;
	GdkRectangle *roi;
	gint stage;	
} ThreadInfo;

static gpointer
thread_func(gpointer _thread_info)
{
	gint x, y;
	ThreadInfo* t = _thread_info;

	if (t->stage == 2) 
	{
		/* Do lensfun vignetting */
		if (t->effective_flags & LF_MODIFY_VIGNETTING)
		{
			lf_modifier_apply_color_modification (t->mod, GET_PIXEL(t->input, t->roi->x, t->start_y), 
				t->roi->x, t->start_y, t->roi->width, t->end_y - t->start_y,
				LF_CR_4 (RED, GREEN, BLUE, UNKNOWN),
				t->input->rowstride*2);
		}
	}

	if (t->stage == 3) 
	{
		/* Do TCA and distortion */
		gfloat *pos = g_new0(gfloat, t->input->w*6);
		const gint pixelsize = t->output->pixelsize;
		
		for(y = t->start_y; y < t->end_y; y++)
		{
			gushort *target;
			lf_modifier_apply_subpixel_geometry_distortion(t->mod, t->roi->x, (gfloat) y, t->roi->width, 1, pos);
			target = GET_PIXEL(t->output, t->roi->x, y);
			gfloat* l_pos = pos;

			for(x = 0; x < t->roi->width ; x++)
			{
#if defined (__SSE2__)
				rs_image16_bilinear_full_sse2(t->input, target, l_pos);
#else
				rs_image16_bilinear_full(t->input, target, l_pos);
#endif
				target += pixelsize;
				l_pos += 6;
			}
		}
		g_free(pos);
	}
	return NULL;
}


static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSLensfun *lensfun = RS_LENSFUN(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	const gchar *make = NULL;
	const gchar *model = NULL;
	GdkRectangle *roi;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	if (rs_filter_request_get_quick(request))
	{
		rs_filter_response_set_quick(response);
		if (input)
		{
			rs_filter_response_set_image(response, input);
			g_object_unref(input);
		}
		return response;
	}

	if (!RS_IS_IMAGE16(input))
		return response;

	gint i;

	if (!lensfun->ldb)
	{
		g_warning ("Failed to create database");
		rs_filter_response_set_image(response, input);
		g_object_unref(input);
		return response;
	}

	if(lensfun->DIRTY)
	{
		if (lensfun->selected_lens)
			lf_free(lensfun->selected_lens);

		const lfCamera **cameras = NULL;
		const lfLens **lenses = NULL;
		lensfun->selected_camera = NULL;
		lensfun->selected_lens = NULL;

		if (lensfun->make && lensfun->model)
			cameras = lf_db_find_cameras(lensfun->ldb, lensfun->make, lensfun->model);

		if (cameras)
		{
			/* FIXME: selecting first camera */
			lensfun->selected_camera = cameras [0];
			lf_free (cameras);

			if (rs_lens_get_lensfun_model(lensfun->lens))
			{
				model = rs_lens_get_lensfun_model(lensfun->lens);
				make = rs_lens_get_lensfun_make(lensfun->lens);
				lenses = lf_db_find_lenses_hd(lensfun->ldb, lensfun->selected_camera, make, model, 0);
				if (lenses)
				{
					lensfun->selected_lens = lf_lens_new();
					/* FIXME: selecting first lens */					
					lf_lens_copy(lensfun->selected_lens, lenses [0]);
					lf_free (lenses);
				}
			}
		} else 
		{
			g_debug("Lensfun: Camera not found. Using camera from same manufacturer.");
			/* Try same manufacturer to be able to use CA-correction and vignetting */
			cameras = lf_db_find_cameras(lensfun->ldb, lensfun->make, NULL);
			if (cameras)
			{
				lensfun->selected_camera = cameras [0];
				lf_free (cameras);
			}
		}

		
		if (!lensfun->selected_lens && lensfun->selected_camera)
		{
			g_debug("Lensfun: Lens not found. Using neutral lense.");
			
			if (ABS(lensfun->tca_kr) + ABS(lensfun->tca_kb) +
				ABS(lensfun->vignetting_k1) + ABS(lensfun->vignetting_k2) + ABS(lensfun->vignetting_k3)
				< 0.001) 
			{
				rs_filter_response_set_image(response, input);
				g_object_unref(input);
				return response;
			}
			lfLens* lens = lf_lens_new ();
			lens->Model = lensfun->model;
			lens->MinFocal = 10.0;
			lens->MaxFocal = 1000.0;
			lens->MinAperture = 1.0;
			lens->MaxAperture = 50.0;
			lensfun->selected_lens = lens;
			/* FIXME: It doesn't really seem to use this, at least we'll know when it does ;)*/
			lens->Mounts = (char**)1;
		}

		lensfun->DIRTY = FALSE;
	}
	
	roi = rs_filter_request_get_roi(request);
	gboolean destroy_roi = FALSE;
	if (!roi) 
	{
		roi = g_new(GdkRectangle, 1);
		roi->x = 0;
		roi->y = 0;
		roi->width = input->w;
		roi->height = input->h;
		destroy_roi = TRUE;
	}
	
	/* Proceed if we got everything */
	if (lensfun->selected_lens && lf_lens_check((lfLens *) lensfun->selected_lens))
	{
		gint effective_flags;

		printf("CA(R): %f, CA(B):%f, VIGN(K1): %f\n", lensfun->tca_kr, lensfun->tca_kb, lensfun->vignetting_k1);

		/* Set TCA */
		if (ABS(lensfun->tca_kr) > 0.01f || ABS(lensfun->tca_kb) > 0.01f) 
		{
			lfLensCalibTCA tca;
			tca.Model = LF_TCA_MODEL_LINEAR;
			const char *details = NULL;
			const lfParameter **params = NULL;
			lf_get_tca_model_desc (tca.Model, &details, &params);
			tca.Terms[0] = (lensfun->tca_kr/100)+1;
			tca.Terms[1] = (lensfun->tca_kb/100)+1;
			lf_lens_add_calib_tca((lfLens *) lensfun->selected_lens, (lfLensCalibTCA *) &tca);
		} else
		{
			lf_lens_remove_calib_tca(lensfun->selected_lens, 0);
			lf_lens_remove_calib_tca(lensfun->selected_lens, 1);
		}

		/* Set vignetting */
		if (ABS(lensfun->vignetting_k1) > 0.01f || ABS(lensfun->vignetting_k2) > 0.01f)
		{
			lfLensCalibVignetting vignetting;
			vignetting.Model = LF_VIGNETTING_MODEL_PA;
			vignetting.Distance = 1.0;
			vignetting.Focal = lensfun->focal;
			vignetting.Aperture = lensfun->aperture;
			vignetting.Terms[0] = -lensfun->vignetting_k1 * 0.5;
			vignetting.Terms[1] = -lensfun->vignetting_k2 * -0.125;
			vignetting.Terms[2] = lensfun->vignetting_k3;
			lf_lens_add_calib_vignetting((lfLens *) lensfun->selected_lens, &vignetting);
		} else
		{
			lf_lens_remove_calib_vignetting(lensfun->selected_lens, 0);
			lf_lens_remove_calib_vignetting(lensfun->selected_lens, 1);
			lf_lens_remove_calib_vignetting(lensfun->selected_lens, 2);
		}

		lfModifier *mod = lf_modifier_new (lensfun->selected_lens, lensfun->selected_camera->CropFactor, input->w, input->h);
		effective_flags = lf_modifier_initialize (mod, lensfun->selected_lens,
			LF_PF_U16, /* lfPixelFormat */
			lensfun->focal, /* focal */
			lensfun->aperture, /* aperture */
			1.0, /* distance */
			1.0, /* scale */
			LF_UNKNOWN, /* lfLensType targeom, */ /* FIXME: ? */
			LF_MODIFY_ALL, /* flags */ /* FIXME: ? */
			FALSE); /* reverse */

		/* Print flags used */
		GString *flags = g_string_new("");
		if (effective_flags & LF_MODIFY_TCA)
			g_string_append(flags, " LF_MODIFY_TCA");
		if (effective_flags & LF_MODIFY_VIGNETTING)
			g_string_append(flags, " LF_MODIFY_VIGNETTING");
		if (effective_flags & LF_MODIFY_CCI)
			g_string_append(flags, " LF_MODIFY_CCI");
		if (effective_flags & LF_MODIFY_DISTORTION)
			g_string_append(flags, " LF_MODIFY_DISTORTION");
		if (effective_flags & LF_MODIFY_GEOMETRY)
			g_string_append(flags, " LF_MODIFY_GEOMETRY");
		if (effective_flags & LF_MODIFY_SCALE)
			g_string_append(flags, " LF_MODIFY_SCALE");
		g_debug("Effective flags:%s", flags->str);
		g_string_free(flags, TRUE);

			
		if (effective_flags > 0)
		{
			guint y_offset, y_per_thread, threaded_h;
			const guint threads = rs_get_number_of_processor_cores();
			ThreadInfo *t = g_new(ThreadInfo, threads);
			threaded_h = roi->height;
			y_per_thread = (threaded_h + threads-1)/threads;
			y_offset = roi->y;

			/* Set up job description for individual threads */
			for (i = 0; i < threads; i++)
			{
				t[i].mod = mod;
				t[i].start_y = y_offset;
				y_offset += y_per_thread;
				y_offset = MIN(roi->y + roi->height, y_offset);
				t[i].end_y = y_offset;
				t[i].effective_flags = effective_flags;
				t[i].roi = roi;
			}

			/* Start threads to apply phase 2, Vignetting and CA Correction */
			if (effective_flags & (LF_MODIFY_VIGNETTING | LF_MODIFY_CCI)) 
			{
				/* Phase 2 is corrected inplace, so copy input first */
				output = rs_image16_copy(input, TRUE);
				g_object_unref(input);
				for (i = 0; i < threads; i++)
				{
					t[i].input = t[i].output = output;
					t[i].stage = 2;
					t[i].threadid = g_thread_create(thread_func, &t[i], TRUE, NULL);
				}
				
				/* Wait for threads to finish */
				for(i = 0; i < threads; i++)
					g_thread_join(t[i].threadid);

				input = output;
			}
			
			/* Start threads to apply phase 1+3, Chromatic abberation and distortion Correction */
			if (effective_flags & (LF_MODIFY_TCA | LF_MODIFY_DISTORTION | LF_MODIFY_GEOMETRY)) 
			{
				output = rs_image16_copy(input, FALSE);
				for (i = 0; i < threads; i++)
				{
					t[i].input = input;
					t[i].output = output;
					t[i].stage = 3;
					t[i].threadid = g_thread_create(thread_func, &t[i], TRUE, NULL);
				}
				
				/* Wait for threads to finish */
				for(i = 0; i < threads; i++)
					g_thread_join(t[i].threadid);
			}
			else
			{
				output = rs_image16_copy(input, TRUE);
			}			
			g_free(t);
			rs_filter_response_set_image(response, output);
			g_object_unref(output);
		}
		else
			rs_filter_response_set_image(response, input);

		lf_modifier_destroy(mod);
	}
	else
	{
		g_debug("lf_lens_check() failed");
		rs_filter_response_set_image(response, input);
	}
	
	if (destroy_roi)
		g_free(roi);

	g_object_unref(input);
	return response;
}

static void inline
rs_image16_nearest_full(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	gint ipos[6];
	gint i;
	for (i = 0; i < 6; i+=2)
	{
		ipos[i] = CLAMP((gint)pos[i], 0, in->w-1);
		ipos[i+1] = CLAMP((gint)pos[i+1], 0, in->h-1);
	}
	out[R] = GET_PIXEL(in, ipos[0], ipos[1])[R];
	out[G] = GET_PIXEL(in, ipos[2], ipos[3])[G];
	out[B] = GET_PIXEL(in, ipos[4], ipos[5])[B];
}

static void inline
rs_image16_bilinear_full(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	gint ipos_x, ipos_y ;
	gint i;
	gint m_w = (in->w-1);
	gint m_h = (in->h-1);
	for (i = 0; i < 3; i++)
	{
		ipos_x = CLAMP((gint)(pos[i*2]*256.0f), 0, m_w << 8);
		ipos_y = CLAMP((gint)(pos[i*2+1]*256.0f), 0, m_h << 8);

		/* Calculate next pixel offset */
		const gint nx = MIN((ipos_x>>8) + 1, m_w);
		const gint ny = MIN((ipos_y>>8) + 1, m_h);

		gushort* a = GET_PIXEL(in, ipos_x>>8, ipos_y>>8);
		gushort* b = GET_PIXEL(in, nx , ipos_y>>8);
		gushort* c = GET_PIXEL(in, ipos_x>>8, ny);
		gushort* d = GET_PIXEL(in, nx, ny);

		/* Calculate distances */
		const gint diffx = ipos_x & 0xff; /* x distance from a */
		const gint diffy = ipos_y & 0xff; /* y distance fromy a */
		const gint inv_diffx = 256 - diffx; /* inverse x distance from a */
		const gint inv_diffy = 256 - diffy; /* inverse y distance from a */

		/* Calculate weightings */
		const gint aw = (inv_diffx * inv_diffy) >> 1;  /* Weight is now 0.15 fp */
		const gint bw = (diffx * inv_diffy) >> 1;
		const gint cw = (inv_diffx * diffy) >> 1;
		const gint dw = (diffx * diffy) >> 1;

		out[i]  = (gushort) ((a[i]*aw  + b[i]*bw  + c[i]*cw  + d[i]*dw + 16384) >> 15 );
	}
}


#if defined (__SSE2__)
static gfloat twofiftytwo_ps[4] __attribute__ ((aligned (16))) = {256.0f, 256.0f, 256.0f, 0.0f};
		
static void inline
rs_image16_bilinear_full_sse2(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	const gint m_w = (in->w-1);
	const gint m_h = (in->h-1);

	__m128 p0, p1;
	if ((uintptr_t)pos & 15)
	{
		p0 = _mm_loadu_ps(pos);		// y1x1 y0x0
		p1 = _mm_loadu_ps(pos+4);	// ---- y2x2
	} else 
	{
		p0 = _mm_load_ps(pos);		// y1x1 y0x0
		p1 = _mm_load_ps(pos+4);	// ---- y2x2
	}
		
	__m128 xf = _mm_shuffle_ps(p1, p0, _MM_SHUFFLE(0,2,2,0));
	__m128 yf = _mm_shuffle_ps(p1, p0, _MM_SHUFFLE(1,3,1,1));
			
	__m128 fl256 = _mm_load_ps(twofiftytwo_ps);
	xf = _mm_mul_ps(xf, fl256);
	yf = _mm_mul_ps(yf, fl256);
	__m128i x = _mm_cvttps_epi32(xf);
	__m128i y = _mm_cvttps_epi32(yf);

	__m128i _m_w = _mm_slli_epi32(_mm_set1_epi32(m_w), 8);
	__m128i _m_h = _mm_slli_epi32(_mm_set1_epi32(m_h), 8);
	
	__m128i x_gt, y_gt;
	
	/* If positions from lensfun is properly clamped this should not be needed */
	/* Enable, if crashes begin occuring here here */
#if 0
	x_gt = _mm_cmpgt_epi32(x, _m_w);
	y_gt = _mm_cmpgt_epi32(y, _m_h);
	
	x = _mm_or_si128(_mm_andnot_si128(x_gt, x), _mm_and_si128(_m_w, x_gt));
	y = _mm_or_si128(_mm_andnot_si128(y_gt, y), _mm_and_si128(_m_h, y_gt));

	__m128i zero = _mm_setzero_si128();
	__m128i x_lt = _mm_cmplt_epi32(x, zero);
	__m128i y_lt = _mm_cmplt_epi32(y, zero);
	x = _mm_andnot_si128(x_lt, x);
	y = _mm_andnot_si128(y_lt, y);
#endif
	__m128i one = _mm_set1_epi32(1);
	__m128i nx = _mm_add_epi32(one, _mm_srai_epi32(x, 8));
	__m128i ny = _mm_add_epi32(one, _mm_srai_epi32(y, 8));

	_m_w = _mm_srai_epi32(_m_w, 8);
	_m_h = _mm_srai_epi32(_m_h, 8);

	x_gt = _mm_cmpgt_epi32(nx, _m_w);
	y_gt = _mm_cmpgt_epi32(ny, _m_h);
	
	nx = _mm_or_si128(_mm_andnot_si128(x_gt, nx), _mm_and_si128(_m_w, x_gt));
	ny = _mm_or_si128(_mm_andnot_si128(y_gt, ny), _mm_and_si128(_m_h, y_gt));

	int xfer[16] __attribute__ ((aligned (16)));

	_mm_store_si128((__m128i*)xfer, _mm_srai_epi32(x, 8));
	_mm_store_si128((__m128i*)&xfer[4], _mm_srai_epi32(y, 8));
	_mm_store_si128((__m128i*)&xfer[8], nx);
	_mm_store_si128((__m128i*)&xfer[12], ny);
	
	gushort* pixels[12];
	
	/* Loop unrolled, allows agressive instruction reordering */
	/* Red, then G & B */
	pixels[0] = GET_PIXEL(in, xfer[0], xfer[4]); 	// a
	pixels[1] = GET_PIXEL(in, xfer[8], xfer[4]);	// b
	pixels[2] = GET_PIXEL(in, xfer[0], xfer[12]);	// c
	pixels[3] = GET_PIXEL(in, xfer[8], xfer[12]);	// d
		
	pixels[4] = GET_PIXEL(in, xfer[1], xfer[1+4]) + 1; 		// a
	pixels[4+1] = GET_PIXEL(in, xfer[1+8], xfer[1+4]) + 1;	// b
	pixels[4+2] = GET_PIXEL(in, xfer[1], xfer[1+12]) + 1;	// c
	pixels[4+3] = GET_PIXEL(in, xfer[1+8], xfer[1+12]) + 1;	// d

	pixels[2*4] = GET_PIXEL(in, xfer[2], xfer[2+4]) + 2; 		// a
	pixels[2*4+1] = GET_PIXEL(in, xfer[2+8], xfer[2+4]) + 2;	// b
	pixels[2*4+2] = GET_PIXEL(in, xfer[2], xfer[2+12]) + 2;		// c
	pixels[2*4+3] = GET_PIXEL(in, xfer[2+8], xfer[2+12]) + 2;	// d

	/* Calculate distances */
	__m128i twofiftyfive = _mm_set1_epi32(255);
	__m128i diffx = _mm_and_si128(x, twofiftyfive);	
	__m128i diffy = _mm_and_si128(y, twofiftyfive);	
	__m128i inv_diffx = _mm_andnot_si128(diffx, twofiftyfive);
	__m128i inv_diffy = _mm_andnot_si128(diffy, twofiftyfive);

	/* Calculate weights */
	__m128i aw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, inv_diffy),1);
	__m128i bw = _mm_srai_epi32(_mm_mullo_epi16(diffx, inv_diffy),1);
	__m128i cw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, diffy),1);
	__m128i dw = _mm_srai_epi32(_mm_mullo_epi16(diffx, diffy),1);

	_mm_store_si128((__m128i*)xfer, aw);
	_mm_store_si128((__m128i*)&xfer[4], bw);
	_mm_store_si128((__m128i*)&xfer[8], cw);
	_mm_store_si128((__m128i*)&xfer[12], dw);
	
	gushort** p = pixels;
	/* Loop unrolled */
	out[0]  = (gushort) ((xfer[0] * *p[0] + xfer[4] * *p[1] + xfer[8] * *p[2] + xfer[12] * *p[3]  + 16384) >> 15 );
	p+=4;
	out[1]  = (gushort) ((xfer[1] * *p[0] + xfer[1+4] * *p[1] + xfer[1+8] * *p[2] + xfer[1+12] * *p[3]  + 16384) >> 15 );
	p+=4;
	out[2]  = (gushort) ((xfer[2] * *p[0] + xfer[2+4] * *p[1] + xfer[2+8] * *p[2] + xfer[2+12] * *p[3]  + 16384) >> 15 );
}

#endif // defined (__SSE2__)
