/* LamerPad
 * Copyright (C) 1999 Anthony Wong
 *
 * KanjiPad - Japanese handwriting recognition front end
 * Copyright (C) 1997 Owen Taylor
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

/* $Id: lamerpad.c,v 1.8 1999/11/30 11:38:51 ypwong Exp $ */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* XIM stuffs */
#include <X11/Xlocale.h>
#include <X11/Xlib.h>
#include <IMdkit.h>
#include <Xi18n.h>
#include <gtk/gtkmain.h>

#include "lamerpad.h"
#include "xim.h"


/* Supported Japanese Encodings */
static XIMEncoding allEncodings[] = {
  "COMPOUND_TEXT",
/*
  "eucJP",
  "SJIS",
  "big5",
  "gb2312",
*/
  NULL
};

#if 0
static XIMEncoding jaEncodings[] = {
  "COMPOUND_TEXT",
  "eucJP",
  "SJIS",
  NULL
};

/* Supported Taiwanese Encodings */
static XIMEncoding zhEncodings[] = {
  "COMPOUND_TEXT",
  "big5",
  "gb2312",
  NULL
};
#endif

static XIMStyle defaultStyles[] = {
  XIMPreeditNothing|XIMStatusNothing,
  NULL
};


#define WCHAR_EQ(a,b) (a.d[0] == b.d[0] && a.d[1] == b.d[1])

/* Wait for child process? */

/* user interface elements */
static GdkPixmap *kpixmap;
GtkWidget *karea;

//static GList *strokes = NULL;

kp_wchar kanjiguess[MAX_GUESSES];
int num_guesses = 0;
kp_wchar kselected;

PadArea *pad_area;

char *locale = NULL;
unsigned int delay = 2000;

/* globals for engine communication */
static int engine_pid;
static FILE *from_engine;
static FILE *to_engine;

static char *data_file = NULL;
static char *progname;

static void exit_callback ();
/* static void save_callback (); */
static void clear_callback ();
void look_up_callback ();
static void set_delay_callback ();
static void annotate_callback ();

static GtkItemFactoryEntry menu_items[] =
{
  { "/_File", NULL, NULL, 0, "<Branch>" },
  { "/File/_Quit", "<alt>Q", exit_callback },

  { "/_Character", NULL, NULL, 0, "<Branch>" },
  { "/Character/_Lookup", "<alt>L", look_up_callback },
  { "/Character/_Clear", "<alt>X", clear_callback },
  { "/Character/Delay Period", NULL, NULL, 0, "<Branch>" },
  { "/Character/Delay Period/Fast",NULL,set_delay_callback,1500,"<RadioItem>" },
  { "/Character/Delay Period/Just right",NULL,set_delay_callback,2000,"/Character/Delay Period/Fast" },
  { "/Character/Delay Period/Normal",NULL,set_delay_callback,2500,"/Character/Delay Period/Just right" },
  { "/Character/Delay Period/Slow",NULL,set_delay_callback,3000,"/Character/Delay Period/Normal" },
  { "/Character/Delay Period/Very slow",NULL,set_delay_callback,4000,"/Character/Delay Period/Slow" },
/*  { "/Character/Delay Period/5",NULL,set_delay_callback,5000,"/Character/Delay Period/4" }, */
/*  { "/Character/_Save", "<control>S", save_callback }, */
  { "/Character/sep1", NULL, NULL, 0, "<Separator>" },
  
  { "/Character/_Annotate", NULL, annotate_callback, 0, "<CheckItem>" },
};

static int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

static void
karea_draw_character (GtkWidget *w, int index, int selected)
{
  gchar str[3];

  if (selected >= 0)
    {
      gdk_draw_rectangle (kpixmap,
			  selected ? w->style->bg_gc[GTK_STATE_SELECTED] :
			  w->style->white_gc,
			  TRUE,
			  0, 30*index, 29, 29);
    }

  str[0] = kanjiguess[index].d[0];
  str[1] = kanjiguess[index].d[1];
  str[2] = '\0';

  gdk_draw_text(kpixmap, w->style->font,
		  (selected > 0) ? w->style->white_gc :
		                   w->style->black_gc,
		  3, 24 + 30*index, str, 2);
/*
  gdk_draw_string(kpixmap, w->style->font,
		  (selected > 0) ? w->style->white_gc :
		                   w->style->black_gc,
		  3, 24 + 30*index, str);
*/
}


