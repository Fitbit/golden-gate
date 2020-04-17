/**
*
* @file
*
* @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
*
* @author Gilles Boccon-Gibod
*
* @date 2020-04-03
*
* @details
*
* Stack Tool Tunnel Transport
*/

//----------------------------------------------------------------------
// includes
//----------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/kern_control.h>
#include <net/if_utun.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "xp/common/gg_common.h"
#include "xp/loop/extensions/gg_loop_fd.h"
#include "gg_stack_tool_macos_tunnel.h"

//----------------------------------------------------------------------
// logging
//----------------------------------------------------------------------
GG_SET_LOCAL_LOGGER("gg.xp.app.stack-tool.macos-tunnel")

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------
#define GG_STACK_TOOL_MACOS_TUNNEL_BUFFER_SIZE 4096
#define GG_STACK_TOOL_TUNNEL_MIN_RECEIVE_SIZE  24   // 4 bytes tunnel overhead plus 20 bytes IP min

//----------------------------------------------------------------------
// types
//----------------------------------------------------------------------
struct GG_StackToolMacosTunnel {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSink);

    GG_LoopFileDescriptorEventHandler handler;  // to monitor the file descriptor
    bool                              trace;    // verbose trace or not
    GG_DataSinkListener*              listener; // listener that may be waiting
    GG_DataSink*                      sink;     // where to send packets to
    uint8_t                           buffer[GG_STACK_TOOL_MACOS_TUNNEL_BUFFER_SIZE];
};

//----------------------------------------------------------------------
// functions
//----------------------------------------------------------------------

