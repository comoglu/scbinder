/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Mustafa Çomoğlu
 *
 * Declarations of the entry points of the verbatim binder_ew core
 * (grid.c / bind.c / hyp.c / assess.c) that are not declared in the
 * headers shipped with the core.  Mirrors the prototypes used by the
 * original driver binder_ew.c.
 *
 * Earthworm is distributed under the MIT license.
 * binder_ew was written by Carl Johnson.
 ***************************************************************************/

#ifndef SCBINDER_BINDERGLUE_H
#define SCBINDER_BINDERGLUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bind.h"

/* Core entry points (see binder_ew.c prototypes) */
int  assess_com( void );
int  bind_com( void );
void bind_init( void );
int  bind_pick( PICK * );
int  hyp_com( void );
void hyp_free( void );
int  grid_com( void );
void grid_init( void );
void grid_free( void );

/* Symbols the core expects the driver to provide (defined in ewglue.cpp) */
extern char EventIdFile[256];
void bndr_quake2k( long quake );
void bndr_link( long quake, long pick );

/* Callback hooks so the C core can notify the C++ application */
typedef void (*scbinder_quake_cb)( long quake );
typedef void (*scbinder_link_cb)( long quake, long pick );
void scbinder_set_callbacks( scbinder_quake_cb qcb, scbinder_link_cb lcb );

#ifdef __cplusplus
}
#endif

#endif
