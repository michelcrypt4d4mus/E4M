/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#include "e4mdefs.h"

#include <string.h>
#include <ctype.h>

#include "uncroot.h"

char *
MakeRootName (char *lpszDest, char *lpszSrc)
{
  strcpy (lpszDest, lpszSrc);

  if (lpszDest[1] == ':' && isalpha (lpszDest[0]))
    lpszDest[3] = 0;		/* Straight drive letter */
  else
    {
      if (lpszDest[0] == '\\' && lpszDest[1] == '\\')
	{
	  char *p = strchr (lpszDest + 2, '\\');
	  if (p != NULL)
	    {
	      p = strchr (p + 1, '\\');
	      if (p != NULL)
		*(p + 1) = 0;	/* UNC + path */
	      else
		{
		  strcat (lpszDest, "\\");	/* UNC + share name no path */
		}
	    }
	  else
	    lpszDest[1] = 0;
	}
      else
	lpszDest[1] = 0;	/* Default drive letter */
    }

  return lpszDest;
}
