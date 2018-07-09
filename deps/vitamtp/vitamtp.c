//
//  Functions for handling MTP commands
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

#include "pthread-support.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ptp.h"
#include "vitamtp.h"
#include "ptp_defines.h"

int g_VitaMTP_logmask = VitaMTP_ERROR;

volatile uint32_t g_register_cancel_id = 0;
volatile uint32_t g_canceltask_event_id = 0;
volatile int g_event_cancelled = 0;
volatile int g_canceltask_set = 0;
volatile read_callback_t read_callback_func = NULL;
volatile write_callback_t write_callback_func = NULL;

pthread_mutex_t g_event_mutex;
pthread_mutex_t g_cancel_mutex;

/**
 * Set the logging level level.
 *  Valid logmask macros:
 *      VitaMTP_DEBUG: Show USB data and advanced info
 *      VitaMTP_VERBOSE: Show more information
 *      VitaMTP_INFO: Show information about key events
 *      VitaMTP_ERROR: (default) Only show errors
 *      VitaMTP_NONE: Do not print any information
 *
 * @param logmask one of the logmask macros
 */
void VitaMTP_Set_Logging(int logmask)
{
    g_VitaMTP_logmask = logmask;
}

extern void VitaMTP_Data_Init(void);
extern void VitaMTP_Data_Cleanup(void);

extern int VitaMTP_USB_Init(void);
extern void VitaMTP_USB_Exit(void);

// since we don't have access to private fields
extern inline PTPParams *VitaMTP_Get_PTP_Params(vita_device_t *device);

/**
 * Called during initialization to get Vita information.
 *
 * @param device a pointer to the device.
 * @param info a pointer to the vita_info struct to fill
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetVitaInfo(vita_device_t *device, vita_info_t *info)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;
    int ret;
    unsigned char *data;
    unsigned int len;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_GetVitaInfo;
    ptp.Nparam = 0;
    ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);

    if (ret != PTP_RC_OK || len == 0)
    {
        return ret;
    }

    if (VitaMTP_Data_Info_From_XML(info, (char *)data+sizeof(uint32_t), len-sizeof(uint32_t)) != 0) // strip header
    {
        return PTP_RC_GeneralError;
    }

    free(data);
    return ret;
}

/**
 * Sends the number of objects to list.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param num the number of objects to list.
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_SendObjectMetadata()
 */
uint16_t VitaMTP_SendNumOfObject(vita_device_t *device, uint32_t event_id, uint32_t num)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_SendNumOfObject;
    ptp.Nparam = 2;
    ptp.Param1 = event_id;
    ptp.Param2 = num;

    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, 0);
}

/**
 * Gets the filter for the kinds of object to show.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param info a pointer to the browse_info structure to fill.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetBrowseInfo(vita_device_t *device, uint32_t event_id, browse_info_t *info)
{
    unsigned char *data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_GetBrowseInfo, &data, NULL);
    info->ohfiParent = le32atoh(data);
    info->unk1 = le32atoh(data+4);
    info->index = le32atoh(data+8);
    info->numObjects = le32atoh(data+12);
    info->unk4 = le32atoh(data+16);
    free(data);
    return ret;
}

/**
 * Sends a linked list of object metadata for the device to display.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param metas the first metadata in the linked list.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendObjectMetadata(vita_device_t *device, uint32_t event_id, metadata_t *metas)
{
    char *data;
    int len = 0;

    if (VitaMTP_Data_Metadata_To_XML(metas, &data, &len) < 0)
        return PTP_RC_GeneralError;

    uint16_t ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendObjectMetadata, (unsigned char *)data, len);
    free(data);
    return ret;
}

/**
 * Sends thumbnail metadata and image data to the device.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param metas the metadata for the thumbnail. Should be only one.
 * @param thumb_data raw image data for the thumbnail.
 * @param thumb_len size of the image data.
 *  Currently, this cannot be larger than sizeof(uint32_t)
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendObjectThumb(vita_device_t *device, uint32_t event_id, metadata_t *meta, unsigned char *thumb_data,
                                 uint64_t thumb_len)
{
    char *data;
    int len = 0;

    if (VitaMTP_Data_Metadata_To_XML(meta, &data, &len) < 0)
        return PTP_RC_GeneralError;

    long new_length = len + sizeof(uint64_t) + thumb_len;
    char *new_data = malloc(new_length);
    memcpy(new_data, data, len);
    htole64a(new_data + len, thumb_len);
    memcpy(new_data + len + sizeof(uint64_t), thumb_data, thumb_len);
    free(data);

    uint16_t ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendObjectThumb, (unsigned char *)new_data,
                                    (unsigned int)new_length); // TODO: Support huge thumbnails
    free(new_data);
    return ret;
}

/**
 * Called after each transaction to indicate that the PC is done
 * sending commands. You should call this after completing the
 * request of a Vita MTP event, whether that takes one command or
 * a series of them (some commands require initialization).
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param result a PTP result code to send
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_ReportResult(vita_device_t *device, uint32_t event_id, uint16_t result)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_ReportResult;
    ptp.Nparam = 2;
    ptp.Param1 = event_id;
    ptp.Param2 = result;

    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, 0);
}

/**
 * Same as VitaMTP_ReportResult(), but also sends another integer
 * as a parameter.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param result a PTP result code to send
 * @param param a parameter to send
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_ReportResult()
 */
