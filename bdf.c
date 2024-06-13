/*  BSD License
    Copyright (c) 2014 Andrey Chilikin https://github.com/achilikin

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

/**
    Basic BDF Exporter - converts BDF files to C structures.
    Only 8 bits wide fonts are supported.
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "bdf.h"

const char *filename(const char *name)
{
	const char *file = name;

	while (*file) file++;

	while (file != name) {
		if (*file == '/' || *file == '\\') {
			file++;
			break;
		}
		file--;
	}

	return file;
}

// check if 'buf' starts with 'key' and store pointer to the argument
static char *key_arg(char *buf, const char *key, char **arg)
{
    char *end;
    size_t len = strlen(key);

    if (strncmp(buf, key, len) == 0)
    {
        end = buf + len;
        if (*end > ' ')
            return NULL;
        if (arg == NULL)
            return buf;
        for (; *end <= ' ' && *end != '\0'; end++)
            ;
        *arg = end;
        return buf;
    }

    return NULL;
}

int bdf_convert(const char *name, unsigned gmin, unsigned gmax, unsigned ascender, int flags)
{
    FILE *fp;
    char *arg;
    char buf[BUFSIZ];
    char startchar[BUFSIZ];
    unsigned dx = 0, dy = 0, ascent = 0;
    unsigned bytes = 0;

    if (name == NULL || (fp = fopen(name, "r")) == NULL)
        return 0;

    memset(buf, 0, sizeof(buf));
    int mute = flags & BDF_MUTE;

    if (!mute && (flags & BDF_HEADER))
    {
        printf("// File generated by 'bdfe");
        if (flags & BDF_NATIVE)
            printf(" -n");
        if (flags & BDF_ROTATE)
            printf(" -r");
        if (ascender)
            printf(" -a %d", ascender);
        printf(" -s %d-%d %s'\n", gmin, gmax, filename(name));
    }

    while (fgets(buf, sizeof(buf) - 2, fp) != NULL)
    {
        arg = strchr(buf, '\0');
        while (*arg < ' ' && arg != buf)
            *arg-- = '\0';

        if (!mute && (flags & BDF_HEADER))
        {
            if (key_arg(buf, "FONT", &arg))
                printf("// %s\n", buf);
            if (key_arg(buf, "COMMENT", &arg))
                printf("// %s\n", buf);
            if (key_arg(buf, "COPYRIGHT", &arg))
                printf("// %s\n", buf);
            if (key_arg(buf, "FONT_ASCENT", &arg))
                printf("// %s\n", buf);
        }

        if (key_arg(buf, "FONTBOUNDINGBOX", &arg))
        {
            if (!mute && (flags & BDF_HEADER))
                printf("// %s\n", buf);
            dx = strtoul(arg, &arg, 10);
            dy = strtoul(arg, &arg, 10);
            strtoul(arg, &arg, 10);
            strtoul(arg, &arg, 10);
            // Calculate the number of bytes needed to store a line of the glyph
            bytes = (dx + 7) / 8;
        }
        if (key_arg(buf, "FONT_ASCENT", &arg))
        {
            if (!mute && (flags & BDF_HEADER))
                printf("// %s\n", buf);
            ascent = strtoul(arg, &arg, 10);
        }
        if (key_arg(buf, "FONT_DESCENT", &arg))
        {
            if (!mute && (flags & BDF_HEADER))
                printf("// %s\n", buf);
            strtoul(arg, &arg, 10);
        }
    }

    if (dy == 0)
    {
        fclose(fp);
        return 0;
    }

    if (!mute && (flags & BDF_HEADER))
        printf("// Converted Font Size %dx%d\n\n", dx, dy);

    fseek(fp, 0, SEEK_SET);

    unsigned gsize = (dy * bytes);
    uint8_t *glyph = (uint8_t *)malloc(gsize);
    memset(glyph, 0, gsize);

    while (fgets(buf, sizeof(buf) - 2, fp) != NULL)
    {
        if (key_arg(buf, "STARTCHAR", &arg))
        {
            int displacement = 0;
            unsigned bitmap = 0, i = 0, idx = 0;
            unsigned bbw = 0, bbh = 0;
            int bbox = 0, bboy = 0;
            // store a copy in case if verbose output is on
            arg = strchr(buf, '\0');
            while (*arg < ' ' && arg != buf)
                *arg-- = '\0';
            strcpy(startchar, buf);

            while (fgets(buf, sizeof(buf) - 2, fp) != NULL)
            {
                arg = strchr(buf, '\0');
                while (*arg < ' ' && arg != buf)
                    *arg-- = '\0';

                if (key_arg(buf, "ENCODING", &arg))
                {
                    idx = atoi(arg);
                    if (idx < gmin || idx > gmax)
                        // break;
                    if (!mute && !(flags & BDF_GPL) && (flags & BDF_VERBOSE))
                    {
                        printf("// %s\n", startchar);
                        printf("// %s\n", buf);
                    }
                }
                if (key_arg(buf, "BBX", &arg))
                {
                    if (!mute && !(flags & BDF_GPL) && (flags & BDF_VERBOSE))
                        printf("// %s\n", buf);
                    bbw = strtol(arg, &arg, 10);
                    bbh = strtol(arg, &arg, 10); // skip bbh, we'll calculate it (i)
                    bbox = strtol(arg, &arg, 10);
                    bboy = strtol(arg, &arg, 10);
                }
                if (key_arg(buf, "ENDCHAR", &arg))
                {
                    bitmap = 0;
                    // Displacement caculation
                    // Ascent - Glyph BBX Y value + Glyph decent X value
                    displacement = ascent - bbh - bboy;

                    if (!mute)
                    {
                        // glyph per line
                        if (flags & BDF_GPL)
                        {
                            printf("\t");
                            for (int disp_i = 0; disp_i < displacement; disp_i++)
                            {
                                for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                                    printf("0x00, ");
                                }
                            }


                            for (i = 0; i < (dy-displacement); i++)
                            {
                                for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                                    printf("0x%02X, ", glyph[(i*bytes)+byte_idx]);
                                }
                            }

                            printf(" // %5d", idx);
                            if (isprint(idx))
                                printf(" '%c'", idx);
                            printf("\n");
                        }
                        else
                        {
                            if(dx > 8 ) {
                                printf("//      %5d '%c' |", idx, isprint(idx) ? idx : ' ');
                            }
                            else {
                                printf("// %5d '%c' |", idx, isprint(idx) ? idx : ' ');
                            }

                            for (i = 0; i < dx; i++)
                                printf("%d", i);
                            printf("|\n");

                            for (int disp_i = 0; disp_i < displacement; disp_i++)
                            {
                                for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                                    printf(" 0x00,");
                                }
                                printf(" //  %2d|", disp_i);

                                for (unsigned bit = 0; bit < dx; bit++)
                                {
                                    printf(" ");
                                }
                                printf("|\n");
                            }

                            for (i = 0; i < (dy - displacement); i++)
                            {
                                for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                                    printf(" 0x%02X,", glyph[(i*bytes)+byte_idx]);
                                }
                                printf(" //  %2d|", i+displacement);

                                for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                                    for(int bit = 0; bit < 8; bit++) {
                                        printf("%c", (glyph[(i*bytes)+byte_idx] & (0x80 >> bit)) ? '#' : ' ');
                                    }
                                }

                                printf("|\n");
                            }
                            printf("\n");
                        }
                    }
                    break;
                }
                if (key_arg(buf, "BITMAP", &arg))
                {
                    bitmap = 1;
                    i = 0;
                    memset(glyph, 0x00, gsize);
                    continue;
                }
                if (bitmap && (i < gsize))
                {
                    uint8_t valCharCount = strlen(buf);
                    uint32_t bitValue = strtoul(buf, NULL, 16);
                    (void)bbw;
                   
                    // Align the bitValue to the left
                    uint8_t left_shift = (bytes - (valCharCount / 2)) * 8;
                    bitValue = bitValue << left_shift;

                    // Shift the bitValue based on the bounding box x offset
                    bitValue = bitValue >> bbox;
                   
                    for(int byte_idx = 0; byte_idx < bytes; byte_idx++) {
                        glyph[i++] = (uint8_t)(bitValue >> ((bytes - byte_idx - 1) * 8));
                    }
                }
            }
        }
    }

    free(glyph);
    fclose(fp);
    return 0;
}
