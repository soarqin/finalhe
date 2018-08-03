//
//  Connecting to a Vita USB device
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

#ifdef PTP_USB_SUPPORT
#define _GNU_SOURCE
#include <iconv.h>
#include "pthread-support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "endian-utils.h"
#include "ptp.h"
#include "vitamtp.h"

#include "ptp-pack.c"

#include <libusb.h>

struct vita_device
{
    PTPParams *params;
    enum vita_device_type device_type;
    char serial[18];
    struct vita_usb
    {
        libusb_device_handle *handle;
        uint8_t interface;
        int inep;
        int inep_maxpacket;
        int outep;
        int outep_maxpacket;
        int intep;
        int callback_active;
        int timeout;
        uint64_t current_transfer_total;
        uint64_t current_transfer_complete;
        VitaMTP_progressfunc_t current_transfer_callback;
        void const *current_transfer_callback_data;
    } usb_device;
};

static libusb_context *g_usb_context = NULL;

extern int g_VitaMTP_logmask;

extern volatile uint32_t g_register_cancel_id;
extern volatile uint32_t g_canceltask_event_id ;
extern volatile int g_event_cancelled;
extern volatile int g_canceltask_set;

extern pthread_mutex_t g_event_mutex;
extern read_callback_t read_callback_func;
extern write_callback_t write_callback_func;

void VitaMTP_hex_dump(const unsigned char *data, unsigned int size, unsigned int num);

#define USB_BULK_READ libusb_bulk_transfer
#define USB_BULK_WRITE libusb_bulk_transfer

// The functions below are taken from libusb1-glue.c from libmtp
// the copyright notice from the file is included

/*
 * \file libusb1-glue.c
 * Low-level USB interface glue towards libusb.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2012 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2012 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
 * Copyright (C) 2008 Chris Bagwell <chris@cnpbagwell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Created by Richard Low on 24/12/2005. (as mtp-utils.c)
 * Modified by Linus Walleij 2006-03-06
 *  (Notice that Anglo-Saxons use little-endian dates and Swedes
 *   use big-endian dates.)
 *
 */

/*
 * ptp_read_func() and ptp_write_func() are
 * based on same functions usb.c in libgphoto2.
 * Much reading packet logs and having fun with trials and errors
 * reveals that WMP / Windows is probably using an algorithm like this
 * for large transfers:
 *
 * 1. Send the command (0x0c bytes) if headers are split, else, send
 *    command plus sizeof(endpoint) - 0x0c bytes.
 * 2. Send first packet, max size to be sizeof(endpoint) but only when using
 *    split headers. Else goto 3.
 * 3. REPEAT send 0x10000 byte chunks UNTIL remaining bytes < 0x10000
 *    We call 0x10000 CONTEXT_BLOCK_SIZE.
 * 4. Send remaining bytes MOD sizeof(endpoint)
 * 5. Send remaining bytes. If this happens to be exactly sizeof(endpoint)
 *    then also send a zero-length package.
 *
 * Further there is some special quirks to handle zero reads from the
 * device, since some devices can't do them at all due to shortcomings
 * of the USB slave controller in the device.
 */
#define CONTEXT_BLOCK_SIZE_1    0x3e00
#define CONTEXT_BLOCK_SIZE_2  0x200
#define CONTEXT_BLOCK_SIZE    CONTEXT_BLOCK_SIZE_1+CONTEXT_BLOCK_SIZE_2
static short
ptp_read_func(
    unsigned long size, PTPDataHandler *handler,void *data,
    unsigned long *readbytes,
    int readzero,
    int use_callback
)
{
    struct vita_usb *ptp_usb = (struct vita_usb *)data;
    unsigned long toread = 0;
    int ret = 0;
    int xread;
    unsigned long curread = 0;
    unsigned long written;
    unsigned char *bytes;
    int expect_terminator_byte = 0;

    // This is the largest block we'll need to read in.
    bytes = malloc(CONTEXT_BLOCK_SIZE);

    while (curread < size)
    {

        VitaMTP_Log(VitaMTP_DEBUG, "Remaining size to read: 0x%04lx bytes\n", size - curread);

        // check equal to condition here
        if (size - curread < CONTEXT_BLOCK_SIZE)
        {
            // this is the last packet
            toread = size - curread;
        }
        else if (curread == 0)
            // we are first packet, but not last packet
            toread = CONTEXT_BLOCK_SIZE_1;
        else if (toread == CONTEXT_BLOCK_SIZE_1)
            toread = CONTEXT_BLOCK_SIZE_2;
        else if (toread == CONTEXT_BLOCK_SIZE_2)
            toread = CONTEXT_BLOCK_SIZE_1;
        else
            VitaMTP_Log(VitaMTP_INFO, "unexpected toread size 0x%04x, 0x%04x remaining bytes\n",
                        (unsigned int) toread, (unsigned int)(size-curread));

        VitaMTP_Log(VitaMTP_DEBUG, "Reading in 0x%04lx bytes\n", toread);

        ret = USB_BULK_READ(ptp_usb->handle,
                            ptp_usb->inep,
                            bytes,
                            (int)toread,
                            &xread,
                            ptp_usb->timeout);

        VitaMTP_Log(VitaMTP_DEBUG, "Result of read: 0x%04x (%d bytes)\n", ret, xread);

        if (ret != LIBUSB_SUCCESS)
            return PTP_ERROR_IO;

        VitaMTP_Log(VitaMTP_DEBUG, "<==USB IN\n");

        if (xread == 0)
            VitaMTP_Log(VitaMTP_DEBUG, "Zero Read\n");
        else if (MASK_SET(g_VitaMTP_logmask, VitaMTP_DEBUG))
            VitaMTP_hex_dump(bytes, xread, 16);

        // want to discard extra byte
        if (expect_terminator_byte && xread == (int)toread)
        {
            VitaMTP_Log(VitaMTP_DEBUG, "<==USB IN\nDiscarding extra byte\n");

            xread--;
        }

        int putfunc_ret;

        if(use_callback && write_callback_func)
        {
            putfunc_ret = write_callback_func(bytes, xread, &written);
        }
        else
        {
            putfunc_ret = handler->putfunc(NULL, handler->priv, xread, bytes, &written);
        }

        if (putfunc_ret != PTP_RC_OK)
            return putfunc_ret;

        ptp_usb->current_transfer_complete += xread;
        curread += xread;

        // Increase counters, call callback
        if (ptp_usb->callback_active)
        {
            if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total)
            {
                // send last update and disable callback.
                ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
                ptp_usb->callback_active = 0;
            }

            if (ptp_usb->current_transfer_callback != NULL)
            {
                int ret;
                ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
                        ptp_usb->current_transfer_total,
                        ptp_usb->current_transfer_callback_data);

                if (ret != 0)
                {
                    return PTP_ERROR_CANCEL;
                }
            }
        }

        if (xread < (int)toread) /* short reads are common */
            break;
    }

    if (readbytes) *readbytes = curread;

    free(bytes);

    // there might be a zero packet waiting for us...
    if (readzero &&
            curread % ptp_usb->outep_maxpacket == 0)
    {
        unsigned char temp;
        int zeroresult = 0, xread;

        VitaMTP_Log(VitaMTP_DEBUG, "<==USB IN\n");
        VitaMTP_Log(VitaMTP_DEBUG, "Zero Read\n");

        zeroresult = USB_BULK_READ(ptp_usb->handle,
                                   ptp_usb->inep,
                                   &temp,
                                   0,
                                   &xread,
                                   ptp_usb->timeout);

        if (zeroresult != LIBUSB_SUCCESS)
            VitaMTP_Log(VitaMTP_INFO, "LIBMTP panic: unable to read in zero packet, response 0x%04x\n", zeroresult);
    }

    return PTP_RC_OK;
}