uint16_t VitaMTP_ReportResultWithParam(vita_device_t *device, uint32_t event_id, uint16_t result, uint32_t param)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_ReportResult;
    ptp.Nparam = 3;
    ptp.Param1 = event_id;
    ptp.Param2 = result;
    ptp.Param3 = param;
    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, 0);
}

/**
 * Called during initialization to send information about the PC.
 *
 * @param device a pointer to the device.
 * @param info a pointer to the initiator_info structure.
 *  You should get this with VitaMTP_Data_Initiator_New()
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_Data_Initiator_New()
 */
uint16_t VitaMTP_SendInitiatorInfo(vita_device_t *device, initiator_info_t *info)
{
    char *data;
    int len = 0;

    if (VitaMTP_Data_Initiator_To_XML(info, &data, &len) < 0)
        return PTP_RC_GeneralError;

    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_SendInitiatorInfo;
    ptp.Nparam = 0;
    uint16_t ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, len, (unsigned char **)&data,
                                   0); // plus one for null terminator, which is required on the vita's side
    free(data);
    return ret;
}

/**
 * Gets a URL request from the Vita.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param url a pointer to the char array to be filled with the URL.
 *  Dynamically allocated and should be freed when done.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetUrl(vita_device_t *device, uint32_t event_id, char **url)
{
    unsigned char *data;
    unsigned int len = 0;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_GetUrl, &data, &len);

    if (ret != PTP_RC_OK || len == 0)
    {
        return ret;
    }

    int url_len = le32atoh((uint8_t *)(data+8)); // TODO: Figure out what data[0], data[1] are. They are always zero.
    *url = malloc(url_len);
    memcpy(*url, data+12, url_len);
    return ret;
}

/**
 * Sends the HTTP content from a URL request.
 * This should be called immediately after VitaMTP_GetUrl().
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param data the data from the HTTP response from the URL request.
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_GetUrl()
 */
uint16_t VitaMTP_SendHttpObjectFromURL(vita_device_t *device, uint32_t event_id, void *data, unsigned int len)
{
    unsigned char *buffer = malloc(len + sizeof(uint64_t));
    htole64a(buffer, len);
    memcpy(buffer + sizeof(uint64_t), data, len);
    uint16_t ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendHttpObjectFromURL, buffer, len + sizeof(uint64_t));
    free(buffer);
    return ret;
}

