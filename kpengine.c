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

/* $Id: kpengine.c,v 1.3 2000/01/12 00:56:07 ypwong Exp $ */

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h> /* for ntohl */
#include "jstroke/jstroke.h"

#define MAX_STROKES 32
#define BUFLEN 1024

static char *stroke_dicts[MAX_STROKES];
static char *progname;
static char *data_file;

char *
make_filename(char *dir, char *file)
{
  int len = strlen(dir) + strlen(file) + 2;
  char *filename = malloc(len);
  if (!filename)
    {
      fprintf(stderr,"%s: Can't allocate %d bytes for filename\n", 
	      progname, len);
      exit(1);
    }
  strcpy (filename, dir);
  strcat (filename, "/");
  strcat (filename, file);

  return filename;
}

void
load_database()
{
  int fd;
  int i;
  char *fname;

  if (data_file)
    {
      fd = open(data_file,0,O_RDONLY);
    }
  else
    {
      fname = make_filename (KP_LIBDIR, "chardata.dat");
      fd = open(fname,0,O_RDONLY);
      
      if (fd < 0)
	{
	  free(fname);
	  fname = make_filename (".", "chardata.dat");
	  fd = open(fname,0,O_RDONLY);
	  
	}
      free(fname);
    }

  if (fd < 0)
    {
      fprintf(stderr,"%s: Can't open %s\n", progname,
	      data_file ? data_file : fname);
      exit(1);
    }


  for (i=0;i<MAX_STROKES;i++)
    stroke_dicts[i] = NULL;

  while (1)
    {
      int bytes_read;
      unsigned int nstrokes;
      unsigned int len;
      int buf[2];

      bytes_read = read(fd,&buf,2*sizeof(int));
      
      nstrokes = ntohl(buf[0]);
      len = ntohl(buf[1]);
      
      if ((bytes_read != 2*sizeof(int)) || (nstrokes > MAX_STROKES))
	{
	  fprintf(stderr, "%s: Corrupt stroke database", progname);
	  exit(1);
	}

      if (nstrokes == 0)
	break;

      stroke_dicts[nstrokes] = malloc(len);
      bytes_read = read(fd, stroke_dicts[nstrokes], len);

      if (bytes_read != len)
	{
	  fprintf(stderr, "%s: Corrupt stroke database", progname);
	  exit(1);
	}
    }
  
  close(fd);
}

/* From Ken Lunde's _Understanding Japanese Information Processing_
   O'Reilly, 1993 */
/*
void 
sjis2jis(unsigned char *p1, unsigned char *p2)
{
  unsigned char c1 = *p1;
  unsigned char c2 = *p2;
  int adjust = c2 < 159;
  int rowOffset = c1 < 160 ? 112 : 176;
  int cellOffset = adjust ? (c2 > 127 ? 32 : 31) : 126;
  
  *p1 = ((c1 - rowOffset) << 1) - adjust;
  *p2 -= cellOffset;  
}
*/

int
process_strokes (FILE *file)
{
  RawStroke strokes[MAX_STROKES];
  char *buffer = malloc(BUFLEN);
  int buflen = BUFLEN;
  int nstrokes = 0;

  /* Read in strokes from standard in, all points for each stroke
   *  strung together on one line, until we get a blank line
   */
  
  while (1)
    {
      char *p,*q;
      int len;

      /* read data from 'file' into buffer, 'file' is possibly stdin */

      if (!fgets(buffer, buflen, file))
	return 0;

      while ((strlen(buffer) == buflen - 1) && (buffer[buflen-2] != '\n'))
	{
	  buflen += BUFLEN;
	  buffer = realloc(buffer, buflen);
	  if (!fgets(buffer+buflen-BUFLEN-1, BUFLEN+1, file))
	    return 0;
	}
      
      len = 0;
      p = buffer;
      
      /* Decompose the input string which represents the 'nstrokes'th * string.
       * Acceptable format is x y x y x y ... */
      while (1) {
	while (isspace (*p)) p++;
	if (*p == 0)
	  break;
	strokes[nstrokes].m_x[len] = strtol (p, &q, 0);
	if (p == q)
	  break;
	p = q;
	  
	while (isspace (*p)) p++;
	if (*p == 0)
	  break;
	strokes[nstrokes].m_y[len] = strtol (p, &q, 0);
	if (p == q)
	  break;
	p = q;
	
	len++;
      }
      
      if (len == 0)
	break;
      
      strokes[nstrokes].m_len = len;
      nstrokes++;       /* increment the number of strokes */
      if (nstrokes == MAX_STROKES)
	break;
    }
  
  if (nstrokes != 0 && stroke_dicts[nstrokes])
    {
      int i;
      ListMem *top_picks;
      StrokeScorer *scorer = StrokeScorerCreate (stroke_dicts[nstrokes],
						 strokes, nstrokes);
      if (scorer)
	{
	  StrokeScorerProcess(scorer, -1);
	  top_picks = StrokeScorerTopPicks(scorer);
	  StrokeScorerDestroy(scorer);
	  
	  printf("K");
	  for (i=0;i<top_picks->m_argc;i++)
	    {
	      unsigned char c[2];
	      if (i)
		printf(" ");
	      c[0] = top_picks->m_argv[i][0];
	      c[1] = top_picks->m_argv[i][1];
//	      sjis2jis(&c[0],&c[1]);
	      printf("%02x%02x",c[0],c[1]);
	    }
	  
	  free(top_picks);
	}
      printf("\n");

      fflush(stdout);
    }
  return 1;
}

void
usage ()
{
  fprintf(stderr, "Usage: %s [-f/--data-file FILE]\n", progname);
  exit (1);
}

int 
main(int argc, char **argv)
{
  int i;
  char *p = progname = argv[0];
  while (*p)
    {
      if (*p == '/') progname = p+1;
      p++;
    }

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
  
  load_database();

  while (process_strokes (stdin))
    ;

  return 0;
}
