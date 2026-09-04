/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Mustafa Çomoğlu
 *
 * The algorithm core under core/ and ewlib/ is verbatim Earthworm code,
 * MIT-licensed; see NOTICE and LICENSE.earthworm.
 ***************************************************************************/

#include "app.h"

int main(int argc, char **argv) {
	Seiscomp::Applications::BinderApp app(argc, argv);
	return app();
}
