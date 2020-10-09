#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CONTROL_STAGE_SETUP     0
#define CONTROL_STAGE_DATA      1
#define CONTROL_STAGE_STATUS    2
#define CONTROL_STAGE_COMPLETE  3

#define TRANSFER_ISOCH          0
#define TRANSFER_INT            1
#define TRANSFER_CTL            2
#define TRANSFER_BULK           3

#define FOCUSRITE_INIT1         0x00000000
#define FOCUSRITE_INIT2         0x00000002
#define FOCUSRITE_SAVE_CONFIG   0x00000006
#define FOCUSRITE_GET_METERS    0x00001001
#define FOCUSRITE_SET_MIX       0x00002002
#define FOCUSRITE_SET_MUX       0x00003002
#define FOCUSRITE_GET_DATA      0x00800000
#define FOCUSRITE_SET_DATA      0x00800001
#define FOCUSRITE_DATA_CMD      0x00800002

typedef uint8_t UCHAR;
typedef uint16_t USHORT;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint64_t ULONG;
typedef uint32_t USBD_STATUS;

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic_number;   /* magic number */
    uint16_t version_major;  /* major version number */
    uint16_t version_minor;  /* minor version number */
    int32_t  thiszone;       /* GMT to local correction */
    uint32_t sigfigs;        /* accuracy of timestamps */
    uint32_t snaplen;        /* max length of captured packets, in octets */
    uint32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct
{
    uint32_t ts_sec;         /* timestamp seconds */
    uint32_t ts_usec;        /* timestamp microseconds */
    uint32_t incl_len;       /* number of octets of packet saved in file */
    uint32_t orig_len;       /* actual length of packet */
} pcap_packet_t;

typedef struct
{
    USHORT       headerLen; /* This header length */
    UINT64       irpId;     /* I/O Request packet ID */
    USBD_STATUS  status;    /* USB status code (on return from host controller) */
    USHORT       function;  /* URB Function */
    UCHAR        info;      /* I/O Request info */
    USHORT       bus;       /* bus (RootHub) number */
    USHORT       device;    /* device address */
    UCHAR        endpoint;  /* endpoint number and transfer direction */
    UCHAR        transfer;  /* transfer type */
    UINT32       dataLength;/* Data length */
} usb_header_t;

typedef struct
{
    ULONG        offset;
    ULONG        length;
    USBD_STATUS  status;
} iso_packet_t;

typedef struct
{
    ULONG             startFrame;
    ULONG             numberOfPackets;
    ULONG             errorCount;
    iso_packet_t      packet[1];
} isoch_header_t;

typedef struct
{
    UCHAR             stage;
} control_header_t;

typedef struct
{
    UCHAR           requestType;
    UCHAR           request;
    USHORT          value;
    USHORT          index;
    USHORT          length;
} control_data_t;

typedef struct focusrite_packet_t
{
    uint32_t        cmd;
    uint16_t        size;
    uint16_t        seq;
    uint32_t        error;
    uint32_t        pad;
} focusrite_packet_t;

typedef struct
{
    uint32_t        offset;
    uint32_t        bytes;
} set_data_packet_t;

#pragma pack(pop)

bool read_header(FILE *fd, pcap_hdr_t *hdr)
{
    fread(hdr, sizeof(pcap_hdr_t), 1, fd);
    return hdr->magic_number == 0xa1b2c3d4;
}

ssize_t read_packet(FILE *fd, pcap_packet_t *packet, uint8_t *data)
{
    if (fread(packet, sizeof(pcap_packet_t), 1, fd) != 1)
        return -1;

    size_t len = packet->incl_len;
    if (fread(data, len, 1, fd) != 1)
        return -1;

    return len;
}

char *decode_func(char *dst, USHORT func)
{
    switch (func)
    {
        case 0x00: strcpy(dst, "SELECT_CONFIGURATION"); break;
        case 0x08: strcpy(dst, "CONTROL_TRANSFER"); break;
        case 0x09: strcpy(dst, "BULK_OR_INTERRUPT"); break;
        case 0x0b: strcpy(dst, "GET_DESCRIPTOR"); break;
        default: sprintf(dst, "func=0x%04x", func); break;
    }

    return dst;
}

char *decode_address(char *dst, usb_header_t *hdr)
{
    const char *dir = (hdr->endpoint & 0x80) ? "IN" : "OUT";
    if (hdr->info & 0x01)
        sprintf(dst, "%d.%d.%d -> host %s", hdr->bus, hdr->device, hdr->endpoint & 0x0f, dir);
    else
        sprintf(dst, "host -> %d.%d.%d %s", hdr->bus, hdr->device, hdr->endpoint & 0x0f, dir);
    return dst;
};