static short
ptp_write_func(
    unsigned long   size,
    PTPDataHandler  *handler,
    void            *data,
    unsigned long   *written,
    int use_callback
)
{
    struct vita_usb *ptp_usb = (struct vita_usb *)data;
    unsigned long towrite = 0;
    int ret = 0;
    unsigned long curwrite = 0;
    unsigned char *bytes;

    // This is the largest block we'll need to read in.
    bytes = malloc(CONTEXT_BLOCK_SIZE);

    if (!bytes)
    {
        return PTP_ERROR_IO;
    }

    while (curwrite < size)
    {
        unsigned long usbwritten = 0;
        int xwritten;

        towrite = size-curwrite;

        if (towrite > CONTEXT_BLOCK_SIZE)
        {
            towrite = CONTEXT_BLOCK_SIZE;
        }
        else
        {
            // This magic makes packets the same size that WMP send them.
            if ((int)towrite > ptp_usb->outep_maxpacket && towrite % ptp_usb->outep_maxpacket != 0)
            {
                towrite -= towrite % ptp_usb->outep_maxpacket;
            }
        }

        int getfunc_ret;

        if(use_callback && read_callback_func)
        {
            getfunc_ret = read_callback_func(bytes, towrite, &towrite);
        }
        else
        {
            getfunc_ret = handler->getfunc(NULL, handler->priv,towrite,bytes,&towrite);
        }

        if (getfunc_ret != PTP_RC_OK)
            return getfunc_ret;

        while (usbwritten < towrite)
        {
            ret = USB_BULK_WRITE(ptp_usb->handle,
                                 ptp_usb->outep,
                                 bytes+usbwritten,
                                 (int)(towrite-usbwritten),
                                 &xwritten,
                                 ptp_usb->timeout);

            VitaMTP_Log(VitaMTP_DEBUG, "USB OUT (1) ==>\n");

            if (ret != LIBUSB_SUCCESS || xwritten == 0)
            {
                free(bytes);
                return PTP_ERROR_IO;
            }

            if (MASK_SET(g_VitaMTP_logmask, VitaMTP_DEBUG)) VitaMTP_hex_dump(bytes+usbwritten, xwritten, 16);

            // check for result == 0 perhaps too.
            // Increase counters
            ptp_usb->current_transfer_complete += xwritten;
            curwrite += xwritten;
            usbwritten += xwritten;
        }

        // call callback
        if (ptp_usb->callback_active)
        {
            if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total)
            {
                // send last update and disable callback.
                ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
                ptp_usb->callback_active = 0;
            }

            if (ptp_usb->current_transfer_callback != NULL)
            {
                int ret;
                ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
                        ptp_usb->current_transfer_total,
                        ptp_usb->current_transfer_callback_data);

                if (ret != 0)
                {
                    free(bytes);
                    return PTP_ERROR_CANCEL;
                }
            }
        }

        if (xwritten < (int)towrite) /* short writes happen */
            break;
    }

    free(bytes);

    if (written)
    {
        *written = curwrite;
    }

    // If this is the last transfer send a zero write if required
    if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total)
    {
        if ((towrite % ptp_usb->outep_maxpacket) == 0)
        {
            int xwritten;

            VitaMTP_Log(VitaMTP_DEBUG, "USB OUT (2) ==>\n");
            VitaMTP_Log(VitaMTP_DEBUG, "Zero Write\n");

            ret =USB_BULK_WRITE(ptp_usb->handle,
                                ptp_usb->outep,
                                (unsigned char *) "x",
                                0,
                                &xwritten,
                                ptp_usb->timeout);
        }
    }

    if (ret != LIBUSB_SUCCESS)
        return PTP_ERROR_IO;

    return PTP_RC_OK;
}

/* memory data get/put handler */
typedef struct
{
    unsigned char   *data;
    unsigned long   size, curoff;
} PTPMemHandlerPrivate;

