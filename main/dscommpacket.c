//
//  dscommpacket.c
//
//
//  Contents are mainly the useful bits of the WPILib HAL I yoinked and translated into C.
//

#include "dscommpacket.h"

void DSCommJoystickPacket_ResetUdp(DSCommJoystickPacket * packet) {
    memset(&(packet->axes), 0, sizeof(packet->axes));
    memset(&(packet->buttons), 0, sizeof(packet->buttons));
    memset(&(packet->povs), 0, sizeof(packet->povs));
}

void DSCommJoystickPacket_ResetTcp(DSCommJoystickPacket * packet) {
    memset(&(packet->descriptor), 0, sizeof(packet->descriptor));
}



void DSCommPacket_Init(DSCommPacket * pckt) {
	for (int i = 0; i < 6; i++) {
    	DSCommJoystickPacket_ResetTcp(&(pckt->m_joystick_packets[i]));
    	DSCommJoystickPacket_ResetUdp(&(pckt->m_joystick_packets[i]));
	}
	pckt->matchInfo.gameSpecificMessageSize = 0;
}

void DSCommPacket_SetControl(DSCommPacket * pckt, uint8_t control, uint8_t request) {
	memset(&(pckt->m_control_word), 0, sizeof(pckt->m_control_word));
	pckt->m_control_word.enabled = (control & kEnabled) != 0;
	pckt->m_control_word.autonomous = (control & kAutonomous) != 0;
	pckt->m_control_word.test = (control & kTest) != 0;
	pckt->m_control_word.eStop = (control & kEmergencyStop) != 0;
	pckt->m_control_word.fmsAttached = (control & kFMS_Attached) != 0;
	pckt->m_control_word.dsAttached = (request & kRequestNormalMask) != 0;
	pckt->m_control_sent = control;
}

void DSCommPacket_SetAlliance(DSCommPacket * pckt, uint8_t station_code) {
 	pckt->m_alliance_station = (HAL_AllianceStationID)station_code	;
}

void DSCommPacket_ReadMatchtimeTag(DSCommPacket * pckt, uint8_t* tagData, size_t size) {
	if (size < 6) return;

	uint32_t store = tagData[2] << 24;
	store |= tagData[3] << 16;
	store |= tagData[4] << 8;
	store |= tagData[5];

	float matchTime = 0;

	memcpy(&matchTime, &store, sizeof(float));
	pckt->m_match_time = matchTime;
}

void DSCommPacket_ReadJoystickTag(DSCommPacket * pckt, uint8_t* tagData, size_t size, int index) {
	DSCommJoystickPacket* stick = &(pckt->m_joystick_packets[index]);
	DSCommJoystickPacket_ResetUdp(stick);

	if (size ==  2) {
		return;
	}

	tagData = tagData + 2; //thanks, i hate it.

	// Read axes
	int axesLength = tagData[0];
	for (int i = 0; i < axesLength; i++) {
		int8_t value = tagData[i + 1];
		if (value < 0) {
		 	stick->axes.axes[i] = value / 128.0;
		} else {
		 	stick->axes.axes[i] = value / 127.0;
		}
	}
	stick->axes.count = axesLength;
	tagData += 1 + axesLength;

	// Read Buttons
	int buttonCount = tagData[0];
	int numBytes = (buttonCount + 7) / 8;
	stick->buttons.buttons = 0;
	for (int i = 0; i < numBytes; i++) {
		stick->buttons.buttons |= tagData[numBytes - i] << (8 * (i));
	}
	stick->buttons.count = buttonCount;

	tagData += 1 + numBytes;

	int povsLength = tagData[0];
	for (int i = 0; i < povsLength * 2; i += 2) {
		stick->povs.povs[i] = (tagData[i + 1] << 8) | tagData[i + 2];
	}

	stick->povs.count = povsLength;
	return;
}

void DSCommPacket_DecodeTCP(DSCommPacket * pckt, uint8_t* tcpData, size_t size) {
	size_t prog = 0;
  	while (prog < size) {
	    int tagLength = tcpData[0] << 8 | tcpData[1];
	    uint8_t* tagPacket = tcpData;
	    size_t tagPacketLength = tagLength + 2;

	    if (tagLength == 0) {
	    	return;
	    }
	    switch (tcpData[2]) {
	    	case kJoystickNameTag:
	        	DSCommPacket_ReadJoystickDescriptionTag(pckt,tagPacket,tagPacketLength);
	        break;
	    	case kGameDataTag:
	        	DSCommPacket_ReadGameSpecificMessageTag(pckt,tagPacket,tagPacketLength);
	        break;
	    	case kMatchInfoTag:
	        	DSCommPacket_ReadNewMatchInfoTag(pckt,tagPacket,tagPacketLength);
	        break;
	    }
    tcpData += tagLength + 2;
    prog += tagLength + 2;
	}
}