char *decode_stage(char *dst, UCHAR stage)
{
    switch (stage)
    {
        case CONTROL_STAGE_SETUP: strcpy(dst, "SETUP"); break;
        case CONTROL_STAGE_DATA: strcpy(dst, "DATA"); break;
        case CONTROL_STAGE_STATUS: strcpy(dst, "STATUS"); break;
        case CONTROL_STAGE_COMPLETE: strcpy(dst, "COMPLETE"); break;
        default: sprintf(dst, "stage=0x%x", stage); break;
    }
    return dst;
};

char *decode_request(char *dst, control_data_t *cdata)
{
    const char *phase = (cdata->requestType & 0x80) ? "DTH" : "HTD";
    const char *type, *recv;
    char cmd[0x20];
    uint8_t rtype = (cdata->requestType >> 5) & 0x03;
    uint8_t recip = cdata->requestType & 0x1f;

    switch (rtype)
    {
        case 0: type = "STANDARD"; break;
        case 1: type = "CLASS"; break;
        case 2: type = "VENDOR"; break;
        case 3: type = "RESERVED"; break;
        default: type = "UNKNOWN"; break;
    }

    switch (recip)
    {
        case 0: recv = "DEVICE"; break;
        case 1: recv = "IFACE"; break;
        case 2: recv = "ENDPOINT"; break;
        case 3: recv = "OTHER"; break;
        default: recv = "RESERVED"; break;
    }

    if (rtype == 0)
    {
        if (recip == 0) // device
        {
            switch (cdata->request)
            {
                case 0x00: strcpy(cmd, "GET_STATUS"); break;
                case 0x01: strcpy(cmd, "CLEAR_FEATURE"); break;
                case 0x03: strcpy(cmd, "SET_FEATURE"); break;
                case 0x05: strcpy(cmd, "SET_ADDRESS"); break;
                case 0x06: strcpy(cmd, "GET_DESCRIPTOR"); break;
                case 0x07: strcpy(cmd, "SET_DESCRIPTOR"); break;
                case 0x08: strcpy(cmd, "GET_CONFIGURATION"); break;
                case 0x09: strcpy(cmd, "SET_CONFIGURATION"); break;
                default: sprintf(cmd, "UNKNOWN(0x%x)", cdata->request); break;
            }
        }
        else if (recip == 1) // iface
        {
            switch (cdata->request)
            {
                case 0x00: strcpy(cmd, "GET_STATUS"); break;
                case 0x01: strcpy(cmd, "CLEAR_FEATURE"); break;
                case 0x03: strcpy(cmd, "SET_FEATURE"); break;
                case 0x0a: strcpy(cmd, "GET_INTERFACE"); break;
                case 0x11: strcpy(cmd, "SET_INTERFACE"); break;
                default: sprintf(cmd, "UNKNOWN(0x%x)", cdata->request); break;
            }
        }
        else if (recip == 2) // endpoint
        {
            switch (cdata->request)
            {
                case 0x00: strcpy(cmd, "GET_STATUS"); break;
                case 0x01: strcpy(cmd, "CLEAR_FEATURE"); break;
                case 0x03: strcpy(cmd, "SET_FEATURE"); break;
                case 0x12: strcpy(cmd, "SYNCH_FRAME"); break;
                default: sprintf(cmd, "UNKNOWN(0x%x)", cdata->request); break;
            }
        }
        else
            sprintf(cmd, "UNKNOWN(0x%x)", cdata->request);
    }
    else
        sprintf(cmd, "UNKNOWN(0x%x)", cdata->request);

    sprintf(dst, "0x%02x[%s %s %s] 0x%02x[%s]", cdata->requestType,phase, type, recv,  cdata->request, cmd);
    return dst;
}

char *decode_focusrite_cmd(char *dst, uint32_t cmd)
{
    switch(cmd)
    {
        case FOCUSRITE_INIT1:           strcpy(dst, "INIT1"); break;
        case FOCUSRITE_INIT2:           strcpy(dst, "INIT2"); break;
        case FOCUSRITE_SAVE_CONFIG:     strcpy(dst, "SAVE_CONFIG"); break;
        case FOCUSRITE_GET_METERS:      strcpy(dst, "GET_METERS"); break;
        case FOCUSRITE_SET_MIX:         strcpy(dst, "SET_MIX"); break;
        case FOCUSRITE_SET_MUX:         strcpy(dst, "SET_MUX"); break;
        case FOCUSRITE_GET_DATA:        strcpy(dst, "GET_DATA"); break;
        case FOCUSRITE_SET_DATA:        strcpy(dst, "SET_DATA"); break;
        case FOCUSRITE_DATA_CMD:        strcpy(dst, "DATA_CMD"); break;
        default: sprintf(dst, "UNKNOWN(0x%x)", cmd); break;
    }

    return dst;
}