/**
 * Unknown function.
 * Only known information is that data is being sent
 * from the computer.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param data unknown.
 * @param len length of data.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendNPAccountInfo(vita_device_t *device, uint32_t event_id, unsigned char *data,
                                   unsigned int len)  // TODO: Figure out data
{
    return VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendNPAccountInfo, data, len);
}

/**
 * Gets information about the PSN account(s) on the device.
 * Returned p_info must be freed with VitaMTP_Data_Free_Settings()
 * when done.
 * This function currently returns only junk data.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param p_info a pointer to the settings_info pointer to fill.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetSettingInfo(vita_device_t *device, uint32_t event_id, settings_info_t **p_info)
{
    unsigned char *data;
    unsigned int len;
    uint32_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_GetSettingInfo, &data, &len);

    if (ret != PTP_RC_OK || len == 0)
    {
        return ret;
    }

    if (VitaMTP_Data_Settings_From_XML(p_info, (char *)data+sizeof(uint32_t), len-sizeof(uint32_t)) != 0) // strip header
    {
        return PTP_RC_GeneralError;
    }

    free(data);
    return ret;
}

/**
 * Gets the file to send metadata on.
 * VitaMTP_SendObjectMetadata() should be called afterwards.
 * The name is a misnomer, it doesn't send anything, but gets data.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param status a pointer to the object_status structure to fill.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendObjectStatus(vita_device_t *device, uint32_t event_id, object_status_t *status)
{
    uint32_t *data;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_SendObjectStatus, (unsigned char **)&data, NULL);

    if (ret != PTP_RC_OK)
    {
        return ret;
    }

    status->ohfiRoot = le32toh(data[0]);
    status->len = le32toh(data[1]);
    status->title = malloc(status->len);
    memcpy(status->title, (char *)&data[2], status->len);
    free(data);
    return ret;
}

/**
 * Sends additional information on an HTTP object.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param prop a pointer to the http_object_prop structure to send.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendHttpObjectPropFromURL(vita_device_t *device, uint32_t event_id, http_object_prop_t *prop)
{
    int header_len = sizeof(uint64_t) + sizeof(uint8_t);
    unsigned char *data = malloc(header_len + prop->timestamp_len);
    htole64a(data, prop->size);
    *(data + sizeof(uint64_t)) = prop->timestamp_len;
    memcpy(data + header_len, prop->timestamp, prop->timestamp_len);
    int ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendHttpObjectPropFromURL, data,
                               header_len + prop->timestamp_len);
    free(data);
    return ret;
}

/**
 * Sends the PC client's status.
 *
 * @param device a pointer to the device.
 * @param status the client's status
 * @return the PTP result code that the Vita returns.
 * @see VITA_HOST_STATUS
 */
uint16_t VitaMTP_SendHostStatus(vita_device_t *device, uint32_t status)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_SendHostStatus;
    ptp.Nparam = 1;
    ptp.Param1 = status;
    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, 0);
}

/**
 * Gets information on what part of the object to send.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param init a pointer to the send_part_init structure to fill.
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_SendPartOfObject()
 */
uint16_t VitaMTP_SendPartOfObjectInit(vita_device_t *device, uint32_t event_id, send_part_init_t *init)
{
    unsigned char *data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_SendPartOfObjectInit, &data, NULL);

    if (ret != PTP_RC_OK)
    {
        return ret;
    }

    init->ohfi = le32atoh(data);
    init->offset = le64atoh(data+4);
    init->size = le64atoh(data+12);
    free(data);
    return ret;
}

/**
 * Sends a part of the object. You should first call
 * VitaMTP_SendPartOfObjectInit() to find out what to send.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param object_data an array containing the data to send.
 * @param object_len the size of that data to send.
 *  Currently, this cannot be larger than sizeof(uint32_t)
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_SendPartOfObjectInit()
 */
uint16_t VitaMTP_SendPartOfObject(vita_device_t *device, uint32_t event_id, unsigned char *object_data,
                                  uint64_t object_len)
{
    unsigned char *data;
    unsigned long len = object_len + sizeof(uint64_t);
    data = malloc(len);
    htole64a(data, object_len);
    memcpy(data + sizeof(uint64_t), object_data, object_len);

    uint16_t ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendPartOfObject, data,
                                    (int)len); // TODO: Support huge part of file
    free(data);
    return ret;
}

