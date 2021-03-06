/* NOTE: This file is taken from libfov: http://libfov.sf.net. The copyright
 * notice follows, and applies to this file and fov.c ONLY:
 *
 * Copyright (c) 2006, Greg McIntyre
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the nor the names of its contributors may be
 *   used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Note also that this file (and fov.c) are *not* unchanged. It has been
 * converted by me (Jeremy Apthorp) to use fixed-point numbers, which are much
 * more suitable for the DS than are floating-point ones, amongst other minor
 * changes.
 */

/**
 * \mainpage Field of View Library
 * 
 * \section about About
 * 
 * This is a C library which implements a course-grained lighting
 * algorithm suitable for tile-based games such as roguelikes.
 * 
 * \section copyright Copyright
 * 
 * \verbinclude COPYING
 * 
 * \section thanks Thanks
 * 
 * Thanks to Bj&ouml;rn Bergstr&ouml;m
 * <bjorn.bergstrom@hyperisland.se> for the algorithm.
 * 
 */

/**
 * \file fov.h
 * Field-of-view algorithm for dynamically casting light/shadow on a
 * low resolution 2D raster.
 */
#ifndef LIBFOV_HEADER
#define LIBFOV_HEADER

#include <stddef.h>
#include <nds/jtypes.h> // for the typedefs. TODO: remove.

