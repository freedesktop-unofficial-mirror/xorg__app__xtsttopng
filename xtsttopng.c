/*
 * Copyright © 2014 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>
#include <math.h>
#include <png.h>

#define MAX_LEVEL       32

struct xts_color {
	uint32_t            pixel;
	uint16_t            r, g, b;
	uint16_t            level;
	struct xts_color    *next[0];
};

struct xts_color    *colors[MAX_LEVEL];
int                 num_colors;

static uint16_t
random_level (void)
{
	/* tricky bit -- each bit is '1' 75% of the time */
	long int	bits = random () | random ();
	uint16_t	level = 0;

	while (++level < MAX_LEVEL)
	{
		if (bits & 1)
			break;
		bits >>= 1;
	}
	return level;
}

static struct xts_color *
alloc_color (void) {
	uint16_t    level = random_level();

	struct xts_color *c = calloc(1, sizeof (struct xts_color) + level * sizeof (struct xts_color *));
	if (!c)
		return NULL;
	c->level = level;
	return c;
}

static uint16_t
float_to_uint(float x) {
	return floor (x * 255.0);
}

static void
assign_hsv(struct xts_color *color, float h, float s, float v)
{
	uint16_t    r, g, b;

	printf ("%f %f %f\n", h, s, v);
	if (v == 0) {
		r = g = b = 0;
	} else if (s == 0) {
		r = g = b = float_to_uint(v);
	} else {
		float   h6 = h * 6;
		int i;
		float f, p, q, t;
		while (h6 >= 6)
			h6 -= 6;
		i = floor (h6);
		f = h6 - i;
		p = v * (1 - s);
		q = v * (1 - (s * f));
		t = v * (1 - (s * (1 - f)));

		switch (i) {
		default:
		case 0:
			r = float_to_uint(v);
			g = float_to_uint(t);
			b = float_to_uint(p);
			break;
		case 1:
			r = float_to_uint(q);
			g = float_to_uint(v);
			b = float_to_uint(p);
			break;
		case 2:
			r = float_to_uint(p);
			g = float_to_uint(v);
			b = float_to_uint(t);
			break;
		case 3:
			r = float_to_uint(p);
			g = float_to_uint(q);
			b = float_to_uint(v);
			break;
		case 4:
			r = float_to_uint(t);
			g = float_to_uint(p);
			b = float_to_uint(v);
			break;
		case 5:
			r = float_to_uint(v);
			g = float_to_uint(p);
			b = float_to_uint(q);
			break;
		}
	}
	printf ("\t%d %d %d\n", r, g, b);
	color->r = r;
	color->g = g;
	color->b = b;
}

struct xts_image {
	struct xts_image	*next;
	char			*dest_file;
	int			width, height, depth;
	uint32_t		pixels[];
};

static void
assign_rgb(void) {
	int i;
	struct xts_color    *c;

	i = 0;
	for (c = colors[0]; c; c = c->next[0]) {
		float	h, s, v;
		if (i >= 2) {
			h = (float) (i - 2) / (num_colors - 2);
			s = 1;
			v = 0.5;
		} else {
			h = 0;
			s = 0;
			v = 1-i;
		}

		assign_hsv(c, h, s, v);
		i++;
	}
}

static struct xts_color *
find_color(struct xts_image *image, uint32_t pixel) {
	struct xts_color    **update[MAX_LEVEL];
	struct xts_color    *s, **next;
	int i;

	next = colors;
	for (i = MAX_LEVEL; --i >= 0;) {
		for (; (s = next[i]); next = s->next) {
			if (s->pixel == pixel)
				return s;
			if (s->pixel > pixel)
				break;
		}
		update[i] = &next[i];
	}

	s = alloc_color();
	s->pixel = pixel;
	++num_colors;

	for (i = 0; i < s->level; i++) {
		s->next[i] = *update[i];
		*update[i] = s;
	}
	return s;
}

static void
free_image(struct xts_image *image)
{
	free (image->dest_file);
	free (image);
}

