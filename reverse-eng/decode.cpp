#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

#define DEVICE_DATA_SIZE        0x10000

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

typedef struct
{
    uint32_t        offset;
    uint32_t        bytes;
} get_data_packet_t;

typedef struct
{
    uint16_t        channel;
    uint16_t        gain[];
} set_mix_packet_t;

#pragma pack(pop)

typedef struct
{
    uint32_t        init_sec;       // Initial seconds
    uint32_t        init_usec;      // Initial milliseconds

    bool            ctl_setup;      // control setup flag
    size_t          requestType;
    size_t          request;

    ssize_t         data_off;       // Data offset for GET_DATA request
    size_t          data_size;      // Maximum size of data
    uint8_t         data[DEVICE_DATA_SIZE];  // Internal data state of the device
} protocol_state_t;

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

void update_protocol_state(protocol_state_t *pstate, usb_header_t *hdr, control_header_t *ctl, control_data_t *cdata)
{
    if (hdr->transfer == TRANSFER_CTL)
    {
        if (ctl->stage == CONTROL_STAGE_SETUP)
        {
            pstate->ctl_setup   = true;
            pstate->requestType = cdata->requestType;
            pstate->request     = cdata->request;
        }
        else if (ctl->stage == CONTROL_STAGE_COMPLETE)
        {
            pstate->ctl_setup   = false;
            pstate->requestType = -1;
            pstate->request     = -1;
        }
    }
}

bool is_focusrite_packet(protocol_state_t *pstate, usb_header_t *hdr, control_header_t *ctl, control_data_t *cdata)
{
    if (pstate->ctl_setup)
    {
        if ((hdr->transfer == TRANSFER_CTL) &&
            (ctl->stage == CONTROL_STAGE_COMPLETE) &&
            (pstate->requestType == 0xa1) &&
            (pstate->request == 0x03))
            return true;
    }

    if ((hdr->transfer == TRANSFER_CTL) &&
        (ctl->stage == CONTROL_STAGE_SETUP) &&
        (cdata->requestType == 0x21) &&
        (cdata->request == 0x02))
        return true;

    return false;
}

inline float decode_gain(uint16_t level)
{
    return 20.0f * log10(level / 8192.0f);
}

void emit_byte_array(const uint8_t *data, size_t count)
{
    putchar('[');
    for (size_t i=0; i<count; ++i)
    {
        if (i > 0)
            putchar(' ');
        printf("%02x", data[i]);
    }
    putchar(']');
}

void commit_data_changes(protocol_state_t *pstate, size_t offset, const uint8_t *src, size_t bytes)
{
    ssize_t start   = -1;
    uint8_t *dst    = &pstate->data[offset];
    size_t count    = 0;

    // Lookup for changes
    for (size_t i=0; i<bytes; ++i)
    {
        if (start < 0)
        {
            // We're tracking start of changes
            if (dst[i] != src[i])
                start       = i;
        }
        else // start >= 0
        {
            // We're tracking end of changes
            if (dst[i] == src[i])
            {
                if ((count++) > 0)
                    fputs(", ", stdout);
                printf("0x%04x:", int(offset + start));
                emit_byte_array(&dst[start], i-start);
                printf("->");
                emit_byte_array(&src[start], i-start);
                memcpy(&dst[start], &src[start], i-start);
                start = -1;
            }
        }
    }

    // Emit tail changes
    if (start >= 0)
    {
        if ((count++) > 0)
            fputs(", ", stdout);
        printf("0x%04x:", int(offset + start));
        emit_byte_array(&dst[start], bytes-start);
        printf("->");
        emit_byte_array(&src[start], bytes-start);
        memcpy(&dst[start], &src[start], bytes-start);
        start = -1;
    }

    // Update maximum data size
    if (pstate->data_size < offset + bytes)
        pstate->data_size = offset + bytes;
}

void decode_focusrite_packet(protocol_state_t *pstate, usb_header_t *hdr, control_header_t *ctl, control_data_t *cdata, uint8_t **buf)
{
    uint8_t *data       = *buf;
    char s_cmd[0x40];
    if (!is_focusrite_packet(pstate, hdr, ctl, cdata))
        return;

    focusrite_packet_t *fpacket = reinterpret_cast<focusrite_packet_t *>(data);
    data           += sizeof(focusrite_packet_t);
    bool req        = (!pstate->ctl_setup);

    // Decode focusrite proprietary protocol
    switch (fpacket->cmd)
    {
        case FOCUSRITE_SET_DATA:
        {
            if (req)
            {
                set_data_packet_t *set_data = reinterpret_cast<set_data_packet_t *>(data);
                data       += sizeof(set_data_packet_t);

                printf(" SET_DATA[offset=0x%x bytes=%d changes={", set_data->offset, set_data->bytes);
                commit_data_changes(pstate, set_data->offset, data, set_data->bytes);
                printf("}]");
            }
            else
                printf(" SET_DATA_ACK");
            break;
        }

        case FOCUSRITE_GET_DATA:
        {
            if (req)
            {
                get_data_packet_t *get_data = reinterpret_cast<get_data_packet_t *>(data);
                data       += sizeof(set_data_packet_t);
                pstate->data_off    = get_data->offset;

                printf(" GET_DATA[offset=0x%x bytes=%d changes={", get_data->offset, get_data->bytes);
                commit_data_changes(pstate, get_data->offset, data, get_data->bytes);
                printf("}]");
            }
            else
            {
                if (pstate->data_off > 0)
                    printf(" DATA[offset=0x%x bytes=%d]", int(pstate->data_off), fpacket->size);
                else
                    printf(" DATA[bytes=%d]", fpacket->size);
                pstate->data_off    = -1;
            }
            break;
        }

        case FOCUSRITE_SET_MIX:
        {
            if (req)
            {
                set_mix_packet_t *set_mix = reinterpret_cast<set_mix_packet_t *>(data);
                size_t gains= (fpacket->size - sizeof(set_mix_packet_t)) / sizeof(uint16_t);

                printf(" SET_MIX[channel=%d, levels=%d, gains={", set_mix->channel, int(gains));
                for (size_t i=0; i<gains; ++i)
                    printf(" %.2f(0x%04x)", decode_gain(set_mix->gain[i]), set_mix->gain[i]);
                printf(" }]");
            }
            else
                printf(" SET_MIX_ACK");
            break;
        }

        default:
            printf(" FOCUSRITE%s[%s size=%d seq=0x%x error=%d pad=%d]",
                    (req) ? "_REQ" : "_RESP",
                    decode_focusrite_cmd(s_cmd, fpacket->cmd),
                    fpacket->size, fpacket->seq, fpacket->error, fpacket->pad
            );
            break;
    }

    // Update data pointer at exit
    *buf    = data;
}

