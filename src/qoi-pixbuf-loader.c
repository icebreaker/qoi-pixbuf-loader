/*{{{
    MIT LICENSE

    Copyright (c) 2021, Mihail Szabolcs

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the 'Software'), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
}}}*/
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>
#undef  GDK_PIXBUF_ENABLE_BACKEND

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#define QOI_MALLOC(sz) g_malloc(sz)
#define QOI_FREE(p) g_free(p)
#include "qoi.h"

#ifndef QOI_PIXBUF_LOADER_UNUSED
	#define QOI_PIXBUF_LOADER_UNUSED(x) (void)(x)
#endif

#ifndef QOI_PIXBUF_LOADER_HEADER_SIZE
	#define QOI_PIXBUF_LOADER_HEADER_SIZE ((gsize) QOI_HEADER_SIZE + sizeof(qoi_padding))
#endif

typedef struct
{
	gpointer user_data;
	GdkPixbufModuleSizeFunc size_func;
	GdkPixbufModulePreparedFunc prepared_func;
	GdkPixbufModuleUpdatedFunc updated_func;
	guchar *data;
	gsize size;
	gsize max_encoded_size;
	gboolean file_info_requested;
} qoi_pixbuf_loader_context_t;

typedef struct
{
	gint w;
	gint h;
	gint channels;
	gsize max_encoded_size;
} qoi_pixbuf_loader_info_t;

static void qoi_pixbuf_loader_context_destroy(qoi_pixbuf_loader_context_t *ctx)
{
	if(ctx == NULL)
		return;

	if(ctx->data != NULL)
		g_free(ctx->data);

	g_free(ctx);
}

static void qoi_pixbuf_loader_free_data(guchar *data, gpointer user_data)
{
	QOI_PIXBUF_LOADER_UNUSED(user_data);

	if(data != NULL)
		g_free(data);
}

static GdkPixbuf *qoi_pixbuf_loader_decode(const guchar *data, const gsize size)
{
	qoi_desc desc;
	guchar *pixels;

	pixels = qoi_decode(data, (gint) size, &desc, 0);
	if(pixels == NULL)
		return NULL;

	return gdk_pixbuf_new_from_data(
		pixels,
		GDK_COLORSPACE_RGB,
		desc.channels == 4,
		8,
		desc.width,
		desc.height,
		desc.width * desc.channels,
		qoi_pixbuf_loader_free_data,
		NULL
	);
}

static guchar *qoi_pixbuf_loader_encode(GdkPixbuf *pixbuf, gsize *size)
{
	qoi_desc desc;
	guchar *pixels;
	gint w, h, channels, n;

	if(gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB)
		return NULL;

	if(gdk_pixbuf_get_bits_per_sample(pixbuf) != 8)
		return NULL;

	channels = gdk_pixbuf_get_n_channels(pixbuf);
	if(channels < 3 || channels > 4)
		return NULL;

	w = gdk_pixbuf_get_width(pixbuf);
	if(w <= 0)
		return NULL;

	h = gdk_pixbuf_get_height(pixbuf);
	if(h <= 0)
		return NULL;

	desc.width = w;
	desc.height = h;
	desc.channels = channels;
	desc.colorspace = QOI_SRGB;

	pixels = qoi_encode(gdk_pixbuf_get_pixels(pixbuf), &desc, &n);
	if(pixels == NULL)
		return NULL;

	if(size != NULL)
		*size = (gsize) n;

	return pixels;
}

static gboolean qoi_pixbuf_loader_load_info(
	const guchar *data,
	const gsize size,
	qoi_pixbuf_loader_info_t *info
)
{
	gint w, h, ch, cs, p = 0;

	if(size < QOI_PIXBUF_LOADER_HEADER_SIZE)
		return FALSE;

	if(qoi_read_32(data, &p) != QOI_MAGIC)
		return FALSE;

	w = qoi_read_32(data, &p);
	h = qoi_read_32(data, &p);
	ch = data[p++];
	cs = data[p++];

	if(w <= 0 || h <= 0 || ch < 3 || ch > 4 || cs < 0 || cs > 1)
		return FALSE;

	if((guint) w >= QOI_PIXELS_MAX / (guint) h)
		return FALSE;

	if((guint) h >= QOI_PIXELS_MAX / (guint) w)
		return FALSE;

	info->w = w;
	info->h = h;
	info->channels = ch;
	info->max_encoded_size = w * h * (ch + 1) + QOI_PIXBUF_LOADER_HEADER_SIZE;
	return TRUE;
}

