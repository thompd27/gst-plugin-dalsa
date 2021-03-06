/* GStreamer Flycap Plugin
 * Copyright (C) 2015 Gray Cancer Institute
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GST_DALSA_SRC_H_
#define _GST_DALSA_SRC_H_

#define NUM_BUF	8

#include <gst/base/gstpushsrc.h>
#include "gevapi.h"				//!< GEV lib definitions.
G_BEGIN_DECLS

#define GST_TYPE_DALSA_SRC   (gst_dalsa_src_get_type())
#define GST_DALSA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DALSA_SRC,GstDalsaSrc))
#define GST_DALSA_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DALSA_SRC,GstDalsaSrcClass))
#define GST_IS_DALSA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DALSA_SRC))
#define GST_IS_DALSA_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DALSA_SRC))

typedef struct _GstDalsaSrc GstDalsaSrc;
typedef struct _GstDalsaSrcClass GstDalsaSrcClass;

typedef enum
{
	GST_WB_MANUAL,
	GST_WB_ONEPUSH,
	GST_WB_AUTO
} WhiteBalanceType;

typedef enum
{
	GST_LUT_OFF,
	GST_LUT_1,
	GST_LUT_2,
	GST_LUT_GAMMA
} LUTType;

struct _GstDalsaSrc
{
  GstPushSrc base_dalsa_src;
  //X_VIEW_HANDLE     View;
	GEV_CAMERA_HANDLE camHandle;
	int					depth;
	int 					format;
	void 					*convertBuffer;
	BOOL					convertFormat;
	BOOL              exit;
  unsigned int cameraID;
  unsigned int width;
  unsigned int height;
  unsigned int bytesPerPixel;
  unsigned int pitch;
  ulong cameraIP;
/*
  // device
  gboolean cameraPresent;
  int lMemId;  // ID of the allocated memory

  unsigned int nBitsPerPixel;
  unsigned int nBytesPerPixel;
     // Stride in bytes between lines
  unsigned int nImageSize;  // Image size in bytes
*/
  //unsigned int nRawWidth;  // because of binning the raw image size may be smaller than nWidth
  //unsigned int nRawHeight;  // because of binning the raw image size may be smaller than nHeight
  //unsigned int nRawPitch;  // because of binning the raw image size may be smaller than nHeight

  gint gst_stride;  // Stride/pitch for the GStreamer buffer

  // gst properties
  gint pixelclock;
  gfloat exposure;     // ms
  gfloat framerate;
  gfloat maxframerate;
  gint gain;           // dB
//  gfloat cam_min_gain, cam_max_gain;  //  min and max settable values for the camera
  gint blacklevel;
  unsigned int rgain;
  unsigned int bgain;
  gint binning;
  gint saturation;
  gint sharpness;
  gint vflip;
  gint hflip;
  WhiteBalanceType whitebalance;
  gboolean WB_in_progress;   // will be >0 when WB in progress, value will be number of frames until we abort WB
  gint WB_progress;   // will be >0 when WB in progress, value will be number of frames until we abort WB
  LUTType lut;
  gint lut_offset[2][3];
  gdouble lut_gain[2];
  gdouble lut_gamma[2];
  gdouble lut_slope[2];
  gdouble lut_linearcutoff[2];
  gdouble lut_outputoffset[2];
  gfloat gamma;

  gboolean exposure_just_changed;
  gboolean gain_just_changed;
  gboolean binning_just_changed;

  // stream
  gboolean acq_started;
  gint n_frames;
  gint total_timeouts;
  GstClockTime duration;
  GstClockTime last_frame_time;

  PUINT8 bufAddress[NUM_BUF];
};

struct _GstDalsaSrcClass
{
  GstPushSrcClass base_dalsa_src_class;
};

GType gst_dalsa_src_get_type (void);

G_END_DECLS

#endif
