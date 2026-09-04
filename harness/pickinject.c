/* pickinject.c — inject TYPE_PICK_SCNL text messages into an EW ring.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Mustafa Çomoğlu
 *
 * Usage: pickinject <RING_NAME> <pickfile>
 *   Each line of pickfile is one TYPE_PICK_SCNL message
 *   (e.g. produced by sc2ewpick.py). Lines are sent in order with a
 *   small pause so binder_ew's buffer thread keeps up.
 *
 * Build (see run_binder_ew.sh): links the Earthworm transport objects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <earthworm.h>
#include <transport.h>

int main( int argc, char **argv )
{
   SHM_INFO region;
   MSG_LOGO logo;
   long key;
   char line[512];
   int n = 0;
   FILE *fp;

   if ( argc != 3 ) {
      fprintf( stderr, "Usage: pickinject <RING_NAME> <pickfile>\n" );
      return 1;
   }

   logit_init( "pickinject", 0, 256, 0 );

   key = GetKey( argv[1] );
   if ( key == -1 ) {
      fprintf( stderr, "pickinject: unknown ring <%s>\n", argv[1] );
      return 1;
   }

   if ( GetLocalInst( &logo.instid ) != 0 ) logo.instid = 255;
   logo.mod = 1;
   if ( GetType( "TYPE_PICK_SCNL", &logo.type ) != 0 ) {
      fprintf( stderr, "pickinject: TYPE_PICK_SCNL unknown\n" );
      return 1;
   }

   fp = fopen( argv[2], "r" );
   if ( !fp ) { perror( argv[2] ); return 1; }

   tport_attach( &region, key );

   while ( fgets( line, sizeof(line), fp ) ) {
      size_t len = strlen( line );
      if ( len < 10 ) continue;
      if ( tport_putmsg( &region, &logo, (long)len, line ) != PUT_OK )
         fprintf( stderr, "pickinject: putmsg failed: %s", line );
      else
         ++n;
      sleep_ew( 20 );   /* ms; let the buffer thread drain */
   }

   fclose( fp );
   tport_detach( &region );
   printf( "pickinject: sent %d picks to %s\n", n, argv[1] );
   return 0;
}
