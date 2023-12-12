/*
 *  tgdbm.c --
 *
 *   This file contains a simple Tcl-Wrapper for gdbm-1.8.3.
 *   Copyright (C) 2000 - 2005 Stefan Vogel
 *
 *   $Log: tgdbm.c,v $
 *   Revision 1.5  2005/04/14 14:13:01  svogel
 *   Added changes from Thomas Maeder
 *
 *   Revision 1.4  2003/12/16 12:29:31  svogel
 *   gdbm_open without args returns list of open handles
 *
 *   Revision 1.3  2003/12/12 11:56:23  svogel
 *   completely rewritten, added array, more modularized
 *
 *   Revision 1.2  2000/10/15 16:25:53  svogel
 *   Changed Tcl_SetResult from TCL_STATIC to TCL_VOLATILE
 *   in count/maxkey. Thanks to Uwe Klein for his patch.
 *
 *   Revision 1.1  2000/10/15 16:25:38  svogel
 *   Initial revision
 *
 */
/*  TGDBM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    TGDBM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    If you don't have a copy of the GNU General Public License,
    write to the Free Software Foundation, 675 Mass Ave, Cambridge,
    MA 02139, USA.

    You may contact the author by:
       e-mail:  stefan.vogel@avinci.de

*************************************************************************/
 
static char rcsid[] = "$Id: tgdbm.c,v 1.5 2005/04/14 14:13:01 svogel Exp $";

// Sync to tcl.h
#if (defined(__WIN32__) && (defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || defined(__LCC__) || defined(__WATCOMC__) || (defined(__GNUC__) && defined(__declspec))))
#   define HAVE_DECLSPEC 1
#   ifdef STATIC_BUILD
#       define DLLIMPORT
#       define DLLEXPORT
#       ifdef _DLL
#           define CRTIMPORT __declspec(dllimport)
#       else
#           define CRTIMPORT
#       endif
#   else
#       define DLLIMPORT __declspec(dllimport)
#       define DLLEXPORT __declspec(dllexport)
#       define CRTIMPORT __declspec(dllimport)
#   endif
#else
#   define DLLIMPORT
#   if defined(__GNUC__) && __GNUC__ > 3
#       define DLLEXPORT __attribute__ ((visibility("default")))
#   else
#       define DLLEXPORT
#   endif
#   define CRTIMPORT
#endif

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <tcl.h>

#include <gdbm.h>
    
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#   define ANYARGS		(...)
#else
#   define ANYARGS		()
#endif

/* These external variables are needed by tgdbm */
// Define in gdbm.h, disable it
//extern const char* gdbm_version;
//extern gdbm_error gdbm_errno;

#define TGDBM_VERSION "0.7"

/* 0x10 */
#define GDBM_SETVAR TCL_TRACE_READS
/* 0x20 */
#define GDBM_DELVAR TCL_TRACE_WRITES
/* 0x40 */
#define GDBM_DELALL TCL_TRACE_UNSETS

#define GDBM_TRACE_ALL (TCL_TRACE_WRITES | TCL_TRACE_READS | TCL_TRACE_UNSETS | TCL_TRACE_ARRAY)


typedef int (*Gdbm_Proc) ANYARGS;

/* -------------------- global variables -------------------- */
typedef struct {
	char *name; 		/* Name of minor command */
	Gdbm_Proc proc;
	int minArgs;		/* Minimum # args required */
	int maxArgs;		/* Maximum # args required */
	char *usage;		/* Usage message */

} Gdbm_ProcSpec;

/* handle to opened gdbm-file, used in Gdbm_Cmd */
Tcl_HashTable thtbl;

/* pointer to the tcl-string (if provided in gdbm_open call),
   the tcl-function specified with this string is called
   in fatal-error-case (hopefully never) */
char* gFatalFunc = NULL;

/* Tcl-Interpreter for fatal-function */
Tcl_Interp* gInterp = NULL;

/* used for returning errors, see defines SET_ERROR */
char sErrno[5];

/* the structure which holds all information about the
   gdbm-file and additional infos about the array */
typedef struct {
  int has_array;       /* is there a corresponding array? */
  int full_cache;      /* is the whole gdbm-file in the array?
					    * 0 == no; 1 == yes*/
  int save_with_null;  /* has the file trailing-nulls in it? */
  GDBM_FILE gdbm_file; /* The gdbm-file-handle */
} Gdbm_Array, *PGdbm_Array;

/*
 * Prototypes for procedures defined later in this file:
 */
static char* VariableProc (ClientData, Tcl_Interp*, char*, char*, int);
int Gdbm_Open_Cmd (ClientData, Tcl_Interp*, int, char**);
int Gdbm_Cmd (ClientData, Tcl_Interp*, int, char**);
int Gdbm_Error_Cmd (ClientData, Tcl_Interp*, int, char**);

/*
 * some defines to make checking of arguments and returning
 * and setting Errors easier (don't insert backslashes and newlines, because
 * it will break vc++ windows-compiler)
 */

#define CHECK_ARGS(ARGC, MIN, MAX, USG) if ((ARGC<MIN+1)||(ARGC>MAX+1)) {Tcl_AppendResult(interp, "Usage: ", USG, NULL);return (TCL_ERROR);}


#define SET_ERROR(errno) sprintf(sErrno, "%d", errno);Tcl_SetVar(interp, "GDBM_ERRNO", (char*)sErrno, TCL_GLOBAL_ONLY);return TCL_ERROR

#define SET_OK Tcl_SetVar(interp, "GDBM_ERRNO", "0", TCL_GLOBAL_ONLY);return TCL_OK

#define SET_LENGTH(gdbm, key) if(gdbm->save_with_null) key.dsize=strlen(key.dptr)+1; else key.dsize=strlen(key.dptr)

/*----------------------------------------------------------------------
 * GetIndexFromArg -- search for a specified string in given string-table
 * 
 * Because of performance no Tcl-Obj-Command was used here (does this
 * still hold?). 
 * This has the affect that no Obj-Command-Utility-Functions could be
 * used, too.
 * This one is the only Utility-Function needed, so it is reimplemented
 * here.
 *
 * Results: 
 *  position/index of string in string-table
 *  -1 : string not found in string-table
 *----------------------------------------------------------------------
 */
int 
GetIndexFromArg(arg, tablePtr)
     char* arg;
     char** tablePtr;
{
  int i;
  char **entryPtr;

  for (entryPtr = tablePtr, i = 0; *entryPtr != NULL;
       entryPtr = (char**) ((long) entryPtr + sizeof(char*)), i++) {
    if (!strcmp(*entryPtr, arg)) return i;
  }
  return -1;
}

/*----------------------------------------------------------------------
 * GetIndexFromSpec -- see GetIndexFromArg
 * 
 * only difference is that index is retrieved from "spec"
 *
 * Results: 
 *  position/index of string in spec
 *  -1 : string not found in spec
 *----------------------------------------------------------------------
 */int 
