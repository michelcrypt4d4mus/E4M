/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#include "e4mdefs.h"

#include <sys\types.h>
#include <sys\stat.h>
#include <direct.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "dir.h"

/* create full directory tree. returns 0 for success, -1 if failure */
int
mkfulldir (char *path, BOOL bCheckonly)
{
	struct _stat st;
	char *uniq_file;

	if (strlen (path) == 3 && path[1] == ':')
		goto is_root;	/* keep final slash in root if present */

	/* strip final forward or backslash if we have one! */
	uniq_file = strrchr (path, '\\');
	if (uniq_file && uniq_file[1] == '\0')
		uniq_file[0] = '\0';
	else
	{
		uniq_file = strrchr (path, '/');
		if (uniq_file && uniq_file[1] == '\0')
			uniq_file[0] = '\0';
	}

      is_root:
	if (bCheckonly == TRUE)
		return _stat (path, &st);

	if (_stat (path, &st))
		return mkfulldir_internal (path);
	else
		return 0;
}


int
mkfulldir_internal (char *path)
{
	char *token;
	struct _stat st;
	static char tokpath[_MAX_PATH];
	static char trail[_MAX_PATH];

	strcpy (tokpath, path);
	trail[0] = '\0';

	token = strtok (tokpath, "\\/");

	if (tokpath[0] == '\\' && tokpath[1] == '\\')
	{			/* unc */
		trail[0] = tokpath[0];
		trail[1] = tokpath[1];
		trail[2] = '\0';
		strcat (trail, token);
		strcat (trail, "\\");
		token = strtok (NULL, "\\/");
		if (token)
		{		/* get share name */
			strcat (trail, token);
			strcat (trail, "\\");
		}
		token = strtok (NULL, "\\/");
	}

	if (tokpath[1] == ':')
	{			/* drive letter */
		strcat (trail, tokpath);
		strcat (trail, "\\");
		token = strtok (NULL, "\\/");
	}

	while (token != NULL)
	{
		int x;
		strcat (trail, token);
		x = mkdir (trail);
		strcat (trail, "\\");
		token = strtok (NULL, "\\/");
	}

	return _stat (path, &st);
}
