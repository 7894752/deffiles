/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Surface parameters shared by model and collision shader processing.
#include "surfaceflags.h"
#include <stddef.h>

InfoParm infoParms[] = {
    { "bark", 0, SURF_TYPE_BARK, 0 },
    { "brick", 0, SURF_TYPE_BRICK, 0 },
    { "carpet", 0, SURF_TYPE_CARPET, 0 },
    { "cloth", 0, SURF_TYPE_CLOTH, 0 },
    { "concrete", 0, SURF_TYPE_CONCRETE, 0 },
    { "dirt", 0, SURF_TYPE_DIRT, 0 },
    { "flesh", 0, SURF_TYPE_FLESH, 0 },
    { "foliage", 1, SURF_TYPE_FOLIAGE, CONTENTS_FOLIAGE },
    { "glass", 1, SURF_TYPE_GLASS, CONTENTS_GLASS },
    { "grass", 0, SURF_TYPE_GRASS, 0 },
    { "gravel", 0, SURF_TYPE_GRAVEL, 0 },
    { "ice", 0, SURF_TYPE_ICE, 0 },
    { "metal", 0, SURF_TYPE_METAL, 0 },
    { "mud", 0, SURF_TYPE_MUD, 0 },
    { "paper", 0, SURF_TYPE_PAPER, 0 },
    { "plaster", 0, SURF_TYPE_PLASTER, 0 },
    { "rock", 0, SURF_TYPE_ROCK, 0 },
    { "sand", 0, SURF_TYPE_SAND, 0 },
    { "snow", 0, SURF_TYPE_SNOW, 0 },
    { "water", 1, SURF_TYPE_WATER, CONTENTS_WATER },
    { "wood", 0, SURF_TYPE_WOOD, 0 },
    { "asphalt", 0, SURF_TYPE_ASPHALT, 0 },
    { "opaqueglass", 0, SURF_TYPE_OPAQUEGLASS, 0 },
    { "clipmissile", 1, 0, CONTENTS_MISSILECLIP },
    { "ai_nosight", 1, 0, CONTENTS_AI_NOSIGHT },
    { "clipshot", 1, 0, CONTENTS_CLIPSHOT },
    { "playerclip", 1, 0, CONTENTS_PLAYERCLIP },
    { "monsterclip", 1, 0, CONTENTS_MONSTERCLIP },
    { "vehicleclip", 1, 0, CONTENTS_VEHICLECLIP },
    { "itemclip", 1, 0, CONTENTS_ITEMCLIP },
    { "nodrop", 1, 0, CONTENTS_NODROP },
    { "nonsolid", 1, SURF_NONSOLID, 0 },
    { "origin", 1, 0, CONTENTS_ORIGIN },
    { "trans", 0, 0, CONTENTS_TRANSLUCENT },
    { "noncolliding", 1, 0, CONTENTS_NONCOLLIDING },
    { "areaportal", 1, 0, CONTENTS_AREAPORTAL },
    { "detail", 0, 0, CONTENTS_DETAIL },
    { "structural", 0, 0, CONTENTS_STRUCTURAL },
    { "portal", 1, SURF_PORTAL, 0 },
    { "canshootclip", 0, 0, CONTENTS_CANSHOOTCLIP },
    { "sky", 0, SURF_SKY, CONTENTS_SKY },
    { "lightfilter", 0, SURF_LIGHTFILTER, 0 },
    { "alphashadow", 0, SURF_ALPHASHADOW, 0 },
    { "castshadow", 0, SURF_CASTSHADOW, 0 },
    { "hint", 0, SURF_HINT, 0 },
    { "slick", 0, SURF_SLICK, 0 },
    { "noimpact", 0, SURF_NOIMPACT, 0 },
    { "nomarks", 0, SURF_NOMARKS, 0 },
    { "ladder", 0, SURF_LADDER, 0 },
    { "nodamage", 0, SURF_NODAMAGE, 0 },
    { "nosteps", 0, SURF_NOSTEPS, 0 },
    { "nodraw", 0, SURF_NODRAW, 0 },
    { "pointlight", 0, SURF_POINTLIGHT, 0 },
    { "nolightmap", 0, SURF_NOLIGHTMAP, 0 },
    { "nodlight", 0, SURF_NODLIGHT, 0 },
    { NULL, 0, 0, 0 }
};