GetIndexFromSpec(arg, numArgs, procSpec)
     char* arg;
	 int   numArgs;
     Gdbm_ProcSpec procSpec[];
{
  int i;

  for (i = 0; i < numArgs; i++) {
    if (!strcmp(procSpec[i].name, arg)) return i;
  }
  return -1;
}


/*----------------------------------------------------------------------
 * toNumber -- convert a given string if it is a valid non-negative
 * integer
 *
 * Results: 
 *  1 : string contains only digits
 *  0 : string is no positive integer
 * 
 * Side effects:
 *  *result is assigned the result of the conversion
 *----------------------------------------------------------------------
 */
int
toNumber(string, result)
     char* string;
     int* result;
{
  char* end;
  unsigned long tmp = strtoul(string,&end,10);
  if (end==string+strlen(string) && tmp<=INT_MAX)
  {
    *result = tmp;
    return 1;
  }
  else
    return 0;
}


/*----------------------------------------------------------------------
 * copyResultNull -- copy the result with trailing null
 *
 *  This is used when gdbm_open is specified with "no_trailing_null".
 *  We have to copy the results, so that they are "C"-conform-strings
 *  (with trailing null).
 *  The result has to be freed by the caller.
 *  Attention the input-string is freed by this function
 *  
 * Results:
 *  string - copy/original from parameter str
 * 
 * Side effects:
 *  str is copied if there is no trailing null. str is freed if
 *  necessary (and freeStr == true)
 *----------------------------------------------------------------------
 */
char*
copyResultNull(str, size, freeStr)
    char* str;
	int   size;
	int   freeStr;
{
  char* result;
  if (size>0 && (char)str[size-1] == (char)NULL) {
	result = str;
  }
  else {
	result = malloc(size+1);
	strncpy(result, str, size);
	result[size] = (char) NULL;
	if (freeStr)
	  free(str);
  }
  return result;
}


/*----------------------------------------------------------------------
 * fillCompleteArray -- do a full-cache of the gdbm-file
 *
 * force = 1 -> don't check if values in array exists (for initial filling
 * of array)!
 *----------------------------------------------------------------------
 */
int
fillCompleteArray(interp, gdbm, array_name, force)
	 Tcl_Interp* interp;
	 PGdbm_Array gdbm;
	 char* array_name;
	 int   force;
{
  int size;
  datum key, key2, result;
  char* res_key;
  char* res_res;

  size = 0;
  key = gdbm_firstkey(gdbm->gdbm_file);
  while (key.dptr != NULL) {
	size++;
	/* dont free key.dptr here, because we need it for nextkey
	   and we free it ourself */
	res_key = copyResultNull(key.dptr, key.dsize, 0);
	if (force
		|| Tcl_GetVar2(interp, array_name, res_key, 0) == NULL) {
	  result = gdbm_fetch(gdbm->gdbm_file, key);
	  res_res = copyResultNull(result.dptr, result.dsize, 1);
	  if (res_res != NULL
		  && Tcl_SetVar2(interp, array_name, res_key, res_res, TCL_LEAVE_ERR_MSG) == NULL) {
		/* TODO error should be returned here?? */
		printf("Error setting %s(%s) to %s\n", array_name, res_key, res_res);
	  }
	  free(res_res);
	}
	key2 = key;
	key = gdbm_nextkey(gdbm->gdbm_file, key2);
	if (key2.dptr != res_key) {
	  free(key2.dptr);
	  free(res_key);
	}
	else {
	  free(key2.dptr);
	}
  }
  return size;
}


/*----------------------------------------------------------------------
 * gdbmFatalFunc -- standard function called from gdbm in fatal error
 *
 *  If a Tcl-Function has been provided with -fatal_func in call to
 *  gdbm_open, this function will be called here.
 *  Hopefully this should be never.
 *----------------------------------------------------------------------
 */
void
gdbmFatalFunc(message)
    char* message;
{
  char* script;

  if (gFatalFunc && gInterp) {
    script = malloc(strlen(gFatalFunc)+strlen(message)+5);
    sprintf(script, "%s {%s}", gFatalFunc, message);
    Tcl_Eval(gInterp, script);
    free(script);
  }
    exit(1);
}


/*----------------------------------------------------------------------
 * Gdbm_Error_Cmd -- Return a textual error-description for error-number
 *
 * This is the aequivalent for gdbm_strerror. If given an errorcode
 * the value of the global Tcl-Variable GDBM_ERRNO an english error-message
 * is returned.
 *
 * Results:
 *  Error-message.
 *
 * Side effects:
 *  None.
 *----------------------------------------------------------------------
 */
int
Gdbm_Error_Cmd(dummy, interp, argc, argv)
    ClientData dummy;
    Tcl_Interp* interp;
    int argc;
    char* argv[];
{
  CHECK_ARGS(argc, 1, 1, "gdbm_strerror $GDBM_ERRNO");
  Tcl_SetResult(interp, gdbm_strerror(atoi(argv[1])), TCL_STATIC);
  SET_OK;
}


/*----------------------------------------------------------------------
 * VariableProc -- Callback for Array-Access
 *
 * This is the heart of the persistant arrays. Every READ/WRITE- or ARRAY-
 * Trace is handled here.
 *
 * Results:
 *  NULL when no error occurs
 *  errorstring - else
 *
 * Side effects:
 *  The values of the array is set or read in case of a TCL_TRACE_ARRAY
 *  the whole gdbm-file is read in the array.
 *----------------------------------------------------------------------
 */
