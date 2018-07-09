//
//  Library for interacting with Vita's MTP connection
//  VitaMTP
//
//  Created by Yifan Lu
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef VitaMTP_h
#define VitaMTP_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

/**
 * Unopened Vita USB device
 *
 * @see VitaMTP_Get_Vitas()
 */
struct vita_raw_device
{
    char serial[18];
    void *data;
};

/**
 * VitaMTP Event structure
 *
 * @see VitaMTP_Read_Event()
 */
struct LIBVitaMTP_event
{
    uint16_t Code;
    uint32_t SessionID;
    uint32_t Transaction_ID;
    uint32_t Param1;
    uint32_t Param2;
    uint32_t Param3;
};

/**
 * Contains protocol version, Vita's system version,
 * and recommended thumbnail sizes.
 *
 * @see VitaMTP_GetVitaInfo()
 */
struct vita_info
{
    char responderVersion[6]; // max: XX.XX\0
    int protocolVersion;
    char *onlineId;
    char *modelInfo;
    struct
    {
        int type;
        int codecType;
        int width;
        int height;
    } photoThumb;
    struct
    {
        int type;
        int codecType;
        int width;
        int height;
        int duration;
    } videoThumb;
    struct
    {
        int type;
        int codecType;
        int width;
        int height;
    } musicThumb;
    struct
    {
        int type;
        int codecType;
        int width;
        int height;
    } gameThumb;
};

/**
 * Contains information about the PC client.
 * Use VitaMTP_Data_Initiator_New() to create and VitaMTP_Data_Free_Initiator()
 * to free.
 *
 *
 * @see VitaMTP_SendInitiatorInfo()
 */
struct initiator_info
{
    char *platformType;
    char *platformSubtype;
    char *osVersion;
    char *version;
    int protocolVersion;
    char *name;
    int applicationType;
};

/**
 * A linked list of accounts on the Vita.
 * Currently unimplemented by the Vita.
 *
 *
 * @see VitaMTP_GetSettingInfo()
 */
struct settings_info
{
    struct account   // remember to free each string!
    {
        char *userName;
        char *signInId;
        char *accountId;
        char *countryCode;
        char *langCode;
        char *birthday;
        char *onlineUser;
        char *passwd;
        struct account *next_account;
    } current_account;
};

/**
 * Returned by Vita to specify what objects
 * to return metadata for.
 *
 *
 * @see VitaMTP_GetSettingInfo()
 */
#pragma pack(push, 1)
struct browse_info
{
    uint32_t ohfiParent;
    uint32_t unk1; // seen: 0 always
    uint32_t index;
    uint32_t numObjects;
    uint32_t unk4; // seen: 0 always
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;
#pragma pack(pop)

/**
 * Used by the metadata structure.
 */
enum DataType
{
    Folder = (1 << 0),
    File = (1 << 1),
    App = (1 << 2),
    Thumbnail = (1 << 3),
    SaveData = (1 << 4),
    Music = (1 << 5),
    Photo = (1 << 6),
    Video = (1 << 7),
    Special = (1 << 8),
    Package = (1 << 9)
};

/**
 * Used media metadata.
 */
struct media_track
{
    int type;
    union
    {
        struct media_track_audio
        {
            int padding[2];
            int codecType;
            int bitrate;
        } track_audio;

        struct media_track_video
        {
            int width;
            int height;
            int codecType;
            int bitrate;
            unsigned long duration;
        } track_video;

        struct media_track_photo
        {
            int width;
            int height;
            int codecType;
        } track_photo;
    } data;
};

/**
 * A linked list of metadata for objects.
 * The items outside of the union MUST be set for all
 * data types. After setting dataType, you are required
 * to fill the data in the union for that type.
 *
 * The ohfi is a unique id that identifies an object. This
 * id is used by the Vita to request objects.
 * The title is what is shown on the screen on the Vita.
 * The index is the order that objects are shown on screen.
 *
 * @see VitaMTP_SendObjectMetadata()
 */
struct metadata
{
    int ohfiParent;
    int ohfi;
    unsigned int handle; // only used by PTP object commands
    char *name;
    char *path;
    int type;
    time_t dateTimeCreated; // unix timestamp
    uint64_t size;
    enum DataType dataType;

    union
    {
        struct metadata_thumbnail
        {
            int codecType;
            int width;
            int height;
            int type;
            int orientationType;
            float aspectRatio;
            int fromType;
        } thumbnail;