/**
 * Gets a command to perform an operation on an object.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param op_object an operate_object structure to fill with the infomation.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_OperateObject(vita_device_t *device, uint32_t event_id, operate_object_t *op_object)
{
    uint32_t len = 0;
    uint32_t *data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_OperateObject, (unsigned char **)&data, &len);
    op_object->cmd = le32toh(data[0]);
    op_object->ohfi = le32toh(data[1]);
    op_object->unk1 = le32toh(data[2]);
    op_object->len = le32toh(data[3]);
    op_object->title = (char *)malloc(op_object->len+1);
    memcpy(op_object->title, (char *)&data[4], op_object->len);
    op_object->title[op_object->len] = '\0';
    free(data);
    return ret;
}

/**
 * Gets a part of the object from the device.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param init a send_part_init_t struct to fill with object's info.
 * @param data a pointer to an array to fill with data (dynamically allocated)
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetPartOfObject(vita_device_t *device, uint32_t event_id, send_part_init_t *init, unsigned char **data)
{
    uint32_t len = 0;
    unsigned char *_data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_GetPartOfObject, (unsigned char **)&_data, &len);
    if(ret == PTP_RC_OK) {
        init->ohfi = le32atoh(_data);
        init->offset = le64atoh(_data + 4);
        init->size = le64atoh(_data + 12);
        *data = malloc(init->size);
        memcpy(*data, _data + sizeof(send_part_init_t), init->size);
        free(_data);
    } else {
        if(_data) {
            free(_data);
        }
    }
    return ret;
}

/**
 * Sends the size of the current storage device on the PC.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param storage_size storage size in bytes.
 * @param available_size free space in bytes.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendStorageSize(vita_device_t *device, uint32_t event_id, uint64_t storage_size,
                                 uint64_t available_size)
{
    static const unsigned char padding[] = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const int len = 25;
    uint16_t ret;
    unsigned char *data = malloc(len);
    htole64a(data, storage_size);
    htole64a(data+8, available_size);
    memcpy(&((uint64_t *)data)[2], padding, sizeof(padding));
    ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendStorageSize, data, len);
    free(data);
    return ret;
}

/**
 * Unknown function.
 * Only known information is that data is being obtained
 * from the computer.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param treat unknown.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetTreatObject(vita_device_t *device, uint32_t event_id, treat_object_t *treat)
{
    unsigned char *data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_GetTreatObject, &data, NULL);
    treat->ohfiParent = le32atoh(data);
    treat->unk0 = le32atoh(data+4);
    treat->handle = le32atoh(data+8);
    free(data);
    return ret;
}

/**
 * Receives information on object to send copy confirmation
 * Report is not needed after call to this command, but
 * VitaMTP_SendCopyConfirmationInfo() should be called soon.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param p_info place to store the ohfis to be checked (dynamically allocated)
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendCopyConfirmationInfoInit(vita_device_t *device, uint32_t event_id,
        copy_confirmation_info_t **p_info)
{
    unsigned char *data = NULL;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_SendCopyConfirmationInfoInit, (unsigned char **)p_info, NULL);
    LE32TOH((*p_info)->count);
    uint32_t i;
    for (i=0; i<(*p_info)->count; i++)
        LE32TOH((*p_info)->ohfi[i]);
    return ret;
}

/**
 * Sends information about an object to copy
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param copy_confirmation_info_t the information to send
 * @param size total size of the objects
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendCopyConfirmationInfo(vita_device_t *device, uint32_t event_id, copy_confirmation_info_t *info,
        uint64_t size)
{
    uint16_t ret;
    int info_size = info->count * sizeof(uint32_t) + sizeof(uint32_t);
    unsigned char *data = malloc(info_size + sizeof(uint64_t));
    htole64a(data, size);
    htole32a(data+8, info->count);
    uint32_t i;
    for (i=0; i<info->count; i++)
        htole32a(data+12+i*sizeof(uint32_t), info->ohfi[i]);
    ret = VitaMTP_SendData(device, event_id, PTP_OC_VITA_SendCopyConfirmationInfo, data,
                           info_size + sizeof(uint64_t));
    free(data);
    return ret;
}

/**
 * Gets the ohfi id of the object requested by the Vita
 * to send metadata on. VitaMTP_SendObjectMetadata() should be
 * called afterwards.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param ohfi a pointer to an integer that will be filled with the id.
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_SendObjectMetadata()
 */
uint16_t VitaMTP_SendObjectMetadataItems(vita_device_t *device, uint32_t event_id, uint32_t *ohfi)
{
    unsigned char *p_ohfi;
    uint16_t ret = VitaMTP_GetData(device, event_id, PTP_OC_VITA_SendObjectMetadataItems, &p_ohfi, NULL);

    if (ret != PTP_RC_OK)
    {
        return ret;
    }

    *ohfi = le32atoh(p_ohfi);
    free(p_ohfi);
    return ret;
}

/**
 * Not currently implemented, in the future, should be able to
 * cancel the event specified.
 *
 * @param device a pointer to the device.
 * @param cancel_event_id the unique ID to the event to cancel.
 */