//----------------------------------------------------------------------
static void
PrintIpAddress(uint32_t ip_addr)
{
    uint8_t bytes[4];
    GG_BytesFromInt32Be(bytes, ip_addr);
    printf("%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

//----------------------------------------------------------------------
static const char*
GetProtocolName(unsigned int protocol) {
    switch (protocol) {
        case 0x01: return "ICMP";
        case 0x06: return "TCP";
        case 0x11: return "UDP";
        default:   return NULL;
    }
}

//----------------------------------------------------------------------
static void
ShowPacket(const char* prefix, const uint8_t* packet, size_t packet_size)
{
    printf("%s, size=%u\n", prefix, (int)packet_size);

    if (packet_size < 20) return;
    unsigned int version         = packet[0] >> 4;
    unsigned int ihl             = packet[0] & 0x0F;
    unsigned int dscp            = packet[1] >> 2;
    unsigned int ecn             = packet[1] & 0x03;
    unsigned int total_length    = GG_BytesToInt16Be(&packet[2]);
    unsigned int identification  = GG_BytesToInt16Be(&packet[4]);
    unsigned int flags           = packet[6] >> 5;
    unsigned int fragment_offset = GG_BytesToInt16Be(&packet[6]) & 0x1EFF;
    unsigned int ttl             = packet[8];
    unsigned int protocol        = packet[9];
    unsigned int checksum        = GG_BytesToInt16Be(&packet[10]);
    uint32_t     src_ip_addr     = GG_BytesToInt32Be(&packet[12]);
    uint32_t     dst_ip_addr     = GG_BytesToInt32Be(&packet[16]);

    printf("Version             = %d\n", version);
    printf("IHL                 = %d\n", ihl);
    printf("DSCP                = %d\n", dscp);
    printf("ECN                 = %d\n", ecn);
    printf("Total Length        = %d\n", total_length);
    printf("Identification      = %x\n", identification);
    printf("Flags               = %x\n", flags);
    printf("Fragment Offset     = %d\n", fragment_offset);
    printf("TTL                 = %d\n", ttl);
    const char* protocol_name = GetProtocolName(protocol);
    if (protocol_name) {
        printf("Protocol            = %s (%d)\n", protocol_name, protocol);
    } else {
        printf("Protocol            = %d\n", protocol);
    }
    printf("Checksum            = %04x\n", checksum);
    printf("Source Address      = ");
    PrintIpAddress(src_ip_addr);
    printf("\n");
    printf("Destination Address = ");
    PrintIpAddress(dst_ip_addr);
    printf("\n");

    if (protocol == 0x11 && packet_size >= 28) {
        // UDP
        uint16_t src_port     = GG_BytesToInt16Be(&packet[20]);
        uint16_t dst_port     = GG_BytesToInt16Be(&packet[22]);
        uint16_t udp_length   = GG_BytesToInt16Be(&packet[24]);
        uint16_t udp_checksum = GG_BytesToInt16Be(&packet[26]);
        printf("UDP:\n");
        printf("  Source port      = %d\n",   (int)src_port);
        printf("  Destination port = %d\n",   (int)dst_port);
        printf("  Datagram Length  = %d\n",   (int)udp_length);
        printf("  Checksum         = %04x\n", (int)udp_checksum);
    }

    printf("\n");
}

//----------------------------------------------------------------------
// Called when data is received from the stack
//----------------------------------------------------------------------
static GG_Result
GG_StackToolMacosTunnel_PutData(GG_DataSink*             _self,
                                GG_Buffer*               data,
                                const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(metadata);
    GG_StackToolMacosTunnel* self = GG_SELF(GG_StackToolMacosTunnel, GG_DataSink);

    const uint8_t* packet      = GG_Buffer_GetData(data);
    size_t         packet_size = GG_Buffer_GetDataSize(data);
    if (self->trace) {
        ShowPacket(">>> IP Packet STACK -> TUNNEL", packet, packet_size);
    }

    // check that the data can fit in our buffer
    if (packet_size + 4 > sizeof(self->buffer)) {
        GG_LOG_WARNING("packet too large, dropping");
        return GG_SUCCESS;
    }

    // prefix the data with the header expected by the tunnel
    self->buffer[0] = 0;
    self->buffer[1] = 0;
    self->buffer[2] = 0;
    self->buffer[3] = AF_INET;
    memcpy(&self->buffer[4], packet, packet_size);
    packet_size += 4;

    // write the data to the tunnel
    ssize_t send_result = send(self->handler.fd, self->buffer, packet_size, 0);
    if (send_result != (ssize_t)packet_size) {
        GG_LOG_WARNING("send returned %u but we expected %u", (int)send_result, (int)packet_size);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_StackToolMacosTunnel_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_StackToolMacosTunnel* self = GG_SELF(GG_StackToolMacosTunnel, GG_DataSink);

    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_StackToolMacosTunnel_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_StackToolMacosTunnel* self = GG_SELF(GG_StackToolMacosTunnel, GG_DataSource);

    // keep a reference to the sink
    self->sink = sink;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Called when data is received from the tunnel
//----------------------------------------------------------------------
static void
GG_StackToolMacosTunnel_OnEvent(GG_LoopEventHandler* _self, GG_Loop* loop)
{
    GG_COMPILER_UNUSED(loop);
    GG_StackToolMacosTunnel* self = GG_SELF_M(handler.base, GG_StackToolMacosTunnel, GG_LoopEventHandler);

    // check if we can read
    if (self->handler.event_flags & GG_EVENT_FLAG_FD_CAN_READ) {
        // read the data that's available
        ssize_t recv_result = recv(self->handler.fd, self->buffer, sizeof(self->buffer), 0);
        GG_LOG_FINER("recv returned %d", (int)recv_result);

        // sanity check
        if (recv_result < GG_STACK_TOOL_TUNNEL_MIN_RECEIVE_SIZE) {
            GG_LOG_WARNING("received packet is too short");
            return;
        }

        GG_DynamicBuffer* packet      = NULL;
        size_t            packet_size = (size_t)recv_result - 4;
        if (self->trace) {
            ShowPacket("<<< IP Packet TUNNEL -> STACK", &self->buffer[4], packet_size);
        }

        // check that we have a sink
        if (!self->sink) {
            GG_LOG_FINE("no sink, dropping");
            return;
        }

        // allocate a packet to copy into and send out
        GG_Result result = GG_DynamicBuffer_Create(packet_size, &packet);
        if (GG_FAILED(result)) {
            GG_LOG_SEVERE("out of memory");
            return;
        }
        GG_DynamicBuffer_SetData(packet, &self->buffer[4], packet_size);
        result = GG_DataSink_PutData(self->sink, GG_DynamicBuffer_AsBuffer(packet), NULL);
        if (GG_FAILED(result)) {
            if (result == GG_ERROR_WOULD_BLOCK) {
                // TODO: maybe queue the packet instead of dropping
                GG_LOG_FINE("GG_DataSink_PutData would block, dropping");
            } else {
                GG_LOG_WARNING("GG_DataSink_PutData failed (%d)", result);
            }
        }
        GG_DynamicBuffer_Release(packet);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_StackToolMacosTunnel, GG_DataSink) {
    GG_StackToolMacosTunnel_PutData,
    GG_StackToolMacosTunnel_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_StackToolMacosTunnel, GG_DataSource) {
    GG_StackToolMacosTunnel_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_StackToolMacosTunnel, GG_LoopEventHandler) {
    GG_StackToolMacosTunnel_OnEvent
};

//----------------------------------------------------------------------
GG_Result
GG_StackToolMacosTunnel_Create(GG_Loop*                  loop,
                               bool                      trace,
                               GG_StackToolMacosTunnel** tunnel)
{

    // create a control socket
    int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        return GG_ERROR_ERRNO(errno);
    }

    // configure the socket
    GG_Result result = GG_SUCCESS;
    struct ctl_info info = { 0 };
    strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);
    int bsd_result = ioctl(fd, CTLIOCGINFO, &info);
    if (bsd_result != 0) {
        result = GG_ERROR_ERRNO(errno);
        GG_LOG_WARNING("ioctl failed (%d)", result);
        goto end;
    }

    // connect the socket
    struct sockaddr_ctl address = {
        .sc_len     = sizeof(struct sockaddr_ctl),
        .sc_family  = AF_SYSTEM,
        .ss_sysaddr = AF_SYS_CONTROL,
        .sc_id      = info.ctl_id,
        .sc_unit    = 0
    };
    bsd_result = connect(fd, (struct sockaddr *)&address, sizeof (address));
    if (bsd_result != 0) {
        result = GG_ERROR_ERRNO(errno);
        GG_LOG_WARNING("connect failed (%d)", result);
        goto end;
    }

    // get the name allocated for this tunnel interface
    char      ifname[32];
    socklen_t ifname_len = sizeof(ifname) - 1;
    ifname[ifname_len] = 0;
    bsd_result = getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifname_len);
    if (bsd_result != 0) {
        result = GG_ERROR_ERRNO(errno);
        GG_LOG_WARNING("getsockopt failed (%d)", result);
        goto end;
    }
    printf("Tunnel Interface Name: %s\n", ifname);

    // make the socket non-blocking
    bsd_result = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (bsd_result != 0) {
        result = GG_ERROR_ERRNO(errno);
        GG_LOG_WARNING("fcntl failed (%d)", result);
        goto end;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    // allocate a new object
    *tunnel = GG_AllocateZeroMemory(sizeof(GG_StackToolMacosTunnel));
    if (!*tunnel) {
        result = GG_ERROR_OUT_OF_MEMORY;
        goto end;
    }
    (*tunnel)->trace               = trace;
    (*tunnel)->handler.fd          = fd;
    (*tunnel)->handler.event_flags = 0;
    (*tunnel)->handler.event_mask  = GG_EVENT_FLAG_FD_CAN_READ | GG_EVENT_FLAG_FD_ERROR;

    // setup interfaces
    GG_SET_INTERFACE(&(*tunnel)->handler.base, GG_StackToolMacosTunnel, GG_LoopEventHandler);
    GG_SET_INTERFACE((*tunnel), GG_StackToolMacosTunnel, GG_DataSource);
    GG_SET_INTERFACE((*tunnel), GG_StackToolMacosTunnel, GG_DataSink);

    // monitor the file descriptor
    GG_Loop_AddFileDescriptorHandler(loop, &(*tunnel)->handler);

end:
    if (GG_FAILED(result)) {
        close(fd);
    }

    return result;
}

//----------------------------------------------------------------------
void
GG_StackToolMacosTunnel_Destroy(GG_StackToolMacosTunnel* self)
{
    if (!self) return;
    close(self->handler.fd);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_StackToolMacosTunnel_AsDataSource(GG_StackToolMacosTunnel* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink* GG_StackToolMacosTunnel_AsDataSink(GG_StackToolMacosTunnel* self)
{
    return GG_CAST(self, GG_DataSink);
}
