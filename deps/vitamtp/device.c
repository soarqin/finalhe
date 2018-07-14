//
//  Functions useful for connecting to USB
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

#include <memory.h>
#include "pthread-support.h"
#include <stdio.h>
#include "ptp.h"
#include "vitamtp.h"
#include "ptp_defines.h"

extern int g_VitaMTP_logmask;

extern volatile uint32_t g_register_cancel_id;

extern pthread_mutex_t g_event_mutex;
extern pthread_mutex_t g_cancel_mutex;

extern read_callback_t read_callback_func;

struct vita_device
{
    PTPParams *params;
    enum vita_device_type device_type;
    char identification[];
};

/**
 * Dummy function needed to interface to upstream
 * ptp.c/ptp.h files.
 */
void ptp_nikon_getptpipguid(unsigned char *guid)
{
    return;
}

#ifndef PTP_USB_SUPPORT

int VitaMTP_USB_Init(void)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
    return -1;
}

vita_device_t *VitaMTP_Open_USB_Vita(vita_raw_device_t *raw_device)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
    return NULL;
}

void VitaMTP_Release_USB_Device(vita_device_t *device)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
}

int VitaMTP_Get_USB_Vitas(vita_raw_device_t **p_raw_devices)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
    return -1;
}

void VitaMTP_Unget_USB_Vitas(vita_raw_device_t *raw_devices, int numdevs)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
}

vita_device_t *VitaMTP_Get_First_USB_Vita(void)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
    return NULL;
}

void VitaMTP_USB_Exit(void)
{
    VitaMTP_Log(VitaMTP_ERROR, "USB is unsupported\n");
}

#endif

#ifndef PTP_IP_SUPPORT
int VitaMTP_Broadcast_Host(wireless_host_info_t *info, unsigned int host_addr)
{
    VitaMTP_Log(VitaMTP_ERROR, "wireless is unsupported\n");
    return -1;
}

void VitaMTP_Stop_Broadcast(void)
{
    VitaMTP_Log(VitaMTP_ERROR, "wireless is unsupported\n");
}

void VitaMTP_Release_Wireless_Device(vita_device_t *device)
{
    VitaMTP_Log(VitaMTP_ERROR, "wireless is unsupported\n");
}

vita_device_t *VitaMTP_Get_First_Wireless_Vita(wireless_host_info_t *info, unsigned int host_addr, int timeout,
        device_registered_callback_t is_registered, register_device_callback_t create_register_pin)
{
    VitaMTP_Log(VitaMTP_ERROR, "wireless is unsupported\n");
    return NULL;
}

int VitaMTP_Get_Device_IP(vita_device_t *device)
{
    VitaMTP_Log(VitaMTP_ERROR, "wireless is unsupported\n");
    return -1;
}
#endif

/**
 @brief Prints out a HEX dump of data.

 For debugging commands.

 @param data Data to dump
 @param size Number of bytes to print
 @param num Number of bytes to print per line
 */
void VitaMTP_hex_dump(const unsigned char *data, unsigned int size, unsigned int num);
void VitaMTP_hex_dump(const unsigned char *data, unsigned int size, unsigned int num)
{
    unsigned int i = 0, j = 0, k = 0, l = 0;

    // I hate letters, but I can't come up with good names
    // i = counter, j = bytes printed, k = number of place values, l = temp
    for (l = size/num, k = 1; l > 0; l/=num, k++); // find number of zeros to prepend line number

    while (j < size)
    {
        // line number
        fprintf(stderr, "%0*X: ", k, j);

        // hex value
        for (i = 0; i < num; i++, j++)
        {
            if (j < size)
            {
                fprintf(stderr, "%02X ", data[j]);
            }
            else     // print blank spaces
            {
                fprintf(stderr, "%s ", "  ");
            }
        }

        // seperator
        fprintf(stderr, "%s", "| ");

        // ascii value
        for (i = num; i > 0; i--)
        {
            if (j-i < size)
            {
                fprintf(stderr, "%c", data[j-i] < 32 || data[j-i] > 126 ? '.' : data[j-i]); // print only visible characters
            }
            else
            {
                fprintf(stderr, "%s", " ");
            }
        }

        // new line
        fprintf(stderr, "%s", "\n");
    }
}

/**
 * Convenience function for freeing any type Vita device
 *
 * @param device the Vita to free
 * @see VitaMTP_Release_USB_Device()
 * @see VitaMTP_Release_Wireless_Device()
 */
void VitaMTP_Release_Device(vita_device_t *device)
{
    if (device->device_type == VitaDeviceUSB)
    {
        VitaMTP_Release_USB_Device(device);
    }
    else if (device->device_type == VitaDeviceWireless)
    {
        VitaMTP_Release_Wireless_Device(device);
    }
    else
    {
        VitaMTP_Log(VitaMTP_ERROR, "The device does not have a type assigned!\n");
    }
}

