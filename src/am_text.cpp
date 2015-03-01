#include "amulet.h"

#define ADVANCE_SCALE 0.015625f

static FT_Library ft_library;
static bool ft_initialized = false;

static void ensure_ft_init() {
    if (!ft_initialized) {
        if (FT_Init_FreeType(&ft_library)) {
            am_abort("error initializing freetype library");
        }
        ft_initialized = true;
    }
}

static int load_font(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    const char *name = luaL_checkstring(L, 1);
    int len;
    char *errmsg;
    void *data = am_read_resource(name, &len, false, &errmsg);
    if (data == NULL) {
        lua_pushstring(L, errmsg);
        free(errmsg);
        return lua_error(L);
    }
    am_font *font = am_new_userdata(L, am_font);
    if (FT_New_Memory_Face(ft_library, (const FT_Byte*)data, len, 0, &font->face)) {
        return luaL_error(L, "error reading font '%s'", name);
    }
    int width = luaL_checkinteger(L, 2);
    int height = 0;
    if (width <= 0) {
        return luaL_error(L, "expecting a positive integer in position 2 (in fact %d)", width);
    }
    if (nargs > 2) {
        height = luaL_checkinteger(L, 3);
        if (height <= 0) {
            return luaL_error(L, "expecting a positive integer in position 3 (in fact %d)", height);
        }
    }
    if (FT_Set_Pixel_Sizes(font->face, width, height)) {
        if (nargs > 2) {
            return luaL_error(L, "size %dx%d is not supported by font '%s'", width, height, name);
        } else {
            return luaL_error(L, "size %d is not supported by font '%s'", width, name);
        }
    }
    return 1;
}

static int font_gc(lua_State *L) {
    am_font *font = am_get_userdata(L, am_font, 1);
    FT_Done_Face(font->face);
    return 0;
}

static void draw_bitmap(FT_Bitmap *bitmap, float leftf, float topf, am_image *img) {
    int bleft = (int)roundf(leftf);
    int btop = (int)roundf(topf);
    int bwidth = bitmap->width;
    int bheight = bitmap->rows;
    int bpitch = bitmap->pitch;
    unsigned char *bptr = bitmap->buffer;
    unsigned char bmode = bitmap->pixel_mode;
    int iwidth = img->width;
    uint8_t *ibuffer = img->buffer->data;
    int isize = img->buffer->size;

    if (bmode != FT_PIXEL_MODE_GRAY) {
        am_log1("WARNING: unsupported pixel mode %d in glyph bitmap (glyph will not be rendered)", bmode);
        return;
    }

    switch (img->format) {
        case AM_PIXEL_FORMAT_RGBA8: {
            for (int row = 0; row < bheight; row++) {
                int ioffset = (btop - row) * iwidth * 4 + bleft * 4;
                if (ioffset >= isize || ioffset < 0) break;
                for (int i = 0; i < bwidth && bleft + i < iwidth; i++) {
                    unsigned char value = bptr[i];
                    ibuffer[ioffset++] = value;
                    ibuffer[ioffset++] = value;
                    ibuffer[ioffset++] = value;
                    ibuffer[ioffset++] = value;
                }
                bptr += bpitch;
            }
            if (img->buffer->track_dirty) {
                img->buffer->dirty_start = btop * iwidth * 4 + bleft * 4;
                img->buffer->dirty_end = (btop + bheight - 1) * iwidth * 4 + (bleft + bwidth) * 4;
            }
            break;
        }
    }
}

static int render_text(lua_State *L) {
    am_check_nargs(L, 4);
    am_font *font = am_get_userdata(L, am_font, 1);
    am_image *img = am_get_userdata(L, am_image, 2);
    glm::vec2 pen = am_get_userdata(L, am_vec2, 3)->v;
    size_t n;
    const char *text = luaL_checklstring(L, 4, &n);
    FT_Face face = font->face;
    FT_GlyphSlot slot = face->glyph;
    for (int i = 0; i < n; i++) {
        uint32_t c = text[i];
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            am_log1("WARNING: error loading font character %u", c);
            continue;
        }
        float left = pen.x + (float)slot->bitmap_left;
        float top = pen.y + (float)slot->bitmap_top;
        draw_bitmap(&slot->bitmap, left, top, img);
        pen.x += ((float)slot->advance.x) * ADVANCE_SCALE;
    }
    return 0;
}

static void register_font_mt(lua_State *L) {
    lua_newtable(L);

    lua_pushcclosure(L, am_default_index_func, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcclosure(L, am_default_newindex_func, 0);
    lua_setfield(L, -2, "__newindex");
    lua_pushcclosure(L, font_gc, 0);
    lua_setfield(L, -2, "__gc");

    lua_pushcclosure(L, render_text, 0);
    lua_setfield(L, -2, "render_text");

    lua_pushstring(L, "font");
    lua_setfield(L, -2, "tname");

    am_register_metatable(L, MT_am_font, 0);
}

void am_open_text_module(lua_State *L) {
    ensure_ft_init();
    luaL_Reg funcs[] = {
        {"load_font", load_font},
        {NULL, NULL}
    };
    am_open_module(L, AMULET_LUA_MODULE_NAME, funcs);
    register_font_mt(L);
}