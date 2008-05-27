/**
 * @file media.h Media API
 * @ingroup core
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MEDIA_H_
#define __MEDIA_H_

#ifdef USE_FARSIGHT
#ifdef USE_GSTPROPS

#include <gst/gst.h>
#include <gst/farsight/fs-stream.h>
#include <gst/farsight/fs-session.h>
#include <glib.h>
#include <glib-object.h>

#include "connection.h"

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA            (purple_media_get_type())
#define PURPLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA, PurpleMedia))
#define PURPLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA, PurpleMediaClass))
#define PURPLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA))
#define PURPLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA))
#define PURPLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA, PurpleMediaClass))

typedef struct _PurpleMedia PurpleMedia;
typedef struct _PurpleMediaClass PurpleMediaClass;
typedef struct _PurpleMediaPrivate PurpleMediaPrivate;

typedef enum {
	PURPLE_MEDIA_RECV_AUDIO = 1 << 0,
	PURPLE_MEDIA_SEND_AUDIO = 1 << 1,
	PURPLE_MEDIA_RECV_VIDEO = 1 << 2,
	PURPLE_MEDIA_SEND_VIDEO = 1 << 3,
	PURPLE_MEDIA_AUDIO = PURPLE_MEDIA_RECV_AUDIO | PURPLE_MEDIA_SEND_AUDIO,
	PURPLE_MEDIA_VIDEO = PURPLE_MEDIA_RECV_VIDEO | PURPLE_MEDIA_SEND_VIDEO
} PurpleMediaStreamType;

struct _PurpleMediaClass
{
	GObjectClass parent_class;
};

struct _PurpleMedia
{
	GObject parent;
	PurpleMediaPrivate *priv;
};

GType purple_media_get_type(void);

void purple_media_get_elements(PurpleMedia *media, GstElement **audio_src, GstElement **audio_sink,
						  GstElement **video_src, GstElement **video_sink);

void purple_media_set_audio_src(PurpleMedia *media, GstElement *video_src);
void purple_media_set_audio_sink(PurpleMedia *media, GstElement *video_src);
void purple_media_set_video_src(PurpleMedia *media, GstElement *video_src);
void purple_media_set_video_sink(PurpleMedia *media, GstElement *video_src);

GstElement *purple_media_get_audio_src(PurpleMedia *media);
GstElement *purple_media_get_audio_sink(PurpleMedia *media);
GstElement *purple_media_get_video_src(PurpleMedia *media);
GstElement *purple_media_get_video_sink(PurpleMedia *media);

GstElement *purple_media_get_audio_pipeline(PurpleMedia *media);

PurpleConnection *purple_media_get_connection(PurpleMedia *media);
const char *purple_media_get_screenname(PurpleMedia *media);
void purple_media_ready(PurpleMedia *media);
void purple_media_wait(PurpleMedia *media);
void purple_media_accept(PurpleMedia *media);
void purple_media_reject(PurpleMedia *media);
void purple_media_hangup(PurpleMedia *media);
void purple_media_got_hangup(PurpleMedia *media);
void purple_media_got_accept(PurpleMedia *media);

gchar *purple_media_get_device_name(GstElement *element, 
										  GValue *device);

GList *purple_media_get_devices(GstElement *element);
void purple_media_element_set_device(GstElement *element, GValue *device);
void purple_media_element_set_device_from_name(GstElement *element,
											   const gchar *name);
GValue *purple_media_element_get_device(GstElement *element);
GstElement *purple_media_get_element(const gchar *factory_name);

void purple_media_audio_init_src(GstElement **sendbin,
                                 GstElement **sendlevel);
void purple_media_video_init_src(GstElement **sendbin);

void purple_media_audio_init_recv(GstElement **recvbin, GstElement **recvlevel);

gboolean purple_media_add_stream(PurpleMedia *media, const gchar *who,
			     PurpleMediaStreamType type, const gchar *transmitter);
void purple_media_remove_stream(PurpleMedia *media, const gchar *who, PurpleMediaStreamType type);

GList *purple_media_get_local_audio_candidates(PurpleMedia *media);
GList *purple_media_get_negotiated_audio_codecs(PurpleMedia *media);

GList *purple_media_get_local_audio_codecs(PurpleMedia *media);
void purple_media_add_remote_audio_candidates(PurpleMedia *media, const gchar *name,	
					       GList *remote_candidates);
FsCandidate *purple_media_get_local_candidate(PurpleMedia *media);
FsCandidate *purple_media_get_remote_candidate(PurpleMedia *media);
void purple_media_set_remote_audio_codecs(PurpleMedia *media, const gchar *name, GList *codecs);

G_END_DECLS

#endif  /* USE_GSTPROPS */
#endif  /* USE_FARSIGHT */


#endif  /* __MEDIA_H_ */
