/* GStreamer Teledyne Dalsa Plugin
 * Copyright (C) 2019 Embry-Riddle Aeronautical University
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: D Thompson
 *
 */
/**
 * SECTION:element-GstDalsa_src
 *
 * The dalsasrc element is a source for a GigE camera supported by the Teledyne DALSA GigeV Library.
 * A live source, operating in push mode.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 dalsasrc ! videoconvert ! autovideosink
 * ]|
 * </refsect2>
 */

// Which functions of the base class to override. Create must alloc and fill the buffer. Fill just needs to fill it
//#define OVERRIDE_FILL  !!! NOT IMPLEMENTED !!!
#define OVERRIDE_CREATE

//#include <unistd.h> // for usleep
#include <string.h> // for memcpy
#include <math.h>  // for pow
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <gstdalsa.h>

GST_DEBUG_CATEGORY_STATIC (gst_dalsa_src_debug);
#define GST_CAT_DEFAULT gst_dalsa_src_debug

#define MAX_NETIF					8
#define MAX_CAMERAS_PER_NETIF	32
#define MAX_CAMERAS		(MAX_NETIF * MAX_CAMERAS_PER_NETIF)
#define NUM_BUF	8
void *m_latestBuffer = NULL;

/* prototypes */
static void gst_dalsa_src_set_property (GObject * object,
		guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_dalsa_src_get_property (GObject * object,
		guint property_id, GValue * value, GParamSpec * pspec);
static void gst_dalsa_src_dispose (GObject * object);
static void gst_dalsa_src_finalize (GObject * object);

static gboolean gst_dalsa_src_start (GstBaseSrc * src);
static gboolean gst_dalsa_src_stop (GstBaseSrc * src);
static GstCaps *gst_dalsa_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_dalsa_src_set_caps (GstBaseSrc * src, GstCaps * caps);

#ifdef OVERRIDE_CREATE
	static GstFlowReturn gst_dalsa_src_create (GstPushSrc * src, GstBuffer ** buf);
#endif
#ifdef OVERRIDE_FILL
	static GstFlowReturn gst_dalsa_src_fill (GstPushSrc * src, GstBuffer * buf);
#endif

//static GstCaps *gst_dalsa_src_create_caps (GstDalsaSrc * src);
static void gst_dalsa_src_reset (GstDalsaSrc * src);
enum
{
	PROP_0,
	PROP_CAMERA,
	PROP_WIDTH,
	PROP_HEIGHT
};

#define	FLYCAP_UPDATE_LOCAL  FALSE
#define	FLYCAP_UPDATE_CAMERA TRUE

#define DEFAULT_PROP_CAMERA	           0
#define DEFAULT_PROP_EXPOSURE           40.0
#define DEFAULT_PROP_GAIN               1
#define DEFAULT_PROP_BLACKLEVEL         15
#define DEFAULT_PROP_RGAIN              425
#define DEFAULT_PROP_BGAIN              727
#define DEFAULT_PROP_BINNING            1
#define DEFAULT_PROP_SHARPNESS			2    // this is 'normal'
#define DEFAULT_PROP_SATURATION			25   // this is 100 on the camera scale 0-400
#define DEFAULT_PROP_HORIZ_FLIP         0
#define DEFAULT_PROP_VERT_FLIP          0
#define DEFAULT_PROP_WHITEBALANCE       GST_WB_MANUAL
#define DEFAULT_PROP_LUT		        GST_LUT_1
#define DEFAULT_PROP_LUT1_OFFSET		0    
#define DEFAULT_PROP_LUT1_GAMMA		    0.45
#define DEFAULT_PROP_LUT1_GAIN		    1.099
#define DEFAULT_PROP_LUT2_OFFSET		10    
#define DEFAULT_PROP_LUT2_GAMMA		    0.45
#define DEFAULT_PROP_LUT2_GAIN		    1.501   
#define DEFAULT_PROP_MAXFRAMERATE       25
#define DEFAULT_PROP_GAMMA			    1.5
#define DEFAULT_PROP_WIDTH 				640
#define DEFAULT_PROP_HEIGHT			    480

#define DEFAULT_GST_VIDEO_FORMAT GST_VIDEO_FORMAT_GRAY8
#define DEFAULT_FLYCAP_VIDEO_FORMAT FC2_PIXEL_FORMAT_RGB8
// Put matching type text in the pad template below

// pad template
static GstStaticPadTemplate gst_dalsa_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
				GST_PAD_SRC,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
						("{ GRAY8 }"))
		);

