/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * Replacements for the Earthworm runtime services the binder core links
 * against: logit() logging, GetInst() installation-id lookup, the
 * EventIdFile global and the bndr_quake2k()/bndr_link() output callbacks
 * that binder_ew.c used to turn into TYPE_QUAKE2K / TYPE_LINK messages.
 ***************************************************************************/

#define SEISCOMP_COMPONENT SCBinder

#include <seiscomp/logging/log.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "binderglue.h"

static scbinder_quake_cb g_quakeCB = nullptr;
static scbinder_link_cb  g_linkCB  = nullptr;

extern "C" {

char EventIdFile[256] = "quake_id.d";

void scbinder_set_callbacks( scbinder_quake_cb qcb, scbinder_link_cb lcb ) {
	g_quakeCB = qcb;
	g_linkCB  = lcb;
}

void bndr_quake2k( long quake ) {
	if ( g_quakeCB ) g_quakeCB(quake);
}

void bndr_link( long quake, long pick ) {
	if ( g_linkCB ) g_linkCB(quake, pick);
}

/* Earthworm logit(): forward to the SeisComP logging system.
 * Flag characters: 'e' -> also stderr (we map to warning level),
 * 't' -> timestamp (implicit in SeisComP), 'd' -> debug. */
void logit( const char *flags, const char *format, ... ) {
	char buf[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	// Strip leading/trailing newlines: SeisComP logging is line-based
	char *msg = buf;
	while ( *msg == '\n' ) ++msg;
	size_t len = strlen(msg);
	while ( len > 0 && msg[len-1] == '\n' ) msg[--len] = '\0';
	if ( !len ) return;

	if ( strchr(flags, 'e') )
		SEISCOMP_WARNING("%s", msg);
	else if ( strchr(flags, 'd') )
		SEISCOMP_DEBUG("%s", msg);
	else
		SEISCOMP_INFO("%s", msg);
}

void logit_init( const char *, short, int, int ) {}

int is_logit_initialized() { return 1; }

/* All picks fed into the core carry instid 0, so every installation name
 * (including INST_WILDCARD) resolves to id 0. Installation-specific
 * grid_wt/bind weights degenerate to global ones. */
int GetInst( char *, unsigned char *id ) {
	*id = 0;
	return 0;
}

}