uint16_t VitaMTP_CancelTask(vita_device_t *device, uint32_t cancel_event_id)
{
    uint16_t ret = PTP_RC_OK;

    pthread_mutex_lock(&g_event_mutex);
    g_canceltask_event_id = cancel_event_id;
    g_canceltask_set = 1;
    pthread_mutex_unlock(&g_event_mutex);

    pthread_mutex_lock(&g_cancel_mutex);
    if(g_event_cancelled)
    {
        PTPContainer resp;
        PTPParams *params = VitaMTP_Get_PTP_Params(device);
        // PTP_RC_TransactionCanceled

        if(VitaMTP_Get_Device_Type(device) == VitaDeviceWireless)
        {
            ret = params->getresp_func(params, &resp);
        }
        else
        {
            VitaMTP_USB_Clear(device);
        }
    }
    else
    {
        ret = PTP_ERROR_CANCEL;
    }
    g_canceltask_set = 0;
    pthread_mutex_unlock(&g_cancel_mutex);
    return ret;
}

/**
 * Tell the Vita that the current event is being processed and not
 * to time out on us.
 *
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 */
uint16_t VitaMTP_KeepAlive(vita_device_t *device, uint32_t event_id)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_KeepAlive;
    ptp.Nparam = 1;
    ptp.Param1 = event_id;

    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, 0);
}
/**
 * Sends a MTP object to the device. Size of the object and other
 * information is found in the metadata.
 *
 * @param device a pointer to the device.
 * @param p_parenthandle a pointer to the parent handle.
 * @param p_handle a pointer to the handle.
 * @param meta the metadata to describe the object.
 * @param data the object data to send.
 */
uint16_t VitaMTP_SendObject(vita_device_t *device, uint32_t *p_parenthandle, uint32_t *p_handle, metadata_t *meta,
                            unsigned char *data)
{
    uint32_t store = VITA_STORAGE_ID;
    uint16_t ret;
    PTPObjectInfo objectinfo;
    memset(&objectinfo, 0x0, sizeof(PTPObjectInfo));

    objectinfo.StorageID = store;
    objectinfo.ParentObject = *p_parenthandle;
    objectinfo.Filename = meta->name;

    pthread_mutex_lock(&g_cancel_mutex);

    if (meta->dataType & Folder)
    {
        objectinfo.ObjectFormat = PTP_OFC_Association; // 0x3001
        objectinfo.AssociationType = PTP_AT_GenericFolder;
        ret = ptp_sendobjectinfo(VitaMTP_Get_PTP_Params(device), &store, p_parenthandle, p_handle, &objectinfo);
    }
    else if (meta->dataType & File)
    {
        if (meta->dataType & SaveData)
        {
            objectinfo.ObjectFormat = PTP_OFC_PSPSave; // 0xB00A
        }
        else
        {
            objectinfo.ObjectFormat = PTP_OFC_Undefined;
        }

        objectinfo.ObjectCompressedSize = (uint32_t)meta->size;
        objectinfo.CaptureDate = meta->dateTimeCreated;
        objectinfo.ModificationDate = meta->dateTimeCreated;
        ret = ptp_sendobjectinfo(VitaMTP_Get_PTP_Params(device), &store, p_parenthandle, p_handle, &objectinfo);

        if (ret == PTP_RC_OK)
        {
            ret = ptp_sendobject(VitaMTP_Get_PTP_Params(device), data, (uint32_t)meta->size);
        }
    }
    else
    {
        // unsupported
        ret = PTP_RC_OperationNotSupported;
    }

    pthread_mutex_unlock(&g_cancel_mutex);

    return ret;
}