#define EXEANDCHECK(function) \
{\
	spinError Ret = function;\
	if (DALSA_ERR_SUCCESS != Ret){\
		GST_ERROR_OBJECT(src, "dalsa call failed: %d", Ret);\
		goto fail;\
	}\
}

G_DEFINE_TYPE_WITH_CODE (GstDalsaSrc, gst_dalsa_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "dalsa", 0,
        "debug category for dalsa element"));

/* class initialisation */
static void
gst_dalsa_src_class_init (GstDalsaSrcClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
	GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
	GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

	GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "GstDalsasrc", 0,
			"dalsa Camera source");

	gobject_class->set_property = gst_dalsa_src_set_property;
	gobject_class->get_property = gst_dalsa_src_get_property;
	gobject_class->dispose = gst_dalsa_src_dispose;
	gobject_class->finalize = gst_dalsa_src_finalize;

	gst_element_class_add_pad_template (gstelement_class,
			gst_static_pad_template_get (&gst_dalsa_src_template));

	gst_element_class_set_static_metadata (gstelement_class,
			"dalsa Video Source", "Source/Video",
			"dalsa Camera video source", "David Thompson <dave@republicofdave.net>");

	gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_dalsa_src_start);
	gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_dalsa_src_stop);
	gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_dalsa_src_get_caps);
	gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_dalsa_src_set_caps);

#ifdef OVERRIDE_CREATE
	gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_dalsa_src_create);
	GST_DEBUG ("Using gst_dalsa_src_create.");
#endif
#ifdef OVERRIDE_FILL
	gstpushsrc_class->fill   = GST_DEBUG_FUNCPTR (gst_dalsa_src_fill);
	GST_DEBUG ("Using gst_dalsa_src_fill.");
#endif
	//camera id property
	g_object_class_install_property (gobject_class, PROP_CAMERA,
		g_param_spec_int("camera-id", "Camera ID", "Camera ID to open.", 0,7, DEFAULT_PROP_CAMERA,
		 (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));
}

static void
init_properties(GstDalsaSrc * src)
{
    //Hardcoded hot-garbage **************************
  src->width = DEFAULT_PROP_WIDTH;
  src->height = DEFAULT_PROP_HEIGHT;
  src->bytesPerPixel = 1;
  src->binning = 1;
  src->n_frames = 0;
  src->framerate = 30;
  src->last_frame_time = 0;
  src->pitch = src->width * src->bytesPerPixel;
  src->gst_stride = src->pitch;
  src->cameraID = DEFAULT_PROP_CAMERA;

}

static void
gst_dalsa_src_init (GstDalsaSrc * src)
{
	/* set source as live (no preroll) */
	gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

	/* override default of BYTES to operate in time mode */
	gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

	init_properties(src);

	gst_dalsa_src_reset (src);
}

static void
gst_dalsa_src_reset (GstDalsaSrc * src)
{
	src->n_frames = 0;
	src->total_timeouts = 0;
	src->last_frame_time = 0;
	src->cameraID = 0;
}

void
gst_dalsa_src_set_property (GObject * object, guint property_id,
		const GValue * value, GParamSpec * pspec)
{
	GstDalsaSrc *src;

	src = GST_DALSA_SRC (object);

	switch(property_id) {
	case PROP_CAMERA:
		src->cameraID = g_value_get_int (value);
		GST_DEBUG_OBJECT (src, "camera id: %d", src->cameraID);
		break;
	case PROP_WIDTH:
		break;
	case PROP_HEIGHT:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}

	fail:
	return;
}

void
gst_dalsa_src_get_property (GObject * object, guint property_id,
		GValue * value, GParamSpec * pspec)
{
	GstDalsaSrc *src;

	g_return_if_fail (GST_IS_DALSA_SRC (object));
	src = GST_DALSA_SRC (object);
}

void
gst_dalsa_src_dispose (GObject * object)
{
	GstDalsaSrc *src;

	g_return_if_fail (GST_IS_DALSA_SRC (object));
	src = GST_DALSA_SRC (object);

	GST_DEBUG_OBJECT (src, "dispose");

	// clean up as possible.  may be called multiple times

	G_OBJECT_CLASS (gst_dalsa_src_parent_class)->dispose (object);
}