static uint16_t
memory_getfunc(PTPParams *params, void *private,
               unsigned long wantlen, unsigned char *data,
               unsigned long *gotlen
              )
{
    PTPMemHandlerPrivate *priv = (PTPMemHandlerPrivate *)private;
    unsigned long tocopy = wantlen;

    if (priv->curoff + tocopy > priv->size)
        tocopy = priv->size - priv->curoff;

    memcpy(data, priv->data + priv->curoff, tocopy);
    priv->curoff += tocopy;
    *gotlen = tocopy;
    return PTP_RC_OK;
}

static uint16_t
memory_putfunc(PTPParams *params, void *private,
               unsigned long sendlen, unsigned char *data,
               unsigned long *putlen
              )
{
    PTPMemHandlerPrivate *priv = (PTPMemHandlerPrivate *)private;

    if (priv->curoff + sendlen > priv->size)
    {
        priv->data = realloc(priv->data, priv->curoff+sendlen);
        priv->size = priv->curoff + sendlen;
    }

    memcpy(priv->data + priv->curoff, data, sendlen);
    priv->curoff += sendlen;
    *putlen = sendlen;
    return PTP_RC_OK;
}

/* init private struct for receiving data. */
static uint16_t
ptp_init_recv_memory_handler(PTPDataHandler *handler)
{
    PTPMemHandlerPrivate *priv;
    priv = malloc(sizeof(PTPMemHandlerPrivate));
    handler->priv = priv;
    handler->getfunc = memory_getfunc;
    handler->putfunc = memory_putfunc;
    priv->data = NULL;
    priv->size = 0;
    priv->curoff = 0;
    return PTP_RC_OK;
}

/* init private struct and put data in for sending data.
 * data is still owned by caller.
 */
static uint16_t
ptp_init_send_memory_handler(PTPDataHandler *handler,
                             unsigned char *data, unsigned long len
                            )
{
    PTPMemHandlerPrivate *priv;
    priv = malloc(sizeof(PTPMemHandlerPrivate));

    if (!priv)
        return PTP_RC_GeneralError;

    handler->priv = priv;
    handler->getfunc = memory_getfunc;
    handler->putfunc = memory_putfunc;
    priv->data = data;
    priv->size = len;
    priv->curoff = 0;
    return PTP_RC_OK;
}

/* free private struct + data */
static uint16_t
ptp_exit_send_memory_handler(PTPDataHandler *handler)
{
    PTPMemHandlerPrivate *priv = (PTPMemHandlerPrivate *)handler->priv;
    /* data is owned by caller */
    free(priv);
    return PTP_RC_OK;
}

/* hand over our internal data to caller */
static uint16_t
ptp_exit_recv_memory_handler(PTPDataHandler *handler,
                             unsigned char **data, unsigned long *size
                            )
{
    PTPMemHandlerPrivate *priv = (PTPMemHandlerPrivate *)handler->priv;
    *data = priv->data;
    *size = priv->size;
    free(priv);
    return PTP_RC_OK;
}

/* send / receive functions */

/*
 * Default USB timeout length.  This can be overridden as needed
 * but should start with a reasonable value so most common
 * requests can be completed.  The original value of 4000 was
 * not long enough for large file transfer.  Also, players can
 * spend a bit of time collecting data.  Higher values also
 * make connecting/disconnecting more reliable.
 */
#define USB_TIMEOUT_DEFAULT     20000
#define USB_TIMEOUT_LONG        60000

uint16_t
ptp_usb_sendreq(PTPParams *params, PTPContainer *req)
{
    uint16_t ret;
    PTPUSBBulkContainer usbreq;
    PTPDataHandler  memhandler;
    unsigned long written = 0;
    unsigned long towrite;

    char txt[256];

    (void) ptp_render_opcode(params, req->Code, sizeof(txt), txt);
    VitaMTP_Log(VitaMTP_DEBUG, "REQUEST: 0x%04x, %s\n", req->Code, txt);

    /* build appropriate USB container */
    usbreq.length=htod32(PTP_USB_BULK_REQ_LEN-
                         (sizeof(uint32_t)*(5-req->Nparam)));
    usbreq.type=htod16(PTP_USB_CONTAINER_COMMAND);
    usbreq.code=htod16(req->Code);
    usbreq.trans_id=htod32(req->Transaction_ID);
    usbreq.payload.params.param1=htod32(req->Param1);
    usbreq.payload.params.param2=htod32(req->Param2);
    usbreq.payload.params.param3=htod32(req->Param3);
    usbreq.payload.params.param4=htod32(req->Param4);
    usbreq.payload.params.param5=htod32(req->Param5);
    /* send it to responder */
    towrite = PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam));
    ptp_init_send_memory_handler(&memhandler, (unsigned char *)&usbreq, towrite);
    ret=ptp_write_func(
            towrite,
            &memhandler,
            params->data,
            &written,
            0
        );
    ptp_exit_send_memory_handler(&memhandler);

    if (ret != PTP_RC_OK && ret != PTP_ERROR_CANCEL)
    {
        ret = PTP_ERROR_IO;
    }

    if (written != towrite && ret != PTP_ERROR_CANCEL && ret != PTP_ERROR_IO)
    {
        VitaMTP_Log(VitaMTP_ERROR,
                    "PTP: request code 0x%04x sending req wrote only %ld bytes instead of %ld\n",
                    req->Code, written, towrite
                   );
        ret = PTP_ERROR_IO;
    }

    return ret;
}

