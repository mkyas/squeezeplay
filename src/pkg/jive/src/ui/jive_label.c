/*
** Copyright 2007 Logitech. All Rights Reserved.
**
** This file is subject to the Logitech Public Source License Version 1.0. Please see the LICENCE file for details.
*/

#include "common.h"
#include "jive.h"


typedef struct label_widget {
	JiveWidget w;

	// skin properties
	JiveFont *font;
	Uint16 line_height;
	Uint16 line_width;
	Uint16 label_x, label_y; // label position
	Uint16 label_w, label_h;
	JiveAlign text_align;
	JiveAlign icon_align;
	bool is_sh;
	Uint32 fg;
	Uint32 sh;
	JiveTile *bg_tile;
} LabelWidget;


static JivePeerMeta labelPeerMeta = {
	sizeof(LabelWidget),
	"JiveLabel",
	jiveL_label_gc,
};



int jiveL_label_skin(lua_State *L) {
	LabelWidget *peer;
	JiveTile *bg_tile;

	/* stack is:
	 * 1: widget
	 */

	lua_pushcfunction(L, jiveL_style_path);
	lua_pushvalue(L, -2);
	lua_call(L, 1, 0);

	peer = jive_getpeer(L, 1, &labelPeerMeta);

	jive_widget_pack(L, 1, (JiveWidget *)peer);

	peer->font = jive_font_ref(jive_style_font(L, 1, "font"));
	peer->fg = jive_style_color(L, 1, "fg", JIVE_COLOR_BLACK, NULL);
	peer->sh = jive_style_color(L, 1, "sh", JIVE_COLOR_WHITE, &(peer->is_sh));

	bg_tile = jive_style_tile(L, 1, "bgImg", NULL);
	if (bg_tile != peer->bg_tile) {
		if (peer->bg_tile) {
			jive_tile_free(peer->bg_tile);
		}
		peer->bg_tile = jive_tile_ref(bg_tile);
	}

	peer->line_height = jive_style_int(L, 1, "lineHeight", jive_font_ascend(peer->font));
	peer->line_width = jive_style_int(L, 1, "textW", JIVE_WH_NIL);

	peer->text_align = jive_style_align(L, 1, "textAlign", JIVE_ALIGN_LEFT);
	peer->icon_align = jive_style_align(L, 1, "iconAlign", JIVE_ALIGN_RIGHT);


	// XXXX should not have to call pack here but when the label style
	// is modified the icon do not get correctly updated

	/* pack widgets */
	lua_getfield(L, 1, "widget");
	if (!lua_isnil(L, -1)) {
		/* pack widget */
		if (jive_getmethod(L, -1, "reSkin")) {
			lua_pushvalue(L, -2);
			lua_call(L, 1, 0);
		}
	}
	lua_pop(L, 1);

	return 0;
}


int jiveL_label_prepare(lua_State *L) {
	LabelWidget *peer;
	int max_width = 0;
	int lines = 1;
	const char *str, *ptr;

	peer = jive_getpeer(L, 1, &labelPeerMeta);

	/* split multi-line text */
	lua_getglobal(L, "tostring");
	lua_getfield(L, 1, "value");
	lua_call(L, 1, 1);

	ptr = str = lua_tostring(L, -1);

	lua_newtable(L);
	if (ptr) {
		while (*ptr != '\0') {
			if (*ptr == '\n' || *ptr == '\r') {
				lua_pushlstring(L, str, ptr - str);
				lua_rawseti(L, -2, lines++);
				
				while (*ptr == '\n' || *ptr == '\r') {
					ptr++;
				}
				str = ptr;
			}

			ptr++;
		} 

		lua_pushlstring(L, str, ptr - str);
		lua_rawseti(L, -2, lines);
	}

	lua_setfield(L, 1, "text");
	lua_pop(L, 1);


	/* text height */
	// FIXME not all lines are the same height
	peer->label_h = lines * peer->line_height;


	/* measure text width */
	if (peer->line_width == JIVE_WH_NIL) {
		lua_getfield(L, 1, "text");
		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			str = lua_tostring(L, -1);
			max_width = MAX(max_width,
					jive_font_width(peer->font, str));
			
			printf("TEXT: %s %d\n", str, max_width);
			lua_pop(L, 1);

		}
		lua_pop(L, 1);

		peer->label_w = max_width;
	}
	else {
		peer->label_w = peer->line_width;
	}

	return 0;
}


int jiveL_label_layout(lua_State *L) {
	LabelWidget *peer;

	/* stack is:
	 * 1: widget
	 */

	peer = jive_getpeer(L, 1, &labelPeerMeta);

	peer->label_x = jive_widget_halign((JiveWidget *)peer, peer->text_align, peer->label_w);
	peer->label_y = jive_widget_valign((JiveWidget *)peer, peer->text_align, peer->label_h);

	/* layout widget */
	lua_getfield(L, 1, "widget");
	if (!lua_isnil(L, -1)) {
		int x, y, w, h;

		x = peer->w.bounds.x;
		y = peer->w.bounds.y;
		w = 0;
		h = 0;

		if (jive_getmethod(L, -1, "getPreferredBounds")) {
			lua_pushvalue(L, -2);
			lua_call(L, 1, 4);

			if (!lua_isnil(L, -4)) {
				x = lua_tointeger(L, -4);
			}
			if (!lua_isnil(L, -3)) {
				y = lua_tointeger(L, -3);
			}
			if (!lua_isnil(L, -2)) {
				w = lua_tointeger(L, -2);
			}
			if (!lua_isnil(L, -1)) {
				h = lua_tointeger(L, -1);
			}

			lua_pop(L, 4);
		}

		x = peer->w.bounds.x + jive_widget_halign((JiveWidget *)peer, peer->icon_align, w);
		y = peer->w.bounds.y + jive_widget_valign((JiveWidget *)peer, peer->icon_align, h);

		if (jive_getmethod(L, -1, "setBounds")) {
			lua_pushvalue(L, -2);
			lua_pushinteger(L, x);
			lua_pushinteger(L, y);
			lua_pushinteger(L, w);
			lua_pushinteger(L, h);
			lua_call(L, 5, 0);
		}
	}

	return 0;
}


