/*
    DeaDBeeF - ultimate music player for GNU/Linux systems with X11
    Copyright (C) 2009-2010 Alexey Yakovenko <waker@users.sourceforge.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <string.h>
#include "ddbtabstrip.h"
#include "drawing.h"
#include "gtkui.h"
#include "interface.h"
#include "support.h"

G_DEFINE_TYPE (DdbTabStrip, ddb_tabstrip, GTK_TYPE_WIDGET);

static void
ddb_tabstrip_send_configure (DdbTabStrip *darea)
{
  GtkWidget *widget;
  GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

  widget = GTK_WIDGET (darea);

  event->configure.window = g_object_ref (widget->window);
  event->configure.send_event = TRUE;
  event->configure.x = widget->allocation.x;
  event->configure.y = widget->allocation.y;
  event->configure.width = widget->allocation.width;
  event->configure.height = widget->allocation.height;
  
  gtk_widget_event (widget, event);
  gdk_event_free (event);
}

static void
ddb_tabstrip_realize (GtkWidget *widget) {
  DdbTabStrip *darea = DDB_TABSTRIP (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      GTK_WIDGET_CLASS (ddb_tabstrip_parent_class)->realize (widget);
    }
  else
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = widget->allocation.x;
      attributes.y = widget->allocation.y;
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget);
      attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, darea);

      widget->style = gtk_style_attach (widget->style, widget->window);
      gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
    }

  ddb_tabstrip_send_configure (DDB_TABSTRIP (widget));
}

static void
ddb_tabstrip_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  g_return_if_fail (DDB_IS_TABSTRIP (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      if (!GTK_WIDGET_NO_WINDOW (widget))
        gdk_window_move_resize (widget->window,
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

      ddb_tabstrip_send_configure (DDB_TABSTRIP (widget));
    }
}

gboolean
on_tabstrip_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event);

gboolean
on_tabstrip_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event);

gboolean
on_tabstrip_configure_event              (GtkWidget       *widget,
                                        GdkEventConfigure *event);

gboolean
on_tabstrip_expose_event                 (GtkWidget       *widget,
                                        GdkEventExpose  *event);

gboolean
on_tabstrip_motion_notify_event          (GtkWidget       *widget,
                                        GdkEventMotion  *event);

static void
ddb_tabstrip_destroy(GtkObject *object)
{
  DdbTabStrip *tabstrip;
  DdbTabStripClass *class;

  g_return_if_fail(object != NULL);
  g_return_if_fail(DDB_IS_TABSTRIP(object));

  tabstrip = DDB_TABSTRIP (object);
  class = gtk_type_class(gtk_widget_get_type());

  if (tabstrip->backbuf) {
      g_object_unref (tabstrip->backbuf);
      tabstrip->backbuf = NULL;
  }

  if (GTK_OBJECT_CLASS (ddb_tabstrip_parent_class)) {
      GTK_OBJECT_CLASS (ddb_tabstrip_parent_class)->destroy (object);
  }
}

static void
ddb_tabstrip_class_init(DdbTabStripClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkObjectClass *object_class = (GtkObjectClass *) class;
  widget_class->realize = ddb_tabstrip_realize;
  widget_class->size_allocate = ddb_tabstrip_size_allocate;
  widget_class->expose_event = on_tabstrip_expose_event;
  widget_class->button_press_event = on_tabstrip_button_press_event;
  widget_class->button_release_event = on_tabstrip_button_release_event;
  widget_class->configure_event = on_tabstrip_configure_event;
  widget_class->motion_notify_event = on_tabstrip_motion_notify_event;
  object_class->destroy = ddb_tabstrip_destroy;
}

GtkWidget * ddb_tabstrip_new() {
    return g_object_new (DDB_TYPE_TABSTRIP, NULL);
}

static void
ddb_tabstrip_init(DdbTabStrip *tabstrip)
{
    tabstrip->hscrollpos = 0;
    tabstrip->dragging = -1;
    tabstrip->prepare = 0;
    tabstrip->dragpt[0] = 0;
    tabstrip->dragpt[1] = 0;
    tabstrip->prev_x = 0;
    tabstrip->movepos = 0;
    tabstrip->backbuf = NULL;
    gtk_widget_set_double_buffered (GTK_WIDGET (tabstrip), FALSE);
}

static int tab_clicked = -1;
static int text_left_padding = 4;
static int text_right_padding = 0; // calculated from widget height
static int text_vert_offset = -2;
static int tab_overlap_size = 0; // widget_height/2
static int tabs_left_margin = 4;
static int min_tab_size = 80;

void
ddb_tabstrip_draw_tab (GtkWidget *widget, GdkDrawable *drawable, int selected, int x, int y, int w, int h) {
    GdkPoint points_filled[] = {
        { x+2, y + h },
        { x+2, y + 2 },
        { x + w - h + 1, y + 2 },
        { x + w - 1 + 1, y + h }
    };
    GdkPoint points_frame1[] = {
        { x, y + h-2 },
        { x, y + 1 },
        { x + 1, y + 0 },
        { x + w - h - 1, y + 0 },
        { x + w - h, y + 1 },
        { x + w - h + 1, y + 1 },
        { x + w - 2, y + h - 2 },
        { x + w - 1, y + h - 2 },
        { x + w-2, y + h - 3 }
    };
    GdkPoint points_frame2[] = {
        { x + 1, y + h + 1 },
        { x + 1, y + 1 },
        { x + w - h - 1, y + 1 },
        { x + w - h, y + 2 },
        { x + w - h + 1, y + 2 },
        { x + w-3, y + h - 2 },
        { x + w-2, y + h - 2 },
    };
    //gdk_draw_rectangle (widget->window, widget->style->black_gc, FALSE, x-1, y-1, w+2, h+2);
    GdkGC *bg = gdk_gc_new (drawable);
    GdkGC *outer_frame = gdk_gc_new (drawable);
    GdkGC *inner_frame = gdk_gc_new (drawable);
    if (selected) {
        gdk_gc_set_rgb_fg_color (bg, gtkui_get_tabstrip_base_color ());//&widget->style->bg[GTK_STATE_NORMAL]); // FIXME: need base color
        gdk_gc_set_rgb_fg_color (outer_frame, gtkui_get_tabstrip_dark_color ());
        gdk_gc_set_rgb_fg_color (inner_frame, gtkui_get_tabstrip_light_color ());
//        bg = widget->style->bg_gc[GTK_STATE_NORMAL];
//        outer_frame = widget->style->dark_gc[GTK_STATE_NORMAL];
//        inner_frame = widget->style->light_gc[GTK_STATE_NORMAL];
    }
    else {
        gdk_gc_set_rgb_fg_color (bg, gtkui_get_tabstrip_mid_color ());
        gdk_gc_set_rgb_fg_color (outer_frame, gtkui_get_tabstrip_dark_color ());
        gdk_gc_set_rgb_fg_color (inner_frame, gtkui_get_tabstrip_mid_color ());
//        bg = widget->style->mid_gc[GTK_STATE_NORMAL];
//        outer_frame = widget->style->dark_gc[GTK_STATE_NORMAL];
//        inner_frame = widget->style->mid_gc[GTK_STATE_NORMAL];
    }
    gdk_draw_polygon (drawable, bg, TRUE, points_filled, 4);
    gdk_draw_lines (drawable, outer_frame, points_frame1, 9);
    gdk_draw_lines (drawable, inner_frame, points_frame2, 7);
    g_object_unref (bg);
    g_object_unref (outer_frame);
    g_object_unref (inner_frame);
}

int
ddb_tabstrip_get_tab_width (DdbTabStrip *ts, int tab) {
    int width;
    char title[100];
    deadbeef->plt_get_title (tab, title, sizeof (title));
    int h = 0;
    draw_get_text_extents (title, strlen (title), &width, &h);
    width += text_left_padding + text_right_padding;
    if (width < min_tab_size) {
        width = min_tab_size;
    }
    return width;
}

void
tabstrip_render (DdbTabStrip *ts) {
    GtkWidget *widget = GTK_WIDGET (ts);
    GdkDrawable *backbuf = ts->backbuf;
    int x = -ts->hscrollpos;
    int w = 0;
    int h = draw_get_font_size ();
    gtk_widget_set_size_request (widget, -1, h + 9 + 4);
    h = widget->allocation.height;
    tab_overlap_size = (h-4)/2;
    text_right_padding = h - 3;

    const char *detail = "button";
    int cnt = deadbeef->plt_get_count ();
    int tab_selected = deadbeef->plt_get_curr ();

    GdkGC *gc = gdk_gc_new (backbuf);

    // fill background
    gdk_gc_set_rgb_fg_color (gc, gtkui_get_tabstrip_mid_color ());
    gdk_draw_rectangle (backbuf, gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
    gdk_gc_set_rgb_fg_color (gc, gtkui_get_tabstrip_dark_color ());
    gdk_draw_line (backbuf, gc, 0, 0, widget->allocation.width, 0);
    int y = 4;
    h = widget->allocation.height - 4;
    draw_begin ((uintptr_t)backbuf);
    int need_draw_moving = 0;
    int idx;
    int widths[cnt];
    int fullwidth = 0;
    for (idx = 0; idx < cnt; idx++) {
        char title[100];
        deadbeef->plt_get_title (idx, title, sizeof (title));
        int h = 0;
        draw_get_text_extents (title, strlen (title), &widths[idx], &h);
        widths[idx] += text_left_padding + text_right_padding;
        if (widths[idx] < min_tab_size) {
            widths[idx] = min_tab_size;
        }
        fullwidth += widths[idx] - tab_overlap_size;
    }
    fullwidth += tab_overlap_size;

    x = -ts->hscrollpos + tabs_left_margin;

    for (idx = 0; idx < cnt; idx++) {
        w = widths[idx];
        GdkRectangle area;
        area.x = x;
        area.y = 0;
        area.width = w;
        area.height = 24;
        if (idx != tab_selected) {
//            gtk_paint_box (widget->style, widget->window, idx == tab_selected ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL, GTK_SHADOW_OUT, &area, widget, "button", x, idx == tab_selected ? 0 : 1, w+margin_size, 32);
            ddb_tabstrip_draw_tab (widget, backbuf, idx == tab_selected, x, y, w, h);
            char tab_title[100];
            deadbeef->plt_get_title (idx, tab_title, sizeof (tab_title));
            GdkColor *color = &widget->style->text[GTK_STATE_NORMAL];
            float fg[3] = {(float)color->red/0xffff, (float)color->green/0xffff, (float)color->blue/0xffff};
            draw_set_fg_color (fg);
            draw_text (x + text_left_padding, y + h/2 - draw_get_font_size()/2 + text_vert_offset, w, 0, tab_title);
        }
        x += w - tab_overlap_size;
    }
    gdk_draw_line (backbuf, widget->style->dark_gc[GTK_STATE_NORMAL], 0, widget->allocation.height-2, widget->allocation.width, widget->allocation.height-2);
    gdk_draw_line (backbuf, widget->style->light_gc[GTK_STATE_NORMAL], 0, widget->allocation.height-1, widget->allocation.width, widget->allocation.height-1);
    // calc position for drawin selected tab
    x = -ts->hscrollpos;
    for (idx = 0; idx < tab_selected; idx++) {
        x += widths[idx] - tab_overlap_size;
    }
    x += tabs_left_margin;
    // draw selected
    if (ts->dragging < 0 || ts->prepare || tab_selected != ts->dragging) {
        idx = tab_selected;
        w = widths[tab_selected];
        GdkRectangle area;
        area.x = x;
        area.y = 0;
        area.width = w;
        area.height = 24;
//        gtk_paint_box (widget->style, widget->window, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT, &area, widget, "button", x, idx == tab_selected ? 0 : 1, w, 32);
        ddb_tabstrip_draw_tab (widget, backbuf, 1, x, y, w, h);
        char tab_title[100];
        deadbeef->plt_get_title (idx, tab_title, sizeof (tab_title));
        GdkColor *color = &widget->style->text[GTK_STATE_NORMAL];
        float fg[3] = {(float)color->red/0xffff, (float)color->green/0xffff, (float)color->blue/0xffff};
        draw_set_fg_color (fg);
        draw_text (x + text_left_padding, y + h/2 - draw_get_font_size()/2 + text_vert_offset, w, 0, tab_title);
    }
    else {
        need_draw_moving = 1;
    }
    if (need_draw_moving) {
        x = -ts->hscrollpos + tabs_left_margin;
        for (idx = 0; idx < 10; idx++) {
            w = widths[idx];
            if (idx == ts->dragging) {
#if 0
                // draw empty slot
                if (x < widget->allocation.width) {
                    gtk_paint_box (widget->style, backbuf, GTK_STATE_ACTIVE, GTK_SHADOW_ETCHED_IN, NULL, widget, "button", x, 0, w, h);
                }
#endif
                x = ts->movepos;
                if (x >= widget->allocation.width) {
                    break;
                }
                if (w > 0) {
//                    gtk_paint_box (widget->style, backbuf, GTK_STATE_SELECTED, GTK_SHADOW_OUT, NULL, widget, "button", x, 0, w, h);
                    ddb_tabstrip_draw_tab (widget, backbuf, 1, x, y, w, h);
                    char tab_title[100];
                    deadbeef->plt_get_title (idx, tab_title, sizeof (tab_title));
                    GdkColor *color = &widget->style->text[GTK_STATE_NORMAL];
                    float fg[3] = {(float)color->red/0xffff, (float)color->green/0xffff, (float)color->blue/0xffff};
                    draw_set_fg_color (fg);
                    draw_text (x + text_left_padding, y + h/2 - draw_get_font_size()/2 + text_vert_offset, w, 0, tab_title);
                }
                break;
            }
            x += w - tab_overlap_size;
        }
    }
    draw_end ();
    g_object_unref (gc);
}

void
tabstrip_expose (DdbTabStrip *ts, int x, int y, int w, int h) {
    gdk_draw_drawable (GTK_WIDGET (ts)->window, GTK_WIDGET (ts)->style->black_gc, ts->backbuf, x, y, x, y, w, h);
}

static int
get_tab_under_cursor (int x) {
    int idx;
    int cnt = deadbeef->plt_get_count ();
    int fw = tabs_left_margin;
    int tab_selected = deadbeef->plt_get_curr ();
    for (idx = 0; idx < cnt; idx++) {
        char title[100];
        deadbeef->plt_get_title (idx, title, sizeof (title));
        int w = 0;
        int h = 0;
        draw_get_text_extents (title, strlen (title), &w, &h);
        w += text_left_padding + text_right_padding;
        if (w < min_tab_size) {
            w = min_tab_size;
        }
        fw += w;
        fw -= tab_overlap_size;
        if (fw > x) {
            return idx;
        }
    }
    return -1;
}

gboolean
on_tabstrip_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event)
{
    DdbTabStrip *ts = DDB_TABSTRIP (widget);
    tab_clicked = get_tab_under_cursor (event->x);
    if (event->button == 1)
    {
        if (tab_clicked != -1) {
            deadbeef->plt_set_curr (tab_clicked);
            deadbeef->conf_set_int ("playlist.current", tab_clicked);
        }

        int x = -ts->hscrollpos + tabs_left_margin;
        int idx;
        for (idx = 0; idx < tab_clicked; idx++) {
            int width = ddb_tabstrip_get_tab_width (ts, idx);
            x += width - tab_overlap_size;
        }
        ts->dragpt[0] = event->x - x;
        ts->dragpt[1] = event->y;
        ts->prepare = 1;
        ts->dragging = tab_clicked;
        ts->prev_x = event->x;
    }
    else if (event->button == 3) {
        GtkWidget *menu = create_plmenu ();
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, widget, 0, gtk_get_current_event_time());
    }
    else if (event->button == 2) {
        if (deadbeef->conf_get_int ("gtkui.mmb_delete_playlist", 0)) {
            if (tab_clicked != -1) {
                deadbeef->plt_remove (tab_clicked);
                int playlist = deadbeef->plt_get_curr ();
                deadbeef->conf_set_int ("playlist.current", playlist);
            }
        }
    }
    return FALSE;
}


gboolean
on_tabstrip_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event)
{
    DdbTabStrip *ts = DDB_TABSTRIP (widget);
    if (event->button == 1) {
        if (ts->prepare || ts->dragging >= 0) {
            ts->dragging = -1;
            ts->prepare = 0;
            tabstrip_render (ts);
            tabstrip_expose (ts, 0, 0, widget->allocation.width, widget->allocation.height);
        }
    }
    return FALSE;
}


gboolean
on_tabstrip_configure_event              (GtkWidget       *widget,
                                        GdkEventConfigure *event)
{
    DdbTabStrip *ts = DDB_TABSTRIP (widget);
    if (ts->backbuf) {
        g_object_unref (ts->backbuf);
        ts->backbuf = NULL;
    }
    ts->backbuf = gdk_pixmap_new (widget->window, widget->allocation.width, widget->allocation.height, -1);
    tabstrip_render (ts);
    return FALSE;
}


gboolean
on_tabstrip_expose_event                 (GtkWidget       *widget,
                                        GdkEventExpose  *event)
{
    tabstrip_expose (DDB_TABSTRIP (widget), event->area.x, event->area.y, event->area.width, event->area.height);
    return FALSE;
}

gboolean
on_tabstrip_motion_notify_event          (GtkWidget       *widget,
                                        GdkEventMotion  *event)
{
    DdbTabStrip *ts = DDB_TABSTRIP (widget);
    int ev_x, ev_y;
    GdkModifierType ev_state;
    ev_x = event->x;
    ev_y = event->y;
    ev_state = event->state;
    gdk_event_request_motions (event);
    if ((ev_state & GDK_BUTTON1_MASK) && ts->prepare) {
        if (gtk_drag_check_threshold (widget, ev_x, ts->prev_x, 0, 0)) {
            ts->prepare = 0;
        }
    }
    if (!ts->prepare && ts->dragging >= 0) {
//        gdk_window_set_cursor (widget->window, cursor_drag);
        ts->movepos = ev_x - ts->dragpt[0];

        // find closest tab to the left
        int idx;
        int x = -ts->hscrollpos + tabs_left_margin;
        int inspos = -1;
        int cnt = deadbeef->plt_get_count ();
        for (idx = 0; idx < cnt; idx++) {
            int width = ddb_tabstrip_get_tab_width (ts, idx);
            if (idx != ts->dragging && x <= ts->movepos && x + width/2 - tab_overlap_size  > ts->movepos) {
                inspos = idx;
                break;
            }
            x += width - tab_overlap_size;
        }
        if (inspos >= 0 && inspos != ts->dragging) {
            char str1[100];
            char str2[100];
            int pos1;
            int pos2;
            snprintf (str1, sizeof (str1), "playlist.scroll.%d", ts->dragging);
            pos1 = deadbeef->conf_get_int (str1, 0);
            snprintf (str2, sizeof (str2), "playlist.scroll.%d", inspos);
            pos2 = deadbeef->conf_get_int (str2, 0);
            deadbeef->plt_move (ts->dragging, inspos);
            deadbeef->conf_set_int (str1, pos2);
            deadbeef->conf_set_int (str2, pos1);
            ts->dragging = inspos;
            deadbeef->conf_set_int ("playlist.current", ts->dragging);
        }
        tabstrip_render (ts);
        tabstrip_expose (ts, 0, 0, widget->allocation.width, widget->allocation.height);
    }
    return FALSE;
}

void
on_rename_playlist1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWidget *dlg = create_editplaylistdlg ();
    gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
    gtk_window_set_title (GTK_WINDOW (dlg), "Edit playlist");
    GtkWidget *e = lookup_widget (dlg, "title");
    char t[100];
    deadbeef->plt_get_title (tab_clicked, t, sizeof (t));
    gtk_entry_set_text (GTK_ENTRY (e), t);
    int res = gtk_dialog_run (GTK_DIALOG (dlg));
    if (res == GTK_RESPONSE_OK) {
        const char *text = gtk_entry_get_text (GTK_ENTRY (e));
        deadbeef->plt_set_title (tab_clicked, text);
        extern GtkWidget *mainwin;
    }
    gtk_widget_destroy (dlg);
}


void
on_remove_playlist1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    if (tab_clicked != -1) {
        deadbeef->plt_remove (tab_clicked);
        int playlist = deadbeef->plt_get_curr ();
        deadbeef->conf_set_int ("playlist.current", playlist);
    }
}

void
on_add_new_playlist1_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    int playlist = gtkui_add_new_playlist ();
    if (playlist != -1) {
        deadbeef->plt_set_curr (playlist);
        deadbeef->conf_set_int ("playlist.current", playlist);
        DdbTabStrip *ts = DDB_TABSTRIP (lookup_widget (mainwin, "tabstrip"));
        tabstrip_render (ts);
        tabstrip_expose (ts, 0, 0, GTK_WIDGET (ts)->allocation.width, GTK_WIDGET (ts)->allocation.height);
    }
}


void
on_load_playlist1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_playlist1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_all_playlists1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

void
ddb_tabstrip_refresh (DdbTabStrip *ts) {
    tabstrip_render (ts);
    tabstrip_expose (ts, 0, 0, GTK_WIDGET (ts)->allocation.width, GTK_WIDGET (ts)->allocation.height);
}