void
gst_dalsa_src_finalize (GObject * object)
{
	GstDalsaSrc *src;

	g_return_if_fail (GST_IS_DALSA_SRC (object));
	src = GST_DALSA_SRC (object);

	GST_DEBUG_OBJECT (src, "finalize");

	/* clean up object here */
	G_OBJECT_CLASS (gst_dalsa_src_parent_class)->finalize (object);
}

//queries camera devices and begins acquisition
static gboolean
gst_dalsa_src_start (GstBaseSrc * bsrc)
{
	GstDalsaSrc *src = GST_DALSA_SRC (bsrc);
    size_t numCameras = 0;

	GST_DEBUG_OBJECT (src, "start");
	
	GEV_DEVICE_INTERFACE  pCamera[MAX_CAMERAS] = {0};
	GEV_STATUS status;
	int numCamera = 0;
	int camIndex = 0;
   	//X_VIEW_HANDLE  View = NULL;
	//MY_CONTEXT context = {0};
   	pthread_t  tid;
	char c;
	int done = FALSE;
	int turboDriveAvailable = 0;
	char uniqueName[128];
	uint32_t macLow = 0; // Low 32-bits of the mac address (for file naming).

	// Boost application RT response (not too high since GEV library boosts data receive thread to max allowed)
	// SCHED_FIFO can cause many unintentional side effects.
	// SCHED_RR has fewer side effects.
	// SCHED_OTHER (normal default scheduler) is not too bad afer all.
	if (0)
	{
		//int policy = SCHED_FIFO;
		int policy = SCHED_RR;
		pthread_attr_t attrib;
		int inherit_sched = 0;
		struct sched_param param = {0};

		// Set an average RT priority (increase/decrease to tuner performance).
		param.sched_priority = (sched_get_priority_max(policy) - sched_get_priority_min(policy)) / 2;
		
		// Set scheduler policy
		pthread_setschedparam( pthread_self(), policy, &param); // Don't care if it fails since we can't do anyting about it.
		
		// Make sure all subsequent threads use the same policy.
		pthread_attr_init(&attrib);
		pthread_attr_getinheritsched( &attrib, &inherit_sched);
		if (inherit_sched != PTHREAD_INHERIT_SCHED)
		{
			inherit_sched = PTHREAD_INHERIT_SCHED;
			pthread_attr_setinheritsched(&attrib, inherit_sched);
		}
	}

	// Set default options for the library.
	{
		GEVLIB_CONFIG_OPTIONS options = {0};

		GevGetLibraryConfigOptions( &options);
		//options.logLevel = GEV_LOG_LEVEL_OFF;
		//options.logLevel = GEV_LOG_LEVEL_TRACE;
		options.logLevel = GEV_LOG_LEVEL_NORMAL;
		GevSetLibraryConfigOptions( &options);
	}

	//====================================================================================
	// DISCOVER Cameras
	//
	// Get all the IP addresses of attached network cards.

	status = GevGetCameraList( pCamera, MAX_CAMERAS, &numCamera);

	printf ("%d camera(s) on the network\n", numCamera);

	// Select the first camera found (unless the command line has a parameter = the camera index)
	if (numCamera != 0)
	{
		if (src->cameraID >= (int)numCamera)
		{
			//GST_ERROR(dalsa,"Camera index out of range");
			return FALSE;
		}

		//====================================================================
			// Connect to Camera
			//
			// Direct instantiation of GenICam XML-based feature node map.
			int i;
			int type;
			UINT32 height = 0;
			UINT32 width = 0;
			UINT32 format = 0;
			UINT32 maxHeight = 1600;
			UINT32 maxWidth = 2048;
			UINT32 maxDepth = 2;
			UINT64 size;
			UINT64 payload_size;
			int numBuffers = NUM_BUF;
			PUINT8 bufAddress[NUM_BUF];
			GEV_CAMERA_HANDLE handle = NULL;
			UINT32 pixFormat = 0;
			UINT32 pixDepth = 0;
			UINT32 convertedGevFormat = 0;
			
			//====================================================================
			// Open the camera.
			status = GevOpenCamera( &pCamera[src->cameraID], GevControlMode, &handle);
			if (status == 0)
			{
				//=================================================================
				// GenICam feature access via Camera XML File enabled by "open"
				// 
				// Get the name of XML file name back (example only - in case you need it somewhere).
				//
				char xmlFileName[MAX_PATH] = {0};
				status = GevGetGenICamXML_FileName( handle, (int)sizeof(xmlFileName), xmlFileName);
				if (status == GEVLIB_OK)
				{
					printf("XML stored as %s\n", xmlFileName);
				}
				status = GEVLIB_OK;

				// Get the camera width, height, and pixel format
				GevGetFeatureValue(handle, "Width", &type, sizeof(width), &width);
				GevGetFeatureValue(handle, "Height", &type, sizeof(height), &height);
				GevGetFeatureValue(handle, "PixelFormat", &type, sizeof(format), &format);
			}
			// Get the low part of the MAC address (use it as part of a unique file name for saving images).
			// Generate a unique base name to be used for saving image files
			// based on the last 3 octets of the MAC address.
			macLow = pCamera[src->cameraID].macLow;
			macLow &= 0x00FFFFFF;
			snprintf(uniqueName, sizeof(uniqueName), "img_%06x", macLow); 
			
			
			// Go on to adjust some API related settings (for tuning / diagnostics / etc....).
			if ( status == 0 )
			{
				GEV_CAMERA_OPTIONS camOptions = {0};

				// Adjust the camera interface options if desired (see the manual)
				GevGetCameraInterfaceOptions( handle, &camOptions);
				//camOptions.heartbeat_timeout_ms = 60000;		// For debugging (delay camera timeout while in debugger)
				camOptions.heartbeat_timeout_ms = 5000;		// Disconnect detection (5 seconds)
				// Write the adjusted interface options back.
				GevSetCameraInterfaceOptions( handle, &camOptions);

				// Access some features using the C-compatible functions.
				{
					UINT32 val = 0;
					char value_str[MAX_PATH] = {0};
						
					printf("Camera ROI set for \n\t");
					GevGetFeatureValueAsString( handle, "Height", &type, MAX_PATH, value_str);
					printf("Height = %s\n\t", value_str);
					GevGetFeatureValueAsString( handle, "Width", &type, MAX_PATH, value_str);
					printf("Width = %s\n\t", value_str);

					//set to mono8
					UINT32 mono_val = 0x01080001;
					GevSetFeatureValue( handle, "PixelFormat", sizeof(UINT32), &mono_val);

					GevGetFeatureValueAsString( handle, "PixelFormat", &type, MAX_PATH, value_str);
					printf("PixelFormat (str) = %s\n\t", value_str);

					GevGetFeatureValue(handle, "PixelFormat", &type, sizeof(UINT32), &val);
					printf("PixelFormat (val) = 0x%x\n", val);
				}


				if (status == 0)
				{
					//=================================================================
					// Set up a grab/transfer from this camera
					//
					GevGetPayloadParameters( handle,  &payload_size, (UINT32 *)&type);
					maxHeight = height;
					maxWidth = width;
					maxDepth = GetPixelSizeInBytes(format);

					// Allocate image buffers
					// (Either the image size or the payload_size, whichever is larger - allows for packed pixel formats).
					size = maxDepth * maxWidth * maxHeight;
					size = (payload_size > size) ? payload_size : size;
					for (i = 0; i < numBuffers; i++)
					{
						bufAddress[i] = (PUINT8)malloc(size);
						memset(bufAddress[i], 0, size);
					}
					// Initialize a transfer with asynchronous buffer handling.
					status = GevInitializeTransfer( handle, Asynchronous, size, numBuffers, bufAddress);
					pixDepth = GevGetPixelDepthInBits(convertedGevFormat);
					//context.format = Convert_SaperaFormat_To_X11( pixFormat);
					src->format = 0x00000001; //Mono
					src->depth = pixDepth;
					src->convertBuffer = NULL;
					src->convertFormat = FALSE;
										// Create a thread to receive images from the API and display them.
					//context.View = View;
					src->camHandle = handle;
					src->exit = FALSE;
		   			//pthread_create(&tid, NULL, ImageDisplayThread, &context); 

					for (i = 0; i < numBuffers; i++)
					{
						memset(bufAddress[i], 0, size);
					}
					status = GevStartTransfer( handle, -1);
					if (status != 0) return FALSE; 
				}
			}
	}


	// NOTE:
	// from now on, the "deviceContext" handle can be used to access the camera board.
	// use fc2DestroyContext to end the usage
	//src->cameraPresent = TRUE;

	return TRUE;
}