static gboolean qoi_pixbuf_loader_load_info_from_file(
	FILE *fp,
	qoi_pixbuf_loader_info_t *info
)
{
	guchar data[QOI_PIXBUF_LOADER_HEADER_SIZE];

	if(fp == NULL)
		return FALSE;

	if(fread(data, sizeof(guchar), QOI_PIXBUF_LOADER_HEADER_SIZE, fp) != QOI_PIXBUF_LOADER_HEADER_SIZE)
		return FALSE;

	return qoi_pixbuf_loader_load_info(data, QOI_PIXBUF_LOADER_HEADER_SIZE, info);
}

static GdkPixbuf *qoi_pixbuf_loader_load(FILE *fp, GError **error)
{
	gsize size;
	guchar *data;
	GdkPixbuf *pixbuf;
	qoi_pixbuf_loader_info_t info;

	if(fp == NULL)
		return NULL;

	if(!qoi_pixbuf_loader_load_info_from_file(fp, &info))
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Invalid QOI image header"
		);
		return FALSE;
	}

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);

	if(size == 0 || size > info.max_encoded_size)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Invalid or corrupt QOI image data"
		);
		return NULL;
	}
	fseek(fp, 0L, SEEK_SET);

	data = g_malloc(size);
	if(data == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			"Failed to allocate enough memory to load QOI image data"
		);
		return NULL;
	}

	if(fread(data, sizeof(guchar), size, fp) != size)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to read QOI image data"
		);
		return NULL;
	}

	pixbuf = qoi_pixbuf_loader_decode(data, size);
	if(pixbuf == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to decode QOI image data"
		);
		g_free(data);
		return NULL;
	}

	g_free(data);
	return pixbuf;
}

static gboolean qoi_pixbuf_loader_save(
	FILE *fp,
	GdkPixbuf *pixbuf,
	gchar **keys,
	gchar **values,
	GError **error
)
{
	gsize size;
	guchar *pixels;

	QOI_PIXBUF_LOADER_UNUSED(keys);
	QOI_PIXBUF_LOADER_UNUSED(values);

	if(fp == NULL)
		return FALSE;

	pixels = qoi_pixbuf_loader_encode(pixbuf, &size);
	if(pixels == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to encode QOI image"
		);
		return FALSE;
	}

	if(fwrite(pixels, 1, size, fp) != size)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to write QOI image to file"
		);
		g_free(pixels);
		return FALSE;
	}

	g_free(pixels);
	return TRUE;
}

static gboolean qoi_pixbuf_loader_save_to_callback(
	GdkPixbufSaveFunc save_func,
	gpointer user_data,
	GdkPixbuf *pixbuf,
	gchar **option_keys,
	gchar **option_values,
	GError **error
)
{
	guchar *pixels;
	gsize size;
	gboolean ret;

	QOI_PIXBUF_LOADER_UNUSED(option_keys);
	QOI_PIXBUF_LOADER_UNUSED(option_values);

	pixels = qoi_pixbuf_loader_encode(pixbuf, &size);
	if(pixels == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to encode QOI image"
		);
		return FALSE;
	}

	ret = save_func((gchar *) pixels, size, error, user_data);

	g_free(pixels);
	return ret;
}

static gpointer qoi_pixbuf_loader_begin_load(
	GdkPixbufModuleSizeFunc size_func,
	GdkPixbufModulePreparedFunc prepared_func,
	GdkPixbufModuleUpdatedFunc updated_func,
	gpointer user_data,
	GError **error
)
{
	qoi_pixbuf_loader_context_t *ctx;

	ctx = g_new0(qoi_pixbuf_loader_context_t, 1);
	if(ctx == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			"Failed to allocate enough memory for QOI image loader context"
		);
		return NULL;
	}

	ctx->size_func = size_func;
	ctx->prepared_func = prepared_func;
	ctx->updated_func = updated_func;
	ctx->user_data = user_data;
	return ctx;
}