static void
karea_draw (GtkWidget *w)
{
  guint16 width = w->allocation.width;
  guint16 height = w->allocation.height;
  int i;

  gdk_draw_rectangle (kpixmap, 
		      w->style->white_gc, TRUE,
		      0, 0, width, height);

#ifdef DEBUG
fprintf(stderr,"Matched %d:\n", num_guesses);
#endif

  for (i=0; i<num_guesses; i++)
    {
      if (WCHAR_EQ (kselected, kanjiguess[i]))
	karea_draw_character (w, i, 1);
      else
	karea_draw_character (w, i, -1);
    }

  gtk_widget_draw (w, NULL);
}

static int
karea_configure_event (GtkWidget *w, GdkEventConfigure *event)
{
  if (kpixmap)
    gdk_pixmap_unref (kpixmap);

  kpixmap = gdk_pixmap_new (w->window, event->width, event->height, -1);

  karea_draw (w);
  
  return TRUE;
}

static int
karea_expose_event (GtkWidget *w, GdkEventExpose *event)
{
  if (!kpixmap)
    return 0;

  gdk_draw_pixmap (w->window,
		   w->style->fg_gc[GTK_STATE_NORMAL], kpixmap,
		   event->area.x, event->area.y,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);

    return 0;
}

static int
karea_erase_selection (GtkWidget *w)
{
  int i;
  if (kselected.d[0] || kselected.d[1])
    {
      for (i=0; i<num_guesses; i++)
	{
	  if (WCHAR_EQ (kselected, kanjiguess[i]))
	    {
	      karea_draw_character (w, i, 0);
	    }
	}
    }
  return TRUE;
}

static int
karea_button_press_event (GtkWidget *w, GdkEventButton *event)
{
  int j;

  karea_erase_selection (w);

  j = event->y/30;
  if (j < num_guesses)
    {
      kselected = kanjiguess[j];
      karea_draw_character (w, j, 1);
      gtk_selection_owner_set (w, GDK_SELECTION_PRIMARY, event->time);

      if ( ic_list != NULL )
        commit_char(kanjiguess[j].d[0], kanjiguess[j].d[1]);
    }
  else
    {
      kselected.d[0] = 0;
      kselected.d[1] = 0;
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == w->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, 0);
    }

  gtk_widget_draw (w, NULL);

  return TRUE;
}

static int
karea_selection_clear (GtkWidget *w, GdkEvent *event)
{
  karea_erase_selection (w);
  kselected.d[0] = kselected.d[1] = 0;

  gtk_widget_draw (w, NULL);

  return FALSE;
}

static void
karea_selection_get (GtkWidget        *widget,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     gpointer          data)
{
  if (kselected.d[0] || kselected.d[1])
    {
      /* Convert the character into compound text (basically, ISO-2022)
       * initial control string sets GR to jisx0208-1983 */
      char str[6];
      str[0] = 0x1b;
      str[1] = 0x24;
      str[2] = 0x29;
      str[3] = 0x42;
      str[4] = 0x80 | kselected.d[0];
      str[5] = 0x80 | kselected.d[1];
      gtk_selection_data_set (selection_data,
			      gdk_atom_intern ("COMPOUND_TEXT", FALSE),
			      8, str, 6);
    }
}

static void 
exit_callback (GtkWidget *w)
{
  gtk_exit (0);
}

void 
look_up_callback (GtkWidget *w)
{
  /*	     kill 'HUP',$engine_pid; */
  GList *tmp_list;
  gint ht = pad_area->widget->allocation.height;
  gint wd = pad_area->widget->allocation.width;
    
  tmp_list = pad_area->strokes;
  while (tmp_list)
    {
     GList *stroke_list = tmp_list->data;
     while (stroke_list)
       {
         /* normalize to 0 to 255, as the scoring routine seems to
          * have problems when the values are larger.  -ypwong */
	 gint16 x = ((GdkPoint *)stroke_list->data)->x * 255 / wd;
	 gint16 y = ((GdkPoint *)stroke_list->data)->y * 255 / ht;
	 fprintf(to_engine, "%d %d ", x, y);
	 stroke_list = stroke_list->next;
       }
     fprintf(to_engine, "\n");
     tmp_list = tmp_list->next;
    }
  fprintf(to_engine, "\n");
  fflush(to_engine);
}

static void 
clear_callback (GtkWidget *w)
{
  pad_area_clear (pad_area);
}

static void 
set_delay_callback (gpointer data, guint action, GtkWidget* w)
{
        delay = action;
}

