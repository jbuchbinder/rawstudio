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

#ifndef RS_SETTINGS_H
#define RS_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_SETTINGS rs_settings_get_type()

#define RS_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_SETTINGS, RSSettings))
#define RS_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_SETTINGS, RSSettingsClass))
#define RS_IS_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_SETTINGS))
#define RS_IS_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_SETTINGS))
#define RS_SETTINGS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_SETTINGS, RSSettingsClass))

typedef enum {
	MASK_EXPOSURE   = (1<<0),
	MASK_SATURATION = (1<<1),
	MASK_HUE        = (1<<2),
	MASK_CONTRAST   = (1<<3),
	MASK_WARMTH     = (1<<4),
	MASK_TINT       = (1<<5),
	MASK_WB         = MASK_WARMTH | MASK_TINT,
	MASK_CURVE      = (1<<6),
	MASK_SHARPEN    = (1<<7),
	MASK_ALL        = 0x00ffffff,
} RSSettingsMask;

typedef struct _RSsettings {
	GObject parent;
	gint commit;
	RSSettingsMask commit_todo;
	gfloat exposure;
	gfloat saturation;
	gfloat hue;
	gfloat contrast;
	gfloat warmth;
	gfloat tint;
	gfloat sharpen;
	gint curve_nknots;
	gfloat *curve_knots;
} RSSettings;

typedef struct {
  GObjectClass parent_class;
} RSSettingsClass;

GType rs_settings_get_type (void);

RSSettings *rs_settings_new (void);

/**
 * Reset a RSSettings
 * @param settings A RSSettings
 * @param mask A mask for only resetting some values 
 */
extern void rs_settings_reset(RSSettings *settings, const RSSettingsMask mask);

/**
 * Stop signal emission from a RSSettings and queue up signals
 * @param settings A RSSettings
 */
extern void rs_settings_commit_start(RSSettings *settings);

/**
 * Restart signal emission and process signal queue if any
 * @param settings A RSSettings
 * @return The mask of changes since rs_settings_commit_start()
 */
extern RSSettingsMask rs_settings_commit_stop(RSSettings *settings);

/**
 * Copy settings from one RSSettins to another
 * @param source The source RSSettings
 * @param mask A RSSettingsMask to do selective copying
 * @param target The target RSSettings
 */
extern RSSettingsMask rs_settings_copy(RSSettings *source, const RSSettingsMask mask, RSSettings *target);

/**
 * Set the exposure value of a RSSettings
 * @param settings A RSSettings
 * @param exposure New value
 * @return Old value
 */
extern gfloat rs_settings_set_exposure(RSSettings *settings, const gfloat exposure);

/**
 * Set the saturation value of a RSSettings
 * @param settings A RSSettings
 * @param saturation New value
 * @return Old value
 */
extern gfloat rs_settings_set_saturation(RSSettings *settings, const gfloat saturation);

/**
 * Set the hue value of a RSSettings
 * @param settings A RSSettings
 * @param hue New value
 * @return Old value
 */
extern gfloat rs_settings_set_hue(RSSettings *settings, const gfloat hue);

/**
 * Set the contrast value of a RSSettings
 * @param settings A RSSettings
 * @param contrast New value
 * @return Old value
 */
extern gfloat rs_settings_set_contrast(RSSettings *settings, const gfloat contrast);

/**
 * Set the warmth value of a RSSettings
 * @param settings A RSSettings
 * @param warmth New value
 * @return Old value
 */
extern gfloat rs_settings_set_warmth(RSSettings *settings, const gfloat warmth);

/**
 * Set the tint value of a RSSettings
 * @param settings A RSSettings
 * @param tint New value
 * @return Old value
 */
extern gfloat rs_settings_set_tint(RSSettings *settings, const gfloat tint);

/**
 * Set the sharpen value of a RSSettings
 * @param settings A RSSettings
 * @param sharpen New value
 * @return Old value
 */
extern gfloat rs_settings_set_sharpen(RSSettings *settings, const gfloat sharpen);

/**
 * Set curve knots
 * @param settings A RSSettings
 * @param knots Knots for curve
 * @param nknots Number of knots
 */
extern void rs_settings_set_curve_knots(RSSettings *settings, const gfloat *knots, const gint nknots);

/**
 * Set the warmth and tint values of a RSSettings
 * @param settings A RSSettings
 * @param exposure New value
 */
extern void rs_settings_set_wb(RSSettings *settings, const gfloat warmth, const gfloat tint);

extern gfloat rs_settings_get_exposure(RSSettings *settings);
extern gfloat rs_settings_get_saturation(RSSettings *settings);
extern gfloat rs_settings_get_hue(RSSettings *settings);
extern gfloat rs_settings_get_contrast(RSSettings *settings);
extern gfloat rs_settings_get_warmth(RSSettings *settings);
extern gfloat rs_settings_get_tint(RSSettings *settings);
extern gfloat rs_settings_get_sharpen(RSSettings *settings);

/**
 * Get the knots from the curve
 * @param settings A RSSettings
 * @return All knots as a newly allocated array
 */
extern gfloat *
rs_settings_get_curve_knots(RSSettings *settings);

/**
 * Get number of knots in curve in a RSSettings
 * @param settings A RSSettings
 * @return Number of knots
 */
extern gint
rs_settings_get_curve_nknots(RSSettings *settings);

/**
 * Use like g_signal_connect(source, "settings-changed", G_CALLBACK(rs_settings_changed), target);
 */
extern void rs_settings_changed(RSSettings *source, const RSSettingsMask mask, RSSettings *target);

/**
 * Link two RSSettings together, if source gets updated, it will propagate to target
 * @param source A RSSettings
 * @param target A RSSettings
 */
extern void rs_settings_link(RSSettings *source, RSSettings *target);

/**
 * Unlink two RSSettings - this will be done automaticly if target from a previous rs_settings_link() is finalized
 * @param source A RSSettings
 * @param target A RSSettings - can be destroyed, doesn't matter, we just need the pointer
 */
extern void rs_settings_unlink(RSSettings *source, RSSettings *target);

G_END_DECLS

#endif /* RS_SETTINGS_H */