static char*
VariableProc(clientData, interp, part1, part2, flags)
	 ClientData clientData;
	 Tcl_Interp* interp;
	 char *part1, *part2;
	 int flags;
{
  datum key, result;
  static char errMsg[255] = "";
  PGdbm_Array gdbm_array = (PGdbm_Array) clientData;
  int iResult;
  char* res_res;

  if (flags & TCL_TRACE_WRITES) {
	if (part2 == NULL) {
	  /* this error is never shown because Tcl already handles this error */
	  return "variable is array";
	}
	key.dptr = part2;
	/* TODO: handle null here? */
	result.dptr = (char*) Tcl_GetVar2(interp, part1, part2, 0);
	SET_LENGTH(gdbm_array, key);
	SET_LENGTH(gdbm_array, result);

	/* test if we can do an insert, if not: replace item */
	if ((gdbm_store(gdbm_array->gdbm_file, key, result, GDBM_INSERT) != 0)
	  && (gdbm_store(gdbm_array->gdbm_file, key, result, GDBM_REPLACE) != 0)) {
	  sprintf(errMsg, "%d: %s", gdbm_errno, gdbm_strerror(gdbm_errno));
	  return errMsg;
	}
	return NULL;
	
  } else if (flags & TCL_TRACE_READS) {
	if (part2 == NULL) {
	  /* this message is like the standard tcl-error-message
	   * but "can't read xx:" is prefixed by tcl? */
	  return "variable is array";
	}
	if (Tcl_GetVar2(interp, part1, part2, 0) == NULL) {
	  key.dptr = part2;
	  SET_LENGTH(gdbm_array, key);
	  result = gdbm_fetch(gdbm_array->gdbm_file, key);
	  if (result.dptr == NULL) {
		Tcl_AppendResult(interp, "can't read \"",
						 part1, "(", part2, ")\": no such variable",
						 NULL);
		return NULL;
	  }
	  else {
		res_res = copyResultNull(result.dptr, result.dsize, 1);

		Tcl_SetVar2(interp, part1, part2, res_res, 0);
		Tcl_SetResult(interp, res_res, TCL_STATIC /* NULL */);
		free(res_res);
		return NULL;
	  }
	}
  } else if (flags & TCL_TRACE_UNSETS) {
	if (part2 != NULL) {
	  /* remove only this one entry */
	  key.dptr = part2;
	  SET_LENGTH(gdbm_array, key);
	  
	  /* retcode == -1 --> no such key or requester is reader
	   *         ==  0 --> successful delete
	   */
	  iResult = gdbm_delete(gdbm_array->gdbm_file, key);
	  if (iResult != 0) {
		Tcl_AppendResult(interp, "error: gdbmN delete - ",
						 gdbm_strerror(gdbm_errno),
						 NULL);
		/* TODO: throw an error here. This doesn't work correctly???!!! */
		sprintf(errMsg, "%d", gdbm_errno);
		Tcl_SetVar(interp, "GDBM_ERRNO", (char*)errMsg, TCL_GLOBAL_ONLY);
		sprintf(errMsg, "%d: %s", gdbm_errno, gdbm_strerror(gdbm_errno));
		return errMsg;
	  }
	}
	else {
	  Tcl_HashEntry *thetr;
	  /* Unsetting everything */
	  if (gdbm_array->gdbm_file != NULL) 
		gdbm_close(gdbm_array->gdbm_file);
	  Tcl_DeleteCommand(interp, part1);
	  Tcl_UntraceVar2(interp, part1, 
					  (char*)NULL,
					  (int) GDBM_TRACE_ALL, 
					  (Tcl_VarTraceProc*) VariableProc,
					  (ClientData) gdbm_array);
	  /* now that the command is deleted we free memory,
		 gdbm is freed in gdbm_close */
	  thetr = Tcl_FindHashEntry(&thtbl, part1);
	  if (thetr != NULL) 
		Tcl_DeleteHashEntry(thetr);
	  free(gdbm_array);
	}
  } else if (flags & TCL_TRACE_ARRAY) {
	/* Tcl_TRACE doesn't seem to be inactivated in this case, bug in sourceforge already fixed 
	   dont know number/version ... therefore no need to remove Trace anymore*/
	if (!gdbm_array->full_cache) {
	  fillCompleteArray(interp, gdbm_array, part1, 0);
	  gdbm_array->full_cache = 1;
	}
	/* else ... array is already fully cached, do nothing */
  }
  return NULL;
}


/*----------------------------------------------------------------------
 * Gdbm_ArrayTrace_Proc -- every access to data calls this callback
 * 
 *  this traces access to all gdbm-data and synchronizes the gdbm-file
 *  resp. the attached array
 * 
 * Results: 
 *
 * Side effects:
 *----------------------------------------------------------------------
 */
