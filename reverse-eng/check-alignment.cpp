#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#define __packed __attribute((packed))

#define SCARLETT2_ANALOGUE_IN_MAX          8    /* Maximum number of analogue inputs */
#define SCARLETT2_ANALOGUE_OUT_MAX         10   /* Maximum number of analogue outputs */
#define SCARLETT2_ALL_OUT_MAX              26   /* Maximum number of all outputs */
#define SCARLETT2_LEVEL_SWITCH_MAX         2    /* Maximum number of LINE/INST switches */
#define SCARLETT2_PAD_SWITCH_MAX           8    /* Maximum number of PAD switches */
#define SCARLETT2_AIR_SWITCH_MAX           8    /* Maximum number of AIR switches */
#define SCARLETT2_48V_SWITCH_MAX           2    /* Maximum number of 48V switches */
#define SCARLETT2_BUTTON_MAX               2    /* Hardware buttons on the 18i20 */
#define SCARLETT2_INPUT_MIX_MAX            24   /* Maximum number of inputs to the mixer */
#define SCARLETT2_OUTPUT_MIX_MAX           12   /* Maximum number of outputs from the mixer */
#define SCARLETT2_MUX_MAX                  77   /* The maximum possible MUX connection count */
#define SCARLETT2_NUM_METERS               56   /* Number of meters: 18 inputs, 20 outputs, 18 matrix inputs (XX FIXME) */
#define SCARLETT2_IN_NAME_LEN              12   /* Maximum length of the input name */
#define SCARLETT2_OUT_NAME_LEN             12   /* Maximum length of the output name */

#define SCARLETT2_SW_CONFIG_BASE                 0xec

#define SCARLETT2_SW_CONFIG_PACKET_SIZE          1024     /* The maximum packet size used to transfer data */

#define SCARLETT2_SW_CONFIG_MIXER_INPUTS         30       /* 30 inputs per one mixer in config */
#define SCARLETT2_SW_CONFIG_MIXER_OUTPUTS        12       /* 12 outputs in config */
#define SCARLETT2_SW_CONFIG_SIZE_OFFSET          0x08     /* 0xf4   - 0xec */
#define SCARLETT2_SW_CONFIG_STEREO_BITS_OFFSET   0x0c8    /* 0x1b4  - 0xec */
#define SCARLETT2_SW_CONFIG_VOLUMES_OFFSET       0x0d0    /* 0x1bc  - 0xec */
#define SCARLETT2_SW_CONFIG_MIXER_OFFSET         0xf04    /* 0xff0  - 0xec */

typedef uint8_t         u8;
typedef uint16_t        __le16;
typedef uint32_t        __le32;

struct scarlett2_sw_cfg_volume {
    __le16 volume;  /* volume */
    u8 changed;     /* change flag */
    u8 flags;       /* some flags? */
};

struct scarlett2_sw_cfg {
    __le16 all_size;          /* +0x0000: overall size, 0x1990 for all devices */
    __le16 magic1;            /* +0x0002: magic number: 0x3006 */
    __le32 version;           /* +0x0004: probably version */
    __le16 szof;              /* +0x0008: the overall size, 0x1984 for all devices  */
    __le16 __pad0;            /* +0x000a: ???????? */
    u8 __pad1[0x00bc];        /* +0x000c: ???????? */
    __le32 stereo_sw;         /* +0x01b4: stereo configuration for each port (bit mask) */
    __le32 mute_sw;           /* +0x01b8: mute switches (bit mask) */
    struct scarlett2_sw_cfg_volume volumes[SCARLETT2_ANALOGUE_OUT_MAX]; /* +0x01bc: Volume settings of each output */
    u8 __pad2[0x01dc];        /* +0x01e4: ???????? */
    u8 in_alias[SCARLETT2_ANALOGUE_IN_MAX][SCARLETT2_IN_NAME_LEN];      /* +0x03c0: Symbolic names of inputs */
    u8 __pad3[0x05d0];        /* +0x0420: ???????? */
    u8 out_alias[SCARLETT2_ALL_OUT_MAX][SCARLETT2_OUT_NAME_LEN];        /* +0x09f0: Symbolic names of outputs */
    u8 __pad4[0x05b4];        /* +0x0b28: ???????? */
    __le32 mixer[SCARLETT2_SW_CONFIG_MIXER_OUTPUTS][SCARLETT2_SW_CONFIG_MIXER_INPUTS]; /* +0x10dc: Matrix mixer settings */
    u8 __pad5[0x03f0];        /* +0x167c: ???????? */
    __le32 checksum;          /* +0x1a6c: checksum of the area */
} __packed;

// This is test for proper field alignment
int main(int argc, const char **argv)
{
    typedef struct scarlett2_sw_cfg cfg;

    #define OFFSET(field) \
        printf("%-10s  :  0x%04x / 0x%04x\n", #field, int(SCARLETT2_SW_CONFIG_BASE + offsetof(cfg, field)), int(offsetof(cfg, field)));

    OFFSET(all_size);
    OFFSET(magic1);
    OFFSET(version);
    OFFSET(szof);
    OFFSET(__pad0);
    OFFSET(__pad1);
    OFFSET(stereo_sw);
    OFFSET(mute_sw);
    OFFSET(volumes);
    OFFSET(__pad2);
    OFFSET(in_alias);
    OFFSET(__pad3);
    OFFSET(out_alias);
    OFFSET(__pad4);
    OFFSET(mixer);
    OFFSET(__pad5);
    OFFSET(checksum);
    printf("%-10s  :  0x%04x\n", "sizeof", int(sizeof(cfg)));

    return 0;
}