uint16_t
ptp_usb_senddata(PTPParams *params, PTPContainer *ptp,
                 uint64_t size, PTPDataHandler *handler
                )
{
    uint16_t ret = PTP_RC_OK;
    int wlen;
    unsigned long datawlen;
    PTPUSBBulkContainer usbdata;
    unsigned long bytes_left_to_transfer, written;
    PTPDataHandler memhandler;


    VitaMTP_Log(VitaMTP_DEBUG, "SEND DATA PHASE\n");

    /* build appropriate USB container */
    usbdata.length  = htod32(PTP_USB_BULK_HDR_LEN+size);
    usbdata.type    = htod16(PTP_USB_CONTAINER_DATA);
    usbdata.code    = htod16(ptp->Code);
    usbdata.trans_id= htod32(ptp->Transaction_ID);

    ((struct vita_usb *)params->data)->current_transfer_complete = 0;
    ((struct vita_usb *)params->data)->current_transfer_total = size+PTP_USB_BULK_HDR_LEN;

    if (params->split_header_data)
    {
        datawlen = 0;
        wlen = PTP_USB_BULK_HDR_LEN;
    }
    else
    {
        unsigned long gotlen;
        /* For all camera devices. */
        datawlen = (int)((size<PTP_USB_BULK_PAYLOAD_LEN_WRITE)?size:PTP_USB_BULK_PAYLOAD_LEN_WRITE);
        wlen = PTP_USB_BULK_HDR_LEN + datawlen;

        if(read_callback_func)
        {
            ret = read_callback_func(usbdata.payload.data, datawlen, &gotlen);
        }
        else
        {
            ret = handler->getfunc(params, handler->priv, datawlen, usbdata.payload.data, &gotlen);
        }

        if (ret != PTP_RC_OK)
            return ret;

        if (gotlen != datawlen)
            return PTP_RC_GeneralError;
    }

    ptp_init_send_memory_handler(&memhandler, (unsigned char *)&usbdata, wlen);
    /* send first part of data */
    ret = ptp_write_func(wlen, &memhandler, params->data, &written, 0);
    ptp_exit_send_memory_handler(&memhandler);

    if (ret != PTP_RC_OK)
    {
        return ret;
    }

    if (size <= datawlen) return ret;

    /* if everything OK send the rest */
    bytes_left_to_transfer = size-datawlen;
    ret = PTP_RC_OK;
    written = 0;

    g_event_cancelled = 0;

    while (bytes_left_to_transfer > 0)
    {
        ret = ptp_write_func(bytes_left_to_transfer, handler, params->data, &written, 1);

        if (ret != PTP_RC_OK)
            break;

        if (written == 0)
        {
            ret = PTP_ERROR_IO;
            break;
        }

        bytes_left_to_transfer -= written;
    }

    if (ret != PTP_RC_OK && ret != PTP_ERROR_CANCEL)
        ret = PTP_ERROR_IO;

    pthread_mutex_lock(&g_event_mutex);
    if(g_canceltask_set)
    {
        if(g_canceltask_event_id == g_register_cancel_id)
        {
            g_event_cancelled = 1;
            VitaMTP_Log(VitaMTP_VERBOSE, "Event with ID %d cancelled by device (1)\n", g_register_cancel_id);
            pthread_mutex_unlock(&g_event_mutex);
            return PTP_ERROR_CANCEL;
        }
    }
    pthread_mutex_unlock(&g_event_mutex);

    return ret;
}

static uint16_t ptp_usb_getpacket(PTPParams *params,
                                  PTPUSBBulkContainer *packet, unsigned long *rlen)
{
    PTPDataHandler  memhandler;
    uint16_t    ret;
    unsigned char   *x = NULL;

    /* read the header and potentially the first data */
    if (params->response_packet_size > 0)
    {
        /* If there is a buffered packet, just use it. */
        memcpy(packet, params->response_packet, params->response_packet_size);
        *rlen = params->response_packet_size;
        free(params->response_packet);
        params->response_packet = NULL;
        params->response_packet_size = 0;
        /* Here this signifies a "virtual read" */
        return PTP_RC_OK;
    }

    ptp_init_recv_memory_handler(&memhandler);
    ret = ptp_read_func(PTP_USB_BULK_HS_MAX_PACKET_LEN_READ, &memhandler, params->data, rlen, 0, 0);
    ptp_exit_recv_memory_handler(&memhandler, &x, rlen);

    if (x)
    {
        memcpy(packet, x, *rlen);
        free(x);
    }

    return ret;
}

