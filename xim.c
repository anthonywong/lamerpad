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

/* $Id: xim.c,v 1.5 1999/11/30 11:22:42 ypwong Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlocale.h>

/* XIM stuffs */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <IMdkit.h>
#include <Xi18n.h>

#include "xim.h"
#include "lamerpad.h"

XIMS ims;

IC* currIC;     /* the current IC that's being served, changes upon focus change */

IC* ic_list = (IC *)NULL;
IC* free_list = (IC *)NULL;
IC_locale* ic_locales = NULL;

int XIMOpenHandler(XIMS ims, IMOpenStruct* call_data)
{
        IC_locale* ic_locale;
        if ( (ic_locale = (IC_locale*)malloc(sizeof(IC_locale))) == NULL ) {
                fprintf(stderr, "No memory\n");
                exit (1);
        }
        ic_locale->connect_id = (int)call_data->connect_id;
        ic_locale->locale = strdup(call_data->lang.name);
        ic_locale->next = ic_locales;
        ic_locales = ic_locale;

        printf("new_client lang = %s\n", call_data->lang.name);
        printf("     connect_id = 0x%x\n", (int)call_data->connect_id);

        return True;
}

int XIMCloseHandler(XIMS ims, IMCloseStruct* call_data)
{
        IC_locale* cur = ic_locales;
        IC_locale* prev = NULL;

        printf("closing connect_id 0x%x\n", (int)call_data->connect_id);

        while ( cur != NULL ) {
                if ( cur->connect_id ==  (int)call_data->connect_id ) {
                        if ( prev ) {
                                prev->next = cur->next;
                        } else {
                                ic_locales = cur->next;
                        }
                        free(cur->locale);
                        free(cur);

                        return True;
                } else {
                        prev = cur;
                        cur = cur->next;
                }
        }

        return False;
}

/*
 * Create a new IC node and manage the ic_list and free_list linked
 * lists.
 */
static IC* newIC(void)
{
        static CARD16 icid = 0;
        IC* rec;

        if (free_list != NULL) {
                /* get the first node from free_list and assign to rec */
                rec = free_list;
                free_list = free_list->next;
        } else {
                if ( (rec = (IC *)malloc(sizeof(IC))) == NULL )
                        return NULL;
        }
        memset(rec, 0, sizeof(IC));
        rec->id = ++icid;

        rec->next = ic_list;
        ic_list = rec;

        return rec;
}


int createICHandler(XIMS ims, IMChangeICStruct* call_data)
{
        IC* ic;
        IC_locale* cur = ic_locales;

        ic = newIC();
        if (ic == NULL)
              return False;

        /* determine the locale for this IC */
        while ( cur != NULL ) {
                if ( cur->connect_id ==  (int)call_data->connect_id ) {
                        ic->locale = cur->locale;
                        break;
                } else {
                        cur = cur->next;
                }
        }
        if ( cur == NULL ) {
                fprintf(stderr, "Abnormal behavior. Program aborted.\n");
                exit (1);
        }

        ic->encoding = locale2encoding(ic->locale);
        ic->connect_id = call_data->connect_id;
        call_data->icid = ic->id;  /* set the IC's id by using our sequential counter */

        currIC = ic;    /* set the current IC to be this new one */

        return True;
}

/*
 * Destroy an IC node and manage the ic_list and free_list linked
 * lists.
 */
int destroyICHandler(XIMS ims, IMDestroyICStruct* call_data)
{
        IC* ic = ic_list;
        IC* prev = NULL;

        while ( ic != NULL ) {
                if ( call_data->icid == ic->id ) {
                        if ( prev != NULL )
                                prev->next = ic->next;
                        else
                                ic_list = ic->next;
                        ic->next = free_list;
                        free_list = ic;

                        return  True;
                } else {
                        prev = ic;
                        ic = ic->next;
                }
        }
        return False;
}

int MyGetICValuesHandler(XIMS ims, IMChangeICStruct* call_data)
{   
//    GetIC(call_data);
    return True;
}

int MySetICValuesHandler(XIMS ims, IMChangeICStruct* call_data)
{
//    SetIC(call_data);
    return True;
}


int setICFocusHandler(XIMS ims, IMChangeFocusStruct* call_data)
{
        IC* cand;

        if ( (cand = ic_find(call_data->icid)) == NULL ) {
                fprintf(stderr, "Abnormal, cannot set focus.\n");
                return False;   /* just continue */
        } else {
                currIC = cand;
                return True;
        }
}


int forwardAllEventsHandler(XIMS ims, IMForwardEventStruct* call_data)
{
        /* Just forward all events, we need none of them!  */
        IMForwardEvent(ims, (XPointer)call_data);
        return True;
}


