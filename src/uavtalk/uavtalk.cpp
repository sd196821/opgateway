/**
 ******************************************************************************
 *
 * @file       uavtalk.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup UAVTalkPlugin UAVTalk Plugin
 * @{
 * @brief The UAVTalk protocol plugin
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
#include "uavtalk.h"

//#define UAVTALK_DEBUG
#ifdef UAVTALK_DEBUG
  #include <ros/console.h>
  #define UAVTALK_LOG_DEBUG(args...) ROS_DEBUG_NAMED("UAVTalk", ##args)
#else // UAVTALK_DEBUG
  #define UAVTALK_LOG_DEBUG(args...)
#endif // UAVTALK_DEBUG

#define SYNC_VAL 0x3C

using namespace openpilot;

const uint8_t UAVTalk::crc_table[256] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};


/** Constructor
 */
UAVTalk::UAVTalk(UAVTalkIOBase *iodev, UAVObjectManager *objMngr)
{
	io = iodev;
	this->objMngr  = objMngr;

	rxState = STATE_SYNC;
	rxPacketLength = 0;

	memset(&stats, 0, sizeof(ComStats));

	io->sig_read.connect(boost::bind(&UAVTalk::processInputStream, this, _1, _2));
}

UAVTalk::~UAVTalk()
{
	// According to Qt, it is not necessary to disconnect upon
	// object deletion.
	// disconnect(io, SIGNAL(readyRead()), this, SLOT(processInputStream()));
	// XXX
}


/** Reset the statistics counters
 */
void UAVTalk::resetStats()
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	memset(&stats, 0, sizeof(ComStats));
}

/** Get the statistics counters
 */
UAVTalk::ComStats UAVTalk::getStats()
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	return stats;
}

/** Called each time there are data in the input buffer
 */
void UAVTalk::processInputStream(uint8_t *data, size_t length)
{
    if (io && io->is_open()) {
        for (size_t i = 0; i < length; i++)
            processInputByte(data[i]);
    }
}

/** Request an update for the specified object, on success the object data would have been
 * updated by the GCS.
 * \param[in] obj Object to update
 * \param[in] allInstances If set true then all instances will be updated
 * \return Success (true), Failure (false)
 */
bool UAVTalk::sendObjectRequest(UAVObject *obj, bool allInstances)
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	return objectTransaction(obj, TYPE_OBJ_REQ, allInstances);
}

/** Send the specified object through the telemetry link.
 * \param[in] obj Object to send
 * \param[in] acked Selects if an ack is required
 * \param[in] allInstances If set true then all instances will be updated
 * \return Success (true), Failure (false)
 */
bool UAVTalk::sendObject(UAVObject *obj, bool acked, bool allInstances)
{
	boost::recursive_mutex::scoped_lock lock(mutex);

	if (acked) {
		return objectTransaction(obj, TYPE_OBJ_ACK, allInstances);
	} else {
		return objectTransaction(obj, TYPE_OBJ, allInstances);
	}
}

/** Cancel a pending transaction
 */
void UAVTalk::cancelTransaction(UAVObject *obj)
{
	boost::recursive_mutex::scoped_lock lock(mutex);
	uint32_t objId = obj->getObjID();

	if (!io) {
		return;
	}

	std::map<uint32_t, Transaction *>::iterator itr = transMap.find(objId);
	if (itr != transMap.end()) {
		transMap.erase(objId);
		delete itr->second;
	}
}

/** Execute the requested transaction on an object.
 * \param[in] obj Object
 * \param[in] type Transaction type
 *                        TYPE_OBJ: send object,
 *                        TYPE_OBJ_REQ: request object update
 *                        TYPE_OBJ_ACK: send object with an ack
 * \param[in] allInstances If set true then all instances will be updated
 * \return Success (true), Failure (false)
 */
bool UAVTalk::objectTransaction(UAVObject *obj, uint8_t type, bool allInstances)
{
	// Send object depending on if a response is needed
	if (type == TYPE_OBJ_ACK || type == TYPE_OBJ_REQ) {
		if (transmitObject(obj, type, allInstances)) {
			Transaction *trans = new Transaction();
			trans->obj = obj;
			trans->allInstances = allInstances;
			transMap[obj->getObjID()] = trans;

			return true;
		} else {
			return false;
		}
	} else if (type == TYPE_OBJ) {
		return transmitObject(obj, TYPE_OBJ, allInstances);
	} else {
		return false;
	}
}

/** Process an byte from the telemetry stream.
 * \param[in] rxbyte Received byte
 * \return Success (true), Failure (false)
 */