static int
Gdbm_ArrayTrace_Proc (clientData, interp, array_name, key, value, flags)
	 ClientData clientData;
	 Tcl_Interp* interp;
	 char *array_name, *key, *value;
	 int flags;
{
  PGdbm_Array gdbm = (PGdbm_Array) clientData;
  if (gdbm->has_array) {
	/* This is because of a bug in Tcl 8.???
	   we have to remove the trace, set the array and afterwards
	   create the array-trace again 
	   If not untraced this causes a core.
	*/

	Tcl_UntraceVar2(interp, array_name, (char*)NULL, 
					GDBM_TRACE_ALL, 
					(Tcl_VarTraceProc*) VariableProc,
					(ClientData) gdbm);
	switch (flags) {
	case GDBM_SETVAR:
	  Tcl_SetVar2(interp, array_name, key, value, 0);
	  break;
	case GDBM_DELVAR:
	  Tcl_UnsetVar2(interp, array_name, key, 0);
	  break;
	case GDBM_DELALL:
	  Tcl_UnsetVar2(interp, array_name, (char*)NULL, 0);
	  break;
	default:
	  Tcl_AppendResult(interp, "error: Wrong arguments for Gdbm_ArrayTrace_Proc\n",
					   "Must be 'GDBM_SETVAR' or 'GDBM_DELVAR'",
					   NULL);
	  SET_ERROR(GDBM_OPT_ILLEGAL);
	}
	if (flags != GDBM_DELALL) {
	  /* only set trace on again, if not completely deleted */
	  Tcl_TraceVar2(interp, array_name, (char*)NULL, 
					GDBM_TRACE_ALL, 
					(Tcl_VarTraceProc*) VariableProc,
					(ClientData) gdbm);
	}
  }
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Create_Array -- initial creation of an array
 * 
 *  This function is called when an array is attached to a gdbm-file.
 *  
 * Results:
 *  Always 1
 *
 * Side effects:
 *  The first element is always tried to be set directly into the array
 *----------------------------------------------------------------------
 */
int
Gdbm_Create_Array(interp, name, gdbm_array)
	 Tcl_Interp* interp;
	 char* name;
	 PGdbm_Array gdbm_array;
{
  datum result, key;
  char* res_key;
  char* res_res;

  /* Setting one element so the trace-read does work.
	 Nevertheless: Trace-Unset doesn't work for not existing elements
   */
  key = gdbm_firstkey(gdbm_array->gdbm_file);
  if (key.dptr != NULL) {
	result = gdbm_fetch(gdbm_array->gdbm_file, key);
	/* remember ... key will be used below! */
	res_key = copyResultNull(key.dptr, key.dsize, 1);
	res_res = copyResultNull(result.dptr, result.dsize, 1);
	Tcl_SetVar2(interp, name, res_key, res_res, TCL_LEAVE_ERR_MSG);
	free(res_key);
	free(res_res);
  }

  if (gdbm_array->full_cache) {
	/* Read the whole file into the array */
	fillCompleteArray(interp, gdbm_array, name, 1);
  }
  Tcl_TraceVar2(interp, name, (char*)NULL, 
				GDBM_TRACE_ALL, 
				(Tcl_VarTraceProc*) VariableProc,
				(ClientData) gdbm_array);
  return 1;
}

/*----------------------------------------------------------------------
 * Gdbm_Open_Cmd -- Open/Create a GDBM-File
 *
 *  This procedure is invoked to open/create a GDBM-Databasefile
 *  It expects several parameters:
 *    -writer, 
 *    -reader:     the calling process does only read from file or write
 *    -wrcreat:    create the db-file, if it is not existent
 *    -newdb:      create the db-file, regardless if one exists
 *    -nolock:     do not lock the db-file (should be used with care)
 *    -sync:       db-operations are synchronized to the disk
 *    -block_size: see gdbm-manual
 *    -mode:       file-mode, if the db-file is created (directly
 *                 passed to open(2))
 *    -fatal_func: a tcl-function-name (which gets one argument)
 *    -array:      name of an array-variable
 *    -full_cache: only with parameter -array; immediately cache the
 *                 whole file in the specified array
 *    -no_trailing_null  no null is appended to keys and values either
 *                       by reading or writing
 *    -help        print the usage-message
 *    and the name of the gdbm-file.
 *
 *  The parameters: -writer, -reader, -wrcreat, -newdb, -nolock
 *  represent the following gdbm-defines:
 *  GDBM_WRITER, GDBM_READER, GDBM_WRCREAT, GDBM_NEWDB
 *  For db-writers the following flags could be specified:
 *  GDBM_NOLOCK or GDBM_SYNC.
 *
 * Results:
 *  A unique gdbm-file-descriptor is returned. This is used for further
 *  access to the gdbm-file as a Tcl-Command.
 *
 * Side effects:
 *  A Tcl-Command is created.
 *----------------------------------------------------------------------
 */
int
Gdbm_Open_Cmd(notused, interp, argc, argv)
    ClientData notused;
    Tcl_Interp* interp;
    int argc;
    char* argv[];
{
  int i, index, flags, block_size, mode, already_created, full_cache;
  char* array_name = NULL;
  GDBM_FILE hd_gdbm = NULL;
  Tcl_HashEntry *thetr;
  PGdbm_Array gdbm_array = NULL;
  static int db_number = 0;
  int save_with_null;
  char gdbm_name[32];
  static char usage[] = "gdbm_open ?options? filename\n\
options=-reader, -writer, -wrcreat, -newdb, -nolock -sync\n\
        -block_size <size> -mode <filemode> -fatal_func <command>\n\
        -array <arrayname> -full_cache -no_trailing_null -help\n\
  where <size>      512, ...\n\
        <filemode>  accessmode of file, e.g. [expr 0664]\n\
        <command>   tcl-proc with one parameter called in case of fatal error\n\
        <arrayname> the (optional) name of an persistent array\n\
        for further information see gdbm-man-page or infos on http://www.vogel-nest.de/tcl";
  static char *switches[] = 
  {"-reader", "-writer", "-wrcreat", "-newdb", "-nolock", "-sync", 
   "-block_size", "-mode", "-fatal_func", "-array", "-full_cache", "-no_trailing_null", "-help", (char*) NULL};
  static int gdbm_flags[] = 
  { GDBM_READER, GDBM_WRITER, GDBM_WRCREAT, GDBM_NEWDB, GDBM_NOLOCK, GDBM_SYNC };

  flags = 0;
  block_size = 0;
  mode = 0644;
  full_cache = 0;
  save_with_null = 1;

  if (argc < 2) {
	/* called gdbm_open without arguments: return a list of open handles */
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch searchPtr;
	Tcl_Obj *keys;
  
	keys = Tcl_NewListObj(0, NULL);
	entryPtr = Tcl_FirstHashEntry(&thtbl, &searchPtr);
	while (entryPtr) {
	  Tcl_ListObjAppendElement(NULL, keys,
							   Tcl_NewStringObj(Tcl_GetHashKey(&thtbl, entryPtr), -1));
	  entryPtr = Tcl_NextHashEntry(&searchPtr);
	}
	Tcl_SetObjResult(interp, keys);
	SET_OK;
  }

  /*
   * Parse arguments
   */
  for (i = 1; i < argc-1; i++) {
    index = GetIndexFromArg(argv[i], switches);
    switch (index) {
    case 0: /* -reader */
    case 1: /* -writer */
    case 2: /* -wrcreat */
    case 3: /* -newdb */
    case 4: /* -nolock */
    case 5: /* -sync */
      flags = flags | gdbm_flags[index];
      break;

    case 6: /* -block_size */
      if ((i++ < argc-1) && toNumber(argv[i],&block_size)) {
        // nothing
      }
      else {
        Tcl_AppendResult(interp, "-block_size numbervalue\n\n", usage, NULL);
        return TCL_ERROR;
      }
      break;

    case 7: /* -mode */
      if ((i++ < argc-1) && toNumber(argv[i],&mode)) {
        // nothing
      }
      else {
        Tcl_AppendResult(interp, "-mode numbervalue\n\n", usage, NULL);
        return TCL_ERROR;
      }
      break;

    case 8: /* -fatal_func */
	  /* Don't really check the next argument, it may be the filename
		 to be opened, if the user provided the wrong parameters to gdbm_open.
		 But that doesn't bother much. */
      if (i++ < argc-1) {
        free(gFatalFunc);
        gFatalFunc = malloc(strlen(argv[i])+1);
        strcpy(gFatalFunc, argv[i]);
        gInterp = interp;
      }
      break;

	case 9: /* -array */
	  if (i++ < argc-1) {
		array_name = argv[i];
	  }
	  else {
		Tcl_AppendResult(interp, "-array array_name\n\n", usage, NULL);
		return TCL_ERROR;
	  }
	  break;
	  
	case 10: /* -full_cache */
	  full_cache = 1;
	  break;

	case 11: /* -no_trailing_null */
	  save_with_null = 0;
	  break;
	  
	case 12: /* -help */
	  Tcl_AppendResult(interp, usage, NULL);
	  return TCL_OK;
	  break; /* just to be sure :-) */

    default: /* error */
      Tcl_AppendResult(interp, "error: gdbm_open - No such argument: ",
                       argv[i], "\n", usage, NULL);
      SET_ERROR(GDBM_OPT_ILLEGAL);
    }
  }

  if (full_cache && (array_name == NULL)) {
	Tcl_AppendResult(interp, "error: gdbm_open - cannot specify '-full_cache', ",
					"when not providing an arrayname with '-array'.", NULL);
	SET_ERROR(GDBM_OPT_ILLEGAL);
  }

  /* call to gdbm_open with the collected parameters and filename argv[argc-1] */
  /* check if "file"-argument is "-help"... than print usage */
  if (strcmp(argv[argc-1], "-help") == 0) {
	Tcl_AppendResult(interp, usage, NULL);
	return TCL_OK;
  }

  hd_gdbm = gdbm_open(argv[argc-1], block_size, flags, mode, gdbmFatalFunc);
  if (hd_gdbm == NULL) {
	/* When opening a non-existing-file with -reader "No error" is returned
	   Therefore explicitly set the error to GDBM_FILE_OPEN_ERROR
	   Or called with: gdbm_open -xyz 
	 */
	if (gdbm_errno == 0)
	  gdbm_errno = GDBM_FILE_OPEN_ERROR;
	Tcl_AppendResult(interp, "error: gdbm_open (file: '", argv[argc-1], "') - ", 
					 gdbm_strerror(gdbm_errno), NULL);
    SET_ERROR(gdbm_errno);
  }
  gdbm_array = malloc(sizeof(Gdbm_Array));
  gdbm_array->has_array = 0;
  gdbm_array->full_cache = full_cache;
  gdbm_array->save_with_null = save_with_null;
  gdbm_array->gdbm_file = hd_gdbm;
  if (array_name != NULL) {
	if (strlen(array_name) > 31) {
	  /* remember to close the already opened file in case of this error */
	  gdbm_close(hd_gdbm);
	  free(gdbm_array);
	  Tcl_AppendResult(interp, "error: gdbm_open -array ...\n",
					   "arrayname ist too long (max. 31 chars).", NULL);
	  SET_ERROR(GDBM_OPT_ILLEGAL);
	}
	if ((thetr = Tcl_FindHashEntry(&thtbl, array_name)) != NULL) {
	  /* This array is already registered inside gdbm, remember to close the file */
	  gdbm_close(hd_gdbm);
	  free(gdbm_array);
	  Tcl_AppendResult(interp, "error: gdbm_open -array ...\n",
					   "arrayname '", array_name, "' is already in use.", NULL);
	  SET_ERROR(GDBM_OPT_ILLEGAL);
	}
	gdbm_array->has_array = 1;
	Gdbm_Create_Array(interp, array_name, gdbm_array);
	sprintf(gdbm_name, "%s", array_name);
  }
  else {
	/* create the unique gdbm-descriptor */
	sprintf(gdbm_name, "gdbm%d", db_number);
	db_number++;
  }

  /* put gdbm-file-handle in HashTable: */
  thetr = Tcl_CreateHashEntry(&thtbl, gdbm_name, &already_created);
  if (thetr == NULL) gdbmFatalFunc("Could not create Tcl-Hashentry!");
  Tcl_SetHashValue(thetr, gdbm_array);
  
  /* create the tcl-command with this descriptor, pass the gdbm-handle */
  Tcl_CreateCommand(interp, gdbm_name, 
					(Tcl_CmdProc*) Gdbm_Cmd, 
                    (ClientData) gdbm_array, NULL);
  
  /* Make a copy of this name */
  Tcl_SetResult(interp, gdbm_name, TCL_VOLATILE);
  SET_OK;
}



/*
 *----------------------------------------------------------------------
 * Gdbm_LookupProc --
 *
 *  Find the command operation given a string name.  This is useful
 *  where a group of command operations have the same argument
 *  signature.
 *
 * Results:
 *  If found, a pointer to the procedure (function pointer) is
 *  returned.  Otherwise NULL is returned and an error message
 *  containing a list of the possible commands is returned in
 *  interp->result.
 *----------------------------------------------------------------------
 */
Gdbm_Proc
Gdbm_LookupProc (interp, numSpecs, specArr, argIndex, numArgs, argArr)
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int numSpecs;		    /* Number of specifications in array */
    Gdbm_ProcSpec specArr[];/* Operation specification array */
    int argIndex;	        /* Index of the operation name argument */
    int numArgs;	     	/* Number of arguments in the argument vector.
				             * This includes any prefixed arguments */
    char *argArr[];		    /* Argument vector */
{
    Gdbm_ProcSpec *specPtr;
    char *string;
    register int i;
    register int specIndex;

    if (numArgs <= argIndex) {	/* No operation argument */
	  Tcl_AppendResult(interp, "wrong # args: ", (char *)NULL);
	usage:
	  Tcl_AppendResult(interp, "should be one of...",
					   (char *)NULL);
	  for (specIndex = 0; specIndex < numSpecs; specIndex++) {
		Tcl_AppendResult(interp, "\n  ", (char *)NULL);
		for (i = 0; i < argIndex; i++) {
		  Tcl_AppendResult(interp, argArr[i], " ", (char *)NULL);
		}
		specPtr = specArr + specIndex;
		Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage,
						 (char *)NULL);
	  }
	  return NULL;
    }
    string = argArr[argIndex];
    specIndex = GetIndexFromSpec(string, numSpecs, specArr);
	
	if (specIndex == -1) {	/* Can't find operation, display help */
	  Tcl_AppendResult(interp, "bad", (char *)NULL);
	  if (argIndex > 2) {
		Tcl_AppendResult(interp, " ", argArr[argIndex - 1], (char *)NULL);
	  }
	  Tcl_AppendResult(interp, " operation \"", string, "\": ", (char *)NULL);
	  goto usage;
    }
    specPtr = specArr + specIndex;
    if ((numArgs < specPtr->minArgs) || ((specPtr->maxArgs > 0) &&
										 (numArgs > specPtr->maxArgs))) {
	  Tcl_AppendResult(interp, "wrong # args: should be \"", (char *)NULL);
	  for (i = 0; i < argIndex; i++) {
		Tcl_AppendResult(interp, argArr[i], " ", (char *)NULL);
	  }
	  Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage, "\"",
					   (char *)NULL);
	  return NULL;
    }
    return (specPtr->proc);
}