void DSCommPacket_DecodeUDP(DSCommPacket * pckt, uint8_t* udpData, size_t size) {
	if (size < 6) return;
	// Decode fixed header
	pckt->m_hi = udpData[0];
	pckt->m_lo = udpData[1];
	// Comm Version is packet 2, ignore
	DSCommPacket_SetControl(pckt,udpData[3], udpData[4]);
	DSCommPacket_SetAlliance(pckt,udpData[5]);

	// Return if packet finished
	if (size == 6) return;

	// Else, handle tagged data
	udpData += 6;

	int joystickNum = 0;

	size_t prog = 6;
	// Loop to handle multiple tags
	while (prog < size) {
	uint8_t tagLength = udpData[0];
	uint8_t* tagPacket = udpData;
	size_t tagPacketLength = tagLength + 1;

	switch (udpData[1]) {
		case kJoystickDataTag:
	    	DSCommPacket_ReadJoystickTag(pckt,tagPacket,tagPacketLength, joystickNum);
	   		joystickNum++;
	    break;
	 	case kMatchTimeTag:
	    	DSCommPacket_ReadMatchtimeTag(pckt,tagPacket,tagPacketLength);
	    break;
	}
	udpData += tagLength + 1;
	prog += tagLength + 1;
	}
}

void DSCommPacket_ReadNewMatchInfoTag(DSCommPacket * pckt, uint8_t* data, size_t size) {
  	// Size 2 bytes, tag 1 byte
  	if (size <= 3) return;

  	int nameLength = MIN(data[3], sizeof(pckt->matchInfo.eventName) - 1);

  	for (int i = 0; i < nameLength; i++) {
    	pckt->matchInfo.eventName[i] = data[4 + i];
  	}

  	pckt->matchInfo.eventName[nameLength] = '\0';

  	data += 4 + nameLength;

  	if (size < 4) return;

  	pckt->matchInfo.matchType = (HAL_MatchType)data[0];  // None, Practice, Qualification, Elimination, Test
  	pckt->matchInfo.matchNumber = (data[1] << 8) | data[2];
	pckt->matchInfo.replayNumber = data[3];
}

void DSCommPacket_ReadGameSpecificMessageTag(DSCommPacket * pckt, uint8_t* data, size_t size) {
	if (size <= 3) return;

	int length = MIN(((data[0] << 8) | data[1]) - 1,sizeof(pckt->matchInfo.gameSpecificMessage));
	for (int i = 0; i < length; i++) {
		pckt->matchInfo.gameSpecificMessage[i] = data[3 + i];
	}

	pckt->matchInfo.gameSpecificMessageSize = length;
}

void DSCommPacket_ReadJoystickDescriptionTag(DSCommPacket * pckt, uint8_t* data, size_t size) {
	if (size < 3) return;
	data += 3;
	int joystickNum = data[0];
	DSCommJoystickPacket* packet = &(pckt->m_joystick_packets[joystickNum]);
	DSCommJoystickPacket_ResetTcp(packet);
	packet->descriptor.isXbox = data[1] != 0 ? 1 : 0;
	packet->descriptor.type = data[2];
	int nameLength = MIN(data[3], (sizeof(packet->descriptor.name) - 1));
	for (int i = 0; i < nameLength; i++) {
		packet->descriptor.name[i] = data[4 + i];
	}
	data += 4 + nameLength;
	packet->descriptor.name[nameLength] = '\0';
	int axesCount = data[0];
	packet->descriptor.axisCount = axesCount;
	for (int i = 0; i < axesCount; i++) {
		packet->descriptor.axisTypes[i] = data[1 + i];
	}
	data += 1 + axesCount;

	packet->descriptor.buttonCount = data[0];
	packet->descriptor.povCount = data[1];
}


void DSCommPacket_SetupSendBuffer(DSCommPacket * pckt, uint8_t* buf) {
  DSCommPacket_SetupSendHeader(pckt,buf);
}

void DSCommPacket_SetupSendHeader(DSCommPacket * pckt, uint8_t* buf) {
  static const uint8_t kCommVersion = 0x01;

  // High low packet index, comm version
  buf[0] = pckt->m_hi;
  buf[1] = pckt->m_lo;
  buf[2] = kCommVersion;

  // Control word and status check
  buf[3] = pckt->m_control_sent;
  buf[4] = kRobotHasCode;
  // Battery voltage high and low
  buf[5] = 5;
  buf[6] = 0;

  // Request (Always 0)
  buf[7] = 0;
}
