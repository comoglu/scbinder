/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * Travel-time dispatcher. The binder core computes travel times through
 * tlay's narrow interface (t_com, t_lay, t_phase, t_region, getPoverS).
 * The verbatim tlay.c is compiled with those five symbols renamed to
 * tlay_* (see CMakeLists.txt); this file exports the original names and
 * routes each call either to the flat-earth tlay model (default,
 * behavior-identical to Earthworm) or to a SeisComP travel-time table
 * (libtau iasp91/ak135, ...), selected with a new command in the binder
 * command file:
 *
 *    velmod tlay          # default: Earthworm layered model (lay/psratio)
 *    velmod iasp91        # libtau iasp91
 *    velmod ak135         # libtau ak135
 *    velmod <iface>:<model>   # any SeisComP TravelTimeTableInterface
 ***************************************************************************/

#define SEISCOMP_COMPONENT SCBinder

#include <seiscomp/logging/log.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/seismology/ttt.h>

#include <cmath>
#include <cstring>
#include <string>

extern "C" {
#include "kom.h"
#include "tlay.h"
}

// Original tlay implementations (renamed at compile time)
extern "C" {
int    tlay_t_com( void );
double tlay_getPoverS( void );
int    tlay_t_region( double, double, TPHASE * );
double tlay_t_phase( int, double, double, double *, double * );
double tlay_t_lay( double, double, double *, double * );
}

using namespace Seiscomp;

namespace {

TravelTimeTableInterfacePtr s_ttt;      // null -> tlay backend
std::string                 s_model;

constexpr double NO_TIME = 1E30;        // huge residual -> never associates

double depthKm(double z) {
	// The core hands depths around with both sign conventions
	// (CART.z is down-negative, QUAKE.z down-positive).
	double d = std::fabs(z);
	return d > 799. ? 799. : d;
}

// Travel time along the equator: (0,0) -> (0,delta)
const TravelTime *find(TravelTimeList *list, const char *phase) {
	for ( auto &tt : *list )
		if ( tt.phase == phase ) return &tt;
	return nullptr;
}

double tableTime(int iph, double r, double z, double *dtdr, double *dtdz) {
	static const char *pfall[] = { "P", "Pn", "Pg", "Pb", nullptr };
	static const char *sfall[] = { "S", "Sn", "Sg", "Sb", nullptr };

	double delta = Math::Geo::km2deg(r);
	double dep   = depthKm(z);

	*dtdr = 0.;
	*dtdz = 0.;

	TravelTimeList *list = nullptr;
	try { list = s_ttt->compute(0., 0., dep, 0., delta, 0., 0); }
	catch ( ... ) { return NO_TIME; }
	if ( !list ) return NO_TIME;

	const char *want = (iph >= 0 && iph < 6) ? Phs[iph] : "P";
	const TravelTime *tt = find(list, want);

	if ( !tt ) {
		// Requested phase does not exist at this distance/depth; fall
		// back to the first arrival of the same wave type.
		const char **fall = (iph == 1 || iph == 3 || iph == 5) ? sfall : pfall;
		for ( const char **p = fall; *p && !tt; ++p ) tt = find(list, *p);
	}

	double result = NO_TIME;
	if ( tt ) {
		result = tt->time;
		*dtdr  = tt->dtdd / Math::Geo::deg2km(1.);
		*dtdz  = tt->dtdh;
	}

	delete list;
	return result;
}

}


extern "C" {

int t_com( void ) {
	if ( k_its(const_cast<char*>("velmod")) ) {
		char *str = k_str();
		if ( !str ) return 1;

		std::string arg(str);
		if ( arg == "tlay" ) {
			s_ttt = nullptr;
			s_model.clear();
			SEISCOMP_INFO("velmod: Earthworm tlay layered model");
			return 1;
		}

		std::string iface = "libtau", model = arg;
		size_t sep = arg.find(':');
		if ( sep != std::string::npos ) {
			iface = arg.substr(0, sep);
			model = arg.substr(sep+1);
		}

		s_ttt = TravelTimeTableInterface::Create(iface.c_str());
		if ( !s_ttt || !s_ttt->setModel(model) ) {
			SEISCOMP_ERROR("velmod: cannot initialize travel time table "
			               "%s/%s - keeping tlay backend", iface.c_str(),
			               model.c_str());
			s_ttt = nullptr;
			return 1;
		}
		s_model = model;
		SEISCOMP_INFO("velmod: travel time table %s/%s",
		              iface.c_str(), model.c_str());
		return 1;
	}

	return tlay_t_com();
}


double getPoverS( void ) {
	return tlay_getPoverS();
}


double t_lay( double r, double z, double *dtdr, double *dtdz ) {
	if ( !s_ttt ) return tlay_t_lay(r, z, dtdr, dtdz);
	return tableTime(0, r, z, dtdr, dtdz);
}


double t_phase( int iph, double r, double z, double *dtdr, double *dtdz ) {
	if ( !s_ttt ) return tlay_t_phase(iph, r, z, dtdr, dtdz);
	return tableTime(iph, r, z, dtdr, dtdz);
}


int t_region( double r, double z, TPHASE *treg ) {
	if ( !s_ttt ) return tlay_t_region(r, z, treg);

	double delta = Math::Geo::km2deg(r);
	double dep   = depthKm(z);

	TravelTimeList *list = nullptr;
	try { list = s_ttt->compute(0., 0., dep, 0., delta, 0., 0); }
	catch ( ... ) { return 0; }
	if ( !list ) return 0;

	// First arrival of each of the six binder phases present in the list.
	// Callers provide TPHASE[10].
	int n = 0;
	for ( int iph = 0; iph < 6 && n < 10; ++iph ) {
		const TravelTime *tt = find(list, Phs[iph]);
		if ( !tt ) continue;
		treg[n].phase = iph;
		treg[n].t     = tt->time;
		treg[n].dtdr  = tt->dtdd / Math::Geo::deg2km(1.);
		treg[n].dtdz  = tt->dtdh;
		++n;
	}

	delete list;
	return n;
}

}