uint16_t
ptp_usb_getdata(PTPParams *params, PTPContainer *ptp, PTPDataHandler *handler)
{
    uint16_t ret;
    PTPUSBBulkContainer usbdata;
    unsigned long   written;
    struct vita_usb *ptp_usb = (struct vita_usb *) params->data;
    int putfunc_ret;

    VitaMTP_Log(VitaMTP_DEBUG, "GET DATA PHASE\n");

    memset(&usbdata,0,sizeof(usbdata));

    do
    {
        unsigned long len, rlen;

        ret = ptp_usb_getpacket(params, &usbdata, &rlen);

        if (ret != PTP_RC_OK)
        {
            ret = PTP_ERROR_IO;
            break;
        }

        if (dtoh16(usbdata.type)!=PTP_USB_CONTAINER_DATA)
        {
            ret = PTP_ERROR_DATA_EXPECTED;
            break;
        }

        if (dtoh16(usbdata.code)!=ptp->Code)
        {
            ret = dtoh16(usbdata.code);

            // This filters entirely insane garbage return codes, but still
            // makes it possible to return error codes in the code field when
            // getting data. It appears Windows ignores the contents of this
            // field entirely.
            if (ret < PTP_RC_Undefined || ret > PTP_RC_SpecificationOfDestinationUnsupported)
            {
                VitaMTP_Log(VitaMTP_DEBUG, "%s\n", "ptp2/ptp_usb_getdata: detected a broken "
                            "PTP header, code field insane.");
                ret = PTP_ERROR_IO;
            }

            break;
        }

        if (usbdata.length == 0xffffffffU)
        {
            /* Copy first part of data to 'data' */

            if(write_callback_func)
            {
                putfunc_ret = write_callback_func(usbdata.payload.data, rlen - PTP_USB_BULK_HDR_LEN, &written);
            }
            else
            {
                putfunc_ret =
                    handler->putfunc(
                        params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN, usbdata.payload.data,
                        &written
                    );
            }

            if (putfunc_ret != PTP_RC_OK)
                return putfunc_ret;

            /* stuff data directly to passed data handler */
            while (1)
            {
                unsigned long readdata;
                uint16_t xret;

                xret = ptp_read_func(
                           PTP_USB_BULK_HS_MAX_PACKET_LEN_READ,
                           handler,
                           params->data,
                           &readdata,
                           0,
                           1
                       );

                if (xret != PTP_RC_OK)
                    return xret;

                if (readdata < PTP_USB_BULK_HS_MAX_PACKET_LEN_READ)
                    break;
            }

            return PTP_RC_OK;
        }

        if (rlen > dtoh32(usbdata.length))
        {
            /*
             * Buffer the surplus response packet if it is >=
             * PTP_USB_BULK_HDR_LEN
             * (i.e. it is probably an entire package)
             * else discard it as erroneous surplus data.
             * This will even work if more than 2 packets appear
             * in the same transaction, they will just be handled
             * iteratively.
             *
             * Marcus observed stray bytes on iRiver devices;
             * these are still discarded.
             */
            unsigned int packlen = dtoh32(usbdata.length);
            unsigned int surplen = (unsigned int)rlen - packlen;

            if (surplen >= PTP_USB_BULK_HDR_LEN)
            {
                params->response_packet = malloc(surplen);
                memcpy(params->response_packet,
                       (uint8_t *) &usbdata + packlen, surplen);
                params->response_packet_size = surplen;
                /* Ignore reading one extra byte if device flags have been set */
            }

            rlen = packlen;
        }

        /* For most PTP devices rlen is 512 == sizeof(usbdata)
         * here. For MTP devices splitting header and data it might
         * be 12.
         */
        /* Evaluate full data length. */
        len=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;

        /* autodetect split header/data MTP devices */
        if (dtoh32(usbdata.length) > 12 && (rlen==12))
            params->split_header_data = 1;

        /* Copy first part of data to 'data' */

        if(write_callback_func)
        {
            putfunc_ret = write_callback_func(usbdata.payload.data, rlen - PTP_USB_BULK_HDR_LEN, &written);
        }
        else
        {
            putfunc_ret =
                handler->putfunc(
                    params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN,
                    usbdata.payload.data,
                    &written
                );
        }

        if (putfunc_ret != PTP_RC_OK)
            return putfunc_ret;

        if (len+PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ && params->split_header_data == 0)
        {
            int zeroresult = 0, xread;
            unsigned char zerobyte = 0;

            VitaMTP_Log(VitaMTP_INFO, "Reading in zero packet after header\n");

            zeroresult = USB_BULK_READ(ptp_usb->handle,
                                       ptp_usb->inep,
                                       &zerobyte,
                                       0,
                                       &xread,
                                       ptp_usb->timeout);

            if (zeroresult != 0)
                VitaMTP_Log(VitaMTP_INFO, "LIBMTP panic: unable to read in zero packet, response 0x%04x\n", zeroresult);
        }

        /* Is that all of data? */
        if (len+PTP_USB_BULK_HDR_LEN<=rlen)
        {
            break;
        }

        ret = ptp_read_func(len - (rlen - PTP_USB_BULK_HDR_LEN),
                            handler,
                            params->data, &rlen, 1, 1);

        if (ret != PTP_RC_OK)
        {
            break;
        }
    }
    while (0);

    return ret;
}

uint16_t
ptp_usb_getresp(PTPParams *params, PTPContainer *resp)
{
    uint16_t ret;
    unsigned long rlen;
    PTPUSBBulkContainer usbresp;


    VitaMTP_Log(VitaMTP_DEBUG, "RESPONSE: ");

    memset(&usbresp,0,sizeof(usbresp));
    /* read response, it should never be longer than sizeof(usbresp) */
    ret = ptp_usb_getpacket(params, &usbresp, &rlen);

    if (ret != PTP_RC_OK)
    {
        ret = PTP_ERROR_IO;
    }
    else if (dtoh16(usbresp.type)!=PTP_USB_CONTAINER_RESPONSE)
    {
        ret = PTP_ERROR_RESP_EXPECTED;
    }
    else if (dtoh16(usbresp.code)!=resp->Code)
    {
        ret = dtoh16(usbresp.code);
    }

    VitaMTP_Log(VitaMTP_DEBUG, "%04x\n", ret);

    if (ret != PTP_RC_OK)
    {
        /*      libusb_glue_error (params,
         "PTP: request code 0x%04x getting resp error 0x%04x",
         resp->Code, ret);*/
        return ret;
    }

    /* build an appropriate PTPContainer */
    resp->Code=dtoh16(usbresp.code);
    resp->SessionID=params->session_id;
    resp->Transaction_ID=dtoh32(usbresp.trans_id);
    resp->Param1=dtoh32(usbresp.payload.params.param1);
    resp->Param2=dtoh32(usbresp.payload.params.param2);
    resp->Param3=dtoh32(usbresp.payload.params.param3);
    resp->Param4=dtoh32(usbresp.payload.params.param4);
    resp->Param5=dtoh32(usbresp.payload.params.param5);
    return ret;
}

/* Event handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK         0x0000  /* waits for */
#define PTP_EVENT_CHECK_FAST        0x0001  /* checks */

