#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t __le32;

/* Maximum number of analogue outputs */
#define SCARLETT2_ANALOGUE_MAX 10

/* Maximum number of level, pad, and air switches */
#define SCARLETT2_LEVEL_SWITCH_MAX 2
#define SCARLETT2_PAD_SWITCH_MAX 8
#define SCARLETT2_AIR_SWITCH_MAX 8
#define SCARLETT2_48V_SWITCH_MAX 2

/* Maximum number of inputs to the mixer */
#define SCARLETT2_INPUT_MIX_MAX 24

/* Maximum number of outputs from the mixer */
#define SCARLETT2_OUTPUT_MIX_MAX 12

/* Maximum size of the data in the USB mux assignment message:
 * 18 inputs, 2 loopbacks, 20 outputs, 24 mixer inputs, 13 spare
 */
#define SCARLETT2_MUX_MAX 77

/* Number of meters:
 * 18 inputs, 20 outputs, 18 matrix inputs (XX FIXME)
 */
#define SCARLETT2_NUM_METERS 56

/* Software config */
#define SCARLETT2_VOLUMES_BASE                   0x34
#define SCARLETT2_SW_CONFIG_BASE                 0xec

#define SCARLETT2_SW_CONFIG_PACKET_SIZE          1024     /* The maximum packet size used to transfer data */
#define SCARLETT2_SW_CONFIG_MIXER_INPUTS         30       /* 30 inputs per one mixer in config */

#define SCARLETT2_SW_CONFIG_SIZE_OFFSET          0x08     /* 0xf4   - 0xec */
#define SCARLETT2_SW_CONFIG_STEREO_BITS_OFFSET   0x0c8    /* 0x1b4  - 0xec */
#define SCARLETT2_SW_CONFIG_VOLUMES_OFFSET       0x0d0    /* 0x1bc  - 0xec */
#define SCARLETT2_SW_CONFIG_MIXER_OFFSET         0xf04    /* 0xff0  - 0xec */

enum {
    SCARLETT2_PORT_IN = 0,
    SCARLETT2_PORT_OUT = 1,
    SCARLETT2_PORT_OUT_44 = 2,
    SCARLETT2_PORT_OUT_88 = 3,
    SCARLETT2_PORT_OUT_176 = 4,
    SCARLETT2_PORT_DIRECTIONS = 5,
};

enum {
    SCARLETT2_PORT_TYPE_NONE = 0,
    SCARLETT2_PORT_TYPE_ANALOGUE = 1,
    SCARLETT2_PORT_TYPE_SPDIF = 2,
    SCARLETT2_PORT_TYPE_ADAT = 3,
    SCARLETT2_PORT_TYPE_MIX = 4,
    SCARLETT2_PORT_TYPE_PCM = 5,
    SCARLETT2_PORT_TYPE_COUNT = 6,
};

struct scarlett2_ports {
    u16 id;
    int num[SCARLETT2_PORT_DIRECTIONS];
    const char * const src_descr;
    int src_num_offset;
    const char * const dst_descr;
};

struct scarlett2_device_info {
    u8 line_out_hw_vol; /* line out hw volume is sw controlled */
    u8 button_count; /* number of buttons */
    u8 level_input_count; /* inputs with level selectable */
    u8 pad_input_count; /* inputs with pad selectable */
    u8 air_input_count; /* inputs with air selectable */
    u8 power_48v_count; /* 48V phantom power */
    u8 has_msd_mode; /* Gen 3 devices have an internal MSD mode switch */
    u8 has_speaker_switching; /* main/alt speaker switching */
    u8 has_talkback; /* 18i20 Gen 3 has 'talkback' feature */
    const char * const line_out_descrs[SCARLETT2_ANALOGUE_MAX];
    struct scarlett2_ports ports[SCARLETT2_PORT_TYPE_COUNT];
};

struct scarlett2_sw_cfg_volume {
    __le16 volume;  /* volume */
    u8 changed;     /* change flag */
    u8 flags;       /* some flags? */
};

