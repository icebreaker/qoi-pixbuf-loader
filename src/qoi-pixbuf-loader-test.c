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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>

static gboolean test_gdk_pixbuf_save_func(const gchar *buf, gsize count, GError **error, gpointer user_data)
{
	FILE *fp = user_data;

	if(fp == NULL)
		return FALSE;

	if(fwrite(buf, sizeof(guchar), count, fp) != count)
	{
		g_set_error_literal(
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_FAILED,
			"Failed to write to file from callback"
		);
		return FALSE;
	}

	return TRUE;
}

static gboolean test_gdk_pixbuf_get_formats(void)
{
	GdkPixbufFormat *format;
	GSList *formats;
	guint i, n;

	g_print("test_gdk_pixbuf_get_formats: ");

	formats = gdk_pixbuf_get_formats();
	if(formats == NULL)
	{
		g_print("ERROR: no pixbuf formats\n");
		return FALSE;
	}

	format = NULL;
	n = g_slist_length(formats);

	for(i = 0; i < n; i++)
	{
		format = g_slist_nth_data(formats, i);

		if(!g_ascii_strcasecmp(gdk_pixbuf_format_get_name(format), "qoi"))
			break;
	}

	if(format == NULL || g_ascii_strcasecmp(gdk_pixbuf_format_get_name(format), "qoi"))
	{
		g_print("ERROR: no qoi pixbuf format found\n");
		g_slist_free(formats);
		return FALSE;
	}

	if(!gdk_pixbuf_format_is_writable(format))
	{
		g_print("ERROR: qoi pixbuf format is not writable\n");
		g_slist_free(formats);
		return FALSE;
	}
	g_slist_free(formats);

	g_print("SUCCESS\n");
	return TRUE;
}

gboolean test_gdk_pixbuf_get_file_info(const gchar *filename, gint *w, gint *h)
{
	GdkPixbufFormat *format;

	g_print("test_gdk_pixbuf_get_file_info: ");

	format = gdk_pixbuf_get_file_info(filename, w, h);
	if(format == NULL)
	{
		g_print("ERROR: could not get file info for '%s'\n", filename);
		return FALSE;
	}

	g_print(
		"SUCCESS\n"
		"\tfilename: %s\n"
		"\twidth: %d\n"
		"\theight: %d\n"
		"\twritable: %s\n",
		filename,
		*w,
		*h,
		gdk_pixbuf_format_is_writable(format) ? "yes" : "no"
	);
	return TRUE;
}

gboolean test_gdk_pixbuf_new_from_file_save(const gchar *input, const gchar *output)
{
	GdkPixbuf *pixbuf;
	gint w, h;
	GError *error = NULL;

	if(!test_gdk_pixbuf_get_file_info(input, &w, &h))
		return FALSE;

	g_print("test_gdk_pixbuf_new_from_file: ");

	pixbuf = gdk_pixbuf_new_from_file(input, &error);
	if(pixbuf == NULL)
	{
		if(error != NULL)
		{
			g_print("ERROR: %s\n", error->message);
			g_error_free(error);
		}
		else
		{
			g_print("ERROR: failed to create pixbuf from file '%s'\n", input);
		}

		return FALSE;
	}

	g_print(
		"SUCCESS\n"
		"\tfilename: %s\n"
		"\twidth: %d\n"
		"\theight: %d\n"
		"\tbits per sample: %d\n"
		"\tnumber of channels: %d\n"
		"\thas alpha: %s\n",
		input,
		gdk_pixbuf_get_width(pixbuf),
		gdk_pixbuf_get_height(pixbuf),
		gdk_pixbuf_get_bits_per_sample(pixbuf),
		gdk_pixbuf_get_n_channels(pixbuf),
		gdk_pixbuf_get_has_alpha(pixbuf) ? "yes" : "no"
	);

	g_print("test_gdk_pixbuf_save: ");

	if(!gdk_pixbuf_save(pixbuf, output, "qoi", &error, NULL))
	{
		if(error != NULL)
		{
			g_print("ERROR: %s\n", error->message);
			g_error_free(error);
		}
		else
		{
			g_print("ERROR: failed to save pixbuf to file '%s'\n", output);
		}

		return FALSE;
	}
	g_object_unref(pixbuf);

	g_print("SUCCESS\n");
	return test_gdk_pixbuf_get_file_info(output, &w, &h);
}

gboolean test_gdk_pixbuf_new_from_file_at_size_save_to_callback(const gchar *input, const gchar *output)
{
	FILE *fp;
	GdkPixbuf *pixbuf;
	gint w, h;
	GError *error = NULL;

	if(!test_gdk_pixbuf_get_file_info(input, &w, &h))
		return FALSE;

	g_print("test_gdk_pixbuf_new_from_file_at_size: ");

	pixbuf = gdk_pixbuf_new_from_file_at_size(input, w, h, &error);
	if(pixbuf == NULL)
	{
		if(error != NULL)
		{
			g_print("ERROR: %s\n", error->message);
			g_error_free(error);
		}
		else
		{
			g_print("ERROR: failed to create pixbuf from file '%s'\n", input);
		}

		return FALSE;
	}

	g_print(
		"SUCCESS\n"
		"\tfilename: %s\n"
		"\twidth: %d\n"
		"\theight: %d\n"
		"\tbits per sample: %d\n"
		"\tnumber of channels: %d\n"
		"\thas alpha: %s\n",
		input,
		gdk_pixbuf_get_width(pixbuf),
		gdk_pixbuf_get_height(pixbuf),
		gdk_pixbuf_get_bits_per_sample(pixbuf),
		gdk_pixbuf_get_n_channels(pixbuf),
		gdk_pixbuf_get_has_alpha(pixbuf) ? "yes" : "no"
	);

	g_print("test_gdk_pixbuf_save_to_callback: ");

	fp = g_fopen(output, "wb");
	if(fp == NULL)
	{
		g_print("ERROR: failed to open '%s' for writing\n", output);
		return FALSE;
	}

	if(!gdk_pixbuf_save_to_callback(pixbuf, test_gdk_pixbuf_save_func, fp, "qoi", &error, NULL))
	{
		if(error != NULL)
		{
			g_print("ERROR: %s\n", error->message);
			g_error_free(error);
		}
		else
		{
			g_print("ERROR: failed to save pixbuf to file '%s'\n", output);
		}

		fclose(fp);
		return FALSE;
	}

	g_object_unref(pixbuf);
	fclose(fp);

	g_print("SUCCESS\n");
	return test_gdk_pixbuf_get_file_info(output, &w, &h);
}

gint main(gint argc, gchar *argv[])
{
	if(argc < 4)
	{
		g_print("usage: %s input.qoi output.qoi output_callback.qoi\n", argv[0]);
		return -1;
	}

	if(!test_gdk_pixbuf_get_formats())
		return -1;

	if(!test_gdk_pixbuf_new_from_file_save(argv[1], argv[2]))
		return -1;

	if(!test_gdk_pixbuf_new_from_file_at_size_save_to_callback(argv[2], argv[3]))
		return -1;

	return 0;
}

/* vim: set ts=4 sw=4 sts=4 noet: */
