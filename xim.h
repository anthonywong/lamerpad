#ifndef __XIM_H
#define __XIM_H

#include <IMdkit.h>
#include <Xi18n.h>
#include <gtk/gtk.h>

typedef struct _IC_locale {
        CARD16  connect_id;
        char*   locale;

        struct _IC_locale* next;
} IC_locale;

typedef struct _IC {
        CARD16  id;     /* ic id, sequentially assigned to new ICs starting from 0 */
        CARD16  connect_id; /* id of connected client */
        Window  client_win; /* client window */
        Window  focus_win;  /* focus window */
        char*   resource_name; /* resource name */
        char*   resource_class; /* resource class */
        char*   locale;
        char*   encoding;

        struct _IC* next;
} IC;


int lpProtoHandler(XIMS ims, IMProtocol* call_data);

int XIMOpenHandler(XIMS ims, IMOpenStruct* call_data);
int XIMCloseHandler(XIMS ims, IMCloseStruct* call_data);
int createICHandler(XIMS ims, IMChangeICStruct* call_data);
int destroyICHandler(XIMS ims, IMDestroyICStruct* call_data);
int MyGetICValuesHandler(XIMS ims, IMChangeICStruct* call_data);
int MySetICValuesHandler(XIMS ims, IMChangeICStruct* call_data);
int setICFocusHandler(XIMS ims, IMChangeFocusStruct* call_data);
int forwardAllEventsHandler(XIMS ims, IMForwardEventStruct* call_data);

#define OUTBUF_SIZE     32768
int is_char_available(char ch1, char ch2);
size_t convert_char(char str[3], char outbuf[OUTBUF_SIZE]);
int commit_char(gchar ch1, gchar ch2);
char* locale2encoding(char* locale);
IC* ic_find(CARD16 icid);


extern XIMS ims;
extern IC* ic_list;
extern IC* free_list;

#endif /* __XIM_H */

/* $Id: xim.h,v 1.3 1999/11/30 11:22:42 ypwong Exp $ */
