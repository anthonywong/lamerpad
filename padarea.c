/* LamerPad
 * Copyright (C) 1999 Anthony Wong <ypwong@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id: padarea.c,v 1.4 1999/11/26 21:48:05 ypwong Exp $ */

#include <stdio.h>
#include <math.h>

#include "lamerpad.h"

static gint pending_timer;
static int timer_exists = FALSE;

static void
pad_area_free_stroke (GList *stroke)
{
  GList *tmp_list = stroke;
  while (tmp_list)
    {
      g_free (tmp_list->data);
      tmp_list = tmp_list->next;
    }
  g_list_free (stroke);
}


static void
pad_area_annotate_stroke (PadArea *area, GList *stroke, gint index)
{
  GdkPoint *cur, *old;

  /* Annotate the stroke with the stroke number - the algorithm
   * for placing the digit is pretty simple. The text is inscribed
   * in a circle tangent to the stroke. The circle will be above
   * and/or to the left of the line */
  if (stroke)
    {
      old = (GdkPoint *)stroke->data;
      
      do
	{
	  cur = (GdkPoint *)stroke->data;
	  stroke = stroke->next;
	}
      while (stroke && abs(cur->x - old->x) < 5 && abs (cur->y - old->y) < 5);
      
      if (stroke)
	{
	  char buffer[16];
	  int swidth, sheight;
	  gint16 x, y;
	  double r;
	  double dx = cur->x - old->x;
	  double dy = cur->y - old->y;
	  double dl = sqrt(dx*dx+dy*dy);
	  int sign = (dy <= dx) ? 1 : -1;
	  GdkRectangle update_area;
	  
	  sprintf (buffer, "%d", index);
	  swidth = gdk_string_width (area->widget->style->font, buffer);
	  sheight = area->widget->style->font->ascent + area->widget->style->font->descent;
	  r = sqrt(swidth*swidth + sheight*sheight);
	  
	  x = 0.5 + old->x + 0.5*r*dx/dl + sign * 0.5*r*dy/dl;
	  y = 0.5 + old->y + 0.5*r*dy/dl - sign * 0.5*r*dx/dl;
	  
	  x -= swidth/2;
	  y += sheight/2 - area->widget->style->font->descent;

	  update_area.x = x;
	  update_area.y = y - area->widget->style->font->ascent;
	  update_area.width = swidth;
	  update_area.height = sheight;
	  
	  x = CLAMP (x, 0, area->widget->allocation.width - swidth);
	  y = CLAMP (y, area->widget->style->font->ascent, 
		     area->widget->allocation.height);
	  
	  gdk_draw_string (area->pixmap, area->widget->style->font,
			   area->widget->style->black_gc, x, y, buffer);

	  gtk_widget_draw (area->widget, &update_area);
	}
    }
}

static void 
pad_area_init (PadArea *area)
{
  GList *tmp_list;
  int index = 1;
  
  guint16 width = area->widget->allocation.width;
  guint16 height = area->widget->allocation.height;

  gdk_draw_rectangle (area->pixmap, 
		      area->widget->style->white_gc, TRUE,
		      0, 0, width, height);

  tmp_list = area->strokes;
  while (tmp_list)
    {
      GdkPoint *cur, *old;
      GList *stroke_list = tmp_list->data;

      old = NULL;

      if (area->annotate)
	pad_area_annotate_stroke (area, stroke_list, index);

      while (stroke_list)
	{
	  cur = (GdkPoint *)stroke_list->data;
	  if (old)
	    gdk_draw_line (area->pixmap, 
			   area->widget->style->black_gc,
			   old->x, old->y, cur->x, cur->y);

	  old = cur;
	  stroke_list = stroke_list->next;
	}
      
      tmp_list = tmp_list->next;
      index++;
    }

  gtk_widget_draw (area->widget, NULL);
  
}

static int
pad_area_configure_event (GtkWidget *w, GdkEventConfigure *event,
			  PadArea *area)
{
  if (area->pixmap)
    gdk_pixmap_unref (area->pixmap);

  area->pixmap = gdk_pixmap_new (w->window, event->width, event->height, -1);

  pad_area_init (area);
  
  return TRUE;
}

static int
pad_area_expose_event (GtkWidget *w, GdkEventExpose *event, PadArea *area)
{
  if (!area->pixmap)
    return 0;

  gdk_draw_pixmap (w->window,
		   w->style->fg_gc[GTK_STATE_NORMAL], area->pixmap,
		   event->area.x, event->area.y,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);

  return TRUE;
}