/*
static void 
save_callback (GtkWidget *w)
{
  static int unknownID = 0;
  static FILE *samples = NULL;

  int found = FALSE;
  int i;
  GList *tmp_list;
  
  if (!samples)
    {
      if (!(samples = fopen("samples.dat", "a")))
	g_error ("Can't open 'samples.dat': %s", g_strerror(errno));
    }
  
  if (kselected.d[0] || kselected.d[1])
    {
      for (i=0; i<num_guesses; i++)
	{
	  if (WCHAR_EQ (kselected, kanjiguess[i]))
	    found = TRUE;
	}
    }
  
  if (found)
    fprintf(samples,"%2x%2x %c%c\n", kselected.d[0], kselected.d[1],
	   0x80 | kselected.d[0], 0x80 | kselected.d[1]);
  else
    {
      fprintf (samples, "0000 ??%d\n", unknownID);
      fprintf (stderr, "Unknown character saved, ID: %d\n", unknownID);
      unknownID++;
    }

  tmp_list = strokes;
  while (tmp_list)
    {
     GList *stroke_list = tmp_list->data;
     while (stroke_list)
       {
	 gint16 x = ((GdkPoint *)stroke_list->data)->x;
	 gint16 y = ((GdkPoint *)stroke_list->data)->y;
	 fprintf(samples, "%d %d ", x, y);
	 stroke_list = stroke_list->next;
       }
     fprintf(samples, "\n");
     tmp_list = tmp_list->next;
    }
  fprintf(samples, "\n");
  fflush(samples);
}
*/

static void
annotate_callback ()
{
  pad_area_set_annotate (pad_area, !pad_area->annotate);
}

//#define BUFLEN 256
#define BUFLEN 1024

static void
engine_input_handler (gpointer data, gint source, GdkInputCondition condition)
{
  static gchar *buffer = NULL;
  static gchar *p;
  static int buflen = BUFLEN;
  int i;

  if (!buffer)
    buffer = g_new (gchar, BUFLEN);
    
  if (!fgets(buffer, buflen, from_engine))
    g_error("Engine no longer exists");
  
  while ((strlen(buffer) == buflen - 1) && (buffer[buflen-1] != '\n'))
    {
      buffer = realloc(buffer, buflen);
      if (!fgets(buffer+buflen-1, BUFLEN+1, from_engine))
	g_error("Engine no longer exists");
      buflen += BUFLEN;
    }

  if (buffer[0] == 'K')
    {
      unsigned int t1, t2;
      p = buffer+1;
      for (i=0; i<MAX_GUESSES; i++)
	{
	  while (*p && isspace(*p)) p++;
	  if (!*p || sscanf(p, "%2x%2x", &t1, &t2) != 2)
	    {
	      i--;
	      break;
	    }
          /* check whether this character is available in this
           * encoding or not, if not, then don't store it.
           * Note: this is only useful when there's at least one XIM
           * client, so we need to check ic_list. */
          if ( ic_list != NULL ) {
              if ( is_char_available(t1, t2) ) {
                  kanjiguess[i].d[0] = t1;  kanjiguess[i].d[1] = t2;
              } else {
                  i--;
              }
          } else {
              kanjiguess[i].d[0] = t1;  kanjiguess[i].d[1] = t2;
          }
	  while (*p && !isspace(*p)) p++;
	}
      num_guesses = i+1;
      karea_draw(karea);

      /* commit the best guess to XIM client */
      if ( ic_list != NULL ) {
          commit_char( kanjiguess[0].d[0], kanjiguess[0].d[1] );
          pad_area_clear (pad_area);
      }
    }
}

/* Open the connection to the engine */
static void 
init_engine(PadArea* pad_area)
{
  int in_fd[2];
  int out_fd[2];

  if (pipe(in_fd))
    g_error ("Can't create pipe: %s", g_strerror(errno));
  if (pipe(out_fd))
    g_error ("Can't create pipe: %s", g_strerror(errno));

  engine_pid = fork ();
  if (engine_pid == (pid_t) 0) /* child */
    {
      close (in_fd[1]);
      close (out_fd[0]);
      
      dup2 (in_fd[0], STDIN_FILENO);
      dup2 (out_fd[1], STDOUT_FILENO);

      if (data_file)
	execlp ("kpengine","kpengine","--data-file", data_file, NULL);
      else
	execlp ("kpengine","kpengine", NULL);

      g_error ("Couldn't exec kpengine: %s", g_strerror(errno));

      g_assert_not_reached();
    }
  else if (engine_pid < (pid_t) 0) /* failure */
    g_error ("Fork failed: %s", g_strerror(errno));

  /* Parent process */

  close (in_fd[0]);
  close (out_fd[1]);

  if (!(to_engine = fdopen (in_fd[1], "w")))
    g_error ("Couldn't create pipe to child process: %s", g_strerror(errno));
  if (!(from_engine = fdopen (out_fd[0], "r")))
    g_error ("Couldn't create pipe from child process: %s", g_strerror(errno));

  gdk_input_add (out_fd[0], GDK_INPUT_READ, engine_input_handler, pad_area);
}

  
/* Create Interface */