//stops streaming and closes the camera
static gboolean
gst_dalsa_src_stop (GstBaseSrc * bsrc)
{
	GstDalsaSrc *src = GST_DALSA_SRC (bsrc);

	GST_DEBUG_OBJECT (src, "stop");
	GevStopTransfer(src->camHandle);
	GevCloseCamera(&src->camHandle);
	gst_dalsa_src_reset (src);
	// Close down the API.
	GevApiUninitialize();

	// Close socket API
	_CloseSocketAPI ();	// must close API even on error
	return TRUE;

	fail:  
	return TRUE;
}

static GstCaps *
gst_dalsa_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
	GstDalsaSrc *src = GST_DALSA_SRC (bsrc);
	GstCaps *caps;

	GstVideoInfo vinfo;

	gst_video_info_init(&vinfo);

	vinfo.width = src->width;
	vinfo.height = src->height;
	vinfo.fps_n = 0; //0 means variable FPS
	vinfo.fps_d = 1;
	vinfo.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
	vinfo.finfo = gst_video_format_get_info(DEFAULT_GST_VIDEO_FORMAT);
  
	caps = gst_video_info_to_caps(&vinfo);

	GST_DEBUG_OBJECT (src, "The caps are %" GST_PTR_FORMAT, caps);

	return caps;
}