uint16_t VitaMTP_SendObject_Callback(vita_device_t *device, uint32_t *p_parenthandle, uint32_t *p_handle, metadata_t *meta,
                            read_callback_t read_callback)
{
    uint32_t store = VITA_STORAGE_ID;
    uint16_t ret;
    PTPObjectInfo objectinfo;
    memset(&objectinfo, 0x0, sizeof(PTPObjectInfo));

    objectinfo.StorageID = store;
    objectinfo.ParentObject = *p_parenthandle;
    objectinfo.Filename = meta->name;

    pthread_mutex_lock(&g_cancel_mutex);

    if (meta->dataType & Folder)
    {
        objectinfo.ObjectFormat = PTP_OFC_Association; // 0x3001
        objectinfo.AssociationType = PTP_AT_GenericFolder;
        ret = ptp_sendobjectinfo(VitaMTP_Get_PTP_Params(device), &store, p_parenthandle, p_handle, &objectinfo);
    }
    else if (meta->dataType & File)
    {
        if (meta->dataType & SaveData)
        {
            objectinfo.ObjectFormat = PTP_OFC_PSPSave; // 0xB00A
        }
        else
        {
            objectinfo.ObjectFormat = PTP_OFC_Undefined;
        }

        objectinfo.ObjectCompressedSize = (uint32_t)meta->size;
        objectinfo.CaptureDate = meta->dateTimeCreated;
        objectinfo.ModificationDate = meta->dateTimeCreated;
        ret = ptp_sendobjectinfo(VitaMTP_Get_PTP_Params(device), &store, p_parenthandle, p_handle, &objectinfo);

        if (ret == PTP_RC_OK)
        {
            read_callback_func = read_callback;
            ret = ptp_sendobject(VitaMTP_Get_PTP_Params(device), NULL, (uint32_t)meta->size);
            read_callback_func = NULL;
        }
    }
    else
    {
        // unsupported
        ret = PTP_RC_OperationNotSupported;
    }

    pthread_mutex_unlock(&g_cancel_mutex);

    return ret;
}

/**
 * Gets a PTP object from the device along with metadata.
 * If object is a handle, *p_data will be a uint32_t array
 * of handles in the directory and *p_len will be the number
 * of handles. If object is a file, *p_data will be a
 * unsigned char* containing the data and *p_len will be
 * the size of the data.
 * meta will contain minimal information. Only name,
 * dataType, size (if file), and handle will be filled.
 *
 * @param device a pointer to the device.
 * @param handle the PTP handle of the object to get.
 * @param meta information about the object, will be incomplete.
 * @param p_data dynamically allocated data.
 * @param p_len size of the data.
 */
uint16_t VitaMTP_GetObject(vita_device_t *device, uint32_t handle, metadata_t *meta, void **p_data,
                           unsigned int *p_len)
{
    PTPPropertyValue value;
    uint16_t ret;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectFormat, &value,
                                          PTP_DTC_UINT16)) != PTP_RC_OK)
    {
        return ret;
    }

    meta->dataType = value.u16 == PTP_OFC_Association ? Folder : File;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectFileName, &value,
                                          PTP_DTC_STR)) != PTP_RC_OK)
    {
        return ret;
    }

    meta->name = value.str;

    // TODO: Make use of date modified and object format
    //ptp_mtp_getobjectpropvalue ((PTPParams*)device->params, handle, PTP_OPC_DateModified, &value, PTP_DTC_STR);
    if (meta->dataType & Folder)
    {
        uint32_t store = VITA_STORAGE_ID;
        PTPObjectHandles handles;

        if ((ret = ptp_getobjecthandles(VitaMTP_Get_PTP_Params(device), store, 0, handle, &handles)) != PTP_RC_OK)
        {
            return ret;
        }

        *(uint32_t **)p_data = handles.Handler;
        *p_len = handles.n;
    }
    else if (meta->dataType & File)
    {
        if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectSize, &value,
                                              PTP_DTC_UINT64)) != PTP_RC_OK)
        {
            return ret;
        }

        meta->size = value.u64;
        ret = ptp_getobject(VitaMTP_Get_PTP_Params(device), handle, (unsigned char **)p_data);
        *p_len = (unsigned int)meta->size;
    }

    meta->handle = handle;
    return ret;
}

uint16_t VitaMTP_GetObject_Info(vita_device_t *device, uint32_t handle, char **name, int *dataType)
{
    PTPPropertyValue value;
    uint16_t ret;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectFormat, &value,
                                          PTP_DTC_UINT16)) != PTP_RC_OK)
    {
        return ret;
    }

    *dataType = value.u16 == PTP_OFC_Association ? Folder : File;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectFileName, &value,
                                          PTP_DTC_STR)) != PTP_RC_OK)
    {
        return ret;
    }

    *name = value.str;

    return PTP_RC_OK;
}

uint16_t VitaMTP_GetObject_Folder(vita_device_t *device, uint32_t handle, uint32_t **p_handles, unsigned int *p_len)
{
    uint32_t store = VITA_STORAGE_ID;
    PTPObjectHandles handles;
    uint16_t ret;

    if ((ret = ptp_getobjecthandles(VitaMTP_Get_PTP_Params(device), store, 0, handle, &handles)) != PTP_RC_OK)
    {
        return ret;
    }

    *p_handles = handles.Handler;
    *p_len = handles.n;

    return PTP_RC_OK;
}

