/**
 ******************************************************************************
 * @file       telemetrymonitor.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @author     Vladimir Ermakov, Copyright (C) 2013.
 * @brief The UAVTalk protocol
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "telemetrymonitor.h"
#include "ros/console.h"

using namespace openpilot;

/** Constructor
 */
TelemetryMonitor::TelemetryMonitor(boost::asio::io_service &io, UAVObjectManager *objMngr, Telemetry *tel) :
	io_service(io),
	statsTimer(io_service),
	connectionTimer(io_service)
{
	this->objMngr    = objMngr;
	this->tel        = tel;
	this->objPending = NULL;

	start_time = boost::posix_time::microsec_clock::universal_time();

	// Get stats objects
	gcsStatsObj    = GCSTelemetryStats::GetInstance(objMngr);
	flightStatsObj = FlightTelemetryStats::GetInstance(objMngr);

	// Listen for flight stats updates
	flightStatsObj->objectUpdated.connect(boost::bind(&TelemetryMonitor::flightStatsUpdated, this, _1));

	// Start update timer
	stats_interval = boost::posix_time::milliseconds(STATS_CONNECT_PERIOD_MS);
	statsTimer.expires_from_now(stats_interval);
	statsTimer.async_wait(boost::bind(&TelemetryMonitor::processStatsUpdates, this, boost::asio::placeholders::error));
}

TelemetryMonitor::~TelemetryMonitor()
{
	statsTimer.cancel();
	connectionTimer.cancel();

	// Before saying goodbye, set the GCS connection status to disconnected too:
	GCSTelemetryStats::DataFields gcsStats = gcsStatsObj->getData();

	gcsStats.Status = GCSTelemetryStats::STATUS_DISCONNECTED;
	// Set data
	gcsStatsObj->setData(gcsStats);
}

/** Initiate object retrieval, initialize queue with objects to be retrieved.
 */
void TelemetryMonitor::startRetrievingObjects()
{
	// Clear object queue
	while (!queue.empty())
		queue.pop();

	// Get all objects, add metaobjects, settings and data objects with OnChange update mode to the queue
	UAVObjectManager::objects_map objs = objMngr->getObjects();
	for (UAVObjectManager::objects_map::iterator it = objs.begin(); it != objs.end(); ++it) {

		UAVObject *obj = it->second[0];
		UAVMetaObject *mobj = dynamic_cast<UAVMetaObject *>(obj);
		UAVDataObject *dobj = dynamic_cast<UAVDataObject *>(obj);
		UAVObject::Metadata mdata = obj->getMetadata();

		if (mobj != NULL) {
			queue.push(obj);
		} else if (dobj != NULL) {
			if (dobj->isSettings() ||
				UAVObject::GetFlightTelemetryUpdateMode(mdata) == UAVObject::UPDATEMODE_ONCHANGE) {
				queue.push(obj);
			}
		}
	}

	// Start retrieving
	ROS_DEBUG_NAMED("TelemetryMonitor", "Starting to retrieve meta and settings objects from the autopilot (%zu objects)",
			queue.size());
	retrieveNextObject();
}

/** Cancel the object retrieval
 */
void TelemetryMonitor::stopRetrievingObjects()
{
	ROS_DEBUG_NAMED("TelemetryMonitor", "Object retrieval has been cancelled");
	while (!queue.empty())
		queue.pop();
}

/** Retrieve the next object in the queue
 */
void TelemetryMonitor::retrieveNextObject()
{
	// If queue is empty return
	if (queue.empty()) {
		ROS_DEBUG_NAMED("TelemetryMonitor", "Object retrieval completed");
		connected(); // emit signal
		return;
	}
	// Get next object from the queue
	UAVObject *obj = queue.front();
	queue.pop();
	// Connect to object
	obj->transactionCompleted.connect(boost::bind(&TelemetryMonitor::transactionCompleted, this, _1, _2));
	// Request update
	obj->requestUpdate();
	objPending = obj;
}

/** Called by the retrieved object when a transaction is completed.
 */
void TelemetryMonitor::transactionCompleted(UAVObject *obj, bool success)
{
	boost::recursive_mutex::scoped_lock lock(mutex);
	// Disconnect from sending object
	obj->transactionCompleted.disconnect(boost::bind(&TelemetryMonitor::transactionCompleted, this, _1, _2));
	objPending = NULL;
	// Process next object if telemetry is still available
	GCSTelemetryStats::DataFields gcsStats = gcsStatsObj->getData();
	if (gcsStats.Status == GCSTelemetryStats::STATUS_CONNECTED) {
		retrieveNextObject();
	} else {
		stopRetrievingObjects();
	}
}

/** Called each time the flight stats object is updated by the autopilot
 */