static gboolean
gst_dalsa_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
	GstDalsaSrc *src = GST_DALSA_SRC (bsrc);
	GstVideoInfo vinfo;

	//Currently using fixed caps
	GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);
	src->acq_started = TRUE;

	return TRUE;

	unsupported_caps:
	GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
	return FALSE;

	fail:
	return FALSE;
}

//Grabs next image from camera and puts it into a gstreamer buffer
#ifdef OVERRIDE_CREATE
static GstFlowReturn
gst_dalsa_src_create (GstPushSrc * psrc, GstBuffer ** buf)
{
	GstDalsaSrc *src = GST_DALSA_SRC (psrc);
	GstMapInfo minfo;

	GEV_BUFFER_OBJECT *img = NULL;
	GEV_STATUS status = 0;
	gboolean test = FALSE;
while (!test){
	// Wait for images to be received
	status = GevWaitForNextImage(src->camHandle, &img, 1000);
	if ((img != NULL) && (status == GEVLIB_OK))
	{	
		if (img->status == 0)
		{
			test = TRUE;
				// Create a new buffer for the image
			*buf = gst_buffer_new_and_alloc (src->height * src->width * src->bytesPerPixel);

			gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);
			//copy image data into gstreamer buffer
			for (int i = 0; i < src->height; i++) {
				memcpy (minfo.data + i * src->gst_stride, img->address + i * src->pitch, src->pitch);
			}

				gst_buffer_unmap (*buf, &minfo);

			src->duration = 1000000000.0/src->framerate; 
			// If we do not use gst_base_src_set_do_timestamp() we need to add timestamps manually
			src->last_frame_time += src->duration;   // Get the timestamp for this frame
			if(!gst_base_src_get_do_timestamp(GST_BASE_SRC(psrc))){
				GST_BUFFER_PTS(*buf) = src->last_frame_time;  // convert ms to ns
				GST_BUFFER_DTS(*buf) = src->last_frame_time;  // convert ms to ns
			}
			GST_BUFFER_DURATION(*buf) = src->duration;
			GST_DEBUG_OBJECT(src, "pts, dts: %" GST_TIME_FORMAT ", duration: %d ms", GST_TIME_ARGS (src->last_frame_time), GST_TIME_AS_MSECONDS(src->duration));

			// count frames, and send EOS when required frame number is reached
			GST_BUFFER_OFFSET(*buf) = src->n_frames;  // from videotestsrc
			src->n_frames++;
			GST_BUFFER_OFFSET_END(*buf) = src->n_frames;  // from videotestsrc
			if (psrc->parent.num_buffers>0)  // If we were asked for a specific number of buffers, stop when complete
				if (G_UNLIKELY(src->n_frames >= psrc->parent.num_buffers))
			return GST_FLOW_EOS;

			return GST_FLOW_OK;

		}
		else
		{
			GST_DEBUG_OBJECT (src, "Image Incomplete***********************!\n\n\n\n");
			//return GST_FLOW_ERROR;
			// Image had an error (incomplete (timeout/overflow/lost)).
			// Do any handling of this condition necessary.
		}
	}
}
GST_DEBUG_OBJECT (src, "Capture Error!");
	return GST_FLOW_OK;
}
#endif // OVERRIDE_CREATE

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "dalsasrc", GST_RANK_NONE,
      GST_TYPE_DALSA_SRC);

}
/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
   
#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "GST DALSA"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "dalsasrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "github.com/thompd27"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dalsa,
    "plugin for interfacing with Teledyne DALSA cameras using the GigEV Libraries",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN);