void
usage ()
{
  fprintf(stderr, "Usage: %s [-f/--data-file FILE]\n", progname);
  exit (1);
}


static int
window_key_press_event (GtkWidget *w, GdkEventKey *event, void* d)
{
        if ( ic_list != NULL ) {
                if ( strncmp(event->string, " ", 1) == 0 ) {
                        commit_char(kanjiguess[0].d[0], kanjiguess[0].d[1]);
                        pad_area_clear (pad_area);
                        return TRUE;
                } else {
                        return FALSE;   /* for other keys, we don't handle */
                }
        } else {
                return FALSE;
        }
}


int 
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *main_hbox;
  GtkWidget *vseparator;
  GtkWidget *button;
  GtkWidget *main_vbox;
  GtkWidget *menubar;
  GtkWidget *vbox;
  GtkWidget *label;
  
  GtkItemFactory *factory;
  GtkAccelGroup *accel_group;

  GtkStyle *style;
  GdkFont *font;
  int i;
  char *p;

  static const GtkTargetEntry targets[] = {
    { "TEXT",   0, 0 },
    { "COMPOUND_TEXT", 0, 0 }
  };
  static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

  /* XIM thingy */
  XIMEncodings encodings;
  XIMStyles input_styles;
  char transport[80];
  
  p = progname = argv[0];
  while (*p)
    {
      if (*p == '/') progname = p+1;
      p++;
    }


  gtk_init (&argc, &argv);

  locale = setlocale(LC_CTYPE, "");
  if ( locale == NULL ) {
    fprintf( stderr, "\nCannot find your chosen locale, falling back to locale \"C\".\n" );
    fprintf( stderr, "However, the program cannot be used as an XIM server in this way.\n" );
    setlocale(LC_CTYPE, "C");
  }