bool UAVTalk::processInputByte(uint8_t rxbyte)
{
	// Update stats
	stats.rxBytes++;

	rxPacketLength++; // update packet byte count

	// Receive state machine
	switch (rxState) {
	case STATE_SYNC:

		if (rxbyte != SYNC_VAL) {
			UAVTALK_LOG_DEBUG("UAVTalk: Sync->Sync (%d 0x%x)", rxbyte, rxbyte);
			break;
		}

		// Initialize and update CRC
		rxCS = updateCRC(0, rxbyte);

		rxPacketLength = 1;

		rxState = STATE_TYPE;
		UAVTALK_LOG_DEBUG("UAVTalk: Sync->Type");
		break;

	case STATE_TYPE:

		// Update CRC
		rxCS = updateCRC(rxCS, rxbyte);

		if ((rxbyte & TYPE_MASK) != TYPE_VER) {
			rxState = STATE_SYNC;
			UAVTALK_LOG_DEBUG("UAVTalk: Type->Sync");
			break;
		}

		rxType     = rxbyte;

		packetSize = 0;

		rxState    = STATE_SIZE;
		UAVTALK_LOG_DEBUG("UAVTalk: Type->Size");
		rxCount    = 0;
		break;

	case STATE_SIZE:

		// Update CRC
		rxCS = updateCRC(rxCS, rxbyte);

		if (rxCount == 0) {
			packetSize += rxbyte;
			rxCount++;
			UAVTALK_LOG_DEBUG("UAVTalk: Size->Size");
			break;
		}

		packetSize += (uint32_t)rxbyte << 8;

		if (packetSize < MIN_HEADER_LENGTH || packetSize > MAX_HEADER_LENGTH + MAX_PAYLOAD_LENGTH) {
			// incorrect packet size
			rxState = STATE_SYNC;
			UAVTALK_LOG_DEBUG("UAVTalk: Size->Sync");
			break;
		}

		rxCount = 0;
		rxState = STATE_OBJID;
		UAVTALK_LOG_DEBUG("UAVTalk: Size->ObjID");
		break;

	case STATE_OBJID:

		// Update CRC
		rxCS = updateCRC(rxCS, rxbyte);

		rxTmpBuffer[rxCount++] = rxbyte;
		if (rxCount < 4) {
			UAVTALK_LOG_DEBUG("UAVTalk: ObjID->ObjID");
			break;
		}

		// Search for object, if not found reset state machine
		memcpy(&rxObjId, rxTmpBuffer, sizeof(rxObjId)); // XXX TODO make conversion for big endian hosts
		{
			UAVObject *rxObj = objMngr->getObject(rxObjId);
			if (rxObj == NULL && rxType != TYPE_OBJ_REQ) {
				stats.rxErrors++;
				rxState = STATE_SYNC;
				UAVTALK_LOG_DEBUG("UAVTalk: ObjID->Sync (badtype) ObjID=0x%08x", rxObjId);
				break;
			}

			// Determine data length
			if (rxType == TYPE_OBJ_REQ || rxType == TYPE_ACK || rxType == TYPE_NACK) {
				rxLength = 0;
				rxInstanceLength = 0;
			} else {
				rxLength = rxObj->getNumBytes();
				rxInstanceLength = (rxObj->isSingleInstance() ? 0 : 2);
			}

			// Check length and determine next state
			if (rxLength >= MAX_PAYLOAD_LENGTH) {
				stats.rxErrors++;
				rxState = STATE_SYNC;
				UAVTALK_LOG_DEBUG("UAVTalk: ObjID->Sync (oversize)");
				break;
			}

			// Check the lengths match
			if ((rxPacketLength + rxInstanceLength + rxLength) != packetSize) { // packet error - mismatched packet size
				stats.rxErrors++;
				rxState = STATE_SYNC;
				UAVTALK_LOG_DEBUG("UAVTalk: ObjID->Sync (length mismatch)");
				break;
			}

			// Check if this is a single instance object (i.e. if the instance ID field is coming next)
			if (rxObj == NULL) {
				// This is a non-existing object, just skip to checksum
				// and we'll send a NACK next.
				rxState  = STATE_CS;
				UAVTALK_LOG_DEBUG("UAVTalk: ObjID->CSum (no obj)");
				rxInstId = 0;
				rxCount  = 0;

			} else if (rxObj->isSingleInstance()) {
				// If there is a payload get it, otherwise receive checksum
				if (rxLength > 0) {
					rxState = STATE_DATA;
					UAVTALK_LOG_DEBUG("UAVTalk: ObjID->Data (needs data)");
				} else {
					rxState = STATE_CS;
					UAVTALK_LOG_DEBUG("UAVTalk: ObjID->Checksum");
				}
				rxInstId = 0;
				rxCount  = 0;

			} else {
				rxState = STATE_INSTID;
				UAVTALK_LOG_DEBUG("UAVTalk: ObjID->InstID");
				rxCount = 0;
			}
		}

		break;

	case STATE_INSTID:

		// Update CRC
		rxCS = updateCRC(rxCS, rxbyte);

		rxTmpBuffer[rxCount++] = rxbyte;
		if (rxCount < 2) {
			UAVTALK_LOG_DEBUG("UAVTalk: InstID->InstID");
			break;
		}

		memcpy(&rxInstId, rxTmpBuffer, sizeof(rxInstId)); // XXX TODO convert for Big endian

		rxCount  = 0;

		// If there is a payload get it, otherwise receive checksum
		if (rxLength > 0) {
			rxState = STATE_DATA;
			UAVTALK_LOG_DEBUG("UAVTalk: InstID->Data");
		} else {
			rxState = STATE_CS;
			UAVTALK_LOG_DEBUG("UAVTalk: InstID->CSum");
		}
		break;

	case STATE_DATA:

		// Update CRC
		rxCS = updateCRC(rxCS, rxbyte);

		rxBuffer[rxCount++] = rxbyte;
		if (rxCount < rxLength) {
			UAVTALK_LOG_DEBUG("UAVTalk: Data->Data");
			break;
		}

		rxState = STATE_CS;
		UAVTALK_LOG_DEBUG("UAVTalk: Data->CSum");
		rxCount = 0;
		break;

	case STATE_CS:

		// The CRC byte
		rxCSPacket = rxbyte;

		if (rxCS != rxCSPacket) { // packet error - faulty CRC
			stats.rxErrors++;
			rxState = STATE_SYNC;
			UAVTALK_LOG_DEBUG("UAVTalk: CSum->Sync (badcrc)");
			break;
		}

		if (rxPacketLength != packetSize + 1) { // packet error - mismatched packet size
			stats.rxErrors++;
			rxState = STATE_SYNC;
			UAVTALK_LOG_DEBUG("UAVTalk: CSum->Sync (length mismatch)");
			break;
		}

		mutex.lock();
		receiveObject(rxType, rxObjId, rxInstId, rxBuffer, rxLength);
		stats.rxObjectBytes += rxLength;
		stats.rxObjects++;
		mutex.unlock();

		rxState = STATE_SYNC;
		UAVTALK_LOG_DEBUG("UAVTalk: CSum->Sync (OK)");
		break;

	default:
		rxState = STATE_SYNC;
		stats.rxErrors++;
		UAVTALK_LOG_DEBUG("UAVTalk: \?\?\?->Sync"); // Use the escape character for '?' so that the tripgraph isn't triggered.
	}

	// Done
	return true;
}