void TelemetryMonitor::flightStatsUpdated(UAVObject *obj)
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	// Force update if not yet connected
	GCSTelemetryStats::DataFields gcsStats = gcsStatsObj->getData();
	FlightTelemetryStats::DataFields flightStats = flightStatsObj->getData();
	if (gcsStats.Status != GCSTelemetryStats::STATUS_CONNECTED ||
			flightStats.Status != FlightTelemetryStats::STATUS_CONNECTED) {
		processStatsUpdates(boost::system::error_code());
	}
}

/** Called periodically to update the statistics and connection status.
 */
void TelemetryMonitor::processStatsUpdates(boost::system::error_code error)
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	if (error)
		return;

	// Get telemetry stats
	GCSTelemetryStats::DataFields gcsStats = gcsStatsObj->getData();
	FlightTelemetryStats::DataFields flightStats = flightStatsObj->getData();
	Telemetry::TelemetryStats telStats     = tel->getStats();

	tel->resetStats();

	boost::posix_time::ptime end_time = boost::posix_time::microsec_clock::universal_time();
	boost::posix_time::time_duration interval = end_time - start_time;
	start_time = end_time;

	// Update stats object
	gcsStats.RxDataRate  = (float)telStats.rxBytes / ((float)interval.total_milliseconds() / 1000.0);
	gcsStats.TxDataRate  = (float)telStats.txBytes / ((float)interval.total_milliseconds() / 1000.0);
	gcsStats.RxFailures += telStats.rxErrors;
	gcsStats.TxFailures += telStats.txErrors;
	gcsStats.TxRetries  += telStats.txRetries;

	// Check for a connection timeout
	if (telStats.rxObjects > 0) {
		connectionTimer.cancel();
		connectionTimeout = false;
		connectionTimer.expires_from_now(boost::posix_time::milliseconds(CONNECTION_TIMEOUT_MS));
		connectionTimer.async_wait(boost::bind(&TelemetryMonitor::connectionTimeoutHandler, this, boost::asio::placeholders::error));
	}

	// Update connection state
	int oldStatus = gcsStats.Status;
	if (gcsStats.Status == GCSTelemetryStats::STATUS_DISCONNECTED) {
		// Request connection
		gcsStats.Status = GCSTelemetryStats::STATUS_HANDSHAKEREQ;
	} else if (gcsStats.Status == GCSTelemetryStats::STATUS_HANDSHAKEREQ) {
		// Check for connection acknowledge
		if (flightStats.Status == FlightTelemetryStats::STATUS_HANDSHAKEACK) {
			gcsStats.Status = GCSTelemetryStats::STATUS_CONNECTED;
		}
	} else if (gcsStats.Status == GCSTelemetryStats::STATUS_CONNECTED) {
		// Check if the connection is still active and the the autopilot is still connected
		if (flightStats.Status == FlightTelemetryStats::STATUS_DISCONNECTED || connectionTimeout) {
			gcsStats.Status = GCSTelemetryStats::STATUS_DISCONNECTED;
		}
	}

	telemetryUpdated(gcsStats.TxDataRate, gcsStats.RxDataRate); // emit signal

	// Set data
	gcsStatsObj->setData(gcsStats);

	// Force telemetry update if not yet connected
	if (gcsStats.Status != GCSTelemetryStats::STATUS_CONNECTED ||
			flightStats.Status != FlightTelemetryStats::STATUS_CONNECTED) {
		gcsStatsObj->updated();
	}

	// Act on new connections or disconnections
	if (gcsStats.Status == GCSTelemetryStats::STATUS_CONNECTED && gcsStats.Status != oldStatus) {
		stats_interval = boost::posix_time::milliseconds(STATS_UPDATE_PERIOD_MS);
		ROS_INFO_NAMED("TelemetryMonitor", "Connection with the autopilot established");
		startRetrievingObjects();
	}
	if (gcsStats.Status == GCSTelemetryStats::STATUS_DISCONNECTED && gcsStats.Status != oldStatus) {
		stats_interval = boost::posix_time::milliseconds(STATS_CONNECT_PERIOD_MS);
		ROS_INFO_NAMED("TelemetryMonitor", "Connection with the autopilot lost");
		ROS_INFO_NAMED("TelemetryMonitor", "Trying to connect to the autopilot");
		disconnected(); // emit signal
	}

	statsTimer.expires_from_now(stats_interval);
	statsTimer.async_wait(boost::bind(&TelemetryMonitor::processStatsUpdates, this, boost::asio::placeholders::error));
}


void TelemetryMonitor::connectionTimeoutHandler(boost::system::error_code error)
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	if (error)
		return;

	connectionTimeout = true;
}