#ifdef __cplusplus
extern "C" {
#endif

/** Eight-way directions. */
typedef enum {
    FOV_EAST = 0,
    FOV_NORTHEAST,
    FOV_NORTH,
    FOV_NORTHWEST,
    FOV_WEST,
    FOV_SOUTHWEST,
    FOV_SOUTH,
    FOV_SOUTHEAST
} fov_direction_type;

/** Values for the shape setting. */
typedef enum {
    FOV_SHAPE_CIRCLE_PRECALCULATE,
    FOV_SHAPE_SQUARE,
    FOV_SHAPE_CIRCLE,
    FOV_SHAPE_OCTAGON
} fov_shape_type;

/** Values for the corner peek setting. */
typedef enum {
    FOV_CORNER_NOPEEK,
    FOV_CORNER_PEEK
} fov_corner_peek_type;

/** Values for the opaque apply setting. */
typedef enum {
    FOV_OPAQUE_APPLY,
    FOV_OPAQUE_NOAPPLY
} fov_opaque_apply_type;

typedef /*@null@*/ unsigned *height_array_t;

typedef struct {
    /** Opacity test callback. */
    /*@null@*/ bool (*opaque)(void *map, int x, int y);

    /** Lighting callback to set lighting on a map tile. */
    /*@null@*/ void (*apply)(void *map, int x, int y, int dx, int dy, void *src);

    /** Shape setting. */
    fov_shape_type shape;
    fov_corner_peek_type corner_peek;
    fov_opaque_apply_type opaque_apply;

    /* Pre-calculated data. */
    /*@null@*/ height_array_t *heights;

    /* Size of pre-calculated data. */
    unsigned numheights;
} fov_settings_type;

/** The opposite direction to that given. */
#define fov_direction_opposite(direction) ((fov_direction_type)(((direction)+4)&0x7))

/**
 * Set all the default options. You must call this option when you
 * create a new settings data structure.
 *
 * These settings are the defaults used:
 *
 * - shape: FOV_SHAPE_CIRCLE_PRECALCULATE
 * - corner_peek: FOV_CORNER_NOPEEK
 * - opaque_apply: FOV_OPAQUE_APPLY
 *
 * Callbacks still need to be set up after calling this function.
 *
 * \param settings Pointer to data structure containing settings.
 */
void fov_settings_init(fov_settings_type *settings);

fov_settings_type *build_fov_settings(
    bool (*opaque)(void *map, int x, int y),
    void (*apply)(void *map, int x, int y, int dx, int dy, void *src),
    fov_shape_type shape);

/**
 * Set the shape of the field of view.
 *
 * \param settings Pointer to data structure containing settings.
 * \param value One of the following values, where R is the radius:
 *
 * - FOV_SHAPE_CIRCLE_PRECALCULATE \b (default): Limit the FOV to a
 * circle with radius R by precalculating, which consumes more memory
 * at the rate of 4*(R+2) bytes per R used in calls to fov_circle. 
 * Each radius is only calculated once so that it can be used again. 
 * Use fov_free() to free this precalculated data's memory.
 *
 * - FOV_SHAPE_CIRCLE: Limit the FOV to a circle with radius R by
 * calculating on-the-fly.
 *
 * - FOV_SHAPE_OCTOGON: Limit the FOV to an octogon with maximum radius R.
 *
 * - FOV_SHAPE_SQUARE: Limit the FOV to an R*R square.
 */
void fov_settings_set_shape(fov_settings_type *settings, fov_shape_type value);

/**
 * <em>NOT YET IMPLEMENTED</em>.
 *
 * Set whether sources will peek around corners.
 *
 * \param settings Pointer to data structure containing settings.
 * \param value One of the following values:
 *
 * - FOV_CORNER_PEEK \b (default): Renders:
\verbatim
  ........
  ........
  ........
  ..@#    
  ...#    
\endverbatim
 * - FOV_CORNER_NOPEEK: Renders:
\verbatim
  ......
  .....
  ....
  ..@#
  ...#
\endverbatim
 */
void fov_settings_set_corner_peek(fov_settings_type *settings, fov_corner_peek_type value);

/**
 * Whether to call the apply callback on opaque tiles.
 *
 * \param settings Pointer to data structure containing settings.
 * \param value One of the following values:
 *
 * - FOV_OPAQUE_APPLY \b (default): Call apply callback on opaque tiles.
 * - FOV_OPAQUE_NOAPPLY: Do not call the apply callback on opaque tiles.
 */
void fov_settings_set_opaque_apply(fov_settings_type *settings, fov_opaque_apply_type value);

/**
 * Set the function used to test whether a map tile is opaque.
 *
 * \param settings Pointer to data structure containing settings.
 * \param f The function called to test whether a map tile is opaque.
 */
void fov_settings_set_opacity_test_function(fov_settings_type *settings, bool (*f)(void *map, int x, int y));

/**
 * Set the function used to apply lighting to a map tile.
 *
 * \param settings Pointer to data structure containing settings.
 * \param f The function called to apply lighting to a map tile.
 */
void fov_settings_set_apply_lighting_function(fov_settings_type *settings, void (*f)(void *map, int x, int y, int dx, int dy, void *src));

/**
 * Free any memory that may have been cached in the settings
 * structure.
 *
 * \param settings Pointer to data structure containing settings.
 */
void fov_settings_free(fov_settings_type *settings);

/**
 * Calculate a full circle field of view from a source at (x,y).
 *
 * \param settings Pointer to data structure containing settings.
 * \param map Pointer to map data structure to be passed to callbacks.
 * \param source Pointer to data structure holding source of light.
 * \param source_x x-axis coordinate from which to start.
 * \param source_y y-axis coordinate from which to start.
 * \param radius Euclidean distance from (x,y) after which to stop.
 */
void fov_circle(fov_settings_type *settings, void *map, void *source,
                int source_x, int source_y, unsigned radius
);

/**
 * Calculate a field of view from source at (x,y), pointing
 * in the given direction and with the given angle. The larger
 * the angle, the wider, "less focused" the beam. Each side of the
 * line pointing in the direction from the source will be half the
 * angle given such that the angle specified will be represented on
 * the raster.
 *
 * \param settings Pointer to data structure containing settings.
 * \param map Pointer to map data structure to be passed to callbacks.
 * \param source Pointer to data structure holding source of light.
 * \param source_x x-axis coordinate from which to start.
 * \param source_y y-axis coordinate from which to start.
 * \param radius Euclidean distance from (x,y) after which to stop.
 * \param direction One of eight directions the beam of light can point.
 * \param angle The angle at the base of the beam of light, in degrees.
 */
void fov_beam(fov_settings_type *settings, void *map, void *source,
              int source_x, int source_y, unsigned radius,
              fov_direction_type direction, int32 angle
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
