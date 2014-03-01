/*
 * GdkPixbuf library - VTF image loader
 *
 * Copyright (C) 2010 Forrest Voight
 *
 * Authors: Forrest Voight <voights@gmail.com> & Jan Dudek <jd@jandudek.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <glib/gstdio.h>

typedef struct
{
	char		signature[4];		// File signature ("VTF\0").
	unsigned int	version[2];		// version[0].version[1] (currently 7.2).
	unsigned int	headerSize;		// Size of the header struct (16 byte aligned; currently 80 bytes).
	unsigned short	width;			// Width of the largest mipmap in pixels. Must be a power of 2.
	unsigned short	height;			// Height of the largest mipmap in pixels. Must be a power of 2.
	unsigned int	flags;			// VTF flags.
	unsigned short	frames;			// Number of frames, if animated (1 for no animation).
	unsigned short	firstFrame;		// First frame in animation (0 based).
	unsigned char	padding0[4];		// reflectivity padding (16 byte alignment).
	float		reflectivity[3];	// reflectivity vector.
	unsigned char	padding1[4];		// reflectivity padding (8 byte packing).
	float		bumpmapScale;		// Bumpmap scale.
	unsigned int	highResImageFormat;	// High resolution image format.
	unsigned char	mipmapCount;		// Number of mipmaps.
	unsigned int	lowResImageFormat;	// Low resolution image format (always DXT1).
	unsigned char	lowResImageWidth;	// Low resolution image width.
	unsigned char	lowResImageHeight;	// Low resolution image height.
	unsigned short	depth;			// Depth of the largest mipmap in pixels.
						// Must be a power of 2. Can be 0 or 1 for a 2D texture (v7.2 only).
} VtfHeader;

enum
{
	IMAGE_FORMAT_NONE = -1,
	IMAGE_FORMAT_RGBA8888 = 0,
	IMAGE_FORMAT_ABGR8888,
	IMAGE_FORMAT_RGB888,
	IMAGE_FORMAT_BGR888,
	IMAGE_FORMAT_RGB565,
	IMAGE_FORMAT_I8,
	IMAGE_FORMAT_IA88,
	IMAGE_FORMAT_P8,
	IMAGE_FORMAT_A8,
	IMAGE_FORMAT_RGB888_BLUESCREEN,
	IMAGE_FORMAT_BGR888_BLUESCREEN,
	IMAGE_FORMAT_ARGB8888,
	IMAGE_FORMAT_BGRA8888,
	IMAGE_FORMAT_DXT1,
	IMAGE_FORMAT_DXT3,
	IMAGE_FORMAT_DXT5,
	IMAGE_FORMAT_BGRX8888,
	IMAGE_FORMAT_BGR565,
	IMAGE_FORMAT_BGRX5551,
	IMAGE_FORMAT_BGRA4444,
	IMAGE_FORMAT_DXT1_ONEBITALPHA,
	IMAGE_FORMAT_BGRA5551,
	IMAGE_FORMAT_UV88,
	IMAGE_FORMAT_UVWQ8888,
	IMAGE_FORMAT_RGBA16161616F,
	IMAGE_FORMAT_RGBA16161616,
	IMAGE_FORMAT_UVLX8888
};

typedef struct
{
	GdkPixbufModuleSizeFunc     size_func;
	GdkPixbufModulePreparedFunc prepared_func; 
	GdkPixbufModuleUpdatedFunc  updated_func;
	gpointer                    user_data;

	guchar *buffer;
	guint buffer_size;
	guint buffer_data_size;
} VtfContext;

static gpointer
gdk_pixbuf__vtf_image_begin_load (GdkPixbufModuleSizeFunc size_func,
                                  GdkPixbufModulePreparedFunc prepared_func,
                                  GdkPixbufModuleUpdatedFunc updated_func,
                                  gpointer user_data,
                                  GError **error)
{
	VtfContext* context = g_malloc(sizeof(VtfContext));
	if (context == NULL) {
		g_set_error (
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			("Not enough memory"));
		return NULL;
	}
	
	context->size_func = size_func;
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;
	
	context->buffer_size = 1000000;
	context->buffer = g_malloc(context->buffer_size);
	context->buffer_data_size = 0;
    
	return (gpointer) context;
}

static gboolean
gdk_pixbuf__vtf_image_stop_load (gpointer context_ptr, GError **error)
{
	VtfContext *context = (VtfContext *) context_ptr;
	gboolean retval = TRUE;

	
	VtfHeader header;
	memcpy(&header, context->buffer, sizeof(header));
	
	if(header.signature[0] != 'V' || header.signature[1] != 'T' || header.signature[2] != 'F' || header.signature[3] != 0) {
		g_set_error (
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			("File corrupt or incomplete"));
	    retval = FALSE;
		goto end;
    }
    
    int i, j;
    GdkPixbuf* pixbuf;
    
    if(header.highResImageFormat == IMAGE_FORMAT_RGBA8888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 4;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 4*j + 0] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 2] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 3] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_ABGR8888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 4;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 4*j + 3] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 2] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 0] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_RGB888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 3;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 3*j + 0] = context->buffer[pos++];
            	pixels[stride*i + 3*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 3*j + 2] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_BGR888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 3;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 3*j + 2] = context->buffer[pos++];
            	pixels[stride*i + 3*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 3*j + 0] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_RGB565) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 2;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
    		    uint16_t c0 = context->buffer[pos++];
    		    c0 |= context->buffer[pos++] << 8;
    		    
                pixels[stride*i + 3*j + 0] = ((c0 >> 11) & 31) << 3;
            	pixels[stride*i + 3*j + 1] = ((c0 >> 5) & 63) << 2;
            	pixels[stride*i + 3*j + 2] = (c0 & 31) << 3;
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_I8) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 1;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
    		    uint8_t v = context->buffer[pos++];
    		    
                pixels[stride*i + 3*j + 0] = v;
            	pixels[stride*i + 3*j + 1] = v;
            	pixels[stride*i + 3*j + 2] = v;
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_IA88) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 2;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
    		    uint8_t v = context->buffer[pos++];
    		    
                pixels[stride*i + 4*j + 0] = v;
            	pixels[stride*i + 4*j + 1] = v;
            	pixels[stride*i + 4*j + 2] = v;
            	pixels[stride*i + 4*j + 3] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_A8) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 1;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 4*j + 0] = 255;
            	pixels[stride*i + 4*j + 1] = 255;
            	pixels[stride*i + 4*j + 2] = 255;
            	pixels[stride*i + 4*j + 3] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_DXT1) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - ((header.width+3)/4) * ((header.height+3)/4) * 8;
    	for (i = 0; i < header.height; i+=4) {
    		for (j = 0; j < header.width; j+=4) {
    		    uint16_t c0 = context->buffer[pos++];
    		    c0 |= context->buffer[pos++] << 8;
    		    
    		    uint16_t c1 = context->buffer[pos++];
    		    c1 |= context->buffer[pos++] << 8;
    		    
    		    uint16_t r[4], g[4], b[4];
    		    
    		    r[0] = ((c0 >> 11) & 31) << 3;
    		    g[0] = ((c0 >> 5) & 63) << 2;
    		    b[0] = (c0 & 31) << 3;
    		    
    		    r[1] = ((c1 >> 11) & 31) << 3;
    		    g[1] = ((c1 >> 5) & 63) << 2;
    		    b[1] = (c1 & 31) << 3;
    		    
    		    uint16_t a[4];
    		    a[0] = 255;
    		    a[1] = 255;
    		    
    		    if(c0 > c1) {
    		        r[2] = (2*r[0] + r[1])/3;
    		        g[2] = (2*g[0] + g[1])/3;
    		        b[2] = (2*b[0] + b[1])/3;
    		        a[2] = 255;
    		        
    		        r[3] = (r[0] + 2*r[1])/3;
    		        g[3] = (g[0] + 2*g[1])/3;
    		        b[3] = (b[0] + 2*b[1])/3;
    		        a[3] = 255;
    		    } else {
    		        r[2] = (r[0] + r[1])/2;
    		        g[2] = (r[0] + r[1])/2;
    		        b[2] = (r[0] + r[1])/2;
    		        a[2] = 255;
    		        
    		        r[3] = 0;
    		        g[3] = 0;
    		        b[3] = 0;
    		        a[3] = 0;
    		    }
    		    
    		    uint32_t sel = context->buffer[pos++];
    		    sel |= context->buffer[pos++] << 8;
    		    sel |= context->buffer[pos++] << 16;
    		    sel |= context->buffer[pos++] << 24;
    		    
    		    int ii, jj;
    		    for(ii = 0; ii < 4; ii++)
        		    for(jj = 0; jj < 4; jj++) {
        		        pixels[stride*(i+ii) + 4*(j+jj) + 0] = r[sel & 3];
        		        pixels[stride*(i+ii) + 4*(j+jj) + 1] = g[sel & 3];
        		        pixels[stride*(i+ii) + 4*(j+jj) + 2] = b[sel & 3];
        		        pixels[stride*(i+ii) + 4*(j+jj) + 3] = a[sel & 3];
            			sel >>= 2;
                    }
    		}
    	}
    } else if(header.highResImageFormat == IMAGE_FORMAT_DXT5) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - ((header.width+3)/4) * ((header.height+3)/4) * 16;
    	for (i = 0; i < header.height; i+=4) {
    		for (j = 0; j < header.width; j+=4) {
    		    {
        		    uint16_t a[8];
        		    
        		    a[0] = context->buffer[pos++];
        		    a[1] = context->buffer[pos++];
        		    
        		    if(a[0] > a[1]) {
        		        a[2] = (6*a[0] + 1*a[1])/7;
        		        a[3] = (5*a[0] + 2*a[1])/7;
        		        a[4] = (4*a[0] + 3*a[1])/7;
        		        a[5] = (3*a[0] + 4*a[1])/7;
        		        a[6] = (2*a[0] + 5*a[1])/7;
        		        a[7] = (1*a[0] + 6*a[1])/7;
        		    } else {
        		        a[2] = (4*a[0] + 1*a[1])/5;
        		        a[3] = (3*a[0] + 2*a[1])/5;
        		        a[4] = (2*a[0] + 3*a[1])/5;
        		        a[5] = (1*a[0] + 4*a[1])/5;
        		        a[6] = 0;
        		        a[7] = 255;
        		    }
        		    
        		    uint64_t sel = context->buffer[pos++];
        		    sel |= (uint64_t)context->buffer[pos++] << 8;
        		    sel |= (uint64_t)context->buffer[pos++] << 16;
        		    sel |= (uint64_t)context->buffer[pos++] << 24;
        		    sel |= (uint64_t)context->buffer[pos++] << 32;
        		    sel |= (uint64_t)context->buffer[pos++] << 40;
        		    
        		    int ii, jj;
        		    for(ii = 0; ii < 4; ii++)
            		    for(jj = 0; jj < 4; jj++) {
            		        pixels[stride*(i+ii) + 4*(j+jj)+3] = a[sel & 7];
                			sel >>= 3;
                        }
                }
                {
        		    uint16_t c0 = context->buffer[pos++];
        		    c0 |= context->buffer[pos++] << 8;
        		    
        		    uint16_t c1 = context->buffer[pos++];
        		    c1 |= context->buffer[pos++] << 8;
        		    
        		    uint16_t r[4], g[4], b[4];
        		    
        		    r[0] = ((c0 >> 11) & 31) << 3;
        		    g[0] = ((c0 >> 5) & 63) << 2;
        		    b[0] = (c0 & 31) << 3;
        		    
        		    r[1] = ((c1 >> 11) & 31) << 3;
        		    g[1] = ((c1 >> 5) & 63) << 2;
        		    b[1] = (c1 & 31) << 3;
        		    
    		        r[2] = (2*r[0] + r[1])/3;
    		        g[2] = (2*g[0] + g[1])/3;
    		        b[2] = (2*b[0] + b[1])/3;
    		        
    		        r[3] = (r[0] + 2*r[1])/3;
    		        g[3] = (g[0] + 2*g[1])/3;
    		        b[3] = (b[0] + 2*b[1])/3;
        		    
        		    uint32_t sel = context->buffer[pos++];
        		    sel |= context->buffer[pos++] << 8;
        		    sel |= context->buffer[pos++] << 16;
        		    sel |= context->buffer[pos++] << 24;
        		    
        		    int ii, jj;
        		    for(ii = 0; ii < 4; ii++)
            		    for(jj = 0; jj < 4; jj++) {
            		        pixels[stride*(i+ii) + 4*(j+jj)+0] = r[sel & 3];
            		        pixels[stride*(i+ii) + 4*(j+jj)+1] = g[sel & 3];
            		        pixels[stride*(i+ii) + 4*(j+jj)+2] = b[sel & 3];
                			sel >>= 2;
                        }
                }
    		}
    	}
    } else if(header.highResImageFormat == IMAGE_FORMAT_ARGB8888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 4;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 4*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 2] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 3] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 0] = context->buffer[pos++];
    		}
    } else if(header.highResImageFormat == IMAGE_FORMAT_BGRA8888) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, header.width, header.height);
        uint32_t stride = gdk_pixbuf_get_rowstride(pixbuf);
        guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
        int pos = context->buffer_data_size - header.width * header.height * 4;
    	for (i = 0; i < header.height; i++)
    		for (j = 0; j < header.width; j++) {
                pixels[stride*i + 4*j + 2] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 1] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 0] = context->buffer[pos++];
            	pixels[stride*i + 4*j + 3] = context->buffer[pos++];
    		}
    } else {
        printf("Unsupported VTF format - %i.\n", header.highResImageFormat);
		g_set_error (
			error,
			GDK_PIXBUF_ERROR,
			GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			("Can't read this yet."));
	    retval = FALSE;
		goto end;
    }
    
	context->prepared_func(pixbuf, NULL, context->user_data);

end:
    g_free(context->buffer);
	g_free(context);
	
	return retval;
}


static gboolean
gdk_pixbuf__vtf_image_load_increment (gpointer      context_ptr,
                                      const guchar *data,
                                      guint         size,
                                      GError      **error)
{
	VtfContext* context = (VtfContext*) context_ptr;
	
	context->buffer_data_size += size;
	
	if(context->buffer_data_size > context->buffer_size) {
	    while(context->buffer_data_size > context->buffer_size)
	        context->buffer_size *= 2;
	    context->buffer = g_realloc(context->buffer, context->buffer_size);
	    if(context->buffer == NULL) {
    		g_set_error (
    			error,
    			GDK_PIXBUF_ERROR,
    			GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
    			("Not enough memory"));
    		return FALSE;
	    }
	}
	
	memcpy(context->buffer + context->buffer_data_size - size, data, size);
	
	return TRUE;
}

#ifndef INCLUDE_vtf
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__vtf_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule* module)
{
	module->begin_load = gdk_pixbuf__vtf_image_begin_load;
	module->stop_load = gdk_pixbuf__vtf_image_stop_load;
	module->load_increment = gdk_pixbuf__vtf_image_load_increment;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
	static GdkPixbufModulePattern signature[] = {
		{ "VTF\0", NULL, 100 },
		{ NULL, NULL, 0 }
	};
	static gchar * mime_types[] = {
		"image/x-vtf",
		NULL
	};
	static gchar * extensions[] = {
		"vtf",
		NULL
	};

	info->name = "vtf";
	info->signature = signature;
	info->description = "Valve Texture format";
	info->mime_types = mime_types;
	info->extensions = extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->flags = 0;
	info->license = "LGPL";
}

