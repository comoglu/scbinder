/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 ***************************************************************************/

#include "app.h"

int main(int argc, char **argv) {
	Seiscomp::Applications::BinderApp app(argc, argv);
	return app();
}