fprintf(stderr,"locale = %s\n", locale);

  for (i=1; i<argc; i++)
    {
      if (!strcmp(argv[i], "--data-file") ||
	  !strcmp(argv[i], "-f"))
	{
	  i++;
	  if (i < argc)
	    data_file = argv[i];
	  else
	    usage();
	}
      else
	{
	  usage();
	}
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, TRUE);

  gtk_signal_connect (GTK_OBJECT(window), "destroy",
		      GTK_SIGNAL_FUNC(gtk_exit), NULL);
  gtk_signal_connect (GTK_OBJECT(window), "delete_event",
		      GTK_SIGNAL_FUNC(gtk_true), NULL);
  /* to catch the space bar */
  gtk_signal_connect (GTK_OBJECT (window), "key_press_event",
                      GTK_SIGNAL_FUNC (window_key_press_event), NULL);

  gtk_window_set_title (GTK_WINDOW(window), "KanjiPad");
  
  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);
  gtk_widget_show (main_vbox);

  /* Menu */

  accel_group = gtk_accel_group_new ();
  factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
  gtk_item_factory_create_items (factory, nmenu_items, menu_items, NULL);

  /* create a menubar */
  menubar = gtk_item_factory_get_widget (factory, "<main>");
  gtk_menu_item_activate(GTK_MENU_ITEM(gtk_item_factory_get_widget(factory, "/Character/Delay Period/Just right")));  /* select the "Just right" item */
  gtk_box_pack_start (GTK_BOX (main_vbox), menubar, FALSE, TRUE, 0);
  gtk_widget_show (menubar);

  /*  Install the accelerator table in the main window  */
  gtk_accel_group_attach (accel_group, GTK_OBJECT (window));

  main_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), main_hbox, TRUE, TRUE, 0);
  gtk_widget_show (main_hbox);

  /* Area for user to draw characters in */

  pad_area = pad_area_create ();

  gtk_box_pack_start (GTK_BOX (main_hbox), pad_area->widget, TRUE, TRUE, 0);
  gtk_widget_show (pad_area->widget);

  vseparator = gtk_vseparator_new();
  gtk_box_pack_start (GTK_BOX (main_hbox), vseparator, FALSE, FALSE, 0);
  gtk_widget_show (vseparator);
  
  /* Area in which to draw guesses */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  karea = gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(karea), 30, 0);

  gtk_signal_connect (GTK_OBJECT (karea), "configure_event",
		      GTK_SIGNAL_FUNC (karea_configure_event), NULL);
  gtk_signal_connect (GTK_OBJECT (karea), "expose_event",
		      GTK_SIGNAL_FUNC (karea_expose_event), NULL);
  gtk_signal_connect (GTK_OBJECT (karea), "button_press_event",
		      GTK_SIGNAL_FUNC (karea_button_press_event), NULL);
  gtk_signal_connect (GTK_OBJECT (karea), "selection_clear_event",
		      GTK_SIGNAL_FUNC (karea_selection_clear), NULL);

  gtk_selection_add_targets (karea, GDK_SELECTION_PRIMARY,
			     targets, n_targets);
  gtk_signal_connect (GTK_OBJECT (karea), "selection_get",
		      GTK_SIGNAL_FUNC (karea_selection_get), NULL);

  gtk_widget_set_events (karea, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
  
  style = gtk_style_new();
//  font = gdk_font_load ("-*-fixed-medium-r-*--24-*-jisx0208.1983-0");
  /* GNU unifont provides this font:
   * -gnu-unifont-medium-r-normal--16-160-75-75-c-80-iso10646-1*/
  font = gdk_font_load ("-*-*-medium-r-*--16-*-iso10646-1");
  gdk_font_unref (style->font);
  style->font = font;

  gtk_widget_set_style (karea, style);
  
  gtk_box_pack_start (GTK_BOX (vbox), karea, TRUE, TRUE, 0);
  gtk_widget_show (karea);

  /* Buttons */

//  label = gtk_label_new ("\x30\x7a");
  label = gtk_label_new ("\x27\x14");   /* 0x2714, a think TICK */
  /* We have to set the alignment here, since GTK+ will fail
   * to get the width of the string appropriately...
   */
  gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);
//  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
  gtk_widget_set_style (label, style);
  gtk_widget_show (label);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (look_up_callback), NULL);

  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  
//  label = gtk_label_new ("\x3e\x43");
  label = gtk_label_new ("\x27\x15");   /* 0x2716, a think CROSS */
  gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);
  gtk_widget_set_style (label, style);
  gtk_widget_show (label);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (clear_callback), NULL);

  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  gtk_widget_show(window);

  init_engine(pad_area);

  /* XIM thingy again */
  encodings.count_encodings = sizeof(allEncodings)/sizeof(XIMEncoding) - 1;
  encodings.supported_encodings = allEncodings;

  input_styles.count_styles = sizeof(defaultStyles)/sizeof(XIMStyle) - 1;
  input_styles.supported_styles = defaultStyles;

#if 0
  if (use_local) {
  {
      char hostname[64];
      char *address = "/tmp/.ximsock";
  
      gethostname(hostname, 64);
      sprintf(transport, "local/%s:%s", hostname, address);
  } else if (use_tcp) {
      char hostname[64];
      int port_number = 9010;
  
      gethostname(hostname, 64);
      sprintf(transport, "tcp/%s:%d", hostname, port_number);
  } else {
#endif /* if 0 */
      strcpy(transport, "X/");
#if 0
  }
#endif /* if 0 */


  ims = IMOpenIM(GDK_WINDOW_XDISPLAY( GTK_WIDGET(window)->window ),
                 IMModifiers, "Xi18n",
                 IMServerWindow, GDK_WINDOW_XWINDOW( GTK_WIDGET(window)->window ),
                 IMServerName, "lamerpad",
                 IMLocale, "zh_TW.Big5,zh_CN.GB2312,ja_JP,ja_JP.ujis",
                 IMServerTransport, transport,
                 IMInputStyles, &input_styles,
                 IMEncodingList, &encodings,
                 IMProtocolHandler, lpProtoHandler,
                 NULL);
  if (ims == (XIMS)NULL) {
      fprintf(stderr, "Can't Open Input Method Service:\n");
      fprintf(stderr, "\tInput Method Name :%s\n", "lamerpad");
      fprintf(stderr, "\tTranport Address:%s\n", transport);
      exit(1);
  }

  gtk_main();

  return 0;
}

