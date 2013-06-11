/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is the MonetDB Database System.
 * 
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2013 MonetDB B.V.
 * All Rights Reserved.
*/

/*
 *  A. de Rijke
 * The UUID module
 * The UUID module contains a wrapper for all function in
 * libuuid.
 */

#include "monetdb_config.h"
#include "uuid.h"
#include "mal.h"
#include "mal_exception.h"
#include "muuid.h"

static str
uuid_GenerateUuid(str *retval) {
  str d;
  char * s;

  s = generateUUID();
  d = GDKstrdup(s);

  if (d == NULL)
    throw(MAL, "uuid.generateUuid", "Allocation failed");

  *retval = d;
  free(s);
  return MAL_SUCCEED;
}

str
UUIDgenerateUuid(str *retval) {
  return uuid_GenerateUuid(retval);
}