static gboolean qoi_pixbuf_loader_stop_load(gpointer data, GError **error)
{
	GdkPixbuf *pixbuf;
    qoi_pixbuf_loader_context_t *ctx = data;

	if(ctx == NULL)
		return FALSE;

	if(ctx->file_info_requested)
	{
		qoi_pixbuf_loader_context_destroy(ctx);
		return TRUE;
	}

	if(ctx->data == NULL)
	{
		qoi_pixbuf_loader_context_destroy(ctx);
		return FALSE;
	}

	pixbuf = qoi_pixbuf_loader_decode(ctx->data, ctx->size);
	if(pixbuf == NULL)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to decode QOI image data"
		);
		qoi_pixbuf_loader_context_destroy(ctx);
		return FALSE;
	}

	ctx->prepared_func(pixbuf, NULL, ctx->user_data);
	ctx->updated_func(
		pixbuf,
		0,
		0,
		gdk_pixbuf_get_width(pixbuf),
		gdk_pixbuf_get_height(pixbuf),
		ctx->user_data
	);

	qoi_pixbuf_loader_context_destroy(ctx);
	return TRUE;
}

static gboolean qoi_pixbuf_loader_load_increment(
	gpointer data,
	const guchar *buf,
	guint size,
	GError **error
)
{
	qoi_pixbuf_loader_info_t info;
	qoi_pixbuf_loader_context_t *ctx = data;

	if(ctx == NULL)
		return FALSE;

	if(ctx->file_info_requested)
		return TRUE;

	if(ctx->data == NULL)
	{
		if(!qoi_pixbuf_loader_load_info(buf, size, &info))
		{
			g_set_error_literal(
				error,
				GDK_PIXBUF_ERROR,
				GDK_PIXBUF_ERROR_FAILED,
				"Invalid QOI image header"
			);
			return FALSE;
		}

		ctx->size_func(&info.w, &info.h, ctx->user_data);
		/*
		   If the function sets width or height to zero, the module should
		   interpret this as a hint that it will be closed soon and shouldn’t
		   allocate further resources. This convention is used to implement
		   gdk_pixbuf_get_file_info() efficiently.

		   See: https://docs.gtk.org/gdk-pixbuf/callback.PixbufModuleSizeFunc.html
		*/
		if(info.w == 0 || info.h == 0)
		{
			ctx->file_info_requested = TRUE;
			return TRUE;
		}

		ctx->max_encoded_size = info.max_encoded_size;
		ctx->data = g_malloc0(info.max_encoded_size);
		if(ctx->data == NULL)
		{
			g_set_error_literal(
				error,
				GDK_PIXBUF_ERROR,
				GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				"Failed to allocate enough memory to load QOI image data"
			);
			return FALSE;
		}
	}

	if(ctx->size + size > ctx->max_encoded_size)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Invalid or corrupt QOI image data"
		);
		return FALSE;
	}

    memcpy(ctx->data + ctx->size, buf, size);
    ctx->size += size;
	return TRUE;
}

G_MODULE_EXPORT void fill_vtable(GdkPixbufModule *module)
{
	module->load = qoi_pixbuf_loader_load;
	module->save = qoi_pixbuf_loader_save;
	module->save_to_callback = qoi_pixbuf_loader_save_to_callback;
	module->begin_load = qoi_pixbuf_loader_begin_load;
	module->stop_load = qoi_pixbuf_loader_stop_load;
	module->load_increment = qoi_pixbuf_loader_load_increment;
}

G_MODULE_EXPORT void fill_info(GdkPixbufFormat *info)
{
	static GdkPixbufModulePattern signature[] = { { "qoif", NULL, 100 }, { NULL, NULL, 0 } };
	static gchar *extensions[] = { "qoi", NULL };
	static gchar *mime_types[] = { "image/x-qoi", NULL };

	info->name        = "qoi";
	info->description = "QOI image";
	info->license     = "MIT";
	info->signature   = signature;
	info->extensions  = extensions;
	info->mime_types  = mime_types;
	info->flags       = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
}

/* vim: set ts=4 sw=4 sts=4 noet: */