struct scarlett2_mixer_data {
    struct usb_mixer_interface *mixer;
    struct mutex usb_mutex; /* prevent sending concurrent USB requests */
    struct mutex data_mutex; /* lock access to this data */
    struct delayed_work work;
    const struct scarlett2_device_info *info;
    __u8 interface; /* vendor-specific interface number */
    __u8 endpoint; /* interrupt endpoint address */
    __le16 maxpacketsize;
    __u8 interval;
    int num_mux_srcs;
    u16 scarlett2_seq;
    u8 vol_updated; /* Flag that indicates that volume has been updated */
    u8 line_ctl_updated; /* Flag that indicates that state of PAD, INST buttons have been updated */
    u8 speaker_updated; /* Flag that indicates that speaker/talkback has been updated */
    u8 master_vol;
    u8 vol[SCARLETT2_ANALOGUE_MAX];
    u8 vol_sw_hw_switch[SCARLETT2_ANALOGUE_MAX];
    u8 level_switch[SCARLETT2_LEVEL_SWITCH_MAX];
    u8 pad_switch[SCARLETT2_PAD_SWITCH_MAX];
    u8 air_switch[SCARLETT2_AIR_SWITCH_MAX];
    u8 pow_switch[SCARLETT2_48V_SWITCH_MAX];
    u8 msd_switch;
    u8 speaker_switch;
    u8 talkback_switch;
    u8 buttons[SCARLETT2_BUTTON_MAX];
    struct snd_kcontrol *master_vol_ctl;
    struct snd_kcontrol *speaker_ctl;
    struct snd_kcontrol *talkback_ctl;
    struct snd_kcontrol *vol_ctls[SCARLETT2_ANALOGUE_MAX];
    struct snd_kcontrol *pad_ctls[SCARLETT2_PAD_SWITCH_MAX];
    struct snd_kcontrol *level_ctls[SCARLETT2_LEVEL_SWITCH_MAX];
    struct snd_kcontrol *pow_ctls[SCARLETT2_48V_SWITCH_MAX];
    struct snd_kcontrol *button_ctls[SCARLETT2_BUTTON_MAX];
    struct snd_kcontrol *mix_talkback_ctls[SCARLETT2_OUTPUT_MIX_MAX]; /* Talkback controls for each mix */
    u8 mux[SCARLETT2_MUX_MAX];
    u8 mix[SCARLETT2_INPUT_MIX_MAX * SCARLETT2_OUTPUT_MIX_MAX];       /* Matrix mixer */
    u8 mix_talkback[SCARLETT2_OUTPUT_MIX_MAX];                        /* Talkback enable for mixer output */

    /* Software configuration */
    int sw_cfg_offset;                                                /* Software configuration offset */
    int sw_cfg_size;                                                  /* Software configuration size   */
    u8 *sw_cfg_data;                                                  /* Software configuration (data) */
    __le32 *sw_cfg_stereo;                                            /* Bitmask containing flags that output is in STEREO mode */
    struct scarlett2_sw_cfg_volume *sw_cfg_volume;                    /* Output volumes of the device, f32le */
    __le32 *sw_cfg_mixer;                                             /* Mixer data stored in the software config, matrix of f32le values */
    int sw_cfg_mixer_size;                                            /* Size of software mixer area */
    __le32 *sw_cfg_cksum;                                             /* Software configuration checksum */
};

    .ports = {
        [SCARLETT2_PORT_TYPE_NONE] = {
            .id = 0x000,
            .num = { 1, 0, 13, 11, 0 },
            .src_descr = "Off",
            .src_num_offset = 0,
        },
        [SCARLETT2_PORT_TYPE_ANALOGUE] = {
            .id = 0x080,
            .num = { 8, 10, 10, 10, 10 },
            .src_descr = "Analogue %d",
            .src_num_offset = 1,
            .dst_descr = "Analogue Output %02d Playback"
        },
        [SCARLETT2_PORT_TYPE_SPDIF] = {
            .id = 0x180,
            .num = { 2, 2, 2, 2, 2 },
            .src_descr = "S/PDIF %d",
            .src_num_offset = 1,
            .dst_descr = "S/PDIF Output %d Playback"
        },
        [SCARLETT2_PORT_TYPE_ADAT] = {
            .id = 0x200,
            .num = { 8, 8, 8, 8, 0 },
            .src_descr = "ADAT %d",
            .src_num_offset = 1,
            .dst_descr = "ADAT Output %d Playback"
        },
        [SCARLETT2_PORT_TYPE_MIX] = {
            .id = 0x300,
            .num = { 12, 24, 24, 24, 24 },
            .src_descr = "Mix %c",
            .src_num_offset = 65,
            .dst_descr = "Mixer Input %02d Capture"
        },
        [SCARLETT2_PORT_TYPE_PCM] = {
            .id = 0x600,
            .num = { 20, 20, 20, 18, 10 },
            .src_descr = "PCM %d",
            .src_num_offset = 1,
            .dst_descr = "PCM %02d Capture"
        },
    }

inline void test()
{
    struct scarlett2_mixer_data *private = mixer->private_data;
    const struct scarlett2_ports *ports = private->info->ports;
    int num_line_out = ports[SCARLETT2_PORT_TYPE_ANALOGUE].num[SCARLETT2_PORT_OUT];
}