/* XIM ProtoHandler */
int lpProtoHandler(XIMS ims, IMProtocol* call_data)
{
        switch (call_data->major_code) {
            case XIM_OPEN:
                fprintf(stderr, "XIM_OPEN:\n");
                return XIMOpenHandler(ims, &call_data->imopen);
            case XIM_CLOSE:
                fprintf(stderr, "XIM_CLOSE:\n");
                return XIMCloseHandler(ims, &call_data->imclose);
            case XIM_CREATE_IC:
                fprintf(stderr, "XIM_CREATE_IC:\n");
                return createICHandler(ims, &call_data->changeic);
            case XIM_DESTROY_IC:
                fprintf(stderr, "XIM_DESTROY_IC.\n");
                return destroyICHandler(ims, &call_data->destroyic);
            case XIM_SET_IC_VALUES:
                fprintf(stderr, "XIM_SET_IC_VALUES:\n");
                return MySetICValuesHandler(ims, &call_data->changeic);
            case XIM_GET_IC_VALUES:
                fprintf(stderr, "XIM_GET_IC_VALUES:\n");
                return MyGetICValuesHandler(ims, &call_data->changeic);
            case XIM_SET_IC_FOCUS:
                fprintf(stderr, "XIM_SET_IC_FOCUS()\n");
                return setICFocusHandler(ims, &call_data->changefocus);
            case XIM_UNSET_IC_FOCUS:
                fprintf(stderr, "XIM_UNSET_IC_FOCUS:\n");
                /* no need to do anything for unset IC focus */
                return True;
            case XIM_RESET_IC:
                fprintf(stderr, "XIM_RESET_IC_FOCUS:\n");
                return True;
            case XIM_FORWARD_EVENT:
                fprintf(stderr, "XIM_FORWARD_EVENT:\n");
                return forwardAllEventsHandler(ims, &call_data->forwardevent);

            default:
                fprintf(stderr, "DEFAULT\n");
                return True;
        }
}


char* locale2encoding(char* locale)
{
        typedef struct {
                char* locale;
                char* encoding;
        } EncodingMapping;

        static EncodingMapping maps[] = {
                { "zh_TW.Big5",   "BIG-5" },
                { "zh_CN.GB2312", "EUC-CN" },
                { "ja_JP",        "EUC-JP" },
                { "ja_JP.ujis",   "UJIS" },
                /* if the locale is C or POSIX, then assume that
                 * unicode should be used */
                { "C",            "UCS-2" },
                { "POSIX",        "UCS-2" },
                { NULL, NULL }
        };
        int i;

        for ( i = 0; i < sizeof(maps)-1; i++ ) {
                if ( strcmp(locale, maps[i].locale) == 0 ) {
#ifdef DEBUG
printf("%s\n",maps[i].encoding);
#endif
                        return maps[i].encoding;
                }
        }
        fprintf(stderr, "Encoding for locale %s is not supported.\n", locale);
        return NULL;
}


#include <iconv.h>
/*  This is a convenience function.
 *  1 - available, 0 - not */
int
is_char_available(char ch1, char ch2)
{
        char str[3] = { ch1, ch2, '\0' };
        char outbuf[OUTBUF_SIZE];

        /* return value of convert_char is (size_t)-1L if conversion
         * is unsuccessful */
        if ( convert_char(str, outbuf) == (size_t)-1L ) {
                return 0;
        } else {
                return 1;
        }
}

/*
 *  Convert the encoding of the char in inptr to outptr according to
 *  the current IC's locale.
 */
size_t
convert_char(char str[3], char outbuf[OUTBUF_SIZE])
{
        iconv_t cd;
        size_t len = 2, outlen = OUTBUF_SIZE;
        char *inptr, *outptr;   /* without this we'll have a warning for incmpt ptr */
        size_t n;

        inptr = str;
        outptr = outbuf;

        /* Convert from UCS-2 to client's locale's encoding */
        cd = iconv_open (currIC->encoding, "UCS-2");
        n = iconv(cd, (const char**) &inptr, &len, &outptr, &outlen);
        return n;
}

/*
 *  Commit the char (composed by ch1+ch2) to the XIM client.
 */
int commit_char(char ch1, char ch2)
{
        char str[3] = { ch1, ch2, '\0' };
        IMCommitStruct out;
        XTextProperty tp;
        char outbuf[OUTBUF_SIZE];
        Display* dpy;
        char *cch_str_list[1];


        locale = setlocale(LC_CTYPE, currIC->locale);

        /* construct the outgoing IMCommitStruct structure */
        bzero(&out, sizeof(out));
        out.major_code = XIM_COMMIT;
        out.minor_code = PROTOCOLMINORVERSION;
        out.connect_id = currIC->connect_id;
        out.icid = currIC->id;
        out.flag = XimLookupChars;

        convert_char(str, outbuf);

#ifdef DEBUG
fprintf(stderr,"\noutput: %s %02hx%02hx from %02hx%02hx\n", outbuf, outbuf[0], outbuf[1], str[0], str[1]);
outbuf[2] = '\0';
#endif
        dpy = ims->core.display;
        cch_str_list[0] = outbuf;
        XmbTextListToTextProperty(dpy, cch_str_list, 1, XCompoundTextStyle, &tp);
#ifdef DEBUG
fprintf(stderr, "commiting string...(%s)\n", tp.value);
#endif
        out.commit_string = tp.value;
        IMCommitString(ims, (XPointer)(&out));

        return True;
}


/*
 * Find the IC structure from its sequential ID, copied from xcin
 */  
IC* ic_find(CARD16 icid)
{
        IC* ic = ic_list;

        while (ic != NULL) {
                if (ic->id == icid)
                        return ic;
                ic = ic->next;
        }
        return NULL;
}