void dump_packet(pcap_packet_t *packet, protocol_state_t *pstate, uint8_t *data)
{
    usb_header_t *hdr;
    isoch_header_t *iso;
    control_header_t *ctl;
    control_data_t *cdata;

    size_t len;
    char s_func[0x40], s_addr[0x40], s_stage[0x40], s_req[0x40];

    len                 = packet->incl_len;
    uint8_t *head       = data;
    hdr                 = reinterpret_cast<usb_header_t *>(data);
    data               += sizeof(usb_header_t);

    // Lazy initialization of time
    if ((pstate->init_sec == 0) && (pstate->init_usec == 0))
    {
        pstate->init_sec    = packet->ts_sec;
        pstate->init_usec   = packet->ts_usec;
    }

    // Subtract time
    if (packet->ts_usec < pstate->init_usec)
    {
        packet->ts_sec  = packet->ts_sec - pstate->init_sec - 1;
        packet->ts_usec = packet->ts_usec + 1000000U - pstate->init_usec;
    }
    else
    {
        packet->ts_sec  = packet->ts_sec - pstate->init_sec;
        packet->ts_usec = packet->ts_usec - pstate->init_usec;
    }


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
    decode_focusrite_packet(pstate, hdr, ctl, cdata, &data);
    update_protocol_state(pstate, hdr, ctl, cdata);

    len    -= (data - head);

    // Print as bytes
    for (size_t i=0; i<len; ++i)
        printf(" %02x", data[i]);

    // Print as character data
    printf(" \"");
    for (size_t i=0; i<len; ++i)
        putchar(((data[i] >= 0x20) && (data[i] < 0x7f)) ? data[i] : '.');
    printf("\"");

    printf("\n");
}

void dump_buffer(const uint8_t *buf, size_t bytes)
{
    printf("      00 01 02 03 | 04 05 06 07 | 08 09 0a 0b | 0c 0d 0e 0f   0123456789abcdef");
    for (size_t i=0; i<bytes; i += 16)
    {
        if (!(i&0xff))
            printf("\n      -----------------------------------------------------   ----------------");
        if (!(i&0x0f))
            printf("\n%04x:", int(i));

        // Dump hex codes
        for (size_t j=i; j<i+16; ++j)
        {
            if (((j & 0x0c) > 0) && (!(j & 0x03)))
                printf(" |");
            if (j < bytes)
                printf(" %02x", buf[j]);
            else
                printf("   ");
        }

        printf("   ");

        for (size_t j=i; j<i+16; ++j)
        {
            if (j < bytes)
                putchar(((buf[j] >= 0x20) && (buf[j] < 0x7f)) ? buf[j] : '.');
            else
                putchar(' ');
        }
    }
    printf("\n");
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "USAGE: decode <in-file> [<out-rom-dump>]\n");
        return -1;
    }

    FILE *fd = fopen(argv[1], "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Could not open for reading: %s\n", argv[1]);
        return -1;
    }

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
    uint8_t *data       = static_cast<uint8_t *>(malloc(pcap_hdr.snaplen + 0x100));

    protocol_state_t state;
    state.init_sec      = 0;
    state.init_usec     = 0;
    state.ctl_setup     = false;
    state.requestType   = -1;
    state.request       = -1;
    state.data_off      = -1;
    state.data_size     = 0;
    memset(state.data, 0, DEVICE_DATA_SIZE);

    // Parse file
    while ((len = read_packet(fd, &packet, data)) > 0)
    {
        dump_packet(&packet, &state, data);
    }
    
    off64_t pos = ftello64(fd);
    printf("Overall read bytes: %lld\n", (long long)pos);
    printf("Maximum data block address: 0x%x\n", int(state.data_size));
    printf("Data dump:\n\n");
    dump_buffer(state.data, (state.data_size > 0) ? state.data_size : DEVICE_DATA_SIZE);

    fclose(fd);

    // Save ROM data
    if (argc >= 3)
    {
        // Dump ROM data
        fd = fopen(argv[2], "w");
        if (fd == NULL)
        {
            fprintf(stderr, "Could not open for writing: %s\n", argv[2]);
            return -1;
        }

        fwrite(state.data, (state.data_size > 0) ? state.data_size : DEVICE_DATA_SIZE, 1, fd);
        fclose(fd);
    }
}