static inline uint16_t
ptp_usb_event(PTPParams *params, PTPContainer *event, int wait)
{
    uint16_t ret;
    int result, xread;
    unsigned long rlen;
    PTPUSBEventContainer usbevent;
    struct vita_usb *ptp_usb = (struct vita_usb *)(params->data);

    memset(&usbevent,0,sizeof(usbevent));

    if ((params==NULL) || (event==NULL))
        return PTP_ERROR_BADPARAM;

    ret = PTP_RC_OK;

    switch (wait)
    {
    case PTP_EVENT_CHECK:
        result = USB_BULK_READ(ptp_usb->handle,
                               ptp_usb->intep,
                               (unsigned char *) &usbevent,
                               sizeof(usbevent),
                               &xread,
                               0);

        if (xread == 0)
            result = USB_BULK_READ(ptp_usb->handle,
                                   ptp_usb->intep,
                                   (unsigned char *) &usbevent,
                                   sizeof(usbevent),
                                   &xread,
                                   0);

        if (result < 0) ret = PTP_ERROR_IO;

        break;

    case PTP_EVENT_CHECK_FAST:
        result = USB_BULK_READ(ptp_usb->handle,
                               ptp_usb->intep,
                               (unsigned char *) &usbevent,
                               sizeof(usbevent),
                               &xread,
                               ptp_usb->timeout);

        if (xread == 0)
            result = USB_BULK_READ(ptp_usb->handle,
                                   ptp_usb->intep,
                                   (unsigned char *) &usbevent,
                                   sizeof(usbevent),
                                   &xread,
                                   ptp_usb->timeout);

        if (result < 0) ret = PTP_ERROR_IO;

        break;

    default:
        ret = PTP_ERROR_BADPARAM;
        break;
    }

    if (ret != PTP_RC_OK)
    {
        VitaMTP_Log(VitaMTP_ERROR,
                    "PTP: reading event an error 0x%04x occurred\n", ret);
        return PTP_ERROR_IO;
    }

    rlen = xread;

    if (rlen < 8)
    {
        VitaMTP_Log(VitaMTP_ERROR,
                    "PTP: reading event an short read of %ld bytes occurred\n", rlen);
        return PTP_ERROR_IO;
    }

    /* if we read anything over interrupt endpoint it must be an event */
    /* build an appropriate PTPContainer */
    event->Code=dtoh16(usbevent.code);
    event->SessionID=params->session_id;
    event->Transaction_ID=dtoh32(usbevent.trans_id);
    event->Param1=dtoh32(usbevent.param1);
    event->Param2=dtoh32(usbevent.param2);
    event->Param3=dtoh32(usbevent.param3);
    return ret;
}