int jiveL_label_draw(lua_State *L) {
	Uint16 y;

	/* stack is:
	 * 1: widget
	 * 2: surface
	 * 3: layer
	 */

	LabelWidget *peer = jive_getpeer(L, 1, &labelPeerMeta);
	JiveSurface *srf = tolua_tousertype(L, 2, 0);
	bool drawLayer = luaL_optinteger(L, 3, JIVE_LAYER_ALL) & peer->w.layer;

	if (drawLayer && peer->bg_tile) {
		jive_tile_blit(peer->bg_tile, srf, peer->w.bounds.x, peer->w.bounds.y, peer->w.bounds.w, peer->w.bounds.h);

		// jive_surface_boxColor(srf, peer->w.bounds.x, peer->w.bounds.y, peer->w.bounds.x + peer->w.bounds.w-1, peer->w.bounds.y + peer->w.bounds.h-1, 0xFF00007F);
	}

	/* draw child widgets */
	lua_getfield(L, 1, "widget");
	if (jive_getmethod(L, -1, "draw")) {
		lua_pushvalue(L, -2);	// widget
		lua_pushvalue(L, 2);	// surface
		lua_pushvalue(L, 3);	// layer
		lua_call(L, 3, 0);
	}
	lua_pop(L, 1);

	/* draw text label */
	lua_getfield(L, 1, "text");
	if (drawLayer && !lua_isnil(L, -1) && peer->font) {

		// FIXME this label cropping is crude, we need "..."
		// FIXME also scrolling when selected
		SDL_Rect old_clip, new_clip;
		jive_surface_get_clip(srf, &old_clip);
		
		new_clip.x = peer->w.bounds.x + peer->label_x;
		new_clip.y = old_clip.y;
		new_clip.w = peer->label_w;
		new_clip.h = old_clip.h;

		jive_rect_intersection(&old_clip, &new_clip, &new_clip);

		jive_surface_set_clip(srf, &new_clip);


		y = peer->w.bounds.y + peer->label_y;

		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			JiveSurface *tsrf;
			const char *label = lua_tostring(L, -1);

			/* shadow text */
			if (peer->is_sh) {
				tsrf = jive_font_draw_text(peer->font, peer->sh, label);
				jive_surface_blit(tsrf, srf, peer->w.bounds.x + peer->label_x + 1, y + 1);
				jive_surface_free(tsrf);
			}

			/* foreground text */
			tsrf = jive_font_draw_text(peer->font, peer->fg, label);
			jive_surface_blit(tsrf, srf, peer->w.bounds.x + peer->label_x, y);
			jive_surface_free(tsrf);

			y += peer->line_height;

			lua_pop(L, 1);
		}


		jive_surface_set_clip(srf, &old_clip);
	}
	lua_pop(L, 1);

	return 0;
}


int jiveL_label_get_preferred_bounds(lua_State *L) {
	LabelWidget *peer;
	Uint16 w, h;

	/* stack is:
	 * 1: widget
	 */

	if (jive_getmethod(L, 1, "doLayout")) {
		lua_pushvalue(L, 1);
		lua_call(L, 1, 0);
	}

	peer = jive_getpeer(L, 1, &labelPeerMeta);

	w = peer->label_w + peer->w.padding.left + peer->w.padding.right;
	h = peer->label_h + peer->w.padding.top + peer->w.padding.bottom;

	lua_pushinteger(L, (peer->w.preferred_bounds.x == JIVE_XY_NIL) ? 0 : peer->w.preferred_bounds.x);
	lua_pushinteger(L, (peer->w.preferred_bounds.y == JIVE_XY_NIL) ? 0 : peer->w.preferred_bounds.y);
	lua_pushinteger(L, (peer->w.preferred_bounds.w == JIVE_WH_NIL) ? w : peer->w.preferred_bounds.w);
	lua_pushinteger(L, (peer->w.preferred_bounds.h == JIVE_WH_NIL) ? h : peer->w.preferred_bounds.h);
	return 4;
}


int jiveL_label_gc(lua_State *L) {
	LabelWidget *peer;

	printf("********************* LABEL GC\n");

	luaL_checkudata(L, 1, labelPeerMeta.magic);

	peer = lua_touserdata(L, 1);

	if (peer->font) {
		jive_font_free(peer->font);
		peer->font = NULL;
	}
	if (peer->bg_tile) {
		jive_tile_free(peer->bg_tile);
		peer->bg_tile = NULL;
	}

	return 0;
}