/** Receive an object. This function process objects received through the telemetry stream.
 * \param[in] type Type of received message (TYPE_OBJ, TYPE_OBJ_REQ, TYPE_OBJ_ACK, TYPE_ACK, TYPE_NACK)
 * \param[in] obj Handle of the received object
 * \param[in] instId The instance ID of UAVOBJ_ALL_INSTANCES for all instances.
 * \param[in] data Data buffer
 * \param[in] length Buffer length
 * \return Success (true), Failure (false)
 */
bool UAVTalk::receiveObject(uint8_t type, uint32_t objId, uint16_t instId, uint8_t *data, size_t length)
{
	//UNUSED(length);
	UAVObject *obj    = NULL;
	bool error        = false;
	bool allInstances = (instId == ALL_INSTANCES);

	// Process message type
	switch (type) {
	case TYPE_OBJ:
		// All instances, not allowed for OBJ messages
		if (!allInstances) {
			// Get object and update its data
			obj = updateObject(objId, instId, data);
			// Check if an ack is pending
			if (obj != NULL) {
				updateAck(obj);
			} else {
				error = true;
			}
		} else {
			error = true;
		}
		break;

	case TYPE_OBJ_ACK:
		// All instances, not allowed for OBJ_ACK messages
		if (!allInstances) {
			// Get object and update its data
			obj = updateObject(objId, instId, data);
			// Transmit ACK
			if (obj != NULL) {
				transmitObject(obj, TYPE_ACK, false);
			} else {
				error = true;
			}
		} else {
			error = true;
		}
		break;

	case TYPE_OBJ_REQ:
		// Get object, if all instances are requested get instance 0 of the object
		if (allInstances) {
			obj = objMngr->getObject(objId);
		} else {
			obj = objMngr->getObject(objId, instId);
		}
		// If object was found transmit it
		if (obj != NULL) {
			transmitObject(obj, TYPE_OBJ, allInstances);
		} else {
			// Object was not found, transmit a NACK with the
			// objId which was not found.
			transmitNack(objId);
			error = true;
		}
		break;

	case TYPE_NACK:
		// All instances, not allowed for NACK messages
		if (!allInstances) {
			// Get object
			obj = objMngr->getObject(objId, instId);
			// Check if object exists:
			if (obj != NULL) {
				updateNack(obj);
			} else {
				error = true;
			}
		}
		break;

	case TYPE_ACK:
		// All instances, not allowed for ACK messages
		if (!allInstances) {
			// Get object
			obj = objMngr->getObject(objId, instId);
			// Check if an ack is pending
			if (obj != NULL) {
				updateAck(obj);
			} else {
				error = true;
			}
		}
		break;

	default:
		error = true;
	}

	// Done
	return !error;
}