void dump_packet(pcap_packet_t *packet, uint8_t *data)
{
    usb_header_t *hdr;
    isoch_header_t *iso;
    control_header_t *ctl;
    control_data_t *cdata;
    focusrite_packet_t *fpacket;
    set_data_packet_t *set_data;

    size_t len;
    char s_func[0x40], s_addr[0x40], s_stage[0x40], s_req[0x40], s_cmd[0x40];

    len                 = packet->incl_len;
    uint8_t *head       = data;
    uint8_t *tail       = &data[len];
    hdr                 = reinterpret_cast<usb_header_t *>(data);
    data               += sizeof(usb_header_t);


    printf("%d.%d: [%s] hlen=%d, st=%x, %s",
            packet->ts_sec, packet->ts_usec,
            decode_address(s_addr, hdr),
            hdr->headerLen, hdr->status,
            decode_func(s_func, hdr->function)
    );

    switch (hdr->transfer)
    {
        case TRANSFER_ISOCH:
            iso         = reinterpret_cast<isoch_header_t *>(data);
            data       += sizeof(isoch_header_t);

            printf(" ISOCHR start=0x%llx, packets=%lld, errors=%lld, packet={off=0x%llx, len=0x%llx, status=0x%x}",
                    (long long)iso->startFrame, (long long)iso->numberOfPackets, (long long)iso->errorCount,
                    (long long)iso->packet[0].offset, (long long)iso->packet[0].length, (int)iso->packet[0].status
                );
            break;
        case TRANSFER_INT:
            printf(" INTERRUPT");
            break;
        case TRANSFER_CTL:
            ctl         = reinterpret_cast<control_header_t *>(data);
            data       += sizeof(control_header_t);
            printf(" CONTROL %s", decode_stage(s_stage, ctl->stage));
            if (ctl->stage != CONTROL_STAGE_COMPLETE)
            {
                cdata       = reinterpret_cast<control_data_t *>(data);
                data       += sizeof(control_data_t);

                printf(" %s value=%d, index=%d, length=%d",
                        decode_request(s_req, cdata),
                        cdata->value, cdata->index, cdata->length);
            }
            break;
        case TRANSFER_BULK:
            printf(" BULK");
            break;
        default:
            printf(" tr=0x%x", hdr->transfer);
            break;
    }

    printf(" |");
    if ((hdr->transfer == TRANSFER_CTL) &&
        (ctl->stage != CONTROL_STAGE_COMPLETE) &&
        (cdata->requestType == 0x21) &&
        (cdata->request == 0x02))
    {
        fpacket     = reinterpret_cast<focusrite_packet_t *>(data);
        data       += sizeof(focusrite_packet_t);

        // Decode focusrite proprietary protocol
        printf(" FOCUSRITE[%s size=%d seq=0x%x error=%d pad=%d]",
                decode_focusrite_cmd(s_cmd, fpacket->cmd),
                fpacket->size, fpacket->seq, fpacket->error, fpacket->pad
        );

        if (fpacket->cmd == FOCUSRITE_SET_DATA)
        {
            set_data    = reinterpret_cast<set_data_packet_t *>(data);
            data       += sizeof(set_data_packet_t);

            printf(" SET_DATA[offset=0x%x, bytes=%d]", set_data->offset, set_data->bytes);
        }
    }

    // Print as bytes
    len    -= (data - head);
    for (size_t i=0; i<len; ++i)
        printf(" %02x", data[i]);

    printf("\n");
}

int main(int argc, const char **argv)
{
    FILE *fd = fopen(argv[1], "r");
    if (fd == NULL)
        return -1;

    ssize_t len;

    // Read header
    pcap_hdr_t pcap_hdr;
    if (!read_header(fd, &pcap_hdr))
    {
        fprintf(stderr, "Could not read PCAP header\n");
        return -1;
    }

    // Allocate packet data
    pcap_packet_t packet;
    uint8_t *data = static_cast<uint8_t *>(malloc(pcap_hdr.snaplen + 0x100));

    // Parse file
    while ((len = read_packet(fd, &packet, data)) > 0)
    {
        dump_packet(&packet, data);
    }
    
    off64_t pos = ftello64(fd);
    printf("Overall read bytes: %lld\n", (long long)pos);

    fclose(fd);
}