/*
 *----------------------------------------------------------------------
 * Gdbm_Keys -- implementation of: $handle keys
 *
 * Results:
 *  a Tcl-list of all(!) keys of gdbm-file. Take care for large files.
 *----------------------------------------------------------------------
 */
static int
Gdbm_Keys(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv; 
{
  datum result, key;
  char* res;
  
  result = gdbm_firstkey(gdbm->gdbm_file);
  while (result.dptr != NULL) {
    res = copyResultNull(result.dptr, result.dsize, 0);
	Tcl_AppendElement(interp, res);
    if (res!=result.dptr)
      free(res);
	key = result;
	result = gdbm_nextkey(gdbm->gdbm_file, key);
	free(key.dptr);
  }
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Count -- count all elements of gdbm-file
 * 
 * Results: 
 *  number of elements in gdbm-file
 *----------------------------------------------------------------------
 */
static int
Gdbm_Count(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv; 
{
  int count;
  datum key1, key2;
  char number[13];

  count = 0;
  key1 = gdbm_firstkey(gdbm->gdbm_file);
  while (key1.dptr != NULL) {
	count++;
	key2 = key1;
	key1 = gdbm_nextkey(gdbm->gdbm_file, key2);
	free(key2.dptr);
  }
  sprintf(number, "%d", count);
  Tcl_SetResult(interp, number, TCL_VOLATILE);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Maxkey -- get the maximum (integer)-key of gdbm-file
 *  if you have integer-numbers as keys, you can select the largest
 *  one (needed to generate primary-key-like keys)
 *   
 * Results: 
 *  Maximum value of keys (or 0 if keys are strings ...)
 *----------------------------------------------------------------------
 */
static int
Gdbm_Maxkey(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv; 
{
  int count, tmp;
  datum result, key;
  char number[13];
  
  count = 0;
  result = gdbm_firstkey(gdbm->gdbm_file);
  while (result.dptr != NULL) {
	if (toNumber(result.dptr,&tmp)) {
	  if (tmp > count)
		count = tmp;
	}
	key = result;
	result = gdbm_nextkey(gdbm->gdbm_file, key);
	free(key.dptr);
  }
  sprintf(number, "%d", count);
  Tcl_SetResult(interp, number, TCL_VOLATILE);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Store -- like gdbm_store
 * 
 * Results: 
 *
 * Side effects:
 *  Stores given data into gdbm-file
 *----------------------------------------------------------------------
 */
static int
Gdbm_Store(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv; 
{
  datum key, content;
  int insert_replace_flag = GDBM_INSERT, result;
  
  if (strcmp("store", argv[2])) {
	Tcl_AppendResult(interp, "error: Wrong arguments. Must be\n\
gdbmN [-insert|-replace] store <key> <data>",
					 NULL);
	SET_ERROR(GDBM_OPT_ILLEGAL);
  }
  if (!strcmp("-replace", argv[1])) {
	insert_replace_flag = GDBM_REPLACE;
  }
  /* argv[3|4] are null-terminated strings */
  key.dptr = argv[3];
  content.dptr = argv[4];
  SET_LENGTH(gdbm, key);
  SET_LENGTH(gdbm, content);
  
  /* 
	 gdbm_store returns:
     -1 if called from reader
	 1 if called with GDBM_INSERT and key is in database
	 0 otherwise 
	 insert-replace-flag is one of GDBM_INSERT==0 or GDBM_REPLACE==1
  */
  result = gdbm_store(gdbm->gdbm_file, key, content, insert_replace_flag);
  switch (result) {
  case -1:
	Tcl_AppendResult(interp, "error: Item not inserted/replaced. gdbm opened as reader.", NULL);
	SET_ERROR(GDBM_READER_CANT_STORE);
	break;
  case 1:
	/* let this be an error, because of compatibility */
	Tcl_AppendResult(interp, "error: Item not inserted, already exists. Use '-replace store ", key.dptr, " ...'", NULL);
	/* there is no error-code given from gdbm for that, so use a less confusing error-code :-) */
    SET_ERROR(GDBM_ILLEGAL_DATA);
	break;
  default:
	/* case 0 - everything is o.k. trickle through */
	/* remember to update the array, if there is one attached */
	return Gdbm_ArrayTrace_Proc(gdbm, interp, argv[0], 
								 key.dptr, content.dptr, GDBM_SETVAR);
  }
}


/*----------------------------------------------------------------------
 * Gdbm_Reorganize -- like gdbm_reorganize
 * 
 * Side effects:
 *  gdbm-file is reorganized (therefore a new file is generated
 *  and all data is written to the new file)
 *----------------------------------------------------------------------
 */
static int
Gdbm_Reorganize(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  Tcl_HashEntry *thetr;
  PGdbm_Array hd_gdbm;

  if (gdbm_reorganize(gdbm->gdbm_file)) {
	Tcl_AppendResult(interp, "error: gdbmN reorganize - ",
					 gdbm_strerror(gdbm_errno), NULL);
	SET_ERROR(gdbm_errno);
  } else {
	/* get according gdbm-file-handle from hash-table */
	thetr = Tcl_FindHashEntry(&thtbl, argv[0]);
	if (thetr == NULL) gdbmFatalFunc("Could not find Tcl-Hashentry!");
	hd_gdbm =  (PGdbm_Array) Tcl_GetHashValue(thetr);
	/* the gdbm-structure has changed we had to create the
	   command again, to let tcl have the new settings
	   (especially the new gdbm-handle for clientdata)
	   Tcl_DeleteCommand(interp, argv[0]) must not be called,
	   this is done by Tcl_CreateCommand */
	Tcl_CreateCommand(interp, argv[0], 
					  (Tcl_CmdProc*) Gdbm_Cmd, 
					  (ClientData) hd_gdbm, NULL);
	Tcl_SetResult(interp, "", TCL_STATIC);
	SET_OK;
  }
}


/*----------------------------------------------------------------------
 * Gdbm_Sync -- like gdbm_sync
 *----------------------------------------------------------------------
 */
static int
Gdbm_Sync(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  gdbm_sync(gdbm->gdbm_file);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Firstkey -- like gdbm_firstkey
 *----------------------------------------------------------------------
 */
static int
Gdbm_Firstkey(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  datum result;
  char* res;
  result = gdbm_firstkey(gdbm->gdbm_file);
  if (result.dptr == NULL) {
	/* no result found return an empty string */
	Tcl_SetResult(interp, "", TCL_STATIC);
  } else {
	res = copyResultNull(result.dptr, result.dsize, 1);
	Tcl_SetResult(interp, res, TCL_VOLATILE);
	free(res);
  }
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Nextkey -- like gdbm_nextkey
 *----------------------------------------------------------------------
 */
static int
Gdbm_Nextkey(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  datum key, result;
  char* res;

  key.dptr = argv[2];
  SET_LENGTH(gdbm, key);

  result = gdbm_nextkey(gdbm->gdbm_file, key);
  if (result.dptr == NULL) {
	/* no result found return an empty string */
	Tcl_SetResult(interp, "", TCL_STATIC);
  } else {
	res = copyResultNull(result.dptr, result.dsize, 1);
	Tcl_SetResult(interp, res, TCL_VOLATILE);
	free(res);
	/* free(result.dptr); */
  }
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Delete -- like gdbm_delete
 *----------------------------------------------------------------------
 */
static int
Gdbm_Delete(gdbm, interp, argc, argv)
    PGdbm_Array gdbm;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
  datum key;
  key.dptr = argv[2];
  SET_LENGTH(gdbm, key);
  /* retcode == -1 --> no such key or requester is reader
   *         ==  0 --> successful delete
   */
  if (gdbm_delete(gdbm->gdbm_file, key) != 0) {
	Tcl_AppendResult(interp, "error: gdbmN delete - ",
					 gdbm_strerror(gdbm_errno),
					 NULL);
	SET_ERROR(gdbm_errno);
  }
  /* SET_OK; */
  return Gdbm_ArrayTrace_Proc(gdbm, interp, argv[0], 
							   key.dptr, (char*)NULL, GDBM_DELVAR);
}


/*----------------------------------------------------------------------
 * Gdbm_Fetch -- like gdbm_fetch
 *----------------------------------------------------------------------
 */
static int
Gdbm_Fetch(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  datum key, result;
  char* res;
  
  key.dptr = argv[2];
  SET_LENGTH(gdbm, key);
  result = gdbm_fetch(gdbm->gdbm_file, key);
  if (result.dptr == NULL) {
	Tcl_AppendResult(interp, "error: gdbm_fetch - ",
					 gdbm_strerror(gdbm_errno),
					 NULL);
	SET_ERROR(gdbm_errno);
  }
  res = copyResultNull(result.dptr, result.dsize, 1);
  Tcl_SetResult(interp, res, TCL_VOLATILE);
  free(res);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Exists -- like gdbm_exists
 *----------------------------------------------------------------------
 */
static int 
Gdbm_Exists(gdbm, interp, argc, argv) 
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  datum key;
  key.dptr = argv[2];
  SET_LENGTH(gdbm, key);
  Tcl_SetResult(interp, 
				(gdbm_exists(gdbm->gdbm_file, key) ? "1" : "0"), 
				TCL_STATIC);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Close -- like gdbm_close
 *----------------------------------------------------------------------
 */
static int
Gdbm_Close(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  Tcl_HashEntry *thetr;
  
  if (gdbm->gdbm_file != NULL) 
	gdbm_close(gdbm->gdbm_file);
  Tcl_DeleteCommand(interp, argv[0]);
  
  Gdbm_ArrayTrace_Proc(gdbm, interp, argv[0], 
						(char*)NULL, (char*)NULL, GDBM_DELALL);
  /* now that the command is deleted we free memory,
	 gdbm is freed in gdbm_close */
  thetr = Tcl_FindHashEntry(&thtbl, argv[0]);
  if (thetr != NULL) Tcl_DeleteHashEntry(thetr);
  free(gdbm);

  Tcl_SetResult(interp, "", TCL_STATIC);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Attach -- attach an array to an already opened gdbm-file
 * 
 * Side effects:
 *  the array (which has the same name as the gdbm-handle) is attached
 *  to the gdbm-file and therefore will stay in sync with the gdbm-file.
 *----------------------------------------------------------------------
 */
static int
Gdbm_Attach(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  if (gdbm->has_array) {
	Tcl_AppendResult(interp, "error: Gdbm_Attach - '",
					 argv[0], "' already has an array attached.\n",
					 NULL);
	SET_ERROR(GDBM_OPT_ILLEGAL);  
  }
  else {
	gdbm->has_array = 1;
	gdbm->full_cache = 0;
	if ((argc == 3)
		&& (!strcmp(argv[2], "-full_cache"))) {
	  gdbm->full_cache = 1;
	}
	Gdbm_Create_Array(interp, argv[0], gdbm);
	SET_OK;
  }
}


/*----------------------------------------------------------------------
 * Gdbm_Detach -- remove the array-traces from the opened gdbm-file
 * 
 * Side effects:
 *  Further settings of the attached arrays will not be synchronized
 *  into the gdbm-file
 *----------------------------------------------------------------------
 */
static int
Gdbm_Detach(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  if (!gdbm->has_array) {
	Tcl_AppendResult(interp, "error: Gdbm_Detach - '",
					 argv[0], "' there is no array to detach.\n",
					 NULL);
	SET_ERROR(GDBM_OPT_ILLEGAL);  
  }
  else {
	Gdbm_ArrayTrace_Proc(gdbm, interp, argv[0], 
						  (char*)NULL, (char*)NULL, GDBM_DELALL);
	gdbm->has_array = 0;
	gdbm->full_cache = 0;
	SET_OK;
  }
}


/*----------------------------------------------------------------------
 * Gdbm_Has_Array -- check if array is attached to gdbm-file
 * 
 * Results: 
 *  true (gdbm-file has array attached); false (no array)
 *  
 *----------------------------------------------------------------------
 */
static int
Gdbm_Has_Array(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  Tcl_SetResult(interp, (gdbm->has_array ? "1" : "0"), TCL_STATIC);
  SET_OK;
}


/*----------------------------------------------------------------------
 * Gdbm_Full_Cache -- check if array is already fully cached
 *  Normally fully caching of data is not needed (until someone calls
 *  e.g. "array names" on an attached array. So fully caching a gdbm-file
 *  is avoided as best as can. Dumping the whole gdbm-file into memory
 *  is especially problematic for large gdbm-files
 *  
 * Results: 
 *  true if gdbm-file is fully cached.
 *----------------------------------------------------------------------
 */
static int
Gdbm_Full_Cache(gdbm, interp, argc, argv)
	 PGdbm_Array gdbm;
	 Tcl_Interp *interp;
	 int argc;
	 char **argv;
{
  Tcl_SetResult(interp, (gdbm->full_cache ? "1" : "0"), TCL_STATIC);
  SET_OK;
}

/* 
  provided standard-procedures:
  mapping of "Tcl"-parameters to gdbm-handle to C-functions, with their 
  min-/max- number of parameters
*/
static Gdbm_ProcSpec procSpecs[] =
{
	{"keys",         (Gdbm_Proc) Gdbm_Keys,       2, 2, "",},
	{"count",        (Gdbm_Proc) Gdbm_Count,      2, 2, "",},
	{"maxkey",       (Gdbm_Proc) Gdbm_Maxkey,     2, 2, "",},
	{"-insert",      (Gdbm_Proc) Gdbm_Store,      5, 5, "store <key> <value>",},
	{"-replace",     (Gdbm_Proc) Gdbm_Store,      5, 5, "store <key> <value>",},
	{"reorganize",   (Gdbm_Proc) Gdbm_Reorganize, 2, 2, "",},
	{"sync",         (Gdbm_Proc) Gdbm_Sync,       2, 2, "",},
	{"firstkey",     (Gdbm_Proc) Gdbm_Firstkey,   2, 2, "",},
	{"nextkey",      (Gdbm_Proc) Gdbm_Nextkey,    3, 3, "",},
	{"delete",       (Gdbm_Proc) Gdbm_Delete,     3, 3, "key",},
	{"fetch",        (Gdbm_Proc) Gdbm_Fetch,      3, 3, "key",},
	{"exists",       (Gdbm_Proc) Gdbm_Exists,     3, 3, "key",},
	{"close",        (Gdbm_Proc) Gdbm_Close,      2, 2, "",},
	{"attach_array", (Gdbm_Proc) Gdbm_Attach,     2, 3, "[-full_cache]",},
	{"detach_array", (Gdbm_Proc) Gdbm_Detach,     2, 2, "", },
	/* supposed to be internal */
	{"has_array",    (Gdbm_Proc) Gdbm_Has_Array,  2, 2, "", },
	/* supposed to be internal */
	{"full_cache",   (Gdbm_Proc) Gdbm_Full_Cache, 2, 2, "", },
};
static int numSpecs = sizeof(procSpecs)/sizeof(Gdbm_ProcSpec);

/*----------------------------------------------------------------------
 * Gdbm_Cmd -- Implementation of all related gdbm-commands
 *
 * The command created with Gdbm_Open_Cmd has several options:
 * (<gdbm0> is the command created with Gdbm_Open_Cmd)
 *   close:       (e.g.:  gdbm0 close)
 *                close the associated gdbm-file
 *   fetch:       (e.g.:  gdbm0 fetch key)
 *                returns the value stored in gdbm-file with key "key"
 *   delete:      (e.g.:  gdbm0 delete key)
 *                deletes the key-value-pair with key "key"
 *   store:       (either:  gdbm0 -insert store key value
 *                     or:  gdbm0 -replace store key value)
 *                Inserts/Replaces the given key-value-pair
 *                If key already exists and -insert is specified an error
 *                is thrown. -replace could be called without error even
 *                if no key "key" is existent.
 *   exists:      (e.g.: gdbm0 exists key)
 *                checks if given key already exists: returns 1 if it exists
 *                0 if it doesn't
 *   firstkey:    (e.g.: gdbm0 firstkey)
 *                returns the first key. Used for iteration through the
 *                gdbm-file. 
 *   nextkey:     (e.g.: gdbm0 nextkey key)
 *                return the nextkey to "key", which is retrieved with
 *                first- or nextkey
 *   reorganize:  (e.g.: gdbm0 reorganize)
 *                when you had deleted lots of entries, this would shrink
 *                the database-file
 *   sync:        (e.g.: gdbm0 sync)
 *                manually sync memory-data with file
 *
 *  These commands all directy match to corresponding gdbm-commands (see
 *  gdbm(3)).
 *  There are more commands, which do not correspond to any gdbm-commands,
 *  but are provided for simplicity:
 * 
 *   count:        (e.g.: gdbm0 count)
 *                 returns number of entries in gdbm-file
 *   maxkey:       (e.g.: gdbm0 maxkey)
 *                 if you had only integer-keys and you need to create
 *                 a unique new-key, this one searches for the maximal
 *                 existing key and returns it.
 *   attach_array: (e.g.: gdbm0 attach_array  or gdbm0 attach_array -full_cache)
 *                 creates an array named gdbm0 which will contain the
 *                 key-value-pairs from gdbm0
 *   detach_array: (e.g.: gdbm0 detach_array)
 *                 removes the array gdbm0 from the corresponding gdbm-file.
 *   keys:         (e.g.: gdbm0 keys)
 *                 like [array keys xxx] return all(!) key-values of gdbm-file.
 *
 *  In case of an error an Tclerror is returned with an errormessage and
 *  the global gdbm_errno-variable is provided in Tcl-Variable GDBM_ERRNO.
 * 
 * Results:
 *  Depends on option to command, see description above.
 *----------------------------------------------------------------------
 */
int
Gdbm_Cmd(cd_gdbm, interp, argc, argv)
	 ClientData cd_gdbm;
	 Tcl_Interp* interp;
	 int argc;
	 char* argv[];
{
  Gdbm_Proc proc;
	
  proc = Gdbm_LookupProc(interp, numSpecs, procSpecs, 1, argc, argv);
  if (proc == NULL) {
	return TCL_ERROR;
  }
  return ((*proc) ((PGdbm_Array)cd_gdbm, interp, argc, argv));
}


/*----------------------------------------------------------------------
 * Tgdbm_Init -- Initialization of Tgdbm
 *
 * Create the gdbm_open and gdbm_strerror-command and provide
 * the used GDBM_VERSION in Tcl-Variable GDBM_VERSION
 *
 * #define DllEntryPoint DllMain 
 *----------------------------------------------------------------------
 */
EXTERN int Tgdbm_Init(Tcl_Interp *interp)
{
    int i;
    char sErrnum[5], sErrMsg[128];
    int iErrors[] = {   
        GDBM_NO_ERROR, GDBM_MALLOC_ERROR, GDBM_BLOCK_SIZE_ERROR, 
        GDBM_FILE_OPEN_ERROR, GDBM_FILE_WRITE_ERROR, GDBM_FILE_SEEK_ERROR, 
        GDBM_FILE_READ_ERROR, GDBM_BAD_MAGIC_NUMBER, GDBM_EMPTY_DATABASE, 
        GDBM_CANT_BE_READER, GDBM_CANT_BE_WRITER, GDBM_READER_CANT_DELETE, 
        GDBM_READER_CANT_STORE, GDBM_READER_CANT_REORGANIZE, 
#if GDBM_VERSION_MAJOR > 1 || \
    (GDBM_VERSION_MAJOR == 1 && GDBM_VERSION_MINOR > 12)
        GDBM_UNKNOWN_ERROR,
#else
        GDBM_UNKNOWN_UPDATE,
#endif
        GDBM_ITEM_NOT_FOUND, GDBM_REORGANIZE_FAILED, GDBM_CANNOT_REPLACE, 
        GDBM_ILLEGAL_DATA, GDBM_OPT_ALREADY_SET, GDBM_OPT_ILLEGAL};
    char *sErrors[] = {
        "GDBM_NO_ERROR","GDBM_MALLOC_ERROR","GDBM_BLOCK_SIZE_ERROR", 
        "GDBM_FILE_OPEN_ERROR","GDBM_FILE_WRITE_ERROR","GDBM_FILE_SEEK_ERROR", 
        "GDBM_FILE_READ_ERROR","GDBM_BAD_MAGIC_NUMBER","GDBM_EMPTY_DATABASE", 
        "GDBM_CANT_BE_READER","GDBM_CANT_BE_WRITER","GDBM_READER_CANT_DELETE", 
        "GDBM_READER_CANT_STORE","GDBM_READER_CANT_REORGANIZE","GDBM_UNKNOWN_ERROR",
        "GDBM_ITEM_NOT_FOUND","GDBM_REORGANIZE_FAILED","GDBM_CANNOT_REPLACE",
        "GDBM_ILLEGAL_DATA","GDBM_OPT_ALREADY_SET","GDBM_OPT_ILLEGAL"};

    
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
      return TCL_ERROR;
	}

    /* initialize hash-table for gdbm-file-descriptors */
    Tcl_InitHashTable(&thtbl, TCL_STRING_KEYS);

    Tcl_SetVar(interp, "GDBM_VERSION", gdbm_version, TCL_GLOBAL_ONLY);
    for (i = 0; i < sizeof(iErrors)/sizeof(int); i++) {
      sprintf(sErrnum, "%d", iErrors[i]);
      sprintf(sErrMsg, "gdbm_error(%s)", sErrors[i]);
      Tcl_SetVar(interp, sErrMsg, sErrnum, TCL_GLOBAL_ONLY);
    }
    Tcl_CreateCommand(interp, "gdbm_open", (Tcl_CmdProc*) Gdbm_Open_Cmd, NULL, NULL);
    Tcl_CreateCommand(interp, "gdbm_strerror", (Tcl_CmdProc*) Gdbm_Error_Cmd, NULL, NULL);
    Tcl_PkgProvide(interp, "tgdbm", TGDBM_VERSION);
    
    return TCL_OK;
}