uint16_t VitaMTP_GetObject_Callback(vita_device_t *device, uint32_t handle, uint64_t *size, write_callback_t write_callback)
{
    PTPPropertyValue value;
    uint16_t ret;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectSize, &value,
                                          PTP_DTC_UINT64)) != PTP_RC_OK)
    {
        return ret;
    }

    *size = value.u64;
    unsigned char *dummy = NULL;
    write_callback_func = write_callback;
    ret = ptp_getobject(VitaMTP_Get_PTP_Params(device), handle, &dummy);
    write_callback_func = NULL;
    return ret;
}

/**
 * Gets the name, size, and a small part of the object specified.
 * At most 0x400 bytes will be read to determine what kind of
 * object is about to be received.
 *
 * @param device a pointer to the device.
 * @param existance pointer to where results will be stored.
 */
uint16_t VitaMTP_CheckExistance(vita_device_t *device, uint32_t handle, existance_object_t *existance)
{
    PTPPropertyValue value;
    uint16_t ret;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectSize, &value,
                                          PTP_DTC_UINT64)) != PTP_RC_OK)
    {
        return ret;
    }

    existance->size = value.u64;

    if ((ret = ptp_mtp_getobjectpropvalue(VitaMTP_Get_PTP_Params(device), handle, PTP_OPC_ObjectFileName, &value,
                                          PTP_DTC_STR)) != PTP_RC_OK)
    {
        return ret;
    }

    existance->name = value.str;
    unsigned char *data;

    if ((ret = ptp_getpartialobject(VitaMTP_Get_PTP_Params(device), handle, 0, sizeof(existance->data), &data,
                                    &existance->data_length)) != PTP_RC_OK)
    {
        return ret;
    }

    memcpy(existance->data, data, existance->data_length);
    free(data);
    return ret;
}

/**
 * Called during initialization to get Vita capabilities.
 * Returned p_info must be freed with VitaMTP_Data_Free_Capability()
 * when done.
 *
 * @param device a pointer to the device.
 * @param info a pointer to the output struct pointer to fill
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_GetVitaCapabilityInfo(vita_device_t *device, capability_info_t **p_info)
{
    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;
    int ret;
    unsigned char *data;
    unsigned int len;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_GetVitaCapabilityInfo;
    ptp.Nparam = 0;
    ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);

    if (ret != PTP_RC_OK || len == 0)
    {
        return ret;
    }

    if (VitaMTP_Data_Capability_From_XML(p_info, (char *)data+sizeof(uint32_t), len-sizeof(uint32_t)) != 0) // strip header
    {
        return PTP_RC_GeneralError;
    }

    free(data);
    return ret;
}

/**
 * Called during initialization to send PC capabilities.
 *
 * @param device a pointer to the device.
 * @param info data to send.
 * @return the PTP result code that the Vita returns.
 */
uint16_t VitaMTP_SendPCCapabilityInfo(vita_device_t *device, capability_info_t *info)
{
    char *data;
    int len = 0;

    if (VitaMTP_Data_Capability_To_XML(info, &data, &len) < 0)
        return PTP_RC_GeneralError;

    PTPParams *params = VitaMTP_Get_PTP_Params(device);
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = PTP_OC_VITA_SendPCCapabilityInfo;
    ptp.Nparam = 0;
    uint16_t ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, len, (unsigned char **)&data,
                                   0); // plus one for null terminator, which is required on the vita's side
    free(data);
    return ret;
}

void VitaMTP_RegisterCancelEventId(uint32_t event_id)
{
    g_register_cancel_id = event_id;
}

/**
 * Call this function in your "main" thread before using any other function from this library
 */
int VitaMTP_Init(void)
{
    VitaMTP_Data_Init();
    pthread_mutex_init(&g_event_mutex, NULL);
    pthread_mutex_init(&g_cancel_mutex, NULL);
    return 0;
}

/**
 * Call it to clean the internal library allocations
 */
void VitaMTP_Cleanup(void)
{
    pthread_mutex_destroy(&g_event_mutex);
    pthread_mutex_destroy(&g_cancel_mutex);
    VitaMTP_Data_Cleanup();
}
