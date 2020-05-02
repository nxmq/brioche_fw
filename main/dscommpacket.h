//
//  dscommpacket.h
//
//
//  Contents are mainly the useful bits of the WPILib HAL I yoinked and translated into C.
//
#include "brioche_fw_esp.h"

#define HAL_IO_CONFIG_DATA_SIZE 32
#define HAL_SYS_STATUS_DATA_SIZE 44
#define HAL_USER_STATUS_DATA_SIZE \
(984 - HAL_IO_CONFIG_DATA_SIZE - HAL_SYS_STATUS_DATA_SIZE)

struct HAL_ControlWord {
    uint32_t enabled : 1;
    uint32_t autonomous : 1;
    uint32_t test : 1;
    uint32_t eStop : 1;
    uint32_t fmsAttached : 1;
    uint32_t dsAttached : 1;
    uint32_t control_reserved : 26;
};
typedef struct HAL_ControlWord HAL_ControlWord;


typedef int32_t HAL_AllianceStationID;
enum HAL_AllianceStationID {
    HAL_AllianceStationID_kRed1,
    HAL_AllianceStationID_kRed2,
    HAL_AllianceStationID_kRed3,
    HAL_AllianceStationID_kBlue1,
    HAL_AllianceStationID_kBlue2,
    HAL_AllianceStationID_kBlue3,
};

typedef int32_t HAL_MatchType;
enum HAL_MatchType {
    HAL_kMatchType_none,
    HAL_kMatchType_practice,
    HAL_kMatchType_qualification,
    HAL_kMatchType_elimination,
};
// clang-format on

/* The maximum number of axes that will be stored in a single HALJoystickAxes
 * struct. This is used for allocating buffers, not bounds checking, since
 * there are usually less axes in practice.
 */
#define HAL_kMaxJoystickAxes 12
#define HAL_kMaxJoystickPOVs 12
#define HAL_kMaxJoysticks 6

struct HAL_JoystickAxes {
    int16_t count;
    float axes[HAL_kMaxJoystickAxes];
};
typedef struct HAL_JoystickAxes HAL_JoystickAxes;

struct HAL_JoystickPOVs {
    int16_t count;
    int16_t povs[HAL_kMaxJoystickPOVs];
};
typedef struct HAL_JoystickPOVs HAL_JoystickPOVs;

struct HAL_JoystickButtons {
    uint32_t buttons;
    uint8_t count;
};
typedef struct HAL_JoystickButtons HAL_JoystickButtons;

struct HAL_JoystickDescriptor {
    uint8_t isXbox;
    uint8_t type;
    char name[256];
    uint8_t axisCount;
    uint8_t axisTypes[HAL_kMaxJoystickAxes];
    uint8_t buttonCount;
    uint8_t povCount;
};
typedef struct HAL_JoystickDescriptor HAL_JoystickDescriptor;

struct HAL_MatchInfo {
    char eventName[64];
    HAL_MatchType matchType;
    uint16_t matchNumber;
    uint8_t replayNumber;
    uint8_t gameSpecificMessage[64];
    uint16_t gameSpecificMessageSize;
};
typedef struct HAL_MatchInfo HAL_MatchInfo;


typedef struct {
    HAL_JoystickAxes axes;
    HAL_JoystickButtons buttons;
    HAL_JoystickPOVs povs;
    HAL_JoystickDescriptor descriptor;
} DSCommJoystickPacket;

void DSCommJoystickPacket_ResetUdp(DSCommJoystickPacket * packet);
void DSCommJoystickPacket_ResetTcp(DSCommJoystickPacket * packet);

#define kGameDataTag 0x0e
#define kJoystickNameTag 0x02
#define kMatchInfoTag 0x07

/* UDP Tags*/
#define kJoystickDataTag 0x0c
#define kMatchTimeTag 0x07

/* Control word bits */
#define kTest 0x01
#define kEnabled 0x04
#define kAutonomous 0x02
#define kFMS_Attached 0x08
#define kEmergencyStop 0x80

/* Control request bitmask */
static const uint8_t kRequestNormalMask = 0xF0;

/* Status bits */
static const uint8_t kRobotHasCode = 0x20;

typedef struct DSCommPacket {
    uint8_t m_hi;
    uint8_t m_lo;
    uint8_t m_control_sent;
    HAL_ControlWord m_control_word;
    HAL_AllianceStationID m_alliance_station;
    HAL_MatchInfo matchInfo;
    DSCommJoystickPacket m_joystick_packets[6];
    double m_match_time;
} DSCommPacket;

typedef struct DataStore {
    uint8_t m_frameData[128];
    size_t m_frameLen;
    size_t m_frameSize;
    struct DSCommPacket dsPacket;
    bool dsConnected;
} DataStore;

void DSCommPacket_Init(DSCommPacket * pckt);
void DSCommPacket_SetControl(DSCommPacket * pckt, uint8_t control, uint8_t request);
void DSCommPacket_SetAlliance(DSCommPacket * pckt, uint8_t station_code);
void DSCommPacket_ReadMatchtimeTag(DSCommPacket * pckt, uint8_t* tagData, size_t size);
void DSCommPacket_ReadJoystickTag(DSCommPacket * pckt, uint8_t* tagData, size_t size, int index);
void DSCommPacket_DecodeTCP(DSCommPacket * pckt, uint8_t* tcpData, size_t size);
void DSCommPacket_DecodeUDP(DSCommPacket * pckt, uint8_t* udpData, size_t size);
void DSCommPacket_ReadNewMatchInfoTag(DSCommPacket * pckt, uint8_t* data, size_t size);
void DSCommPacket_ReadGameSpecificMessageTag(DSCommPacket * pckt, uint8_t* data, size_t size);
void DSCommPacket_ReadJoystickDescriptionTag(DSCommPacket * pckt, uint8_t* data, size_t size );
void DSCommPacket_SetupSendBuffer(DSCommPacket * pckt, uint8_t* buf);
void DSCommPacket_SetupSendHeader(DSCommPacket * pckt, uint8_t* buf);