uint16_t
ptp_usb_event_check(PTPParams *params, PTPContainer *event)
{

    return ptp_usb_event(params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait(PTPParams *params, PTPContainer *event)
{

    return ptp_usb_event(params, event, PTP_EVENT_CHECK);
}

uint16_t
ptp_usb_control_cancel_request(PTPParams *params, uint32_t transactionid)
{
    struct vita_usb *ptp_usb = (struct vita_usb *)(params->data);
    int ret;
    unsigned char buffer[6];

    htod16a(&buffer[0],PTP_EC_CancelTransaction);
    htod32a(&buffer[2],transactionid);
    ret = libusb_control_transfer(ptp_usb->handle,
                                  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  0x64, 0x0000, 0x0000,
                                  buffer,
                                  sizeof(buffer),
                                  ptp_usb->timeout);

    if (ret < (int)sizeof(buffer))
        return PTP_ERROR_IO;

    return PTP_RC_OK;
}

/**
 * Self-explanatory?
 */
static int find_interface_and_endpoints(libusb_device *dev,
                                        uint8_t *interface,
                                        int *inep,
                                        int *inep_maxpacket,
                                        int *outep,
                                        int *outep_maxpacket,
                                        int *intep)
{
    int i, ret;
    struct libusb_device_descriptor desc;

    ret = libusb_get_device_descriptor(dev, &desc);

    if (ret != LIBUSB_SUCCESS) return -1;

    // Loop over the device configurations
    for (i = 0; i < desc.bNumConfigurations; i++)
    {
        uint8_t j;
        struct libusb_config_descriptor *config;

        ret = libusb_get_config_descriptor(dev, i, &config);

        if (ret != LIBUSB_SUCCESS) continue;

        // Loop over each configurations interfaces
        for (j = 0; j < config->bNumInterfaces; j++)
        {
            uint8_t k;
            uint8_t no_ep;
            int found_inep = 0;
            int found_outep = 0;
            int found_intep = 0;
            const struct libusb_endpoint_descriptor *ep;

            // MTP devices shall have 3 endpoints, ignore those interfaces
            // that haven't.
            no_ep = config->interface[j].altsetting->bNumEndpoints;

            if (no_ep != 3)
                continue;

            *interface = config->interface[j].altsetting->bInterfaceNumber;
            ep = config->interface[j].altsetting->endpoint;

            // Loop over the three endpoints to locate two bulk and
            // one interrupt endpoint and FAIL if we cannot, and continue.
            for (k = 0; k < no_ep; k++)
            {
                if (ep[k].bmAttributes == LIBUSB_TRANSFER_TYPE_BULK)
                {
                    if ((ep[k].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                            LIBUSB_ENDPOINT_DIR_MASK)
                    {
                        *inep = ep[k].bEndpointAddress;
                        *inep_maxpacket = ep[k].wMaxPacketSize;
                        found_inep = 1;
                    }

                    if ((ep[k].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == 0)
                    {
                        *outep = ep[k].bEndpointAddress;
                        *outep_maxpacket = ep[k].wMaxPacketSize;
                        found_outep = 1;
                    }
                }
                else if (ep[k].bmAttributes == LIBUSB_TRANSFER_TYPE_INTERRUPT)
                {
                    if ((ep[k].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                            LIBUSB_ENDPOINT_DIR_MASK)
                    {
                        *intep = ep[k].bEndpointAddress;
                        found_intep = 1;
                    }
                }
            }

            if (found_inep && found_outep && found_intep)
            {
                libusb_free_config_descriptor(config);
                // We assigned the endpoints so return here.
                return 0;
            }

            // Else loop to next interface/config
        }

        libusb_free_config_descriptor(config);
    }

    return -1;
}

static int configure_usb_device(vita_raw_device_t *raw_device, vita_device_t *dev, PTPParams *params)
{
    /* Assign interface and endpoints to usbinfo... */
    if (find_interface_and_endpoints((libusb_device *)raw_device->data,
                                     &dev->usb_device.interface,
                                     &dev->usb_device.inep,
                                     &dev->usb_device.inep_maxpacket,
                                     &dev->usb_device.outep,
                                     &dev->usb_device.outep_maxpacket,
                                     &dev->usb_device.intep))
    {
        VitaMTP_Log(VitaMTP_ERROR, "Unable to find interface & endpoints of device\n");
        return -1;
    }

    // set up usb
    params->sendreq_func=ptp_usb_sendreq;
    params->senddata_func=ptp_usb_senddata;
    params->getresp_func=ptp_usb_getresp;
    params->getdata_func=ptp_usb_getdata;
    params->cancelreq_func=ptp_usb_control_cancel_request;
    params->event_wait=ptp_usb_event_wait;
    params->event_check=ptp_usb_event_check;
    params->data=&dev->usb_device;
    params->transaction_id=0;
    dev->usb_device.timeout = USB_TIMEOUT_DEFAULT;

    if (libusb_open((libusb_device *)raw_device->data, &dev->usb_device.handle) != LIBUSB_SUCCESS)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Failed to open usb device with libusb\n");
        return -1;
    }

    // It seems like on kernel 2.6.31 if we already have it open on another
    // pthread in our app, we'll get an error if we try to claim it again,
    // but that error is harmless because our process already claimed the interface
    libusb_claim_interface(dev->usb_device.handle, dev->usb_device.interface);

    // FIXME : Discovered in the Barry project
    // kernels >= 2.6.28 don't set the interface the same way as
    // previous versions did, and the Blackberry gets confused
    // if it isn't explicitly set
    // See above, same issue with pthreads means that if this fails it is not
    // fatal
    // However, this causes problems on Macs so disable here
#ifndef __APPLE__
    libusb_set_interface_alt_setting(dev->usb_device.handle, dev->usb_device.interface, 0);
#endif

    int ret;

    /*
     * This works in situations where previous bad applications
     * have not used LIBMTP_Release_Device on exit
     */
    if ((ret = ptp_opensession(params, 1)) == PTP_ERROR_IO)
    {
        VitaMTP_Log(VitaMTP_ERROR, "PTP_ERROR_IO: failed to open session\n");
        return -1;
    }

    /* Was the transaction id invalid? Try again */
    if (ret == PTP_RC_InvalidTransactionID)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Transaction ID was invalid, increment and try again\n");
        params->transaction_id += 10;
        ret = ptp_opensession(params, 1);
    }

    if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Could not open session! "
                    "(Return code %d)\n  Try to reset the device.\n",
                    ret);
        libusb_release_interface(dev->usb_device.handle, dev->usb_device.interface);
        return -1;
    }

    return 0;
}

vita_device_t *VitaMTP_Open_USB_Vita(vita_raw_device_t *raw_device)
{
    vita_device_t *dev = calloc(1, sizeof(vita_device_t));
    PTPParams *current_params = calloc(1, sizeof(PTPParams));

    if (dev == NULL || current_params == NULL)
    {
        VitaMTP_Log(VitaMTP_ERROR, "out of memory\n");
        return NULL;
    }

    /* Set upp local debug and error functions */
    // TODO: Error checking functions
    //current_params->debug_func = LIBMTP_ptp_debug;
    //current_params->error_func = LIBMTP_ptp_error;
    /* TODO: Will this always be little endian? */
    current_params->byteorder = PTP_DL_LE;
#ifdef HAVE_ICONV
    current_params->cd_locale_to_ucs2 = iconv_open("UCS-2LE", "UTF-8");
    current_params->cd_ucs2_to_locale = iconv_open("UTF-8", "UCS-2LE");

    if (current_params->cd_locale_to_ucs2 == (iconv_t) -1 ||
            current_params->cd_ucs2_to_locale == (iconv_t) -1)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Cannot open iconv() converters to/from UCS-2!\n"
                    "Too old stdlibc, glibc and libiconv?\n");
        free(current_params);
        free(dev);
        return NULL;
    }

#endif

    if (configure_usb_device(raw_device, dev, current_params) < 0)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Cannot configure USB device.\n");
        free(current_params);
        free(dev);
        return NULL;
    }

    dev->params = current_params;
    memcpy(dev->serial, raw_device->serial, sizeof(raw_device->serial));
    dev->device_type = VitaDeviceUSB;

    if(ptp_getdeviceinfo(current_params, &current_params->deviceinfo) != PTP_RC_OK) {
        VitaMTP_Log(VitaMTP_ERROR, "Unable to read device information on device.\n");
        free(current_params);
        free(dev);
        return NULL;
    }

    if(current_params->deviceinfo.VendorExtensionID != 0x00000006)
    {
        VitaMTP_Log(VitaMTP_ERROR, "No MTP vendor extension on device, trying to continue\n");
    }

    PTPDevicePropDesc dpd;
    memset(&dpd,0,sizeof(dpd));

    int ret = ptp_getdevicepropdesc(current_params, PTP_DPC_MTP_DeviceFriendlyName, &dpd);
    if(ret == PTP_ERROR_DATA_EXPECTED)
    {
        VitaMTP_Log(VitaMTP_ERROR, "Cannot read device name, device on standby\n");
        free(current_params);
        free(dev);
        return NULL;
    }
    else
    {
        if(dpd.DataType == PTP_DTC_STR)
        {
            VitaMTP_Log(VitaMTP_INFO, "Detected device name: %s\n", dpd.CurrentValue.str);
            ptp_free_devicepropdesc(&dpd);
        }
        else
        {
            VitaMTP_Log(VitaMTP_ERROR, "Wrong DataType for friendly name: %i\n", dpd.DataType);
        }
    }

    return dev;
}

