/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Mustafa Çomoğlu
 *
 * The application layer replaces binder_ew.c: SeisComP messaging replaces
 * the transport rings, Inventory replaces the site file, DataModel::Origin
 * replaces TYPE_QUAKE2K/TYPE_LINK. The algorithm core (grid.c, bind.c,
 * hyp.c, ...) is verbatim Earthworm code.
 ***************************************************************************/

#define SEISCOMP_COMPONENT SCBinder

#include "app.h"

#include <seiscomp/client/inventory.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/arrival.h>
#include <seiscomp/datamodel/comment.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/network.h>
#include <seiscomp/datamodel/notifier.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/originquality.h>
#include <seiscomp/datamodel/sensorlocation.h>
#include <seiscomp/datamodel/station.h>
#include <seiscomp/datamodel/stream.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/logging/log.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/system/environment.h>

#include <algorithm>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "binderglue.h"

extern "C" {
#include "kom.h"
#include "site.h"
#include "tlay.h"
#include "chron3.h"
}

using namespace Seiscomp;
using namespace Seiscomp::DataModel;

namespace Seiscomp {
namespace Applications {

BinderApp *BinderApp::_instance = nullptr;

namespace {

double toEpoch(const Core::Time &t) {
	return static_cast<double>(t.seconds())
	     + static_cast<double>(t.microseconds()) * 1E-6;
}

void quakeCallback(long quake) {
	if ( BinderApp::Instance() ) BinderApp::Instance()->onQuake(quake);
}

void linkCallback(long quake, long pick) {
	if ( BinderApp::Instance() ) BinderApp::Instance()->onLink(quake, pick);
}

// Earthworm convention: empty location code is "--"
const char *ewLoc(const std::string &loc) {
	return loc.empty() ? "--" : loc.c_str();
}

}


const char *BinderApp::version() {
	return "scbinder 1.0 (binder_ew 1.0.43 core) - "
	       "SeisComP port by Mustafa Comoglu, AGPL-3.0-or-later; "
	       "binder_ew core (c) Seismic Software, MIT";
}


BinderApp::BinderApp(int argc, char **argv)
: Client::Application(argc, argv) {
	setMessagingEnabled(true);
	setDatabaseEnabled(true, true);
	setLoadStationsEnabled(true);
	setPrimaryMessagingGroup("LOCATION");
	addMessagingSubscription("PICK");
	setInterpretNotifierEnabled(true);
	setAutoApplyNotifierEnabled(false);

	_instance = this;
}


BinderApp::~BinderApp() {
	_instance = nullptr;
}


void BinderApp::createCommandLineDescription() {
	Client::Application::createCommandLineDescription();
	commandline().addGroup("Binder");
	commandline().addOption("Binder", "binder-config",
	                        "binder_ew-style command file with the "
	                        "algorithm configuration (grid, model, "
	                        "association and location commands)",
	                        &_binderConfigFile);
	commandline().addOption("Binder", "test",
	                        "do not send any object to messaging");
	commandline().addOption("Binder", "ep",
	                        "event parameters XML file for offline playback: "
	                        "feed all contained picks in time order, print "
	                        "resulting origins, then exit",
	                        &_epFile);
	commandline().addOption("Binder", "no-inventory-sites",
	                        "do not fill the site table from the inventory; "
	                        "use only site/site_file/maxsite commands from "
	                        "the binder command file (exact Earthworm "
	                        "behavior, incl. the site-table-mean projection "
	                        "origin of the L1 locator)");
	commandline().addOption("Binder", "ep-output",
	                        "write the origins produced during --ep playback "
	                        "as SCML to this file ('-' = stdout)",
	                        &_epOutputFile);
}


bool BinderApp::validateParameters() {
	if ( !Client::Application::validateParameters() ) return false;

	if ( _binderConfigFile.empty() ) {
		try { _binderConfigFile = configGetString("binder.config"); }
		catch ( ... ) {
			_binderConfigFile = Environment::Instance()->configDir()
			                  + "/binder.d";
		}
	}
	_binderConfigFile = Environment::Instance()->absolutePath(_binderConfigFile);

	try { _eventIdFile = configGetString("binder.eventIdFile"); }
	catch ( ... ) {
		_eventIdFile = "@ROOTDIR@/var/run/scbinder/quake_id.d";
	}
	_eventIdFile = Environment::Instance()->absolutePath(_eventIdFile);

	try { _defaultPickWeight = configGetInt("binder.defaultPickWeight"); }
	catch ( ... ) {}

	try {
		std::vector<double> t = configGetDoubles("binder.weightThresholds");
		if ( t.size() == 4 ) _weightThresholds = t;
		else SEISCOMP_ERROR("binder.weightThresholds needs exactly 4 values "
		                    "- keeping defaults");
	}
	catch ( ... ) {}

	_testMode = commandline().hasOption("test");

	try { _inventorySites = configGetBool("binder.inventorySites"); }
	catch ( ... ) {}
	if ( commandline().hasOption("no-inventory-sites") )
		_inventorySites = false;

	if ( !_epFile.empty() ) {
		setMessagingEnabled(false);
		_testMode = true;
	}

	return true;
}


bool BinderApp::init() {
	if ( !Client::Application::init() ) return false;

	scbinder_set_callbacks(quakeCallback, linkCallback);

	_inputPicks    = addInputObjectLog("pick");
	_outputOrigins = addOutputObjectLog("origin", primaryMessagingGroup());

	if ( _inventorySites && !loadSites() ) return false;
	if ( !loadBinderConfig() ) return false;
	if ( !ensureEventIdFile() ) return false;

	if ( nSite < 1 ) {
		SEISCOMP_ERROR("Site table is empty (inventory sites disabled and "
		               "no site commands in %s)", _binderConfigFile.c_str());
		return false;
	}

	bind_init();
	grid_init();

	if ( !is_nextID_set() ) {
		SEISCOMP_ERROR("Event id initialization failed (no next_id)");
		return false;
	}

	SEISCOMP_INFO("scbinder initialized: %d sites, config %s",
	              nSite, _binderConfigFile.c_str());
	return true;
}


bool BinderApp::run() {
	if ( _epFile.empty() )
		return Client::Application::run();

	IO::XMLArchive ar;
	if ( !ar.open(_epFile.c_str()) ) {
		SEISCOMP_ERROR("Cannot open event parameters file %s", _epFile.c_str());
		return false;
	}

	EventParametersPtr ep;
	ar >> ep;
	ar.close();

	if ( !ep ) {
		SEISCOMP_ERROR("No event parameters found in %s", _epFile.c_str());
		return false;
	}

	std::vector<PickPtr> picks;
	for ( size_t i = 0; i < ep->pickCount(); ++i )
		picks.push_back(ep->pick(i));

	std::sort(picks.begin(), picks.end(),
	          [](const PickPtr &a, const PickPtr &b) {
	              return a->time().value() < b->time().value();
	          });

	SEISCOMP_INFO("Playback of %zu picks from %s", picks.size(), _epFile.c_str());

	if ( !_epOutputFile.empty() )
		_outputEP = new EventParameters;

	for ( const PickPtr &pick : picks )
		feedPick(pick.get());

	SEISCOMP_INFO("Playback finished: %zu origin updates emitted", _originCount);

	if ( _outputEP ) {
		IO::XMLArchive out;
		if ( !out.create(_epOutputFile.c_str()) ) {
			SEISCOMP_ERROR("Cannot create %s", _epOutputFile.c_str());
			return false;
		}
		out.setFormattedOutput(true);
		out << _outputEP;
		out.close();
		SEISCOMP_INFO("Wrote %zu origins to %s",
		              _outputEP->originCount(), _epOutputFile.c_str());
	}

	return true;
}


void BinderApp::done() {
	grid_free();
	hyp_free();
	Client::Application::done();
}


// Scan the binder command file (and @includes) for the association grid
// bounds and the radii that matter for station usage. hyp.c derives its
// flat-earth projection from the MEAN of the whole site table, so the
// table must contain only stations around the grid - exactly like an
// Earthworm site file - or every location and residual is computed in a
// skewed projection.
bool BinderApp::scanGridBounds(const std::string &path,
                               double &lat0, double &lat1,
                               double &lon0, double &lon1,
                               double &marginKm, int depth) {
	if ( depth > 8 ) return false;

	std::ifstream f(path);
	if ( !f.is_open() ) {
		SEISCOMP_WARNING("scanGridBounds: cannot open %s", path.c_str());
		return false;
	}

	bool haveLat = false, haveLon = false;
	static double rstack = 100, rmax = 400;   // binder defaults-ish
	std::string line;
	while ( std::getline(f, line) ) {
		std::istringstream is(line);
		std::string tok;
		if ( !(is >> tok) || tok.empty() || tok[0] == '#' ) continue;

		if ( tok[0] == '@' ) {
			std::filesystem::path inc(tok.substr(1));
			if ( inc.is_relative() )
				inc = std::filesystem::path(path).parent_path() / inc;
			double dummy;
			scanGridBounds(inc.string(), lat0, lat1, lon0, lon1,
			               dummy, depth+1);
			haveLat = haveLat || (lat0 <= lat1);
			haveLon = haveLon || (lon0 <= lon1);
		}
		else if ( tok == "grdlat" ) { is >> lat0 >> lat1; haveLat = true; }
		else if ( tok == "grdlon" ) { is >> lon0 >> lon1; haveLon = true; }
		else if ( tok == "rstack" ) { is >> rstack; }
		else if ( tok == "r_max"  ) { is >> rmax; }
	}

	marginKm = rstack + rmax + 100;
	return haveLat && haveLon;
}


// Fill the Earthworm site table from the SeisComP inventory. The site
// table is loaded through the verbatim site.c by feeding it a generated
// kom command file, so site_index() lookups behave exactly as in
// Earthworm (including the "--" empty location convention).
bool BinderApp::loadSites() {
	DataModel::Inventory *inv = Client::Inventory::Instance()->inventory();
	if ( !inv ) {
		SEISCOMP_ERROR("No inventory available");
		return false;
	}

	double glat0 = 1, glat1 = -1, glon0 = 1, glon1 = -1, marginKm = 800;
	bool haveBounds = scanGridBounds(_binderConfigFile, glat0, glat1,
	                                 glon0, glon1, marginKm, 0);
	double dlat = 0, dlonl = 0, dlonh = 0;
	if ( haveBounds ) {
		dlat = Math::Geo::km2deg(marginKm);
		double coslat = std::max(0.2,
			std::cos(std::max(std::fabs(glat0), std::fabs(glat1)) * M_PI/180.));
		dlonl = dlonh = Math::Geo::km2deg(marginKm) / coslat;
		SEISCOMP_INFO("Site table restricted to grid %.2f..%.2f / "
		              "%.2f..%.2f + %.0f km margin",
		              glat0, glat1, glon0, glon1, marginKm);
	}
	else {
		SEISCOMP_WARNING("Could not determine grid bounds from %s - "
		                 "loading ALL inventory channels. hyp.c derives "
		                 "its projection from the site table mean; a "
		                 "worldwide table skews all locations!",
		                 _binderConfigFile.c_str());
	}

	// channel key -> site line; deduplicates repeated epochs, last one wins
	std::map<std::string, std::string> channels;

	for ( size_t n = 0; n < inv->networkCount(); ++n ) {
		Network *net = inv->network(n);
		for ( size_t s = 0; s < net->stationCount(); ++s ) {
			Station *sta = net->station(s);
			for ( size_t l = 0; l < sta->sensorLocationCount(); ++l ) {
				SensorLocation *loc = sta->sensorLocation(l);
				for ( size_t c = 0; c < loc->streamCount(); ++c ) {
					Stream *cha = loc->stream(c);

					double lat, lon, elev;
					try {
						lat  = loc->latitude();
						lon  = loc->longitude();
						elev = loc->elevation();
					}
					catch ( ... ) {
						try {
							lat  = sta->latitude();
							lon  = sta->longitude();
							elev = sta->elevation();
						}
						catch ( ... ) { continue; }
					}

					if ( haveBounds &&
					     (lat < glat0 - dlat || lat > glat1 + dlat ||
					      lon < glon0 - dlonl || lon > glon1 + dlonh) )
						continue;

					std::string key = sta->code() + "." + net->code() + "."
					                + cha->code() + "." + loc->code();

					char line[256];
					snprintf(line, sizeof(line),
					         "site %s %s %s %s %.6f %.6f %.1f\n",
					         sta->code().c_str(), net->code().c_str(),
					         cha->code().c_str(), ewLoc(loc->code()),
					         lat, lon, elev);
					channels[key] = line;
				}
			}
		}
	}

	if ( channels.empty() ) {
		SEISCOMP_ERROR("Inventory produced no channels for the site table");
		return false;
	}

	set_maxsite(static_cast<int>(channels.size()) + 100);

	std::string path = std::filesystem::temp_directory_path()
	                 / ("scbinder_sites_" + std::to_string(getpid()) + ".d");
	{
		std::ofstream f(path);
		if ( !f.is_open() ) {
			SEISCOMP_ERROR("Cannot write temporary site file %s", path.c_str());
			return false;
		}
		for ( const auto &it : channels ) f << it.second;
	}

	int res = site_load(const_cast<char*>(path.c_str()));
	std::filesystem::remove(path);

	if ( res != 1 ) {
		SEISCOMP_ERROR("site_load() failed for generated site table");
		return false;
	}

	SEISCOMP_INFO("Loaded %d channels into the binder site table", nSite);
	return true;
}


// Process the binder_ew-style command file with the same dispatch chain
// as bndr_config() in binder_ew.c. Earthworm transport/plumbing commands
// are recognized and skipped so an existing binder_ew.d works unchanged.
bool BinderApp::loadBinderConfig() {
	static const char *ewPlumbing[] = {
		"MyModuleId", "RingName", "LogFile", "HeartbeatInt",
		"GetPicksFrom", "BufferRing", "BufferRingKB", nullptr
	};

	int nfiles = k_open(const_cast<char*>(_binderConfigFile.c_str()));
	if ( nfiles == 0 ) {
		SEISCOMP_ERROR("Cannot open binder command file %s",
		               _binderConfigFile.c_str());
		return false;
	}
	SEISCOMP_INFO("Processing binder command file %s", _binderConfigFile.c_str());

	while ( nfiles > 0 ) {
		while ( k_rd() ) {
			char *com = k_str();
			if ( !com ) continue;
			if ( com[0] == '#' ) continue;

			if ( com[0] == '@' ) {
				int success = nfiles + 1;
				nfiles = k_open(&com[1]);
				if ( nfiles != success ) {
					SEISCOMP_ERROR("Cannot open nested command file %s", &com[1]);
					return false;
				}
				continue;
			}

			if ( k_its(const_cast<char*>("EventIdFile")) ) {
				char *str = k_str();
				if ( !str ) {
					SEISCOMP_ERROR("EventIdFile command without file name");
					return false;
				}
				std::string path = Environment::Instance()->absolutePath(str);
				strncpy(EventIdFile, path.c_str(), sizeof(EventIdFile)-1);
				int success = nfiles + 1;
				nfiles = k_open(EventIdFile);
				if ( nfiles != success ) {
					SEISCOMP_ERROR("Cannot open event id file %s", EventIdFile);
					return false;
				}
				continue;
			}

			bool skipped = false;
			for ( const char **cmd = ewPlumbing; *cmd; ++cmd ) {
				if ( k_its(const_cast<char*>(*cmd)) ) {
					SEISCOMP_DEBUG("Ignoring Earthworm transport command <%s>", com);
					skipped = true;
					break;
				}
			}
			if ( skipped ) continue;

			if      ( hyp_com()    ) {}
			else if ( t_com()      ) {}
			else if ( bind_com()   ) {}
			else if ( assess_com() ) {}
			else if ( grid_com()   ) {}
			else if ( site_com()   ) {}
			else {
				SEISCOMP_WARNING("Unknown command <%s> in %s", com,
				                 _binderConfigFile.c_str());
				continue;
			}

			if ( k_err() ) {
				SEISCOMP_ERROR("Bad <%s> command in %s", com,
				               _binderConfigFile.c_str());
				return false;
			}
		}
		nfiles = k_close();
	}

	return true;
}


// If the command file did not reference an event id file, provide one:
// create it with "next_id 1" on first run, then feed it through
// bind_com() exactly as binder_ew.c did via k_open/EventIdFile.
bool BinderApp::ensureEventIdFile() {
	if ( is_nextID_set() ) return true;

	std::filesystem::path path(_eventIdFile);
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

	if ( !std::filesystem::exists(path) ) {
		std::ofstream f(path);
		if ( !f.is_open() ) {
			SEISCOMP_ERROR("Cannot create event id file %s", _eventIdFile.c_str());
			return false;
		}
		f << "next_id 1\n";
		SEISCOMP_INFO("Created event id file %s", _eventIdFile.c_str());
	}

	strncpy(EventIdFile, _eventIdFile.c_str(), sizeof(EventIdFile)-1);

	int nfiles = k_open(EventIdFile);
	if ( nfiles == 0 ) {
		SEISCOMP_ERROR("Cannot open event id file %s", EventIdFile);
		return false;
	}
	while ( nfiles > 0 ) {
		while ( k_rd() ) {
			char *com = k_str();
			if ( !com || com[0] == '#' ) continue;
			if ( !bind_com() )
				SEISCOMP_WARNING("Unknown command <%s> in %s", com, EventIdFile);
		}
		nfiles = k_close();
	}

	return true;
}


void BinderApp::addObject(const std::string &parentID, DataModel::Object *obj) {
	Pick *pick = Pick::Cast(obj);
	if ( pick ) {
		logObject(_inputPicks, Core::Time::UTC());
		feedPick(pick);
	}
}


int BinderApp::pickWeight(DataModel::Pick *pick) const {
	double u = -1;
	try { u = pick->time().uncertainty(); }
	catch ( ... ) {
		try {
			u = 0.5 * (pick->time().lowerUncertainty()
			         + pick->time().upperUncertainty());
		}
		catch ( ... ) {}
	}

	if ( u < 0 ) return _defaultPickWeight;

	for ( size_t i = 0; i < _weightThresholds.size(); ++i )
		if ( u <= _weightThresholds[i] ) return static_cast<int>(i);
	return 4;
}


void BinderApp::feedPick(DataModel::Pick *pick) {
	const WaveformStreamID &wf = pick->waveformID();

	int isite = site_index(const_cast<char*>(wf.stationCode().c_str()),
	                       const_cast<char*>(wf.networkCode().c_str()),
	                       const_cast<char*>(wf.channelCode().c_str()),
	                       const_cast<char*>(ewLoc(wf.locationCode())));
	if ( isite < 0 ) {
		SEISCOMP_DEBUG("%s: %s.%s.%s.%s not in site table - ignored",
		               pick->publicID().c_str(),
		               wf.networkCode().c_str(), wf.stationCode().c_str(),
		               wf.locationCode().c_str(), wf.channelCode().c_str());
		return;
	}

	PICK pk;
	memset(&pk, 0, sizeof(pk));
	pk.t      = toEpoch(pick->time().value()) + GSEC1970;
	pk.instid = 0;
	pk.src    = 0;
	pk.seq    = ++_seq;
	pk.quake  = 0;
	pk.site   = isite;
	pk.phase  = 0;      // P assumed; binder assigns the phase itself
	pk.ie     = ' ';
	pk.fm     = '?';    // EW pick text convention for unknown polarity
	pk.wt     = static_cast<char>('0' + pickWeight(pick));
	pk.lpick  = 0;
	pk.amp    = 0;
	pk.dup    = 0;
	pk.glitch = 0;

	try {
		switch ( pick->polarity() ) {
			case POSITIVE:     pk.fm = 'U'; break;
			case NEGATIVE:     pk.fm = 'D'; break;
			case UNDECIDABLE:  pk.fm = '?'; break;
			default: break;
		}
	}
	catch ( ... ) {}

	_pickIDs[pk.seq] = pick->publicID();

	// Bound the pick-id map: entries older than the binder pick fifo
	// can never be referenced again.
	if ( static_cast<long>(_pickIDs.size()) > 2*mPick && mPick > 0 )
		_pickIDs.erase(_pickIDs.begin(),
		               _pickIDs.lower_bound(_seq - static_cast<int>(mPick)));

	SEISCOMP_DEBUG("Feeding pick %s (site %d, wt %c)",
	               pick->publicID().c_str(), isite, pk.wt);

	_dirtyQuakes.clear();
	bind_pick(&pk);

	for ( long q : _dirtyQuakes ) emitQuake(q);
	_dirtyQuakes.clear();
}


void BinderApp::onQuake(long quake) {
	_dirtyQuakes.insert(quake);
}


void BinderApp::onLink(long quake, long pick) {
	_dirtyQuakes.insert(quake);
}


void BinderApp::emitQuake(long quake) {
	if ( mQuake <= 0 ) return;
	// NB: QUAKE.qid is never filled in by the core - don't test it.
	// The fifo slot is fresh as long as the id is within one fifo length.
	if ( quake < lQuake - mQuake ) return;
	const QUAKE &q = pQuake[quake % mQuake];

	if ( q.npix < 4 ) {
		SEISCOMP_INFO("Quake %ld below 4 picks (npix=%d) - not exported",
		              quake, q.npix);
		return;
	}

	OriginPtr org = Origin::Create();
	if ( !org ) {
		SEISCOMP_ERROR("Failed to create origin for quake %ld", quake);
		return;
	}

	org->setTime(TimeQuantity(Core::Time(q.t - GSEC1970)));
	org->setLatitude(RealQuantity(q.lat));
	org->setLongitude(RealQuantity(q.lon));
	org->setDepth(RealQuantity(q.z));
	org->setMethodID("binder_ew");
	org->setEvaluationMode(EvaluationMode(AUTOMATIC));

	{
		// Traceability: which internal binder quake this origin snapshots
		Comment *cmt = new Comment;
		cmt->setId("binder:quake");
		cmt->setText(Core::toString(quake));
		org->add(cmt);
	}

	CreationInfo ci;
	ci.setAgencyID(agencyID());
	ci.setAuthor(author());
	ci.setCreationTime(Core::Time::UTC());
	org->setCreationInfo(ci);

	int arrivals = 0, used = 0;
	if ( pPick && mPick > 0 ) {
		long first = lPick > mPick ? lPick - mPick : 0;
		for ( long l = first; l < lPick; ++l ) {
			const PICK &p = pPick[l % mPick];
			if ( static_cast<long>(p.quake) != quake ) continue;

			auto it = _pickIDs.find(p.seq);
			if ( it == _pickIDs.end() ) continue;

			Arrival *ar = new Arrival;
			ar->setPickID(it->second);
			int iph = p.phase;
			ar->setPhase(Phase(iph >= 0 && iph < 6 ? Phs[iph] : "P"));
			bool active = !p.dup && !p.glitch;
			ar->setWeight(active ? 1.0 : 0.0);
			ar->setTimeUsed(active);

			if ( p.site >= 0 && p.site < nSite ) {
				double dist, az, baz;
				Math::Geo::delazi(q.lat, q.lon,
				                  Site[p.site].lat, Site[p.site].lon,
				                  &dist, &az, &baz);
				ar->setDistance(dist);
				ar->setAzimuth(az);

				double dtdr, dtdz;
				double tt = t_phase(iph, Math::Geo::deg2km(dist), q.z,
				                    &dtdr, &dtdz);
				if ( tt < 1E29 )
					ar->setTimeResidual(p.t - q.t - tt);
			}

			org->add(ar);
			++arrivals;
			if ( active ) ++used;
		}
	}

	OriginQuality qual;
	qual.setStandardError(q.rms);
	qual.setAzimuthalGap(q.gap);
	qual.setMinimumDistance(Math::Geo::km2deg(q.dmin));
	qual.setMedianDistance(Math::Geo::km2deg(q.ravg));
	qual.setUsedPhaseCount(q.npix);
	qual.setAssociatedPhaseCount(arrivals);
	org->setQuality(qual);

	SEISCOMP_INFO("Quake %ld -> origin %s: %.4f %.4f %.1fkm rms=%.2f "
	              "gap=%.0f npix=%d arrivals=%d(%d used)",
	              quake, org->publicID().c_str(), q.lat, q.lon, q.z,
	              q.rms, q.gap, q.npix, arrivals, used);

	++_originCount;
	logObject(_outputOrigins, Core::Time::UTC());

	if ( _outputEP ) _outputEP->add(org.get());

	if ( _testMode || !connection() ) return;

	EventParameters ep;
	bool wasEnabled = Notifier::IsEnabled();
	Notifier::Enable();
	ep.add(org.get());
	Notifier::SetEnabled(wasEnabled);

	NotifierMessagePtr nmsg = Notifier::GetMessage(true);
	if ( !connection()->send(nmsg.get()) )
		SEISCOMP_ERROR("Sending origin %s failed", org->publicID().c_str());
}


}
}