/** Update the data of an object from a byte array (unpack).
 * If the object instance could not be found in the list, then a
 * new one is created.
 */
UAVObject *UAVTalk::updateObject(uint32_t objId, uint16_t instId, uint8_t *data)
{
    // Get object
    UAVObject *obj = objMngr->getObject(objId, instId);

    // If the instance does not exist create it
    if (obj == NULL) {
        // Get the object type
        UAVObject *tobj = objMngr->getObject(objId);
        if (tobj == NULL) {
            return NULL;
        }

        // Make sure this is a data object
        UAVDataObject *dobj = dynamic_cast<UAVDataObject *>(tobj);
        if (dobj == NULL) {
            return NULL;
        }

        // Create a new instance, unpack and register
        UAVDataObject *instobj = dobj->clone(instId);
        if (!objMngr->registerObject(instobj)) {
            return NULL;
        }

        instobj->deserialize(data);

        return instobj;
    } else {
        // Unpack data into object instance
        obj->deserialize(data);

        return obj;
    }
}

/** Check if a transaction is pending and if yes complete it.
 */
void UAVTalk::updateNack(UAVObject *obj)
{
	//Q_ASSERT(obj);
	if (!obj) {
		return;
	}

	uint32_t objId = obj->getObjID();

	std::map<uint32_t, Transaction *>::iterator itr = transMap.find(objId);
	if (itr != transMap.end() && (itr->second->obj->getInstID() == obj->getInstID() || itr->second->allInstances)) {
		transMap.erase(objId);
		delete itr->second;

		transactionCompleted(obj, false); // emit signal
	}
}


/** Check if a transaction is pending and if yes complete it.
 */
void UAVTalk::updateAck(UAVObject *obj)
{
	uint32_t objId = obj->getObjID();

	std::map<uint32_t, Transaction *>::iterator itr = transMap.find(objId);
	if (itr != transMap.end() && (itr->second->obj->getInstID() == obj->getInstID() || itr->second->allInstances)) {
		transMap.erase(objId);
		delete itr->second;

		transactionCompleted(obj, true); // emit signal
	}
}


/** Send an object through the telemetry link.
 * \param[in] obj Object to send
 * \param[in] type Transaction type
 * \param[in] allInstances True is all instances of the object are to be sent
 * \return Success (true), Failure (false)
 */
bool UAVTalk::transmitObject(UAVObject *obj, uint8_t type, bool allInstances)
{
	// If all instances are requested on a single instance object it is an error
	if (allInstances && obj->isSingleInstance()) {
		allInstances = false;
	}

	// Process message type
	if (type == TYPE_OBJ || type == TYPE_OBJ_ACK) {
		if (allInstances) {
			// Get number of instances
			ssize_t numInst = objMngr->getNumInstances(obj->getObjID());
			// Send all instances
			for (uint32_t instId = 0; instId < numInst; ++instId) {
				UAVObject *inst = objMngr->getObject(obj->getObjID(), instId);
				transmitSingleObject(inst, type, false);
			}
			return true;
		} else {
			return transmitSingleObject(obj, type, false);
		}

	} else if (type == TYPE_OBJ_REQ) {
		return transmitSingleObject(obj, TYPE_OBJ_REQ, allInstances);

	} else if (type == TYPE_ACK) {
		if (!allInstances) {
			return transmitSingleObject(obj, TYPE_ACK, false);
		} else {
			return false;
		}

	} else {
		return false;
	}
}

