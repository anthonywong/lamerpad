#ifndef __LAMERPAD_H
#define __LAMERPAD_H

#include <gtk/gtk.h>

typedef struct _PadArea PadArea;

struct _PadArea {
        GtkWidget *widget;

        gint annotate;
        GList *strokes;

        /* Private */
        GdkPixmap *pixmap;
        GList *curstroke;
        int instroke;
};

void look_up_callback (GtkWidget *w);

PadArea *pad_area_create ();
void pad_area_clear (PadArea *area);
void pad_area_set_annotate (PadArea *area, gint annotate);

extern char* locale;    /* locale of where lamerpad is launched */
extern unsigned int delay;  /* delay period */

typedef struct {
        gchar d[2];
} kp_wchar;

#define MAX_GUESSES 32  /* should be same as diMaxListCount in jstroke/jstroke.h */
extern kp_wchar kanjiguess[MAX_GUESSES];
extern int num_guesses;

#endif /* __LAMERPAD_H */
