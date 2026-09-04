/***************************************************************************
 * scbinder — SeisComP port of Earthworm's binder_ew associator
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Mustafa Çomoğlu
 *
 * BinderApp: Client::Application wrapper around the verbatim binder_ew
 * core. Subscribes to PICK, feeds picks into bind_pick(), emits the
 * resulting quakes as DataModel::Origin to LOCATION.
 ***************************************************************************/

#ifndef SCBINDER_APP_H
#define SCBINDER_APP_H

#include <seiscomp/client/application.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Seiscomp {
namespace Applications {

class BinderApp : public Client::Application {
	public:
		BinderApp(int argc, char **argv);
		~BinderApp() override;

		static BinderApp *Instance() { return _instance; }

		// Called from the C glue (bndr_quake2k / bndr_link)
		void onQuake(long quake);
		void onLink(long quake, long pick);

	protected:
		const char *version() override;
		void createCommandLineDescription() override;
		bool validateParameters() override;
		bool init() override;
		bool run() override;
		void done() override;

		void addObject(const std::string &parentID,
		               DataModel::Object *obj) override;

	private:
		bool scanGridBounds(const std::string &path,
		                    double &lat0, double &lat1,
		                    double &lon0, double &lon1,
		                    double &marginKm, int depth);
		bool loadSites();
		bool loadBinderConfig();
		bool ensureEventIdFile();

		void feedPick(DataModel::Pick *pick);
		int  pickWeight(DataModel::Pick *pick) const;
		void emitQuake(long quake);

	private:
		static BinderApp   *_instance;

		std::string         _binderConfigFile;
		std::string         _eventIdFile;
		std::string         _epFile;
		std::string         _epOutputFile;
		DataModel::EventParametersPtr _outputEP;
		int                 _defaultPickWeight{0};
		std::vector<double> _weightThresholds{0.1, 0.2, 0.4, 0.8};
		bool                _testMode{false};
		bool                _inventorySites{true};

		ObjectLog                   *_inputPicks{nullptr};
		ObjectLog                   *_outputOrigins{nullptr};

		int                          _seq{0};       // our pick sequence counter
		std::map<int, std::string>   _pickIDs;      // seq -> pick publicID
		std::set<long>               _dirtyQuakes;  // quakes touched by current pick
		size_t                       _originCount{0};
};

}
}

#endif