        struct metadata_saveData
        {
            int padding[2];
            char *title;
            char *dirName;
            char *savedataTitle;
            int statusType;
            char *detail;
            long dateTimeUpdated; // unix timestamp
        } saveData;

        struct metadata_photo
        {
            int numTracks;
            struct media_track *tracks;
            char *title;
            char *fileName;
            int fileFormatType;
            int statusType;
            long dateTimeOriginal;
        } photo;

        struct metadata_music
        {
            int numTracks;
            struct media_track *tracks;
            char *title;
            char *fileName;
            int fileFormatType;
            int statusType;
            char *album;
            char *artist;
        } music;

        struct metadata_video
        {
            int numTracks;
            struct media_track *tracks;
            char *title;
            char *fileName;
            int fileFormatType;
            int statusType;
            long dateTimeUpdated;
            int parentalLevel;
            char *explanation;
            char *copyright;
        } video;
    } data;

    struct metadata *next_metadata;
};

/**
 * A request from the Vita to obtain metadata
 * for the file named.
 *
 *
 * @see VitaMTP_SendObjectStatus()
 */
#pragma pack(push, 1)
struct object_status
{
    uint32_t ohfiRoot;
    uint32_t len;
    char *title;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;

/**
 * Details on part of an object.
 *
 *
 * @see VitaMTP_SendPartOfObjectInit()
 * @see VitaMTP_GetPartOfObject()
 */
struct send_part_init
{
    uint32_t ohfi;
    uint64_t offset;
    uint64_t size;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;


/**
 * Information on a HTTP object request.
 *
 *
 * @see VitaMTP_SendHttpObjectPropFromURL()
 */
struct http_object_prop
{
    uint64_t size;
    uint8_t timestamp_len;
    char *timestamp;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;


/**
 * Command from the Vita to perform an operation.
 *
 *
 * @see VitaMTP_OperateObject()
 */
struct operate_object
{
    uint32_t cmd;
    uint32_t ohfi;
    uint32_t unk1;
    uint32_t len;
    char *title;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;


/**
 * Command from the Vita to treat an object.
 *
 *
 * @see VitaMTP_OperateObject()
 */
struct treat_object
{
    uint32_t ohfiParent;
    uint32_t unk0;
    uint32_t handle;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;
#pragma pack(pop)


/**
 * Information on the object to be sent from
 * the Vita. data will contain at most 0x400
 * bytes of data from the object (or the size
 * of it, whichever is smaller.
 *
 *
 * @see VitaMTP_CheckExistance()
 */
struct existance_object
{
    uint64_t size;
    char *name;
    unsigned int data_length;
    char data[0x400];
};

/**
 * Data sent to Vita to confirm copying.
 *
 *
 * @see VitaMTP_SendCopyConfirmationInfo()
 */
#pragma pack(push, 1)
struct copy_confirmation_info
{
    uint32_t count;
    uint32_t ohfi[];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;
#pragma pack(pop)


/**
 * Capability information
 *
 *
 * @see VitaMTP_GetVitaCapabilityInfo()
 * @see VitaMTP_SendPCCapabilityInfo()
 */
struct capability_info
{
    char *version;
    struct capability_info_function
    {
        char *type;
        struct capability_info_format
        {
            char *contentType;
            char *codec;
            struct capability_info_format *next_item;
        } formats;
        struct capability_info_option
        {
            char *name;
            struct capability_info_option *next_item;
        } options;
        struct capability_info_function *next_item;
    } functions;
};

/**
 * For identifying the current Vita
 */
enum vita_device_type
{
    VitaDeviceUndefined,
    VitaDeviceUSB,
    VitaDeviceWireless
};

/**
 * Wireless host information
 *
 *
 * @see VitaMTP_Get_First_Wireless_Device()
 */
struct wireless_host_info
{
    const char *guid;
    const char *type;
    const char *name;
    int port;
};

/**
 * Wireless client information
 *
 *
 * @see VitaMTP_Get_First_Wireless_Device()
 */
struct wireless_vita_info
{
    const char *deviceid;
    const char *type;
    const char *mac_addr;
    const char *name;
};

/**
 * These make referring to the structs easier.
 */
typedef struct vita_raw_device vita_raw_device_t;
typedef struct vita_device vita_device_t;
typedef struct LIBVitaMTP_event vita_event_t;
typedef struct vita_info vita_info_t;
typedef struct initiator_info initiator_info_t;
typedef struct settings_info settings_info_t;
typedef struct browse_info browse_info_t;
typedef struct metadata metadata_t;
typedef struct thumbnail thumbnail_t;
typedef struct object_status object_status_t;
typedef struct send_part_init send_part_init_t;
typedef struct http_object_prop http_object_prop_t;
typedef struct operate_object operate_object_t;
typedef struct treat_object treat_object_t;
typedef struct existance_object existance_object_t;
typedef struct copy_confirmation_info copy_confirmation_info_t;
typedef struct capability_info capability_info_t;
typedef struct wireless_host_info wireless_host_info_t;
typedef struct wireless_vita_info wireless_vita_info_t;

typedef int (*device_registered_callback_t)(const char *deviceid);
typedef int (*register_device_callback_t)(wireless_vita_info_t *info, int *p_err);
typedef void (*device_reg_complete_callback_t)(void);
typedef int (*read_callback_t)(unsigned char *data, unsigned long wantlen, unsigned long *gotlen);
typedef int (*write_callback_t)(const unsigned char *data, unsigned long size, unsigned long *written);

/**
 * The callback type definition. Notice that a progress percentage ratio
 * is easy to calculate by dividing <code>sent</code> by
 * <code>total</code>.
 * @param sent the number of bytes sent so far
 * @param total the total number of bytes to send
 * @param data a user-defined dereferencable pointer
 * @return if anything else than 0 is returned, the current transfer will be
 *         interrupted / cancelled.
 */
typedef int (* VitaMTP_progressfunc_t)(uint64_t const sent, uint64_t const total,
                                       void const *const data);


/**
 * This is the USB information for the Vita.
 */
#define VITA_PID 0x04E4
#define VITA_VID 0x054C

/**
 * Version information on VitaMTP.
 */
#define VITAMTP_VERSION_MAJOR 2
#define VITAMTP_VERSION_MINOR 5
#define VITAMTP_PROTOCOL_FW_3_30 1900010
#define VITAMTP_PROTOCOL_FW_3_10 1800010
#define VITAMTP_PROTOCOL_FW_3_00 1700010
#define VITAMTP_PROTOCOL_FW_2_60 1600010
#define VITAMTP_PROTOCOL_FW_2_10 1500010
#define VITAMTP_PROTOCOL_FW_2_00 1400010
#define VITAMTP_PROTOCOL_FW_1_80 1300010
#define VITAMTP_PROTOCOL_FW_1_60 1200010
#define VITAMTP_PROTOCOL_FW_1_50 1100010
#define VITAMTP_PROTOCOL_FW_1_00 1000010
#define VITAMTP_PROTOCOL_MAX_VERSION VITAMTP_PROTOCOL_FW_3_30
#define VITAMTP_WIRELESS_FW_2_00 1000000
#define VITAMTP_WIRELESS_MAX_VERSION VITAMTP_WIRELESS_FW_2_00

/**
 * PTP event IDs from Sony's Vita extensions to MTP.
 */
#ifndef PTP_RC_OK
#define PTP_RC_OK 0x2001
#endif

#ifndef PTP_RC_GeneralError
#define PTP_RC_GeneralError 0x2002
#endif

#ifndef PTP_ERROR_CANCEL
#define PTP_ERROR_CANCEL 0x02FB
#endif

#define PTP_EC_VITA_RequestSendNumOfObject 0xC104
#define PTP_EC_VITA_RequestSendObjectMetadata 0xC105
#define PTP_EC_VITA_RequestSendObject 0xC107
#define PTP_EC_VITA_RequestCancelTask 0xC108
#define PTP_EC_VITA_RequestSendHttpObjectFromURL 0xC10B
#define PTP_EC_VITA_Unknown1 0xC10D
#define PTP_EC_VITA_RequestSendObjectStatus 0xC10F
#define PTP_EC_VITA_RequestSendObjectThumb 0xC110
#define PTP_EC_VITA_RequestDeleteObject 0xC111
#define PTP_EC_VITA_RequestGetSettingInfo 0xC112
#define PTP_EC_VITA_RequestSendHttpObjectPropFromURL 0xC113
#define PTP_EC_VITA_RequestSendPartOfObject 0xC115
#define PTP_EC_VITA_RequestOperateObject 0xC117
#define PTP_EC_VITA_RequestGetPartOfObject 0xC118
#define PTP_EC_VITA_RequestSendStorageSize 0xC119
#define PTP_EC_VITA_RequestCheckExistance 0xC120
#define PTP_EC_VITA_RequestGetTreatObject 0xC122
#define PTP_EC_VITA_RequestSendCopyConfirmationInfo 0xC123
#define PTP_EC_VITA_RequestSendObjectMetadataItems 0xC124
#define PTP_EC_VITA_RequestSendNPAccountInfo 0xC125
#define PTP_EC_VITA_RequestTerminate 0xC126

/**
 * Command IDs from Sony's Vita extensions to MTP.
 */
#define PTP_OC_VITA_GetVitaInfo 0x9511
#define PTP_OC_VITA_SendNumOfObject 0x9513
#define PTP_OC_VITA_GetBrowseInfo 0x9514
#define PTP_OC_VITA_SendObjectMetadata 0x9515
#define PTP_OC_VITA_SendObjectThumb 0x9516
#define PTP_OC_VITA_ReportResult 0x9518
#define PTP_OC_VITA_SendInitiatorInfo 0x951C
#define PTP_OC_VITA_GetUrl 0x951F
#define PTP_OC_VITA_SendHttpObjectFromURL 0x9520
#define PTP_OC_VITA_SendNPAccountInfo 0x9523
#define PTP_OC_VITA_GetSettingInfo 0x9524
#define PTP_OC_VITA_SendObjectStatus 0x9528
#define PTP_OC_VITA_SendHttpObjectPropFromURL 0x9529
#define PTP_OC_VITA_SendHostStatus 0x952A
#define PTP_OC_VITA_SendPartOfObjectInit 0x952B
#define PTP_OC_VITA_SendPartOfObject 0x952C
#define PTP_OC_VITA_OperateObject 0x952E
#define PTP_OC_VITA_GetPartOfObject 0x952F
#define PTP_OC_VITA_SendStorageSize 0x9533
#define PTP_OC_VITA_GetTreatObject 0x9534
#define PTP_OC_VITA_SendCopyConfirmationInfo 0x9535
#define PTP_OC_VITA_SendObjectMetadataItems 0x9536
#define PTP_OC_VITA_SendCopyConfirmationInfoInit 0x9537
#define PTP_OC_VITA_KeepAlive 0x9538
#define PTP_OC_VITA_Unknown1 0x953A
#define PTP_OC_VITA_GetVitaCapabilityInfo 0x953B
#define PTP_OC_VITA_SendPCCapabilityInfo 0x953C

/**
 * Result codes from Sony's Vita extensions to MTP.
 */
#define PTP_RC_VITA_Invalid_Context 0xA001
#define PTP_RC_VITA_Not_Ready 0xA002
#define PTP_RC_VITA_Invalid_OHFI 0xA003
#define PTP_RC_VITA_Invalid_Data 0xA004
#define PTP_RC_VITA_Too_Large_Data 0xA005
#define PTP_RC_VITA_Invalid_Result_Code 0xA006
#define PTP_RC_VITA_Cannot_Access_Server 0xA008
#define PTP_RC_VITA_Cannot_Read_Info 0xA009
#define PTP_RC_VITA_Not_Exist_Object 0xA00A
#define PTP_RC_VITA_Invalid_Protocol_Version 0xA00B
#define PTP_RC_VITA_Invalid_App_Version 0xA00C
#define PTP_RC_VITA_Disconnect_Network 0xA00D
#define PTP_RC_VITA_Cannot_Cancel_Operation 0xA00F
#define PTP_RC_VITA_Timeout_Server 0xA010
#define PTP_RC_VITA_Already_Finish 0xA011
#define PTP_RC_VITA_Invalid_Permission 0xA012
#define PTP_RC_VITA_Busy_Object 0xA013
#define PTP_RC_VITA_Locked_Object 0xA014
#define PTP_RC_VITA_Under_Maintenance 0xA017
#define PTP_RC_VITA_Failed_Download 0xA018
#define PTP_RC_VITA_Not_Support_Property 0xA019
#define PTP_RC_VITA_Over_End 0xA01A
#define PTP_RC_VITA_Cannot_Access_DB 0xA01B
#define PTP_RC_VITA_Reconstructing_DB 0xA01C
#define PTP_RC_VITA_Invalid_Charactor 0xA01D
#define PTP_RC_VITA_Long_String 0xA01E
#define PTP_RC_VITA_Failed_Operate_Object 0xA01F
#define PTP_RC_VITA_Canceled 0xA020
#define PTP_RC_VITA_Invalid_Account_Info 0xA021
#define PTP_RC_VITA_Same_Object 0xA022
#define PTP_RC_VITA_Different_Object 0xA023
#define PTP_RC_VITA_Invalid_Metadata_Item 0xA024
#define PTP_RC_VITA_Same_OHFI 0xA025
#define PTP_RC_VITA_Folder_WriteProtected 0xA027
#define PTP_RC_VITA_NP_Error 0xA029

/**
 * Content types from Sony's Vita extensions to MTP.
 */
#define PTP_OFC_Unknown1 0xB005
#define PTP_OFC_Unknown2 0xB006
#define PTP_OFC_PSPGame 0xB007
#define PTP_OFC_PSPSave 0xB00A
#define PTP_OFC_Unknown3 0xB00B
#define PTP_OFC_Unknown4 0xB00F
#define PTP_OFC_Unknown5 0xB010
#define PTP_OFC_VitaGame 0xB014

/**
 * Default MTP StorageID for Vita
 */
#define VITA_STORAGE_ID 0x00010001

/**
 * Host statuses.
 *
 * @see VitaMTP_SendHostStatus()
 */
#define VITA_HOST_STATUS_Connected 0x0
#define VITA_HOST_STATUS_Unknown1 0x1
#define VITA_HOST_STATUS_Deactivate 0x2
#define VITA_HOST_STATUS_EndConnection 0x3
#define VITA_HOST_STATUS_StartConnection 0x4
#define VITA_HOST_STATUS_Unknown2 0x5

/**
 * Filters for showing objects.
 * Each object contains their own ohfi and
 * their parent's ohfi.
 * These are the "master" ohfi.
 *
 * @see VitaMTP_GetBrowseInfo()
 */
#define VITA_OHFI_MUSIC 0x01
#define VITA_OHFI_PHOTO 0x02
#define VITA_OHFI_VIDEO 0x03
#define VITA_OHFI_PACKAGE 0x05
#define VITA_OHFI_BACKUP 0x06
#define VITA_OHFI_VITAAPP 0x0A
#define VITA_OHFI_PSPAPP 0x0D
#define VITA_OHFI_PSPSAVE 0x0E
#define VITA_OHFI_PSXAPP 0x10
#define VITA_OHFI_PSMAPP 0x12

#define VITA_OHFI_SUBNONE 0x00
#define VITA_OHFI_SUBFILE 0x01

#define VITA_DIR_TYPE_MASK_MUSIC        0x1000000
#define VITA_DIR_TYPE_MASK_PHOTO        0x2000000
#define VITA_DIR_TYPE_MASK_VIDEO        0x4000000
#define VITA_DIR_TYPE_MASK_ROOT         0x0010000
#define VITA_DIR_TYPE_MASK_REGULAR      0x0000001
#define VITA_DIR_TYPE_MASK_ALL          0x0000200
#define VITA_DIR_TYPE_MASK_ARTISTS      0x0000004
#define VITA_DIR_TYPE_MASK_ALBUMS       0x0000005
#define VITA_DIR_TYPE_MASK_GENRES       0x0000006
#define VITA_DIR_TYPE_MASK_PLAYLISTS    0x0000007
#define VITA_DIR_TYPE_MASK_SONGS        0x0000008
#define VITA_DIR_TYPE_MASK_MONTH        0x000000A

#define VITA_TRACK_TYPE_AUDIO   0x1
#define VITA_TRACK_TYPE_VIDEO   0x2
#define VITA_TRACK_TYPE_PHOTO   0x3

/**
 * Commands for operate object.
 *
 * @see VitaMTP_OperateObject()
 */
#define VITA_OPERATE_CREATE_FOLDER 1
#define VITA_OPERATE_UNKNOWN 2
#define VITA_OPERATE_RENAME 3
#define VITA_OPERATE_CREATE_FILE 4

#define MASK_SET(v,m) (((v) & (m)) == (m))

#define VitaMTP_DEBUG       15
#define VitaMTP_VERBOSE     7
#define VitaMTP_INFO        3
#define VitaMTP_ERROR       1
#define VitaMTP_NONE        0
#define VitaMTP_Log(mask, format, ...) \
    do { \
        if (MASK_SET (g_VitaMTP_logmask, mask)) { \
            if (mask == VitaMTP_DEBUG) { \
                fprintf(stderr, "VitaMTP %s[%d]: " format, __FUNCTION__, __LINE__, __VA_ARGS__); \
            } else { \
                fprintf(stderr, "VitaMTP: " format, __VA_ARGS__); \
            } \
        } \
    } while (0)

// TODO: Const correctness

/**
 * Functions to interact with device
 */
void VitaMTP_Release_Device(vita_device_t *device);
int VitaMTP_Read_Event(vita_device_t *device, vita_event_t *event);
int VitaMTP_Peek_Event(vita_device_t *device, vita_event_t *event);
const char *VitaMTP_Get_Identification(vita_device_t *device);
enum vita_device_type VitaMTP_Get_Device_Type(vita_device_t *device);
uint16_t VitaMTP_SendData(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned char *data,
                          unsigned int len);
uint16_t VitaMTP_SendData_Callback(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned int len,
                                   read_callback_t read_callback);
uint16_t VitaMTP_GetData(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned char **p_data,
                         unsigned int *p_len);

/**
 * Function for USB devices
 */
vita_device_t *VitaMTP_Open_USB_Vita(vita_raw_device_t *raw_device);
void VitaMTP_Release_USB_Device(vita_device_t *device);
int VitaMTP_Get_USB_Vitas(vita_raw_device_t **p_raw_devices);
void VitaMTP_Unget_USB_Vitas(vita_raw_device_t *raw_devices, int numdevs);
vita_device_t *VitaMTP_Get_First_USB_Vita(void);
int VitaMTP_USB_Init(void);
int VitaMTP_USB_Clear(vita_device_t *vita_device);
int VitaMTP_USB_Reset(vita_device_t *vita_device);
void VitaMTP_USB_Exit(void);

/**
 * Funcions for wireless devices
 */
int VitaMTP_Broadcast_Host(wireless_host_info_t *info, unsigned int host_addr);
void VitaMTP_Stop_Broadcast(void);
void VitaMTP_Release_Wireless_Device(vita_device_t *device);
vita_device_t *VitaMTP_Get_First_Wireless_Vita(wireless_host_info_t *info, unsigned int host_addr, device_registered_callback_t is_registered,
                                               register_device_callback_t create_register_pin, device_reg_complete_callback_t reg_complete);
int VitaMTP_Get_Device_IP(vita_device_t *device);
void VitaMTP_Cancel_Get_Wireless_Vita(void);

/**
 * Functions to handle MTP commands
 */
void VitaMTP_Set_Logging(int logmask);
uint16_t VitaMTP_GetVitaInfo(vita_device_t *device, vita_info_t *info);
uint16_t VitaMTP_SendNumOfObject(vita_device_t *device, uint32_t event_id, uint32_t num);
uint16_t VitaMTP_GetBrowseInfo(vita_device_t *device, uint32_t event_id, browse_info_t *info);
uint16_t VitaMTP_SendObjectMetadata(vita_device_t *device, uint32_t event_id, metadata_t *metas);
uint16_t VitaMTP_SendObjectThumb(vita_device_t *device, uint32_t event_id, metadata_t *meta, unsigned char *thumb_data,
                                 uint64_t thumb_len);
uint16_t VitaMTP_ReportResult(vita_device_t *device, uint32_t event_id, uint16_t result);
uint16_t VitaMTP_ReportResultWithParam(vita_device_t *device, uint32_t event_id, uint16_t result, uint32_t param);
uint16_t VitaMTP_SendInitiatorInfo(vita_device_t *device, initiator_info_t *info);
uint16_t VitaMTP_GetUrl(vita_device_t *device, uint32_t event_id, char **url);
uint16_t VitaMTP_SendHttpObjectFromURL(vita_device_t *device, uint32_t event_id, void *data, unsigned int len);
uint16_t VitaMTP_SendNPAccountInfo(vita_device_t *device, uint32_t event_id, unsigned char *data,
                                   unsigned int len); // unused?
uint16_t VitaMTP_GetSettingInfo(vita_device_t *device, uint32_t event_id, settings_info_t **p_info);
uint16_t VitaMTP_SendObjectStatus(vita_device_t *device, uint32_t event_id, object_status_t *status);
uint16_t VitaMTP_SendHttpObjectPropFromURL(vita_device_t *device, uint32_t event_id, http_object_prop_t *prop);
uint16_t VitaMTP_SendHostStatus(vita_device_t *device, uint32_t status);
uint16_t VitaMTP_SendPartOfObjectInit(vita_device_t *device, uint32_t event_id, send_part_init_t *init);
uint16_t VitaMTP_SendPartOfObject(vita_device_t *device, uint32_t event_id, unsigned char *object_data,
                                  uint64_t object_len);
uint16_t VitaMTP_OperateObject(vita_device_t *device, uint32_t event_id, operate_object_t *op_object);
uint16_t VitaMTP_GetPartOfObject(vita_device_t *device, uint32_t event_id, send_part_init_t *init,
                                 unsigned char **data);
uint16_t VitaMTP_SendStorageSize(vita_device_t *device, uint32_t event_id, uint64_t storage_size,
                                 uint64_t available_size);
uint16_t VitaMTP_GetTreatObject(vita_device_t *device, uint32_t event_id, treat_object_t *treat);
uint16_t VitaMTP_SendCopyConfirmationInfoInit(vita_device_t *device, uint32_t event_id,
        copy_confirmation_info_t **p_info);
uint16_t VitaMTP_SendCopyConfirmationInfo(vita_device_t *device, uint32_t event_id, copy_confirmation_info_t *info,
        uint64_t size);
uint16_t VitaMTP_SendObjectMetadataItems(vita_device_t *device, uint32_t event_id, uint32_t *ohfi);
uint16_t VitaMTP_CancelTask(vita_device_t *device, uint32_t cancel_event_id);
uint16_t VitaMTP_KeepAlive(vita_device_t *device, uint32_t event_id);
uint16_t VitaMTP_SendObject(vita_device_t *device, uint32_t *parenthandle, uint32_t *p_handle, metadata_t *p_meta,
                            unsigned char *data);
uint16_t VitaMTP_SendObject_Callback(vita_device_t *device, uint32_t *parenthandle, uint32_t *p_handle, metadata_t *p_meta,
                            read_callback_t read_callback);
uint16_t VitaMTP_GetObject(vita_device_t *device, uint32_t handle, metadata_t *meta, void **p_data,
                           unsigned int *p_len);
uint16_t VitaMTP_GetObject_Callback(vita_device_t *device, uint32_t handle, uint64_t *size, write_callback_t write_callback);
uint16_t VitaMTP_GetObject_Info(vita_device_t *device, uint32_t handle, char **name, int *dataType);
uint16_t VitaMTP_GetObject_Folder(vita_device_t *device, uint32_t handle, uint32_t **p_handles, unsigned int *p_len);
uint16_t VitaMTP_CheckExistance(vita_device_t *device, uint32_t handle, existance_object_t *existance);
uint16_t VitaMTP_GetVitaCapabilityInfo(vita_device_t *device, capability_info_t **p_info);
uint16_t VitaMTP_SendPCCapabilityInfo(vita_device_t *device, capability_info_t *info);
void VitaMTP_RegisterCancelEventId(uint32_t event_id);

/**
 * Functions to parse data
 */
char *VitaMTP_Data_Add_Size_Header(const char *orig, uint32_t len);
char *VitaMTP_Data_Make_Timestamp(time_t time);
int VitaMTP_Data_Info_From_XML(vita_info_t *vita_info, const char *raw_data, const int len);
int VitaMTP_Data_Initiator_To_XML(const initiator_info_t *p_initiator_info, char **data, int *len);
const initiator_info_t *VitaMTP_Data_Initiator_New(const char *host_name, int protocol_version);
void VitaMTP_Data_Free_Initiator(const initiator_info_t *init_info);
int VitaMTP_Data_Settings_From_XML(settings_info_t **p_settings_info, const char *raw_data, const int len);
int VitaMTP_Data_Free_Settings(settings_info_t *settings_info);
int VitaMTP_Data_Metadata_To_XML(const metadata_t *p_metadata, char **data, int *len);
int VitaMTP_Data_Capability_From_XML(capability_info_t **p_info, const char *data, int len);
int VitaMTP_Data_Capability_To_XML(const capability_info_t *info, char **p_data, int *p_len);
int VitaMTP_Data_Free_Capability(capability_info_t *info);
int VitaMTP_Data_Free_VitaInfo(vita_info_t *info);

/**
 * Funtions to initialize/cleanup the library
 */
int VitaMTP_Init(void);
void VitaMTP_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