/** Transmit a NACK through the telemetry link.
 * \param[in] objId the ObjectID we rejected
 */
bool UAVTalk::transmitNack(uint32_t objId)
{
	int dataOffset = 8;

	txBuffer[0] = SYNC_VAL;
	txBuffer[1] = TYPE_NACK;
	//qToLittleEndian<quint32>(objId, &txBuffer[4]);
	memcpy(&txBuffer[4], &objId, sizeof(objId)); // XXX htole32

	// Calculate checksum
	txBuffer[dataOffset] = updateCRC(0, txBuffer, dataOffset);

	//qToLittleEndian<quint16>(dataOffset, &txBuffer[2]);
	memcpy(&txBuffer[2], &dataOffset, 2); // XXX

	// Send buffer, check that the transmit backlog does not grow above limit
	if (io && io->is_open() /*&& io->bytesToWrite() < TX_BUFFER_SIZE*/) {
		io->write(txBuffer, dataOffset + CHECKSUM_LENGTH);
	} else {
		++stats.txErrors;
		return false;
	}

	// Update stats
	stats.txBytes += 8 + CHECKSUM_LENGTH;

	// Done
	return true;
}


/** Send an object through the telemetry link.
 * \param[in] obj Object handle to send
 * \param[in] type Transaction type
 * \return Success (true), Failure (false)
 */
bool UAVTalk::transmitSingleObject(UAVObject *obj, uint8_t type, bool allInstances)
{
	int32_t length;
	int32_t dataOffset;
	uint32_t objId;
	uint16_t instId;
	uint16_t allInstId = ALL_INSTANCES;

	// Setup type and object id fields
	objId = obj->getObjID();
	txBuffer[0] = SYNC_VAL;
	txBuffer[1] = type;
	//qToLittleEndian<quint32>(objId, &txBuffer[4]);
	memcpy(&txBuffer[4], &objId, sizeof(objId)); // XXX htole32

	// Setup instance ID if one is required
	if (obj->isSingleInstance()) {
		dataOffset = 8;
	} else {
		// Check if all instances are requested
		if (allInstances) {
			//qToLittleEndian<quint16>(allInstId, &txBuffer[8]);
			memcpy(&txBuffer[8], &allInstId, sizeof(allInstId)); // XXX htole16
		} else {
			instId = obj->getInstID();
			//qToLittleEndian<quint16>(instId, &txBuffer[8]);
			memcpy(&txBuffer[8], &instId, sizeof(instId)); // XXX htole16
		}
		dataOffset = 10;
	}

	// Determine data length
	if (type == TYPE_OBJ_REQ || type == TYPE_ACK) {
		length = 0;
	} else {
		length = obj->getNumBytes();
	}

	// Check length
	if (length >= MAX_PAYLOAD_LENGTH) {
		return false;
	}

	// Copy data (if any)
	if (length > 0) {
		if (!obj->serialize(&txBuffer[dataOffset])) {
			return false;
		}
	}

	//qToLittleEndian<quint16>(dataOffset + length, &txBuffer[2]);
	uint16_t t_len = dataOffset + length;
	memcpy(&txBuffer[2], &t_len, sizeof(t_len)); // XXX htole16

	// Calculate checksum
	txBuffer[dataOffset + length] = updateCRC(0, txBuffer, dataOffset + length);

	// Send buffer, check that the transmit backlog does not grow above limit
	if (io && io->is_open() /*&& io->bytesToWrite() < TX_BUFFER_SIZE*/) {
		io->write(txBuffer, dataOffset + length + CHECKSUM_LENGTH);
	} else {
		++stats.txErrors;
		return false;
	}

	// Update stats
	++stats.txObjects;
	stats.txBytes += dataOffset + length + CHECKSUM_LENGTH;
	stats.txObjectBytes += length;

	// Done
	return true;
}

/** Update the crc value with new data.
 *
 * Generated by pycrc v0.7.5, http://www.tty1.net/pycrc/
 * using the configuration:
 *    Width        = 8
 *    Poly         = 0x07
 *    XorIn        = 0x00
 *    ReflectIn    = False
 *    XorOut       = 0x00
 *    ReflectOut   = False
 *    Algorithm    = table-driven
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param length   Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 */
uint8_t UAVTalk::updateCRC(uint8_t crc, const uint8_t data)
{
    return crc_table[crc ^ data];
}

uint8_t UAVTalk::updateCRC(uint8_t crc, const uint8_t *data, size_t length)
{
    while (length--) {
        crc = crc_table[crc ^ *data++];
    }
    return crc;
}

