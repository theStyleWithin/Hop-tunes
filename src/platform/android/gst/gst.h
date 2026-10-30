// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Stand-in for GStreamer on Android, where there is none yet (see CMakeLists.txt). Every call
// does nothing and every "make an element" call returns null, so Player's own "the playbin is
// missing" path runs and the app reports that playback is unavailable. It goes away when a
// real Android playback engine replaces GStreamer there.

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
#define GST_MESSAGE_SRC(m) (static_cast<void *>(nullptr))
#define GST_OBJECT(o) (o)
#define GST_MESSAGE_TYPE(m) (static_cast<GstMessageType>(0 * sizeof(m)))
#define G_CALLBACK(f) (reinterpret_cast<GCallback>(f))
#define g_signal_connect(i, s, c, d) (static_cast<void>(sizeof(i) + sizeof(c) + sizeof(d)))
#define g_object_set(o, ...) (static_cast<void>(sizeof(o)))
#define g_signal_handlers_disconnect_by_data(i, d) (static_cast<void>(sizeof(i) + sizeof(d)))

inline void g_free(void *) {}
inline void g_error_free(GError *) {}
inline gboolean gst_is_initialized() { return TRUE; }
inline void gst_init(int *, char ***) {}
inline void gst_version(guint *a, guint *b, guint *c, guint *d) { *a = *b = *c = *d = 0; }
inline gchar *gst_filename_to_uri(const char *, GError **) { return nullptr; }
inline GstElementFactory *gst_element_factory_find(const char *) { return nullptr; }
inline GstElement *gst_element_factory_make(const char *, const char *) { return nullptr; }
inline GstElement *gst_parse_launch(const char *, GError **) { return nullptr; }
inline GstElement *gst_parse_bin_from_description(const char *, gboolean, GError **) { return nullptr; }
inline GstElement *gst_bin_get_by_name(GstBin *, const char *) { return nullptr; }
inline GstBus *gst_element_get_bus(GstElement *) { return nullptr; }
inline void *gst_object_ref_sink(void *o) { return o; }
inline void gst_object_unref(void *) {}
inline GstStateChangeReturn gst_element_set_state(GstElement *, GstState) { return GST_STATE_CHANGE_FAILURE; }
inline GstStateChangeReturn gst_element_get_state(GstElement *, GstState *s, GstState *, int64_t) { if (s) *s = GST_STATE_NULL; return GST_STATE_CHANGE_FAILURE; }
inline gboolean gst_element_seek_simple(GstElement *, GstFormat, GstSeekFlags, gint64) { return FALSE; }
inline gboolean gst_element_query_position(GstElement *, GstFormat, gint64 *) { return FALSE; }
inline gboolean gst_element_query_duration(GstElement *, GstFormat, gint64 *) { return FALSE; }
inline GstMessage *gst_bus_pop(GstBus *) { return nullptr; }
inline void gst_bus_set_flushing(GstBus *, gboolean) {}
inline void gst_message_unref(GstMessage *) {}
inline void gst_message_parse_error(GstMessage *, GError **e, gchar **d) { if (e) *e = nullptr; if (d) *d = nullptr; }
