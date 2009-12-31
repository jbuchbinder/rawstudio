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

#include <lcms.h>
#include "rs-cmm.h"

struct _RSCmm {
	GObject parent;

	const RSIccProfile *input_profile;
	const RSIccProfile *output_profile;
	gint num_threads;

	gboolean dirty8;
	gboolean dirty16;

	cmsHPROFILE lcms_input_profile;
	cmsHPROFILE lcms_output_profile;

	cmsHTRANSFORM lcms_transform8;
	cmsHTRANSFORM lcms_transform16;
};

G_DEFINE_TYPE (RSCmm, rs_cmm, G_TYPE_OBJECT)

static void load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target);
static void prepare8(RSCmm *cmm);
static void prepare16(RSCmm *cmm);

static void
rs_cmm_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_cmm_parent_class)->dispose (object);
}

static void
rs_cmm_class_init(RSCmmClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = rs_cmm_dispose;
}

static void
rs_cmm_init(RSCmm *cmm)
{
}

RSCmm *
rs_cmm_new(void)
{
	return g_object_new(RS_TYPE_CMM, NULL);
}

void
rs_cmm_set_input_profile(RSCmm *cmm, const RSIccProfile *input_profile)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_ICC_PROFILE(input_profile));

	load_profile(cmm, input_profile, &cmm->input_profile, &cmm->lcms_input_profile);
}

void
rs_cmm_set_output_profile(RSCmm *cmm, const RSIccProfile *output_profile)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_ICC_PROFILE(output_profile));

	load_profile(cmm, output_profile, &cmm->output_profile, &cmm->lcms_output_profile);
}

void
rs_cmm_set_num_threads(RSCmm *cmm, const gint num_threads)
{
	g_assert(RS_IS_CMM(cmm));

	cmm->num_threads = MAX(1, num_threads);
}

gboolean
rs_cmm_transform16(RSCmm *cmm, RS_IMAGE16 *input, RS_IMAGE16 *output)
{
	printf("rs_cms_transform16()\n");
	gint y;
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(RS_IS_IMAGE16(output));

	g_return_val_if_fail(input->w == output->w, FALSE);
	g_return_val_if_fail(input->h == output->h, FALSE);
	g_return_val_if_fail(input->pixelsize == 4, FALSE);

	if (cmm->dirty16)
		prepare16(cmm);

	for(y=0;y<input->h;y++)
	{
		gushort *in = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		cmsDoTransform(cmm->lcms_transform16, in, out, input->w);
	}
	return TRUE;
}

gboolean
rs_cmm_transform8(RSCmm *cmm, RS_IMAGE16 *input, GdkPixbuf *output)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(GDK_IS_PIXBUF(output));

	g_return_val_if_fail(input->w == gdk_pixbuf_get_width(output), FALSE);
	g_return_val_if_fail(input->h == gdk_pixbuf_get_height(output), FALSE);
	g_return_val_if_fail(input->pixelsize == 4, FALSE);

	if (cmm->dirty8)
		prepare8(cmm);

	/* FIXME: Render */
	g_warning("rs_cmm_transform8() is a stub");

	return TRUE;
}

static guchar *
pack_rgb_w4(void *info, register WORD wOut[], register LPBYTE output)
{
	*(LPWORD) output = wOut[0]; output+= 2;
	*(LPWORD) output = wOut[1]; output+= 2;
	*(LPWORD) output = wOut[2]; output+= 4;

	return(output);
}

static guchar *
unroll_rgb_w4(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = *(LPWORD) accum; accum+= 2;
	wIn[1] = *(LPWORD) accum; accum+= 2;
	wIn[2] = *(LPWORD) accum; accum+= 4;

	return(accum);
}

static void
load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target)
{
// DEBUG START
	gchar *filename;
	g_object_get((void *) profile, "filename", &filename, NULL);
	printf("load_profile(%p [%s])\n", profile, filename);
// DEBUG END
	gchar *data;
	gsize length;

	if (*profile_target == profile)
		return;

	*profile_target = profile;

	if (*lcms_target)
		cmsCloseProfile(*lcms_target);

	if (rs_icc_profile_get_data(profile, &data, &length))
		*lcms_target = cmsOpenProfileFromMem(data, length);

	g_warn_if_fail(*lcms_target != NULL);

	cmm->dirty8 = TRUE;
	cmm->dirty16 = TRUE;
	printf("load_profile() DONE\n");
}

static void
prepare8(RSCmm *cmm)
{
}

gdouble
estimate_gamma(cmsHPROFILE *profile)
{
	gdouble gamma_value[3] = {1.0, 1.0, 1.0};
	LPGAMMATABLE gamma[3];

	gamma[0] = cmsReadICCGamma(profile, icSigRedTRCTag);
	gamma[1] = cmsReadICCGamma(profile, icSigGreenTRCTag);
	gamma[2] = cmsReadICCGamma(profile, icSigBlueTRCTag);

	if (gamma[0] && gamma[1] && gamma[2])
	{
		gamma_value[0] = cmsEstimateGamma(gamma[0]);
		gamma_value[1] = cmsEstimateGamma(gamma[1]);
		gamma_value[2] = cmsEstimateGamma(gamma[2]);
	}
	else
		printf("No tables\n");

	return (gamma_value[0] + gamma_value[1] + gamma_value[2]) / 3.0;
}

static void
prepare16(RSCmm *cmm)
{
	if (!cmm->dirty16)
		return;

	if (cmm->lcms_transform16)
		cmsDeleteTransform(cmm->lcms_transform16);

	printf("INPUT GAMMA: %f\n", estimate_gamma(cmm->lcms_input_profile));
	printf("OUTPUT GAMMA: %f\n", estimate_gamma(cmm->lcms_output_profile));

	cmm->lcms_transform16 = cmsCreateTransform(
		cmm->lcms_input_profile, TYPE_RGB_16,
		cmm->lcms_output_profile, TYPE_RGB_16,
		INTENT_PERCEPTUAL, 0);

	g_warn_if_fail(cmm->lcms_transform16 != NULL);

	/* Enable packing/unpacking for pixelsize==4 */
	cmsSetUserFormatters(cmm->lcms_transform16,
		TYPE_RGB_16, unroll_rgb_w4,
		TYPE_RGB_16, pack_rgb_w4);

	cmm->dirty16 = FALSE;
}