/**
 * This closes and releases an allocated MTP device.
 * @param device a pointer to the MTP device to release.
 */
void VitaMTP_Release_USB_Device(vita_device_t *device)
{
    PTPParams *params = (PTPParams *) device->params;
    struct vita_usb *ptp_usb = (struct vita_usb *) &device->usb_device;

    if (ptp_closesession(params) != PTP_RC_OK)
    {
        VitaMTP_Log(VitaMTP_ERROR, "ERROR: Could not close session!\n");
    }

    libusb_close(ptp_usb->handle);
#ifdef HAVE_ICONV
    // Free iconv() converters...
    iconv_close(params->cd_locale_to_ucs2);
    iconv_close(params->cd_ucs2_to_locale);
#endif
    ptp_free_params(params);
    free(params);
    free(device);
}

int VitaMTP_Get_USB_Vitas(vita_raw_device_t **p_raw_devices)
{
    int i = 0;
    int n = 0;
    libusb_device *dev;
    libusb_device **devs;

    if (libusb_get_device_list(g_usb_context, &devs) < 0)
    {
        VitaMTP_Log(VitaMTP_ERROR, "libusb failed to get device list.\n");
        return -1;
    }

    vita_raw_device_t *vitas = malloc(sizeof(vita_raw_device_t));
    libusb_device_handle *handle;
    int size = 1;

    if (vitas == NULL)
    {
        VitaMTP_Log(VitaMTP_ERROR, "out of memory\n");
        return -1;
    }

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        if (libusb_get_device_descriptor(dev, &desc) < 0)
        {
            VitaMTP_Log(VitaMTP_ERROR, "libusb failed to get device descriptor.\n");
            free(vitas);
            return -1;
        }

        if (!(desc.idVendor == VITA_VID && desc.idProduct == VITA_PID))
        {
            continue; // not a vita
        }

        if (n+1 > size)
        {
            size = (n+1) * 2;

            if ((vitas = realloc(vitas, size * sizeof(vita_raw_device_t))) == NULL)
            {
                VitaMTP_Log(VitaMTP_ERROR, "out of memory\n");
                return -1;
            }
        }

        if (libusb_open(dev, &handle) != LIBUSB_SUCCESS)
        {
            VitaMTP_Log(VitaMTP_ERROR, "cannot open usb\n");
            free(vitas);
            return -1;
        }

        if (libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *)vitas[n].serial,
                                               sizeof(vitas[n].serial)) < 0)
        {
            VitaMTP_Log(VitaMTP_ERROR, "cannot get device serial\n");
            free(vitas);
            return -1;
        }

        vitas[n].serial[sizeof(vitas[n].serial) - 1] = '\0';  // null terminate
        libusb_ref_device(dev);  // don't release this device
        vitas[n].data = dev; // save USB device
        libusb_close(handle);
        n++;
    }

    // shrink device array
    if (n > 0 && (vitas = realloc(vitas, n * sizeof(vita_raw_device_t))) == NULL)
    {
        VitaMTP_Log(VitaMTP_ERROR, "out of memory\n");
        return -1;
    }

    libusb_free_device_list(devs, 1);
    *p_raw_devices = vitas;

    return n;
}

void VitaMTP_Unget_USB_Vitas(vita_raw_device_t *raw_devices, int numdevs)
{
    for (int i = 0; i < numdevs; i++)
    {
        libusb_unref_device((libusb_device *)raw_devices[i].data);
    }

    free(raw_devices);
}

/**
 * Get the first (as in "first in the list of") connected Vita MTP device.
 * @return a device pointer. NULL if error, no connected device, or no connected Vita
 * @see LIBMTP_Get_Connected_Devices()
 */
vita_device_t *VitaMTP_Get_First_USB_Vita(void)
{
    vita_device_t *first_device = NULL;
    vita_raw_device_t *devices;
    int numdevs;

    if ((numdevs = VitaMTP_Get_USB_Vitas(&devices)) < 0)
    {
        return NULL;
    }

    if (devices == NULL || numdevs == 0)
    {
        free(devices);
        return NULL;
    }

    first_device = VitaMTP_Open_USB_Vita(&devices[0]);

    VitaMTP_Unget_USB_Vitas(devices, numdevs);
    return first_device;
}

/**
 * Call libusb_init only once so the thread spamming from calling get_usb_vita every second can
 * be avoided
 */
int VitaMTP_USB_Init(void)
{
    if (libusb_init(&g_usb_context) < 0)
    {
        VitaMTP_Log(VitaMTP_ERROR, "libusb init failed.\n");
        return -1;
    }
    return 0;
}

void VitaMTP_USB_Exit(void)
{
    if(g_usb_context)
    {
        libusb_exit(g_usb_context);
    }
}

/**
 * Clear the halt condition on the endpoint. Should be called after a cancel event.
 */
int VitaMTP_USB_Clear(vita_device_t *vita_device)
{
    return libusb_clear_halt(vita_device->usb_device.handle, vita_device->usb_device.outep);
}

/**
 * Reinitialize the device, call it something wrong happens with the endpoint
 */
int VitaMTP_USB_Reset(vita_device_t *vita_device)
{
    return libusb_reset_device(vita_device->usb_device.handle);
}

// end of functions taken from libmtp
#endif // ifdef PTP_USB_SUPPORT