static struct xts_image *
read_image(FILE *file)
{
	int width, height, depth;
	struct xts_image *image;
	int count;
	int run;
	uint32_t pixel;
	uint32_t *pixels;
	char line[80];

	if (fscanf(file, "%d %d %d\n", &width, &height, &depth) != 3)
		return NULL;

	image = malloc (sizeof (struct xts_image) +
			(width * height * sizeof (uint32_t)));
	if (!image)
		return NULL;
    
	count = width * height;
	pixels = image->pixels;
	image->width = width;
	image->height = height;
	image->depth = depth;
	while (count > 0) {
		if (fgets(line, sizeof(line), file) == NULL) {
			printf ("run bad\n");
			free(image);
			return NULL;
		}
		if (sscanf (line, "%x,%x\n", &run, &pixel) != 2) {
			if (sscanf(line, "%x", &pixel) != 1) {
				printf ("run bad\n");
				free(image);
				return NULL;
			}
			run = 1;
		}
		find_color(image, pixel);
		while (run > 0 && count > 0) {
			*pixels++ = pixel;
			run--;
			count--;
		}
		if (run) {
			printf ("run left over\n");
			free(image);
			return NULL;
		}
	}
	return image;
}

static void
png_simple_output_flush_fn (png_structp png_ptr)
{
}

static void
stdio_write_func (png_structp png, png_bytep data, png_size_t size)
{
	FILE *fp;

	fp = png_get_io_ptr (png);
	while (size) {
		size_t ret = fwrite (data, 1, size, fp);
		size -= ret;
		data += ret;
		if (size && ferror (fp)) {
			exit (1);
		}
	}
}

static void
dump_png(FILE *file, struct xts_image *image)
{
	png_struct *png;
	png_info *info;
	png_byte **rows = NULL;
	int status;
	uint32_t *rgb, *r;
	uint32_t *pixels, pixel;
	int i;
	int count;
	struct xts_color *color;

	rgb = malloc (image->height * image->width * sizeof (uint32_t));
	if (!rgb)
		return;
    
	r = rgb;
	pixels = image->pixels;
	count = image->width * image->height;
	while (count-- > 0)  {
		pixel = *pixels++;
		color = find_color(image, pixel);
		*r++ = (color->b << 16) | (color->g << 8) | color->r;
	}

	rows = calloc(image->height, sizeof (png_byte *));

	for (i = 0; i < image->height; i++)
		rows[i] = (png_byte *) rgb + i * image->width * sizeof (uint32_t);

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &status,
				      NULL, NULL);

	info = png_create_info_struct(png);

	png_set_write_fn (png, file, stdio_write_func, png_simple_output_flush_fn);

	png_set_IHDR (png, info,
		      image->width,
		      image->height,
		      8,
		      PNG_COLOR_TYPE_RGB,
		      PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png, info);
	png_set_filler (png, 0, PNG_FILLER_AFTER);
	png_write_image (png, rows);
	png_write_end (png, info);
	png_destroy_write_struct (&png, &info);
	free (rows);
	free (rgb);
}

static char *
newname(char *orig_name, int i, const char *extension) {
	char        *b = basename(orig_name);
	char        *dot = strrchr(b, '.');
	char        *new;
	if (!dot)
		dot = b + strlen(b);
	*dot = '\0';

	if (!asprintf(&new, "%s-%d.%s", b, i, extension))
		new = NULL;
	return new;
}

int
main (int argc, char **argv)
{
	struct xts_image *image;
	FILE        *input;
	FILE        *output;
	char        *inname;
	int         f, i;
	struct xts_image	*images = NULL, **last = &images;

	for (f = 1; f < argc; f++) {
		inname = argv[f];
		input = fopen(inname, "r");
		if (!input) {
			perror(inname);
			continue;
		}
		i = 0;
		while ((image = read_image(input)) != NULL) {
			image->dest_file = newname(inname, i++, "png");
			image->next = NULL;
			*last = image;
			last = &image->next;
		}
	}
	printf ("%d colors\n", num_colors);
	assign_rgb();
	while ((image = images) != NULL) {
		if (image->dest_file) {
			output = fopen(image->dest_file, "w");
			if (!output)
				perror(image->dest_file);
			else {
				dump_png(output, image);
				fclose(output);
			}
		}
		images = image->next;
		free_image(image);
	}
}
