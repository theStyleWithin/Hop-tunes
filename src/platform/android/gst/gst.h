// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Stand-in for the few GStreamer calls Player makes, for Android where there is no GStreamer
// (see CMakeLists.txt). GstShim.cpp backs the "playbin" with Qt Multimedia (QMediaPlayer) and turns
// its signals into bus messages, so Player's queue, shuffle, repeat and resume logic is shared with
// the desktop. Gapless and crossfade are not available here (the track ends, the next one starts).

#include <cstdint>

typedef char gchar;
typedef int gboolean;
typedef int gint;
typedef int64_t gint64;
typedef unsigned int guint;
typedef unsigned int guint32;
typedef void *gpointer;
typedef void (*GCallback)(void);
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

struct GError { gchar *message = nullptr; };
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
struct _GstMessage { int type = 0; void *src = nullptr; char *text = nullptr; };
typedef struct _GstMessage GstMessage;
typedef struct _GstElementFactory GstElementFactory;
typedef struct _GstBin GstBin;

enum GstState { GST_STATE_VOID_PENDING, GST_STATE_NULL, GST_STATE_READY, GST_STATE_PAUSED, GST_STATE_PLAYING };
enum GstStateChangeReturn { GST_STATE_CHANGE_FAILURE, GST_STATE_CHANGE_SUCCESS, GST_STATE_CHANGE_ASYNC, GST_STATE_CHANGE_NO_PREROLL };
enum GstFormat { GST_FORMAT_UNDEFINED, GST_FORMAT_TIME };
enum GstSeekFlags { GST_SEEK_FLAG_NONE = 0, GST_SEEK_FLAG_FLUSH = 1, GST_SEEK_FLAG_KEY_UNIT = 4 };
enum GstMessageType { GST_MESSAGE_UNKNOWN = 0, GST_MESSAGE_EOS = 1, GST_MESSAGE_ERROR = 2, GST_MESSAGE_STATE_CHANGED = 64, GST_MESSAGE_DURATION_CHANGED = 1 << 18,
                      GST_MESSAGE_STREAM_START = 1 << 20, GST_MESSAGE_ASYNC_DONE = 1 << 21 };

#define GST_SECOND (1000000000LL)
#define GST_MSECOND (1000000LL)
#define GST_BIN(x) (reinterpret_cast<GstBin *>(x))
#define GST_MESSAGE_SRC(m) ((m)->src)
#define GST_OBJECT(o) (static_cast<void *>(o))
#define GST_MESSAGE_TYPE(m) (static_cast<GstMessageType>((m)->type))
#define G_CALLBACK(f) (reinterpret_cast<GCallback>(f))
#define g_signal_connect(i, s, c, d) (static_cast<void>(sizeof(i) + sizeof(c) + sizeof(d)))
#define g_signal_handlers_disconnect_by_data(i, d) (static_cast<void>(sizeof(i) + sizeof(d)))

void g_object_set(void *obj, const char *first, ...);   // "uri", "volume" matter here; the rest are accepted and ignored
void g_free(void *p);
void g_error_free(GError *e);
gboolean gst_is_initialized();
void gst_init(int *, char ***);
void gst_version(guint *a, guint *b, guint *c, guint *d);
gchar *gst_filename_to_uri(const char *path, GError **err);
GstElementFactory *gst_element_factory_find(const char *name);
GstElement *gst_element_factory_make(const char *name, const char *id);
GstElement *gst_parse_launch(const char *, GError **);
GstElement *gst_parse_bin_from_description(const char *, gboolean, GError **);
GstElement *gst_bin_get_by_name(GstBin *, const char *);
GstBus *gst_element_get_bus(GstElement *e);
void *gst_object_ref_sink(void *o);
void gst_object_unref(void *o);
GstStateChangeReturn gst_element_set_state(GstElement *e, GstState s);
GstStateChangeReturn gst_element_get_state(GstElement *e, GstState *s, GstState *, int64_t);
gboolean gst_element_seek_simple(GstElement *e, GstFormat, GstSeekFlags, gint64 ns);
gboolean gst_element_query_position(GstElement *e, GstFormat, gint64 *ns);
gboolean gst_element_query_duration(GstElement *e, GstFormat, gint64 *ns);
GstMessage *gst_bus_pop(GstBus *b);
void gst_bus_set_flushing(GstBus *b, gboolean);
void gst_message_unref(GstMessage *m);
void gst_message_parse_error(GstMessage *m, GError **e, gchar **d);