/**
 * To read events sent by the device, repeatedly call this function from a secondary
 * thread until the return value is < 0.
 *
 * @param device a pointer to the MTP device to poll for events.
 * @param event contains a pointer to be filled in with the event retrieved if the call
 * is successful.
 * @return 0 on success, any other value means the polling loop shall be
 * terminated immediately for this session.
 */
int VitaMTP_Read_Event(vita_device_t *device, vita_event_t *event)
{
    /*
     * FIXME: Potential race-condition here, if client deallocs device
     * while we're *not* waiting for input. As we'll be waiting for
     * input most of the time, it's unlikely but still worth considering
     * for improvement. Also we cannot affect the state of the cache etc
     * unless we know we are the sole user on the device. A spinlock or
     * mutex in the LIBMTP_mtpdevice_t is needed for this to work.
     */
    PTPParams *params = (PTPParams *)device->params;
    PTPContainer ptp_event;
    memset(&ptp_event, 0, sizeof(PTPContainer)); // zero it so params are zeroed too
    uint16_t ret = params->event_wait(params, &ptp_event);

    memcpy(event, &ptp_event, sizeof(vita_event_t));

    if (ret != PTP_RC_OK)
    {
        /* Device is closing down or other fatal stuff, exit thread */
        return -1;
    }

    return 0;
}

/**
 * @brief Gets PTP params.
 *
 * @param device where to get PTP params from
 */
PTPParams *VitaMTP_Get_PTP_Params(vita_device_t *device);
PTPParams *VitaMTP_Get_PTP_Params(vita_device_t *device)
{
    return device->params;
}

/**
 * Returns the device's serial
 *
 * Gets the device's serial as an ASCII string
 * @param device a pointer to the device.
 * @return serial string of the device.
 */
const char *VitaMTP_Get_Identification(vita_device_t *device)
{
    return device->identification;
}

/**
 * Returns the device's type
 *
 * The connected device could be a USB device
 * or it could be wireless.
 * @param device a pointer to the device.
 * @return device type.
 */
enum vita_device_type VitaMTP_Get_Device_Type(vita_device_t *device)
{
    return device->device_type;
}

/**
 * Sends a command with a data send phase and a event id
 *
 * The Vita uses the PC as a slave. It sends an event asking the PC to
 * send a command, and it complies, sending the command with the event_id
 * as its first parameter. Most commands only has this paramater, so this
 * function it called internally to make our lives easier.
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param code the command to send.
 * @param data the array of data to send.
 * @param len the length of "data".
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_GetData()
 */
uint16_t VitaMTP_SendData(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned char *data,
                          unsigned int len)
{
    PTPParams *params = (PTPParams *)device->params;
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = code;
    ptp.Nparam = 1;
    ptp.Param1 = event_id;

    g_register_cancel_id = event_id;
    pthread_mutex_lock(&g_cancel_mutex);
    uint16_t ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, len, &data, 0);
    pthread_mutex_unlock(&g_cancel_mutex);
    return ret;
}

uint16_t VitaMTP_SendData_Callback(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned int len,
                                   read_callback_t read_callback)
{
    PTPParams *params = (PTPParams *)device->params;
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = code;
    ptp.Nparam = 1;
    ptp.Param1 = event_id;

    g_register_cancel_id = event_id;
    pthread_mutex_lock(&g_cancel_mutex);

    unsigned char *dummy = NULL;
    read_callback_func = read_callback;
    uint16_t ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, len, &dummy, 0);
    read_callback_func = NULL;

    pthread_mutex_unlock(&g_cancel_mutex);
    return ret;
}

/**
 * Sends a command with a data get phase and a event id
 *
 * The Vita uses the PC as a slave. It sends an event asking the PC to
 * send a command, and it complies, sending the command with the event_id
 * as its first parameter. Most commands only has this paramater, so this
 * function it called internally to make our lives easier.
 * @param device a pointer to the device.
 * @param event_id the unique ID sent by the Vita with the event.
 * @param code the command to send.
 * @param p_data a pointer to the char array to fill (dynamically allocated).
 * @param p_len a pointer to the length of the filled array.
 * @return the PTP result code that the Vita returns.
 * @see VitaMTP_GetData()
 */
uint16_t VitaMTP_GetData(vita_device_t *device, uint32_t event_id, uint32_t code, unsigned char **p_data,
                         unsigned int *p_len)
{
    PTPParams *params = (PTPParams *)device->params;
    PTPContainer ptp;

    PTP_CNT_INIT(ptp);
    ptp.Code = code;
    ptp.Nparam = 1;
    ptp.Param1 = event_id;

    uint16_t ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, p_data, p_len);

    return ret;
}