static int
pad_area_button_press_event (GtkWidget *w, GdkEventButton *event, PadArea *area)
{
  if (event->button == 1)
    {
      GdkPoint *p = g_new (GdkPoint, 1);

      if ( timer_exists ) {
        gtk_timeout_remove(pending_timer);  /* remove the timer */
        timer_exists = FALSE;
      }
      p->x = event->x;
      p->y = event->y;
      area->curstroke = g_list_append (area->curstroke, p);
      area->instroke = TRUE;
    }

  return TRUE;
}

static gint auto_lookup_char(PadArea* pad_area)
{
        look_up_callback((GtkWidget*) pad_area);
        return FALSE;
}

static int
pad_area_button_release_event (GtkWidget *w, GdkEventButton *event, PadArea *area)
{
  if (area->annotate)
    pad_area_annotate_stroke (area, area->curstroke, g_list_length (area->strokes) + 1);

  area->strokes = g_list_append (area->strokes, area->curstroke);
  area->curstroke = NULL;
  area->instroke = FALSE;

/*  look_up_callback(w); */

  /* wait for 150 ms (or 1.5 seconds), before committing the best
   * guess automatically */
  pending_timer = gtk_timeout_add(delay, (GtkFunction) auto_lookup_char, area);
  timer_exists = TRUE;

  return TRUE;
}

static int
pad_area_motion_event (GtkWidget *w, GdkEventMotion *event, PadArea *area)
{
  gint x,y;
  GdkModifierType state;

  if (event->is_hint)
    {
      gdk_window_get_pointer (w->window, &x, &y, &state);
    }
  else
    {
      x = event->x;
      y = event->y;
      state = event->state;
    }

  if (area->instroke && state & GDK_BUTTON1_MASK)
    {
      GdkRectangle rect;
      GdkPoint *p;
      int xmin, ymin, xmax, ymax;
      GdkPoint *old = (GdkPoint *)g_list_last (area->curstroke)->data;

      if ( timer_exists ) {
        gtk_timeout_remove(pending_timer);  /* remove the timer */
        timer_exists = FALSE;
      }

      gdk_draw_line (area->pixmap, w->style->black_gc,
		     old->x, old->y, x, y);

      if (old->x < x) { xmin = old->x; xmax = x; }
      else            { xmin = x;      xmax = old->x; }

      if (old->y < y) { ymin = old->y; ymax = y; }
      else            { ymin = y;      ymax = old->y; }

      rect.x = xmin - 1; 
      rect.y = ymin = 1;
      rect.width  = xmax - xmin + 2;
      rect.height = ymax - ymin + 2;
      gtk_widget_draw (w, &rect);

      p = g_new (GdkPoint, 1);
      p->x = x;
      p->y = y;
      area->curstroke = g_list_append (area->curstroke, p);
    }

  return TRUE;
}


PadArea *pad_area_create ()
{
  PadArea *area = g_new (PadArea, 1);
  
  area->widget = gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(area->widget), 300, 300);

  gtk_signal_connect (GTK_OBJECT (area->widget), "configure_event",
		      GTK_SIGNAL_FUNC (pad_area_configure_event), area);
  gtk_signal_connect (GTK_OBJECT (area->widget), "expose_event",
		      GTK_SIGNAL_FUNC (pad_area_expose_event), area);
  gtk_signal_connect (GTK_OBJECT (area->widget), "button_press_event",
		      GTK_SIGNAL_FUNC (pad_area_button_press_event), area);
  gtk_signal_connect (GTK_OBJECT (area->widget), "button_release_event",
		      GTK_SIGNAL_FUNC (pad_area_button_release_event), area);
  gtk_signal_connect (GTK_OBJECT (area->widget), "motion_notify_event",
		      GTK_SIGNAL_FUNC (pad_area_motion_event), area);

  gtk_widget_set_events (area->widget, 
			 GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
			 | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK 
			 | GDK_POINTER_MOTION_HINT_MASK);

  area->strokes = NULL;
  area->curstroke = NULL;
  area->instroke = FALSE;
  area->annotate = FALSE;
  area->pixmap = NULL;

  return area;
}

void pad_area_clear (PadArea *area)
{
  GList *tmp_list;

  tmp_list = area->strokes;
  while (tmp_list)
    {
      pad_area_free_stroke (tmp_list->data);
      tmp_list = tmp_list->next;
    }
  g_list_free (area->strokes);
  area->strokes = NULL;

#if 0
  tmp_list = thinned;
  while (tmp_list)
    {
      pad_area_free_stroke (tmp_list->data);
      tmp_list = tmp_list->next;
    }
  g_list_free (thinned);
  thinned = NULL;
#endif

  g_list_free (area->curstroke);
  area->curstroke = NULL;

  pad_area_init (area);
}

void pad_area_set_annotate (PadArea *area, gint annotate)
{
  if (area->annotate != annotate)
    {
      area->annotate = annotate;
      pad_area_init (area);
    }
}

