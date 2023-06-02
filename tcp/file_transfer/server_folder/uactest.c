#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rts_io_gpio.h"
#include "librt_uac_rw.h" // UAC xmem r/w library by USB vendor command

#define USB_DEV_ID "Audio"

#define RTS3901_FLASH_ID 5 // valid range from 1~15

#define ERR_SET_CAPT_VOL (1 << 0)
#define ERR_SET_PLAY_VOL (1 << 1)
#define ERR_RUN_LOOPBACK (1 << 2)

#define GPIO_OUTPUT 1
#define GPIO_INPUT 0

#define GPIO_HIGH 1
#define GPIO_LOW 0

#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08

// RTS3901 IO register control
#define UART2TXD_IO_0_bit 4
#define UART0CTS_IO_1_bit 10
#define UART0RTS_IO_2_bit 11
#define UART0RXD_IO_3_bit 9
#define UART0TXD_IO_4_bit 8

#define UART2TXD_IO_0 0x0010
#define UART0CTS_IO_1 0x0400
#define UART0RTS_IO_2 0x0800
#define UART0RXD_IO_3 0x0200
#define UART0TXD_IO_4 0x0100
#define BI_DIR_IO_MASK (UART0CTS_IO_1 | UART0RTS_IO_2 | UART0RXD_IO_3 | UART0TXD_IO_4)

// FT test item signal
// #define ENUM_SIGNAL                      (UART2TXD_IO_0|UART0CTS_IO_1)
// #define REG_SETTINGS_SIGNAL              (UART2TXD_IO_0|UART0RTS_IO_2)
// #define REG_SETTINGS2_SIGNAL             (UART2TXD_IO_0|UART0CTS_IO_1|UART0RTS_IO_2)
// #define OTP_CHECK_SIGNAL                 (UART2TXD_IO_0|UART0RXD_IO_3)
// #define HP_LOOPBACK_SIGNAL               (UART2TXD_IO_0|UART0CTS_IO_1|UART0RXD_IO_3)
// #define HP_LOOPBACK_SIGNAL2              (UART2TXD_IO_0|UART0RTS_IO_2|UART0RXD_IO_3)
// #define CHECK_RTS3901_FLASH_SIGNAL       (UART2TXD_IO_0|UART0CTS_IO_1|UART0RTS_IO_2|UART0RXD_IO_3)
// #define REMOTE_WAKEUP_TEST_SIGNAL        (UART2TXD_IO_0|UART0TXD_IO_4)
// #define UPDATE_OTP_SIGNAL                (UART2TXD_IO_0|UART0CTS_IO_1|UART0TXD_IO_4)

// #define READ_CORRELATION_ID_PID_SIGNAL   (UART2TXD_IO_0|UART0RTS_IO_2|UART0TXD_IO_4)
// #define WRITE_CORRELATION_ID_02_SIGNAL   (UART2TXD_IO_0|UART0CTS_IO_1|UART0RTS_IO_2|UART0TXD_IO_4)
// #define WRITE_CORRELATION_ID_01_SIGNAL   (UART2TXD_IO_0|UART0RXD_IO_3|UART0TXD_IO_4)
// #define UPDATE_OTP_4108_SIGNAL           (UART2TXD_IO_0|UART0CTS_IO_1|UART0RXD_IO_3|UART0TXD_IO_4)

// ACK IO signal
// #define ENUM_ACK_IO                      (UART0RTS_IO_2|UART0RXD_IO_3|UART0TXD_IO_4)
// #define REG_SETTINGS_ACK_IO              (UART0RXD_IO_3|UART0TXD_IO_4)
// #define REG_SETTINGS2_ACK_IO             (UART0CTS_IO_1|UART0RXD_IO_3|UART0TXD_IO_4)
// #define OTP_CHECK_ACK_IO                 (UART0CTS_IO_1|UART0RTS_IO_2|UART0TXD_IO_4)
// #define HP_LOOPBACK_ACK_IO               (UART0RTS_IO_2|UART0TXD_IO_4)
// #define HP_LOOPBACK_SIGNAL2_ACK_IO       (UART0CTS_IO_1|UART0TXD_IO_4)
// #define CHECK_RTS3901_FLASH_ACK_IO       (UART0TXD_IO_4)
// #define REMOTE_WAKEUP_TEST_ACK_IO        (UART0CTS_IO_1|UART0RTS_IO_2|UART0RXD_IO_3)
// #define UPDATE_OTP_ACK_IO                (UART0RTS_IO_2|UART0RXD_IO_3)



/* -------------------------------------------- DUT Register Definition ----------------------------------------------------- */
#define CBJDET_EN_RST_MD_CFG_CTL            ( 0x2B01 )
#define CBJDET_TRIGGERSOR_DETTYPE           ( 0x2B02 )
#define CBJDET_POWPIN_VREFO_HPAMPGND_SEL    ( 0x2B03 )
#define CBJDET_CFG_CTL2                     ( 0x2B04 )
#define CBJDET_CFG_CTL3                     ( 0x2B05 )
#define CBJDET_CFG_CTL5                     ( 0x2B07 )
#define CBJDET_POW_MICBIAS_MBIAS_CTL        ( 0x2B11 )

#define GPIO25_TESTOUT                      ( 0x0141 )
#define GPIO26_TESTOUT                      ( 0x0142 )
#define GPIO32_TESTOUT                      ( 0x0143 )
#define GPIO33_TESTOUT                      ( 0x0144 )

#define POW_MICDET1B_MICDETMB_MICDETVTH_CTL ( 0x0348 )
#define ENBTN4_BTN4VREF_CBJGAIN_CTL         ( 0x034B )

#define SARADC_DBCLK_PSVDBCLK_CLK_SEL       ( 0x3311 )

#define GPIO_TYPE_IO24_IO25_SEL             ( 0x050C )
#define GPIO_TYPE_IO26_IO27_SEL             ( 0x050D )
#define GPIO_TYPE_IO32_IO33_SEL             ( 0x0510 )
#define GPIO_DIR_IO24_IO31_CTL              ( 0x0518 )
#define GPIO_DIR_IO32_IO39_CTL              ( 0x0519 )

#define HP_SIDET_SOR_SEL                    ( 0x011D )
/* --------------------------------------------------------------------------------------------------------------------------- */



/* --------------------------------------------- DUT Register Field Definition ----------------------------------------------- */

//===============================================================================================================================
//
// CBJDET_EN_RST_MD_CFG_CTL  0x2B01 : MX0010 Combo Jack and Type Detection Control 2
//
//===============================================================================================================================
#define REG_MODE ( 0x20 )

//===============================================================================================================================
//
// CBJDET_TRIGGERSOR_DETTYPE  0x2B02 : MX0011 Combo Jack and Type Detection Control 2
//
//===============================================================================================================================
#define JACK_TYPE_MASK  ( 0x0F )
#define IPHONE_TYPE     ( 0x01 )

//===============================================================================================================================
//
// CBJDET_POWPIN_VREFO_HPAMPGND_SEL  0x2B03
//
//===============================================================================================================================
#define EN_CBJ_TIE_GL_GR_DIGITAL ( 0x18 )

//===============================================================================================================================
//
// CBJDET_CFG_CTL2   0x2B04 : Combo Jack and Type Detection Control 6
//
//===============================================================================================================================
#define MICSEL_LR_BST1_DIGITAL ( 0x30 )

//===============================================================================================================================
//
// CBJDET_CFG_CTL3   0x2B05
//
//===============================================================================================================================
#define MIC_INPUT_WITH_CBJ_DET ( 0x40 )

//===============================================================================================================================
//
// CBJDET_CFG_CTL5   0x2B07
//
//===============================================================================================================================
#define R2_SLEEVE_CPVREF_MASK   ( 0x0E )
#define EN_CPVREF               ( 0x02 )

//===============================================================================================================================
//
// GPIO25_TESTOUT   0x0141
//
//===============================================================================================================================
#define SAR1_CMPO_DIGITAL_0 ( 0x4B )
#define JD_CMPO_12_2        ( 0x0C )

//===============================================================================================================================
//
// GPIO26_TESTOUT   0x0142
//
//===============================================================================================================================
#define SAR1_CMPO_DIGITAL_1 ( 0x4B )
#define JD_CMPO_12_1        ( 0x0C )

//===============================================================================================================================
//
// GPIO32_TESTOUT   0x0143
//
//===============================================================================================================================
#define SAR1_CMPO_DIGITAL_2 ( 0x4B )
#define JD_CMPO_12_0        ( 0x0C )

//===============================================================================================================================
//
// GPIO33_TESTOUT   0x0144
//
//===============================================================================================================================
#define SAR1_CMPO_DIGITAL_3     ( 0x4B )
#define CMP0_MIC_IN_DET_1BIT    ( 0x15 )

//===============================================================================================================================
//
// ENBTN4_BTN4VREF_CBJGAIN_CTL  (register, 0x034B)
//
//===============================================================================================================================
#define CBJ_LR_BOOST_MASK ( 0x0F )

/* --------------------------------------------------------------------------------------------------------------------------- */

enum COMMAND_SIGNAL
{
    FIRST_COMMAND_SIGNAL = 1,

    ENUM_CHECK_SIGNAL = FIRST_COMMAND_SIGNAL,   // start1
    CBJ_IPHONE_CHECK_SIGNAL,                    // start2
    GBTN_COMPARATOR_CHECK_SIGNAL,               // start3
    MIC_DET_1BIT_CMP_CHECK_SIGNAL,              // start4
    TEST_LPBK_HP_LINE1_AMIC_SIGNAL,             // start5
    SETUP_CBJ_NORMAL_LINEIN_SIGNAL,             // start6
    MEASURE_MICBIAS_123_SIGNAL,                 // start7
    JD_PLUG_CHECK_SIGNAL,                       // start8
    HP_CALIB_SIGNAL,                            // start9

    /* 
    TEST_LPBK_HP_NOKIA_AMIC_LINE1_SIGNAL,
    TEST_LPBK_HP_IPHONE_AMIC_LINE2_SIGNAL,
    OTP_CHECK_SIGNAL,
    CHECK_3901_FLASHID_SIGNAL,
    REMOTE_WAKEUP_SIGNAL,
    RESERVE_3_SIGNAL,
    CHECK_PID_4030_SIGNAL,
    */

    CHECK_3901_FLASHID_SIGNAL,
    LAST_COMMAND_SIGNAL,
};

enum REMOTE_CONTROL_SIGNAL
{
    REGISTER_RW_SIGNAL,
    FILE_TRANSMISSION_SIGNAL,
    COMMANDLINE_SIGNAL,
};

#define READ_CORRELATION_ID_PID_ACK_IO (UART0CTS_IO_1 | UART0RXD_IO_3)
#define WRITE_CORRELATION_ID_02_ACK_IO (UART0RXD_IO_3)
#define WRITE_CORRELATION_ID_01_ACK_IO (UART0CTS_IO_1 | UART0RTS_IO_2)
#define UPDATE_OTP_4108_ACK_IO (UART0RTS_IO_2)

#define GPIO_PULL_MASK 0x00FF0300
#define GPIO_PULL_DOWN_MASK 0x00550100

// RST3901 ERROR
#define COPY_DOWN_CODE_TOOL_ERR 1
#define COPY_RFW_ERR            2
#define CHMOD_755_DOWN_CODE_ERR 3
#define COPY_OTP_CHECK_ERR      4
#define CHMOD_755_OTP_CHECK_ERR 5

#define DEBUG3901 0

#define PASS    0
#define FAIL    1
#define TRUE    1
#define FALSE   0

// #define DEMO_CODE_CHECKSUM 0x4CA5
// #define UPDATE_FW_CHECKSUM 0x6b32
// #define AUDIO_TEST_FW_CHECKSUM 0xdd25

// global variable
struct rts_gpio *rts_UART2TXD_IO_0;
struct rts_gpio *rts_UART0CTS_IO_1;
struct rts_gpio *rts_UART0RTS_IO_2;
struct rts_gpio *rts_UART0RXD_IO_3;
struct rts_gpio *rts_UART0TXD_IO_4;
struct rts_gpio *rts_gpio7;

char start9UpdateRfwPath[] = "/tmp/ALC4040_OTP.rfw";
char start13UpdateRfwPath[] = "/tmp/ALC4108_OTP.rfw";

char cmdCpDownCodeTool[] = "cp /media/sdcard/rt_UAC_utility /tmp/";
char cmdCpLoopbackTool[] = "cp -r /media/sdcard/audio_loopback /tmp/";

char cmdCpRfw[] = "cp /media/sdcard/ALC4042_FW.rfw /tmp/";
char cmdCpStart7Rfw[] = "cp /media/sdcard/0x17EF_ALC4104_LV_0x306F_V12_20171212.rfw /tmp/";
char cmdCpOtpRfw[] = "cp /media/sdcard/ALC4030_OTP.rfw /tmp/";

char cmdCpOtpCheckTool[] = "cp /media/sdcard/rt_UAC_FT /tmp/";
char cmdCpRegWriteTool[] = "cp /media/sdcard/rt_UAC_rw /tmp/";
char cmdCpRegWriteOnceTool[] = "cp /media/sdcard/rt_UAC_rw_once /tmp/";
char cmdCpFtTable[] = "cp /media/sdcard/FT_table.txt /tmp/";
char cmdCpRegSettingsWriteIni[] = "cp /media/sdcard/reg_settings.ini /tmp/";
char cmdCpRegSettings2WriteIni[] = "cp /media/sdcard/reg_settings2.ini /tmp/";
char cmdCpRegWriteIniTool[] = "cp /media/sdcard/rt_UAC_rw_ini /tmp/";

char cmdChmod755DownCode[] = "chmod 755 /tmp/rt_UAC_utility";
char cmdChmod755OtpCheck[] = "chmod 755 /tmp/rt_UAC_FT";
char cmdChmod755RegWrite[] = "chmod 755 /tmp/rt_UAC_rw";
char cmdChmod755RegWriteOnce[] = "chmod 755 /tmp/rt_UAC_rw_once";
char cmdChmod755Loopback[] = "chmod 755 /tmp/audio_loopback/rt_UAC_loopback";
char cmdChmod755RegWriteIni[] = "chmod 755 /tmp/rt_UAC_rw_ini";

char cmdUpdate[] = "/tmp/rt_UAC_utility --init_download /tmp/ALC4050_FW.rfw";
char cmdUpdateOtp[] = "/tmp/rt_UAC_utility --download_fw /tmp/ALC4040_OTP.rfw";
char cmdUpdateOtp4108[] = "/tmp/rt_UAC_utility --download_fw /tmp/ALC4108_OTP.rfw";

char cmdCpOnBoardRfw[] = "cp /media/sdcard/ALC4042_ONBOARD_FW.rfw /tmp/";
char cmdOnBoardRst2UpdateFw[] = "/tmp/rt_UAC_utility --reset RST_2_UPDATEFW";
char cmdOnBoardUpdate[] = "/tmp/rt_UAC_utility --force /tmp/ALC4040_ONBOARD_FW.rfw";
char cmdOnBoardRst2Attach[] = "/tmp/rt_UAC_utility --reset RST_2_ATTACH";

char cmdOtpCheck[] = "/tmp/rt_UAC_FT --otpcheck --use_class_cmd";
char cmdRegSettingsToCache[] = "/tmp/rt_UAC_rw_ini --cache_ini  /tmp/reg_settings.ini";
char cmdRegSettings2ToCache[] = "/tmp/rt_UAC_rw_ini --cache_ini  /tmp/reg_settings2.ini";

char cmdRegSettingsWrite[] = "/tmp/rt_UAC_rw_ini --load_ini_cache /tmp/reg_settings.ini.dat";
char cmdRegSettings2Write[] = "/tmp/rt_UAC_rw_ini --load_ini_cache /tmp/reg_settings2.ini.dat";

char cmdAutoSuspend[] = "echo auto > /sys/devices/platform/ehci-platform/usb1/1-1/power/control";
char cmdAutoSuspendDelay[] = "echo 500 > /sys/devices/platform/ehci-platform/usb1/1-1/power/autosuspend_delay_ms";

int rts3901FailFlag = 0;

unsigned short start3_rfw_checksum = 0;
unsigned short start9_rfw_checksum = 0;
unsigned short start13_rfw_checksum = 0;

#define PORT_DEBUG      3001
#define PORT_CONTROL    3002
#define BUF_SIZE        4
#define BUF_SIZE2       128

int tcp_debug_idx = 0;
char tcp_debug_result = 0;

int dev_idx = 1; // USB Device Index, Updated at USB Enumerate check

void rts3901Fail(int errType)
{
    int ret, toggle = 0;

    ret = rts_io_gpio_set_direction(rts_gpio7, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n rts_gpio7 set output fail uactest \n");
    }

    while (1)
    {
        printf("\n RTS3901 ERROR uactest \n");
        if (toggle == 0)
        {
            ret = rts_io_gpio_set_value(rts_gpio7, GPIO_LOW);
            toggle = 1;
        }
        else
        {
            ret = rts_io_gpio_set_value(rts_gpio7, GPIO_HIGH);
            toggle = 0;
        }
        usleep(10000000);
    }
}

// rtn 0:USB device not found
//     1:USB device found
int test_dev_id(int idx)
{
    FILE *fp;
    char name[256];
    int rtn = 0;

    name[0] = 0;
    sprintf(name, "/proc/asound/card%d/id", idx);

    fp = fopen(name, "r");
    if (fp == NULL)
        goto finish;

    name[0] = 0;
    fscanf(fp, "%s\n", name);
    if (0 != strcmp(name, USB_DEV_ID))
        goto finish;

    rtn = 1;

finish:
    if (fp != NULL)
        fclose(fp);
    return rtn;
}

void uDelay(int w100us)
{
    int i;
    while (w100us)
    {
        i = 12110;
        while (i)
        {
            i--;
        }
        w100us--;
    }
}

// After receiving SIGINT or SIGTERM signal, execute sig_handler function
void sig_handler(int signo)
{
    if (signo == SIGINT)
        printf("received SIGINT\n");
    if (signo == SIGTERM)
        printf("received SIGTERM\n");
    
    printf("Exit uactest program\n");
    RtUac_CloseUSBDevice();
    rts_io_gpio_free(rts_UART2TXD_IO_0);
    rts_io_gpio_free(rts_UART0CTS_IO_1);
    rts_io_gpio_free(rts_UART0RTS_IO_2);
    rts_io_gpio_free(rts_UART0RXD_IO_3);
    rts_io_gpio_free(rts_UART0TXD_IO_4);
    rts_io_gpio_free(rts_gpio7);
    exit(0);
}

void initSig(void)
{
    printf("\n in initSig uactest \n");
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");
    if (signal(SIGTERM, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGTERM\n");
}

unsigned char readmem(uint16_t address)
{
    int err = 0;
    unsigned char r_data[1] = {0};
    
    if (err = RtUac_ReadMem(address, r_data, 1))
    {
        printf("\n Read Mem %x failed: %d \n", address, err);
        // RtUac_CloseUSBDevice();
        return -ENODEV;
    }
    else
        printf("0x%X	=>	0x%X\n", address, r_data[0]);

    return r_data[0];
}


int writemem(uint16_t address, unsigned char data)
{
    int err = 0;
    unsigned char val[1] = {0};
    
    val[0] = data;
    if(err = RtUac_WriteMem(address, val, 1))
    {
        printf("\n Write Mem %x failed: %d \n", address, err);
        // RtUac_CloseUSBDevice();
        return FAIL;
    }
    else
        printf("0x%X	<=	0x%X\n", address, val[0]);
    
    return PASS;
}


// Meaning: writememAND(address, data) --> XBYTE[address] &= data
int writememAND(uint16_t address, unsigned char data)
{
    unsigned char value = 0x00;

    value = readmem(address);

    return writemem(address, (value&data));
}

// Meaning: writememOR(address, data) --> XBYTE[address] |= data
int writememOR(uint16_t address, unsigned char data)
{
    unsigned char value = 0x00;

    value = readmem(address);

    return writemem(address, (value|data));
}

typedef struct
{
    char dirName[16];
    float amp;
    float thdn;
    float snr;
    float balance;
    float pb_volume;
    uint16_t fft_size;
    uint16_t debug;
    uint16_t log;
    char checkCmd[256];
} wav_test_profile_t;

typedef struct
{
    char aplayId[16];
    char wavFilePath[128];
    char format[16];
    char channel[16];
    char sampleRate[16];
    char duration[16];
    char *cmdArgv[13];
} PlaybackProfile_t;

typedef struct
{
    char arecordId[16];
    char wavFilePath[128];
    char format[16];
    char channel[16];
    char sampleRate[16];
    char duration[16];
    char *cmdArgv[13];
    wav_test_profile_t testProfile;
} RecordingProfile_t;

typedef struct
{
    PlaybackProfile_t *p_playbackProfile;
    RecordingProfile_t *p_recordingProfile1; // Recording if not empty
    RecordingProfile_t *p_recordingProfile2; // Recording if not empty
    RecordingProfile_t *p_recordingProfile3; // Recording if not empty
    RecordingProfile_t *p_recordingProfile4; // Recording if not empty
} LpbkTestProfile_t;

PlaybackProfile_t g_hp_playback_profile;
RecordingProfile_t g_normal_amic_record_profile; // Wen-Yuan Design for ALC4052 start5
RecordingProfile_t g_line1_record_profile;
RecordingProfile_t g_line2_record_profile;
RecordingProfile_t g_iphone_amic_record_profile;
RecordingProfile_t g_nokia_amic_record_profile;

LpbkTestProfile_t g_hp2iphone_lpbk_profile;
LpbkTestProfile_t g_hp2nokia_lpbk_profile;
LpbkTestProfile_t g_hp2line1_lpbk_profile;

LpbkTestProfile_t g_hp2normalLineIn_lpbk_profile; // Wen-Yuan Design for ALC4052 start5
LpbkTestProfile_t g_hp2nokiaLine1_lpbk_profile;
LpbkTestProfile_t g_hp2iphoneLine2_lpbk_profile;

void initLpbkProfile(void)
{
    // Setup playback&recording profile
    sprintf(g_hp_playback_profile.aplayId, "hw:%d,1", dev_idx);
    strcpy(g_hp_playback_profile.wavFilePath, "/tmp/audio_loopback/sin1k_48k_24b_2ch.wav");
    strcpy(g_hp_playback_profile.format, "S24_3LE");
    strcpy(g_hp_playback_profile.channel, "2");
    strcpy(g_hp_playback_profile.sampleRate, "48000");
    strcpy(g_hp_playback_profile.duration, "1");

    sprintf(g_line2_record_profile.arecordId, "hw:%d,0", dev_idx);
    strcpy(g_line2_record_profile.wavFilePath, "/tmp/audio_loopback/hp_2_line2_result/lpbk_sin1k_48k_24b_2ch.wav");
    strcpy(g_line2_record_profile.format, "S24_3LE");
    strcpy(g_line2_record_profile.channel, "2");
    strcpy(g_line2_record_profile.sampleRate, "48000");
    strcpy(g_line2_record_profile.duration, "1");

    sprintf(g_line1_record_profile.arecordId, "hw:%d,1", dev_idx);
    strcpy(g_line1_record_profile.wavFilePath, "/tmp/audio_loopback/hp_2_line1_result/lpbk_sin1k_48k_24b_2ch.wav");
    strcpy(g_line1_record_profile.format, "S24_3LE");
    strcpy(g_line1_record_profile.channel, "2");
    strcpy(g_line1_record_profile.sampleRate, "48000");
    strcpy(g_line1_record_profile.duration, "1");

    // Wen-Yuan Design for ALC4052 start5
    sprintf(g_normal_amic_record_profile.arecordId, "hw:%d,0", dev_idx);
    strcpy(g_normal_amic_record_profile.wavFilePath, "/tmp/audio_loopback/hp_2_normal_linein_result/lpbk_sin1k_48k_24b_2ch.wav");
    strcpy(g_normal_amic_record_profile.format, "S24_3LE");
    strcpy(g_normal_amic_record_profile.channel, "2");
    strcpy(g_normal_amic_record_profile.sampleRate, "48000");
    strcpy(g_normal_amic_record_profile.duration, "1");

    sprintf(g_iphone_amic_record_profile.arecordId, "hw:%d,0", dev_idx);
    strcpy(g_iphone_amic_record_profile.wavFilePath, "/tmp/audio_loopback/hp_2_iphone_mic_result/lpbk_sin1k_48k_24b_2ch.wav");
    strcpy(g_iphone_amic_record_profile.format, "S24_3LE");
    strcpy(g_iphone_amic_record_profile.channel, "2");
    strcpy(g_iphone_amic_record_profile.sampleRate, "48000");
    strcpy(g_iphone_amic_record_profile.duration, "1");

    sprintf(g_nokia_amic_record_profile.arecordId, "hw:%d,0", dev_idx);
    strcpy(g_nokia_amic_record_profile.wavFilePath, "/tmp/audio_loopback/hp_2_nokia_mic_result/lpbk_sin1k_48k_24b_2ch.wav");
    strcpy(g_nokia_amic_record_profile.format, "S24_3LE");
    strcpy(g_nokia_amic_record_profile.channel, "2");
    strcpy(g_nokia_amic_record_profile.sampleRate, "48000");
    strcpy(g_nokia_amic_record_profile.duration, "1");

    // Setup loopback test profile
    memset(&g_hp2line1_lpbk_profile, 0, sizeof(g_hp2line1_lpbk_profile));
    memset(&g_hp2iphone_lpbk_profile, 0, sizeof(g_hp2iphone_lpbk_profile));
    memset(&g_hp2nokia_lpbk_profile, 0, sizeof(g_hp2nokia_lpbk_profile));
    memset(&g_hp2normalLineIn_lpbk_profile, 0, sizeof(g_hp2normalLineIn_lpbk_profile)); // Wen-Yuan Design for ALC4052 start5
    memset(&g_hp2nokiaLine1_lpbk_profile, 0, sizeof(g_hp2nokiaLine1_lpbk_profile));
    memset(&g_hp2iphoneLine2_lpbk_profile, 0, sizeof(g_hp2iphoneLine2_lpbk_profile));

    g_hp2line1_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2line1_lpbk_profile.p_recordingProfile1 = &g_line1_record_profile;

    g_hp2iphone_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2iphone_lpbk_profile.p_recordingProfile1 = &g_iphone_amic_record_profile;

    g_hp2nokia_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2nokia_lpbk_profile.p_recordingProfile1 = &g_nokia_amic_record_profile;

    // Wen-Yuan Design for ALC4052 start5
    g_hp2normalLineIn_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2normalLineIn_lpbk_profile.p_recordingProfile1 = &g_normal_amic_record_profile;
    g_hp2normalLineIn_lpbk_profile.p_recordingProfile2 = &g_line1_record_profile;

    g_hp2nokiaLine1_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2nokiaLine1_lpbk_profile.p_recordingProfile1 = &g_nokia_amic_record_profile;
    g_hp2nokiaLine1_lpbk_profile.p_recordingProfile2 = &g_line1_record_profile;

    g_hp2iphoneLine2_lpbk_profile.p_playbackProfile = &g_hp_playback_profile;
    g_hp2iphoneLine2_lpbk_profile.p_recordingProfile1 = &g_iphone_amic_record_profile;
    g_hp2iphoneLine2_lpbk_profile.p_recordingProfile2 = &g_line2_record_profile;
}

// Replace a specific substring in a full string with a specific substring
char *strrep(char *str, const char *substr, const char *replacement) {
    static char buffer[128];
    char *p = str;
    int substr_len = strlen(substr);
    int replacement_len = strlen(replacement);
    int buffer_len = 0;

    while (*p) {
        if (strncmp(p, substr, substr_len) == 0) {
            strncpy(buffer + buffer_len, replacement, replacement_len);
            p += substr_len;
            buffer_len += replacement_len;
        } else {
            buffer[buffer_len++] = *p++;
        }
    }

    buffer[buffer_len] = '\0';
    return buffer;
}

// for example: /media/sdcard/hp_2_normal_linein_profile.txt (profile path) --> hp_2_normal_linein_result (folder name)
char *getWavFolderName(char *path)
{
    char buf[128] = {0};
    char *token = NULL;
    char *profileName = NULL;
    char *wavFolderName = NULL;

    strcpy(buf, path);
    token = strtok(buf, "/");
    do {
        profileName = token; // Last one is the profile name
        token = strtok(NULL, "/");
    } while (token);
    wavFolderName = strrep(profileName, "profile.txt", "result");
    
    return wavFolderName;
}

void loadWavTestProfile(char *path, wav_test_profile_t *profile_p)
{
    FILE *fp;
    char textbuf[256];
    char tokenbuf[16];
    char *wavFolderName = NULL;
    float value;
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("\n %s profile not found \n", path);
    }

    profile_p->log = 0; // Default disable log

    // char * line = strtok(strdup(textbuf), "\n");
    // while(line)
    while (fgets(textbuf, sizeof(textbuf), fp))
    {
        // printf("%s", textbuf);
        // sscanf(textbuf, "%s:%d", tokenbuf, &value);
        char *line = strtok(strdup(textbuf), ":\n");
        sscanf(line, "%s", tokenbuf);
        line = strtok(NULL, ":\n");
        sscanf(line, "%f", &value);
        printf("  > Token: %s, value: %f\n", tokenbuf, value);
        switch (tokenbuf[0])
        {
        case 'a':
            profile_p->amp = value;
            break;
        case 't':
            profile_p->thdn = value;
            break;
        case 's':
            profile_p->snr = value;
            break;
        case 'b':
            profile_p->balance = value;
            break;
        case 'v':
            profile_p->pb_volume = value;
            break;
        case 'f':
            profile_p->fft_size = (int)value;
            break;
        case 'd':
            profile_p->debug = (int)value;
            break;
        case 'l':
            profile_p->log = (int)value;
            break;
        default:
            break;
        }
    }
    printf("  Wav test profile[%s]\n", path);
    printf("    Amp    :%f dB\n", profile_p->amp);
    printf("    Thdn   :-%f dB\n", profile_p->thdn);
    printf("    SNR    :%f dB\n", profile_p->snr);
    printf("    Balance:%f dB\n", profile_p->balance);
    printf("    Playback Volume : -%f dB\n", profile_p->pb_volume);
    printf("    Test with %d FFT\n", profile_p->fft_size);
    if (profile_p->debug)
        printf("    Debug message enable\n\n");
    else
        printf("    Debug message disable\n\n");
    if (profile_p->log)
        printf("    Log enable\n\n");
    else
        printf("    Log disable\n\n");
    
    wavFolderName = getWavFolderName(path);
    sprintf(profile_p->checkCmd,
            "/tmp/audio_loopback/rt_UAC_loopback --calcu-thdn /tmp/audio_loopback/%s/lpbk_sin1k_48k_24b_2ch.wav --balance %f --thdn %f --amp %f --snr %f --fft-size %d %s %s",
            wavFolderName,
            profile_p->balance,
            profile_p->thdn,
            profile_p->amp,
            profile_p->snr,
            profile_p->fft_size,
            profile_p->debug ? "--verbose" : "",
            profile_p->log ? "--output /tmp/log.txt" : "");
    
    printf("\n THDN calculation command according to %s: \n\t[%s] \n\n\n", wavFolderName, profile_p->checkCmd);
}

void loadConfig()
{
    loadWavTestProfile("/media/sdcard/hp_2_normal_linein_profile.txt", &g_normal_amic_record_profile.testProfile); // Wen-Yuan Design for ALC4052 start5
    loadWavTestProfile("/media/sdcard/hp_2_line1_profile.txt", &g_line1_record_profile.testProfile);
    // loadWavTestProfile("/media/sdcard/hp_2_line2_profile.txt", &g_line2_record_profile.testProfile);
    // loadWavTestProfile("/media/sdcard/hp_2_mic_iphone_profile.txt", &g_iphone_amic_record_profile.testProfile);
    // loadWavTestProfile("/media/sdcard/hp_2_mic_nokia_profile.txt", &g_nokia_amic_record_profile.testProfile);

    // Check log dir is exist, if not then create it
    if (access("/media/sdcard/log", F_OK) == -1)
    {
        printf("[Info] log dir not exist in sdcard, create one.");
        system("mkdir /media/sdcard/log");
    }
}

void updateLpbkLog(char *p_str_type, char is_fail)
{
    char path_buffer[128];
    char buffer[128];
    FILE *fp;

    // Check lpbk log dir is exist, if not then create it
    sprintf(path_buffer, "/media/sdcard/log/%s", p_str_type);
    if (access(path_buffer, F_OK) == -1)
    {
        printf("[Info] log dir \"%s\" not exist in sdcard, create one.\n", path_buffer);
        sprintf(buffer, "mkdir /media/sdcard/log/%s", p_str_type);
        system(buffer);
    }

    // check summary file is exist, then update total count
    sprintf(path_buffer, "/media/sdcard/log/%s/summary.txt", p_str_type);
    if (access(path_buffer, F_OK) == -1)
    {
        // There is no summy file, create one
        printf("[Info] log dir \"%s\" not exist in sdcard, create one.\n", path_buffer);
        fp = (FILE *)fopen(path_buffer, "w");
        if (fp)
        {
            fputs("1\n1\n0\n", fp);
            fclose(fp);
        }
    }
    if (access(path_buffer, F_OK) != -1)
    {
        int total_count = 0;

        // Update total count
        fp = (FILE *)fopen(path_buffer, "r");
        if (!fp)
        {
            printf("Error occure at update summary file %s", p_str_type);
            return;
        }
        if (fgets(buffer, sizeof(buffer), fp))
        {
            sscanf(buffer, "%d", &total_count);
            printf("[Info] %s summary : %d\n", p_str_type, total_count);
        }
        else
        {
            printf("Error occure at read summary file %s", p_str_type);
            return;
        }
        fclose(fp);
        fp = (FILE *)fopen(path_buffer, "w");
        if (fp)
        {
            sprintf(buffer, "%d", total_count + 1);
            fputs(buffer, fp);
            fclose(fp);
            printf("[Info] %s summary update : %d\n", p_str_type, total_count + 1);

            // TODO: Copy result file here..
            sprintf(buffer, "cp /tmp/log.txt /media/sdcard/log/%s/log-%d%s.txt",
                    p_str_type,
                    total_count,
                    is_fail ? "-fail" : "-pass");
            printf("Exec > %s \n", buffer);
            system(buffer);
        }
    }
}

void checkOnboardFwUpdate(void)
{
    // To update the DUT FW when the RTS3901 is powered on, we should put the .rfw file in the SDCard.
    printf("\n check Onboard FW update \n");
    if (system(cmdCpOnBoardRfw) != 0)
    {
        printf("\n ignore onboard FW update \n");
    }
    else
    {
        int result = 0;
        printf("Reset to Update FW ... \n");
        result = system(cmdOnBoardRst2UpdateFw);
        if (!result)
        {
            sleep(1);
            printf("Start FW update (force) ... \n");
            result = system(cmdOnBoardUpdate);
        }
        if (!result)
        {
            sleep(1);
            result = system(cmdOnBoardRst2Attach);
        }
        if (!result)
            printf("Onboard FW update successful. \n");
        printf("Reset to Attach.. \n\n");
    }
}

void initGpio(void)
{
    int ret;
    char cmd[256];
    FILE *fp;
    char buffr[256];
    int regVal = 0;

    printf("\n in initGpio uactest \n");
    rts_UART2TXD_IO_0 = rts_io_gpio_request(0, 12);
    rts_UART0CTS_IO_1 = rts_io_gpio_request(0, 18);
    rts_UART0RTS_IO_2 = rts_io_gpio_request(0, 19);
    rts_UART0RXD_IO_3 = rts_io_gpio_request(0, 17);
    rts_UART0TXD_IO_4 = rts_io_gpio_request(0, 16);
    rts_gpio7 = rts_io_gpio_request(0, 7);

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%% set used GPIO pull-low %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // Execute iomem command and read the result of the command
    fp = popen("iomem -r 0xb8800054", "r");
    if (fp == NULL)
    {
        printf("\n initGpio: read pull reg popen fail uactest \n");
    }
    else
    {
        if (fgets(buffr, sizeof(buffr) - 1, fp) != NULL)
        {
            regVal = (int)strtol(buffr, NULL, 0);
        }
        pclose(fp);
    }
    // printf("\n initGpio: pull reg = 0x%08x \n", regVal);
    regVal &= ~GPIO_PULL_MASK;     // clear
    regVal |= GPIO_PULL_DOWN_MASK; // set used GPIO pull-down
                                   // printf("\n initGpio: after mask pull reg = 0x%08x \n", regVal);
    sprintf(cmd, "iomem -w 0xb8800054 %d", regVal); // set output
    if (system(cmd) != 0) // system() function will execute the given command, then return 0 or -1
    {
        printf("\n initGpio: set pull reg pull-down fail uactest \n");
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%% set out reg low for clear signal %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    regVal = 0;
    fp = popen("iomem -r 0xb8800058", "r");
    if (fp == NULL)
    {
        printf("\n initGpio: read out reg popen for clear result signal fail uactest \n");
    }
    else
    {
        if (fgets(buffr, sizeof(buffr) - 1, fp) != NULL)
        {
            regVal = (int)strtol(buffr, NULL, 0);
        }
        pclose(fp);
    }
    regVal &= ~BI_DIR_IO_MASK;                      // clear, set all output low
    sprintf(cmd, "iomem -w 0xb8800058 %d", regVal); // set output
    if (system(cmd) != 0)
    {
        printf("\n initGpio: set out reg low fail uactest \n");
    }

    //%%%%%%%%%%%%%%%%%%%%%% set GPIO0~4 input %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_direction(rts_UART2TXD_IO_0, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_UART2TXD_IO_0 set direction input fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0CTS_IO_1, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_UART0CTS_IO_1 set direction input fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0RTS_IO_2, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_UART0RTS_IO_2 set direction input fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0RXD_IO_3, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_UART0RXD_IO_3 set direction input fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0TXD_IO_4, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_UART0TXD_IO_4 set direction input fail \n");
    }

    //%%%%%%%%%%%%%%%%%%%%%% init GPIO7(debug pin) %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_direction(rts_gpio7, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n initGpio: rts_gpio7 set output fail uactest \n");
    }
}

int detectStar(void)
{
    int i;
    int sigHist[32] = {0};
    int commandItor;
    int io0 = 0, io1 = 0, io2 = 0, io3 = 0, io4 = 0;
    int state;
    int const detectCnt = 4;

    printf("\n ********** in detectStar ********** \n");

    while (1)
    {
        if (tcp_debug_idx > 0) // If There is TCP debug command, execute
        {
            return tcp_debug_idx;
        }

        io0 = rts_io_gpio_get_value(rts_UART2TXD_IO_0); // 100~200us
        io1 = rts_io_gpio_get_value(rts_UART0CTS_IO_1); // 100~200us
        io2 = rts_io_gpio_get_value(rts_UART0RTS_IO_2); // 100~200us
        io3 = rts_io_gpio_get_value(rts_UART0RXD_IO_3); // 100~200us
        io4 = rts_io_gpio_get_value(rts_UART0TXD_IO_4); // 100~200us

        state = (io0 << 0) | (io1 << 1) | (io2 << 2) | (io3 << 3) | (io4 << 4);

        if (state > 0)
        {
            sigHist[state]++;
        }
        else
        {
            for (i = FIRST_COMMAND_SIGNAL; i < LAST_COMMAND_SIGNAL; i++)
            {
                commandItor = (i << 1) | 0x01;        // 4 bit command at MSB + start bit at LSB
                if (sigHist[commandItor] > detectCnt) // Check debounced signal ( counter > 4 )
                {
                    printf("\n [CMD#%d Cnt = %d uactest] \n", i, sigHist[commandItor]);
                    return i;
                }
            }

            // If none of one is trigger, clear all counter
            for (i = 0; i < 32; i++)
            {
                sigHist[i] = 0;
            }
        }
    }
}

void detectFinishAndTransInput(void)
{
    int debounceCnt = 0, cnt = 0, ret;
    int const detectCnt = 4;

    printf("\n in detectFinishAndTransInput uactest \n");

    ret = rts_io_gpio_set_direction(rts_UART2TXD_IO_0, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART2TXD_IO_0 set direction fail \n");
    }

    while (1)
    {
        if (tcp_debug_idx > 0)
        {
            printf("\t[TCP/IP] Fake finish ACK (debounceCnt = 5)\n");
            tcp_debug_idx = 0;
            debounceCnt = 5;
        }

        ret = rts_io_gpio_get_value(rts_UART2TXD_IO_0); // 100~200us

        if (ret != 0) // must check first
        {
            debounceCnt++;
        }
        else if (ret == 0 && debounceCnt > 0)
        {
            if (debounceCnt >= detectCnt) // debounceCnt=4, =>interval 3~4, (300us~500us)*3~4 => 0.9~2ms
            {
                printf("\n detectFinishAndTransInput: debounceCnt = %d uactest \n", debounceCnt);
                break;
            }
            else
            {
                debounceCnt = 0;
            }
        }
        uDelay(1); // 1: 100us

        cnt++;
        if (cnt == 32388) // 7sec
        {
            break;
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%% set output low for clear result signal %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0CTS_IO_1 set output low fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0RTS_IO_2 set output low fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0RXD_IO_3 set output low fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0TXD_IO_4 set output low fail \n");
    }

    //%%%%%%%%%%%%%%%%%%%%%% set GPIO 1~4 input %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_direction(rts_UART0CTS_IO_1, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0CTS_IO_1 set direction fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0RTS_IO_2, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0RTS_IO_2 set direction fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0RXD_IO_3, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0RXD_IO_3 set direction fail \n");
    }
    ret = rts_io_gpio_set_direction(rts_UART0TXD_IO_4, GPIO_INPUT);
    if (ret != 0)
    {
        printf("\n detectFinishAndTransInput: rts_UART0TXD_IO_4 set direction fail \n");
    }
}

void transOutputAndAck(int const ackIoMask, int const ackNum)
{
    int ret;

    printf("\n in transOutputAndAck%d uactest \n", ackNum);

    //%%%%%%%%%%%%%%%%%%%%%% set IO direction output %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_direction(rts_UART0CTS_IO_1, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0CTS_IO_1 set direction fail \n", ackNum);
    }
    ret = rts_io_gpio_set_direction(rts_UART0RTS_IO_2, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0RTS_IO_2 set direction fail \n", ackNum);
    }
    ret = rts_io_gpio_set_direction(rts_UART0RXD_IO_3, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0RXD_IO_3 set direction fail \n", ackNum);
    }
    ret = rts_io_gpio_set_direction(rts_UART0TXD_IO_4, GPIO_OUTPUT);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0TXD_IO_4 set direction fail \n", ackNum);
    }

    //%%%%%%%%%%%%%%%%%%%%%% set output ack signal %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    if (ackIoMask & 0x01)
    {
        ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0CTS_IO_1 set output high fail \n", ackNum);
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0CTS_IO_1 set output low fail \n", ackNum);
        }
    }

    if (ackIoMask & 0x02)
    {
        ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0RTS_IO_2 set output high fail \n", ackNum);
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0RTS_IO_2 set output low fail \n", ackNum);
        }
    }

    if (ackIoMask & 0x04)
    {
        ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0RXD_IO_3 set output high fail \n", ackNum);
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0RXD_IO_3 set output low fail \n", ackNum);
        }
    }

    if (ackIoMask & 0x08)
    {
        ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0TXD_IO_4 set output high fail \n", ackNum);
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n transOutputAndAck%d: rts_UART0TXD_IO_4 set output low fail \n", ackNum);
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%% keep ack %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    uDelay(97); // delay 10ms

    //%%%%%%%%%%%%%%%%%%%%%% clear ack signal %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0CTS_IO_1 set output low fail \n", ackNum);
    }

    ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0RTS_IO_2 set output low fail \n", ackNum);
    }

    ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0RXD_IO_3 set output low fail \n", ackNum);
    }

    ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n transOutputAndAck%d: rts_UART0TXD_IO_4 set output low fail \n", ackNum);
    }
}

int usbEnumeration(void)
{
    printf("\n in checkAudioDevice uactest \n");

    if (test_dev_id(0) == 1) // if card0 is exist
    { // usb at card0

        dev_idx = 0;
        initLpbkProfile();
        printf("\n usb at card%d uactest \n", dev_idx);

        goto finish;
    }

    if (test_dev_id(1) == 1) // if card1 is exist
    { // usb at card1

        dev_idx = 1;
        initLpbkProfile();
        printf("\n usb at card%d uactest \n", dev_idx);

        goto finish;
    }

    printf("\n USB checkAudioDevice fail uactest \n");

    return FAIL;

finish:
    return PASS;
}

int iphoneTypeCheck(void)
{
    printf("\n in iphoneTypeCheck uactest \n");

    unsigned char value = 0x00;
    
    value = readmem(CBJDET_TRIGGERSOR_DETTYPE);

    if ((value & JACK_TYPE_MASK) == IPHONE_TYPE)
        return PASS;
    else
        return FAIL;
}

int gbtnComparatorCheck(void)
{
    printf("\n in gbtnComparatorCheck uactest \n");

    return (
        writememAND(GPIO_TYPE_IO24_IO25_SEL, ~(0x07))   | // GPIO25 pin share GPIO
        writememAND(GPIO_TYPE_IO26_IO27_SEL, ~(0x70))   | // GPIO26 pin share GPIO
        writememAND(GPIO_TYPE_IO32_IO33_SEL, ~(0x77))   | // GPIO32/33 pin share GPIO
        writememOR(GPIO_DIR_IO24_IO31_CTL, 0x06)        | // GPIO25/26 set output
        writememOR(GPIO_DIR_IO32_IO39_CTL, 0x03)        | // GPIO32/33 set output
        writemem(GPIO25_TESTOUT, SAR1_CMPO_DIGITAL_0)   | 
        writemem(GPIO26_TESTOUT, SAR1_CMPO_DIGITAL_1)   | 
        writemem(GPIO32_TESTOUT, SAR1_CMPO_DIGITAL_2)   | 
        writemem(GPIO33_TESTOUT, SAR1_CMPO_DIGITAL_3)     
    );
}

int micDetect1BitCmpCheck(void)
{
    printf("\n in micDetect1BitCmpCheck uactest \n");

    return (
        writememAND(GPIO_TYPE_IO32_IO33_SEL, ~(0x07))       | // GPIO33 pin share GPIO
        writememOR(GPIO_DIR_IO32_IO39_CTL, 0x02)            | // GPIO33 set output
        writemem(POW_MICDET1B_MICDETMB_MICDETVTH_CTL, 0xA2) | 
        writemem(GPIO33_TESTOUT, CMP0_MIC_IN_DET_1BIT)
    );
}

int cbjNormalLineinSetup(void)
{
    printf("\n in cbjNormalLineinSetup uactest \n");

    return (
        writememAND(CBJDET_POWPIN_VREFO_HPAMPGND_SEL, ~(EN_CBJ_TIE_GL_GR_DIGITAL))  | 
        writememOR(CBJDET_CFG_CTL2, MICSEL_LR_BST1_DIGITAL)                         | 
        writememOR(CBJDET_EN_RST_MD_CFG_CTL, REG_MODE)                              | 
        writememAND(CBJDET_CFG_CTL5, ~(R2_SLEEVE_CPVREF_MASK))                      | 
        writememOR(CBJDET_CFG_CTL5, EN_CPVREF)                                      |
        writememAND(ENBTN4_BTN4VREF_CBJGAIN_CTL, ~(CBJ_LR_BOOST_MASK))
    );
}

int lpbkHP2Line1Amic(void)
{
    printf("\n in lpbkHP2Line1Amic uactest \n");

    return lpbkTest(&g_hp2normalLineIn_lpbk_profile);
}

// Open MICBIAS1/2 power
int micbias123Measure(void)
{
    printf("\n in micbias123Measure uactest \n");

    return (
        writememOR(CBJDET_POW_MICBIAS_MBIAS_CTL, 0x60)  | // Open MICBIAS1/2 power
        writememOR(0x0344, 0x01)                          // Open MICBIAS3 power
    );
}

// Set JD_CMPO_12[2:0] test output to GPIO25/26/32
int jdPlugCheck(void)
{
    printf("\n in jdPlugCheck uactest \n");

    return (
        writememAND(GPIO_TYPE_IO24_IO25_SEL, ~(0x07))   | // GPIO25 pin share GPIO
        writememAND(GPIO_TYPE_IO26_IO27_SEL, ~(0x70))   | // GPIO26 pin share GPIO
        writememAND(GPIO_TYPE_IO32_IO33_SEL, ~(0x70))   | // GPIO32 pin share GPIO
        writememOR(GPIO_DIR_IO24_IO31_CTL, 0x06)        | // GPIO25/26 set output
        writememOR(GPIO_DIR_IO32_IO39_CTL, 0x01)        | // GPIO32 set output
        writemem(GPIO25_TESTOUT, JD_CMPO_12_2)          | 
        writemem(GPIO26_TESTOUT, JD_CMPO_12_1)          | 
        writemem(GPIO32_TESTOUT, JD_CMPO_12_0)
    );
}

int hpCalibration(void)
{
    printf("\n in hpCalibration uactest \n");

    return (
        writemem(HP_SIDET_SOR_SEL, 0x00) // HP_AMP mandatorily open
    );
}

int regSettings(void)
{
    printf("\n in regSettings uactest \n");

    if (system(cmdChmod755RegWriteIni) != 0)
    {
        printf("\n fail chmod 755 register read write tool \n");
    }

    if (system(cmdRegSettingsWrite) != 0)
    {
        printf("\n fail reg settings write ini \n");
        return 1;
    }

    return 0;
}

int regSettings2(void)
{
    printf("\n in regSettings2 uactest \n");

    if (system(cmdChmod755RegWriteIni) != 0)
    {
        printf("\n fail chmod 755 register read write tool \n");
    }

    if (system(cmdRegSettings2Write) != 0)
    {
        printf("\n fail reg settings2 write ini \n");
        return 1;
    }

    return 0;
}

void returnRts3901FlashId(int id)
{
    int ret;

    printf("\n in returnRts3901FlashId uactest \n");

    // transOutputAndAck() function already set pin output

    // %%%%%%%%%%%%%%%%%%%%%%% set ID signal %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    if (id & BIT0)
    {
        ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0CTS_IO_1 set output high fail \n");
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0CTS_IO_1 set output low fail \n");
        }
    }

    if (id & BIT1)
    {
        ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0RTS_IO_2 set output high fail \n");
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0RTS_IO_2 set output low fail \n");
        }
    }

    if (id & BIT2)
    {
        ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0RXD_IO_3 set output high fail \n");
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0RXD_IO_3 set output low fail \n");
        }
    }

    if (id & BIT3)
    {
        ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_HIGH);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0TXD_IO_4 set output high fail \n");
        }
    }
    else
    {
        ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_LOW);
        if (ret != 0)
        {
            printf("\n returnRts3901FlashId: rts_UART0TXD_IO_4 set output low fail \n");
        }
    }
}

int remoteWakeupTest()
{
    printf("\n in remoteWakeupTest uactest \n");

    writemem(0xFA00, 0x09); // Enable remote wakeup

    if (system(cmdAutoSuspendDelay) != 0)
    {
        printf("\n fail auto suspend delay setting uactest \n");
        return 1;
    }

    if (system(cmdAutoSuspend) != 0)
    {
        printf("\n fail auto suspend setting uactest \n");
        return 1;
    }

    return 0;
}

int updateOtpCode(void)
{
    char cmd[256];

    printf("\n in updateOtpCode uactest \n");

    if (system(cmdChmod755RegWrite) != 0)
    {
        printf("\n fail chmod 755 RegWrite uactest \n");
        return 1; // fail
    }

    if (system(cmdChmod755DownCode) != 0)
    {
        printf("\n updateOtpCode: fail chmod 755 DownCode uactest \n");
        rts3901Fail(CHMOD_755_DOWN_CODE_ERR);
    }

    sprintf(cmd, "/tmp/rt_UAC_rw --write_mem 0xF940 2 %x %x ", (start9_rfw_checksum & 0x00FF), ((start9_rfw_checksum >> 8) & 0x00FF)); // write burst I2C reg
    printf("\n write reg string = %s \n", cmd);
    if (system(cmd) != 0)
    {
        printf("\n Error rt_UAC_rw write 0xF940\n");
    }

    if (system(cmdUpdateOtp) != 0)
    {
        printf("\n updateOtpCode fail uactest \n");
        return 1;
    }

    return 0;
}

int updateOtp4108Code(void)
{
    char cmd[256];

    printf("\n in updateOtp4108Code uactest \n");

    if (system(cmdChmod755RegWrite) != 0)
    {
        printf("\n fail chmod 755 RegWrite uactest \n");
        return 1; // fail
    }

    if (system(cmdChmod755DownCode) != 0)
    {
        printf("\n updateOtp4108Code: fail chmod 755 DownCode uactest \n");
        rts3901Fail(CHMOD_755_DOWN_CODE_ERR);
    }

    sprintf(cmd, "/tmp/rt_UAC_rw --write_mem 0xD2EE 2 %x %x ", (start13_rfw_checksum & 0x00FF), ((start13_rfw_checksum >> 8) & 0x00FF)); // write burst I2C reg
    printf("\n write reg string = %s \n", cmd);
    if (system(cmd) != 0)
    {
        printf("\n Error rt_UAC_rw write 0xD2EC\n");
    }

    if (system(cmdUpdateOtp4108) != 0)
    {
        printf("\n updateOtp4108Code fail uactest \n");
        return 1;
    }

    return 0;
}

int otpCheck(void)
{
    int ret, cnt;
    // int test=0;

    printf("\n in otpCheck uactest \n");

    if (system(cmdChmod755OtpCheck) != 0)
    {
        printf("\n fail chmod 755 otpCheck uactest \n");
    }

    if (system(cmdOtpCheck) != 0)
    {
        printf("\n otpCheck fail uactest \n");
        return 1;
    }

    return 0;
}

int lpbkHP2IPhoneAmic(void)
{
    printf("\n in lpbkHP2IPhoneAmic uactest \n");
    return lpbkTest(&g_hp2iphone_lpbk_profile);
}

int lpbkHP2NokiaAmic(void)
{
    printf("\n in lpbkHP2NokiaAmic uactest \n");
    return lpbkTest(&g_hp2nokia_lpbk_profile);
}

int lpbkHP2Line1(void)
{
    printf("\n in lpbkHP2Line1 uactest \n");
    return lpbkTest(&g_hp2line1_lpbk_profile);
}

int lpbkHP2NokiaAmicLine1(void)
{
    printf("\n in lpbkHP2NokiaAmicLine1 uactest \n");
    return lpbkTest(&g_hp2nokiaLine1_lpbk_profile);
}

int lpbkHP2IPhoneAmicLine2(void)
{
    printf("\n in lpbkHP2IPhoneAmicLine2 uactest \n");
    return lpbkTest(&g_hp2iphoneLine2_lpbk_profile);
}

int lpbkTest(LpbkTestProfile_t *const p_testProfile)
{

    printf("\n in Loopback uactest \n");

    int isFail = 0;
    int process_count = 0;
    int MAX_PROCESS_COUNT = 5;
    char cmdSetVolume[128];
    pid_t pid_list[MAX_PROCESS_COUNT];
    pid_t wpid;
    int status;
    int i = 0;

    if (system(cmdChmod755Loopback) != 0)
    {
        printf("\n fail chmod 755 loopback uactest \n");
    }

    // Set volume
    if (p_testProfile->p_recordingProfile1)
    {
        sprintf(cmdSetVolume, "amixer -c%d -- sset 'PCM',0 -%fdB", dev_idx, p_testProfile->p_recordingProfile1->testProfile.pb_volume);
        printf("[Set Volume] %s\n", cmdSetVolume);
        if (system(cmdSetVolume) != 0)
        {
            printf("\n fail set playback volume :\n > %s \n", cmdSetVolume);
        }
    }

    char *argvPlay[] = {
        "aplay",
        "-D", p_testProfile->p_playbackProfile->aplayId,
        p_testProfile->p_playbackProfile->wavFilePath,
        "-r", p_testProfile->p_playbackProfile->sampleRate,
        "-c", p_testProfile->p_playbackProfile->channel,
        "-f", p_testProfile->p_playbackProfile->format,
        "-d", p_testProfile->p_playbackProfile->duration,
        "--buffer-time=16000",
        NULL};

    printf(" > *argvPlay[] \n");
    for (i = 0; i < 12; i++)
    {
        printf("    [%d] -> %s\n", i, argvPlay[i]);
    }
    printf("\n");

    // Multi-process
    if ((pid_list[process_count] = fork()) < 0)
    {
        // fork() fail
        printf("[loopback aplay item] fork() fail");
        return -1;
    }
    else if (pid_list[process_count] == 0)
    {
        // Child process 0
        printf("Create child process 0 [playback 2ch] \n");
        // usleep(60000);
        execvp("aplay", argvPlay);
        _exit(127);
    }
    process_count++;

    if (p_testProfile->p_recordingProfile1)
    {
        printf("\n in p_recordingProfile1 uactest \n");
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile1;
        if ((pid_list[process_count] = fork()) < 0)
        {
            // fork() fail
            printf("[loopback arecord stream0 item] fork() fail");
            return -1;
        }
        else if (pid_list[process_count] == 0)
        {
            // Child process
            printf("Create child process %d [capture %s] \n", process_count, p_recProfile->testProfile.dirName);
            char *argvRec[] = {
                "arecord",
                "-D", p_recProfile->arecordId,
                p_recProfile->wavFilePath,
                "-r", p_recProfile->sampleRate,
                "-c", p_recProfile->channel,
                "-f", p_recProfile->format,
                "-d", p_recProfile->duration,
                "--buffer-time=16000",
                NULL};

            printf(" > *argvRec[] \n");
            for (i = 0; i < 12; i++)
            {
                printf("    [%d] -> %s\n", i, argvRec[i]);
            }
            printf("\n");

            usleep(40000);
            execvp("arecord", argvRec);
            _exit(127);
        }
        process_count++;
    }
    printf("\n out p_recordingProfile1 uactest \n");

    if (p_testProfile->p_recordingProfile2)
    {
        printf("\n in p_recordingProfile2 uactest \n");

        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile2;
        if ((pid_list[process_count] = fork()) < 0)
        {
            // fork() fail
            printf("[loopback arecord stream0 item] fork() fail");
            return -1;
        }
        else if (pid_list[process_count] == 0)
        {
            // Child process
            printf("Create child process %d [capture %s] \n", process_count, p_recProfile->testProfile.dirName);
            char *argvRec[] = {
                "arecord",
                "-D", p_recProfile->arecordId,
                p_recProfile->wavFilePath,
                "-r", p_recProfile->sampleRate,
                "-c", p_recProfile->channel,
                "-f", p_recProfile->format,
                "-d", p_recProfile->duration,
                "--buffer-time=16000",
                NULL};

            printf(" > *argvRec[] \n");
            for (i = 0; i < 12; i++)
            {
                printf("    [%d] -> %s\n", i, argvRec[i]);
            }
            printf("\n");

            usleep(40000);
            execvp("arecord", argvRec);
            _exit(127);
        }
        process_count++;
    }
    printf("\n out p_recordingProfile1 uactest \n");

    if (p_testProfile->p_recordingProfile3)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile3;
        if ((pid_list[process_count] = fork()) < 0)
        {
            // fork() fail
            printf("[loopback arecord stream0 item] fork() fail");
            return -1;
        }
        else if (pid_list[process_count] == 0)
        {
            // Child process
            printf("Create child process %d [capture %s] \n", process_count, p_recProfile->testProfile.dirName);
            char *argvRec[] = {
                "arecord",
                "-D", p_recProfile->arecordId,
                p_recProfile->wavFilePath,
                "-r", p_recProfile->sampleRate,
                "-c", p_recProfile->channel,
                "-f", p_recProfile->format,
                "-d", p_recProfile->duration,
                "--buffer-time=16000",
                NULL};
            usleep(40000);
            execvp("arecord", argvRec);
            _exit(127);
        }
        process_count++;
    }

    if (p_testProfile->p_recordingProfile4)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile4;
        if ((pid_list[process_count] = fork()) < 0)
        {
            // fork() fail
            printf("[loopback arecord stream0 item] fork() fail");
            return -1;
        }
        else if (pid_list[process_count] == 0)
        {
            // Child process
            printf("Create child process %d [capture %s] \n", process_count, p_recProfile->testProfile.dirName);
            char *argvRec[] = {
                "arecord",
                "-D", p_recProfile->arecordId,
                p_recProfile->wavFilePath,
                "-r", p_recProfile->sampleRate,
                "-c", p_recProfile->channel,
                "-f", p_recProfile->format,
                "-d", p_recProfile->duration,
                "--buffer-time=16000",
                NULL};
            usleep(40000);
            execvp("arecord", argvRec);
            _exit(127);
        }
        process_count++;
    }

    // Playback & Record wav for 350 ms and kill
    usleep(350000); // Experiment
    for (i = 0; i < process_count; i++)
    {
        kill(pid_list[i], SIGINT);
    }

    // Wait for each child process finish
    while ((wpid = wait(&status)) > 0)
    {
        if (status != 0)
        {
            printf("\n loopback fail uactest \n");
            for (i = 0; i < process_count; i++)
            {
                if (pid_list[i] == wpid)
                {
                    printf(" # Loopback fail at %d \n", i);
                }
            }
            isFail = 1;
        }
    }

    // Calc THDN, SNR, Amp, Balance here, and check that there is below threshold
    if (p_testProfile->p_recordingProfile1)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile1;
        printf("\n > Exec: %s\n", p_recProfile->testProfile.checkCmd);
        if (system(p_recProfile->testProfile.checkCmd) != 0)
        {
            printf("\n %s loopback fail uactest \n", p_recProfile->testProfile.dirName);
            isFail = 1;
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 1);
            }
        }
        else
        {
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 0);
            }
        }
    }
    if (p_testProfile->p_recordingProfile2)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile2;
        printf("\n > Exec: %s\n", p_recProfile->testProfile.checkCmd);
        if (system(p_recProfile->testProfile.checkCmd) != 0)
        {
            printf("\n %s loopback fail uactest \n", p_recProfile->testProfile.dirName);
            isFail = 1;
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 1);
            }
        }
        else
        {
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 0);
            }
        }
    }
    if (p_testProfile->p_recordingProfile3)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile3;
        if (system(p_recProfile->testProfile.checkCmd) != 0)
        {
            printf("\n %s loopback fail uactest \n", p_recProfile->testProfile.dirName);
            isFail = 1;
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 1);
            }
        }
        else
        {
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 0);
            }
        }
    }
    if (p_testProfile->p_recordingProfile4)
    {
        RecordingProfile_t *const p_recProfile = p_testProfile->p_recordingProfile4;
        if (system(p_recProfile->testProfile.checkCmd) != 0)
        {
            printf("\n %s loopback fail uactest \n", p_recProfile->testProfile.dirName);
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 1);
            }
        }
        else
        {
            if (p_recProfile->testProfile.log)
            {
                updateLpbkLog(p_recProfile->testProfile.dirName, 0);
            }
        }
    }

    if (isFail)
    {
        printf("\n loopback fail uactest \n");
        return FAIL;
    }
    else
    {
        return PASS;
    }
}

int initAmic()
{

    printf("\n in Loopback uactest \n");

    pid_t amic_pid;
    pid_t wpid;
    int i;
    int isFail = 0;
    int status;

    RecordingProfile_t *const p_recProfile = &g_nokia_amic_record_profile;

    if ((amic_pid = fork()) < 0)
    {
        // fork() fail
        printf("[loopback arecord stream0 item] fork() fail");
        return -1;
    }
    else if (amic_pid == 0)
    {
        // Child process
        printf("Create child process [capture %s] \n", p_recProfile->testProfile.dirName);
        char *argvRec[] = {
            "arecord",
            "-D", p_recProfile->arecordId,
            p_recProfile->wavFilePath,
            "-r", p_recProfile->sampleRate,
            "-c", p_recProfile->channel,
            "-f", p_recProfile->format,
            "-d", p_recProfile->duration,
            "--buffer-time=16000",
            NULL};
        printf(" > *argvRec[] \n");
        for (i = 0; i < 12; i++)
        {
            printf("    [%d] -> %s\n", i, argvRec[i]);
        }
        execvp("arecord", argvRec);
        _exit(127);
    }

    // Playback & Record wav for 350 ms and kill
    usleep(100000); // Experiment
    kill(amic_pid, SIGINT);

    // Wait for each child process finish
    while ((wpid = wait(&status)) > 0)
    {
        if (status != 0)
        {
            printf("\n loopback fail uactest \n");
            isFail = 1;
        }
    }

    return isFail;
}

int readCorrelationIdPid(void)
{
    char cmd[100];
    char readBuf[30];
    char *pchr;
    int regVal = 0;
    FILE *fp;
    char pidPath[256];

    printf("\n in readCorrelationIdPid uactest \n");

    if (system(cmdChmod755RegWrite) != 0)
    {
        printf("\n fail chmod 755 RegWrite uactest \n");
        return 1; // fail
    }

    //%%%%%%%%%%%%%%%%%% read PID %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    pidPath[0] = 0;

    sprintf(pidPath, "/sys/devices/platform/ehci-platform/usb1/1-1/idProduct");

    fp = fopen(pidPath, "r");
    if (fp == NULL)
    {
        return 1; // fail
    }
    readBuf[0] = 0;
    fscanf(fp, "%s\n", readBuf);
    fclose(fp);

    regVal = (int)strtol(readBuf, NULL, 16);
    printf("\n usb PID read = %x\n", regVal);

    sprintf(cmd, "/tmp/rt_UAC_rw --write_mem 0xF940 2 %x %x ", (regVal & 0x00FF), ((regVal >> 8) & 0x00FF)); // write burst I2C reg
    printf("\n write reg string = %s \n", cmd);
    if (system(cmd) != 0)
    {
        printf("\n Error rt_UAC_rw write 0xF940\n");
    }

    //%%%%%%%%%%%%%%%%%% read Correlation ID %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    fp = popen("/tmp/rt_UAC_rw --read_otp 0x3f62 1", "r");
    if (fp == NULL)
    {
        printf("\n Error rt_UAC_rw read otp 0x3f62 1\n");
        return 1; // fail
    }

    if (fgets(readBuf, sizeof(readBuf) - 1, fp) != NULL)
    {
        pclose(fp);
        pchr = strchr(readBuf, '>');
        pchr++;
        regVal = (int)strtol(pchr, NULL, 0);
        printf("\n 0x3f62 read otp = %d\n", regVal);

        if (regVal != 0 && regVal != 1 && regVal != 2)
        {
            return 1; // fail
        }

        sprintf(cmd, "/tmp/rt_UAC_rw --write_mem 0xFE97 1 %x", regVal); // write burst I2C reg
        if (system(cmd) != 0)
        {
            printf("\n Error rt_UAC_rw write 0xFE97\n");
            return 1; // fail
        }
    }
    else
    {
        printf("\n gets 0x3f62 fail \n");
        return 1; // fail
    }

    return 0;
}

int writeCorrelationId02(void)
{
    char cmd[100];

    printf("\n in writeCorrelationId02 uactest \n");

    if (system(cmdChmod755RegWrite) != 0)
    {
        printf("\n fail chmod 755 RegWrite uactest \n");
        return 1; // fail
    }

    //%%%%%%%%%%%%%%%%%% write Correlation ID %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    sprintf(cmd, "/tmp/rt_UAC_rw --write_otp 0x3f62 1 0x02");
    if (system(cmd) != 0)
    {
        printf("\n Error rt_UAC_rw write otp 0x3f62 1 0x02\n");
        return 1; // fail
    }

    /*
    //%%%%%%%%%%%%%%%%%% read Correlation ID to check %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    fp = popen("/tmp/rt_UAC_rw --read_otp 0x3f62 1", "r");
    if( fp == NULL )
    {
        printf("\n Error rt_UAC_rw read otp 0x3f62 1\n");
        return 1;//fail
    }

    if( fgets(readBuf, sizeof(readBuf)-1, fp) != NULL )
    {
        pclose( fp );
        pchr = strchr(readBuf,'>');
        pchr++;
        regVal = (int)strtol( pchr, NULL, 0 );
        printf("\n 0x3f62 read otp = %d\n", regVal);

        if( regVal != 0x02)
        {
            return 1;//fail
        }

    }
    else
    {
        printf("\n gets 0x3f62 fail \n");
        return 1;//fail
    }*/

    return 0;
}

int writeCorrelationId01(void)
{
    char cmd[100];

    printf("\n in writeCorrelationId01 uactest \n");

    if (system(cmdChmod755RegWrite) != 0)
    {
        printf("\n fail chmod 755 RegWrite uactest \n");
        return 1; // fail
    }

    //%%%%%%%%%%%%%%%%%% write Correlation ID %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    sprintf(cmd, "/tmp/rt_UAC_rw --write_otp 0x3f62 1 0x01");
    if (system(cmd) != 0)
    {
        printf("\n Error rt_UAC_rw write otp 0x3f62 1 0x01\n");
        return 1; // fail
    }

    return 0;
}

int PidCheck(void)
{
    char readBuf[30];
    int regVal = 0;
    FILE *fp;
    char pidPath[256];

    //%%%%%%%%%%%%%%%%%% read PID %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    pidPath[0] = 0;

    sprintf(pidPath, "/sys/devices/platform/ehci-platform/usb1/1-1/idProduct");

    fp = fopen(pidPath, "r");
    if (fp == NULL)
    {
        return 1; // fail
    }
    readBuf[0] = 0;
    fscanf(fp, "%s\n", readBuf);
    fclose(fp);

    regVal = (int)strtol(readBuf, NULL, 16);
    printf("\n usb PID read = %x\n", regVal);

    if (regVal == 0x4030)
    {
        printf("\n PID == 0x4030 uactest \n");
        return 0;
    }

    printf("\n PID is not 0x4030 uactest \n");
    return 1;
}

void sendPass(void)
{
    int ret;

    printf("\n ********** in sendPass ********** \n");

    tcp_debug_result = 1;

    ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_HIGH);
    if (ret != 0)
    {
        printf("\n sendPass: rts_UART0CTS_IO_1 set output high fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_HIGH);
    if (ret != 0)
    {
        printf("\n sendPass: rts_UART0RTS_IO_2 set output high fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_HIGH);
    if (ret != 0)
    {
        printf("\n sendPass: rts_UART0RXD_IO_3 set output high fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_HIGH);
    if (ret != 0)
    {
        printf("\n sendPass: rts_UART0TXD_IO_4 set output high fail \n");
    }
}

void sendFail(void)
{
    int ret;

    printf("\n ********** in sendFail ********** \n");

    tcp_debug_result = 2;

    ret = rts_io_gpio_set_value(rts_UART0CTS_IO_1, GPIO_HIGH);
    if (ret != 0)
    {
        printf("\n sendFail: rts_UART0CTS_IO_1 set output high fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RTS_IO_2, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n sendFail: rts_UART0RTS_IO_2 set output low fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0RXD_IO_3, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n sendFail: rts_UART0RXD_IO_3 set output low fail \n");
    }
    ret = rts_io_gpio_set_value(rts_UART0TXD_IO_4, GPIO_LOW);
    if (ret != 0)
    {
        printf("\n sendFail: rts_UART0TXD_IO_4 set output low fail \n");
    }
}

unsigned short calCheckSum(char *rfwPath)
{
    unsigned short checksumValue = 0;
    FILE *fp = NULL;
    unsigned char temp = 0;

    printf("\n in checkCheckSum \n");

    fp = fopen(rfwPath, "rb");
    if (fp != NULL)
    {
        while (fread(&temp, sizeof(unsigned char), 1, fp))
        {
            checksumValue = checksumValue + temp;
        }
        fclose(fp);

        printf("\n Checksum = 0x%04x\n", checksumValue);
        return checksumValue;
    }
    else
    {
        printf("checkCheckSum: rfw open fail");
        rts3901FailFlag = 1;
        return 0;
    }

    printf("\n out checkCheckSum \n");
}

// Copy data from SDCard to RTS3901 /tmp/ folder
void copyToolFromSdCard(void)
{
    int cnt;
    unsigned short checkSum = 0;

    printf("\n copy UAC utility uactest \n");
    if (system(cmdCpDownCodeTool) != 0)
    {
        printf("\n fail copy UAC utility tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    printf("\n copy audio loopback tool uactest \n");
    if (system(cmdCpLoopbackTool) != 0)
    {
        printf("\n fail copy audio loopback tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    printf("\n copy otp check tool uactest \n");
    if (system(cmdCpOtpCheckTool) != 0)
    {
        printf("\n fail copy otp check tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    printf("\n copy reg write tool uactest \n");
    if (system(cmdCpRegWriteTool) != 0)
    {
        printf("\n fail copy reg write tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    printf("\n copy reg write ini tool uactest \n");
    if (system(cmdCpRegWriteIniTool) != 0)
    {
        printf("\n fail copy reg write ini tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    printf("\n chmod reg write ini tool uactest \n");
    if (system(cmdChmod755RegWriteIni) != 0)
    {
        printf("\n fail chmod reg write ini tool uactest \n");
        rts3901FailFlag = 1;
    }
    sleep(1);

    // printf("\n copy reg setting ini uactest \n");
    // if( system( cmdCpRegSettingsWriteIni ) != 0 )
    // {
    // 	printf("\n fail copy reg settings ini uactest \n");
    // 	rts3901FailFlag = 1;
    // }
    // sleep(1);

    // printf("\n copy reg setting2 ini uactest \n");
    // if( system( cmdCpRegSettings2WriteIni ) != 0 )
    // {
    // 	printf("\n fail copy reg settings2 ini uactest \n");
    // 	rts3901FailFlag = 1;
    // }
    // sleep(1);

    // printf("\n transfer reg setting ini to dat(cache) uactest \n");
    // if( system( cmdRegSettingsToCache ) != 0 )
    // {
    // 	printf("\n fail transfer reg setting ini to dat(cache) uactest \n");
    // 	rts3901FailFlag = 1;
    // }
    // sleep(1);

    // printf("\n transfer reg setting2 ini to dat(cache) uactest \n");
    // if( system( cmdRegSettings2ToCache ) != 0 )
    // {
    // 	printf("\n fail transfer reg setting2 ini to dat(cache) uactest \n");
    // 	rts3901FailFlag = 1;
    // }
    // sleep(1);

    if (rts3901FailFlag == 1)
    {
        rts3901Fail(COPY_OTP_CHECK_ERR);
    }
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void *socketThreadDebug(void *arg)
{
    int sockfd, newsockfd;
    socklen_t clilen;
    char buffer[BUF_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("\t[TCP/IP] ERROR opening socket");
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_DEBUG);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("\t[TCP/IP] ERROR on binding");
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1)
    {
        int read_wait_time = 0;

        printf("\t[TCP/IP] Waiting for client connection...\n");
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0)
        {
            error("\t[TCP/IP] ERROR on accept");
        }

        printf("\t[TCP/IP] Client connected from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        while (1)
        {
            memset(buffer, 0, BUF_SIZE);

            n = read(newsockfd, buffer, BUF_SIZE);

            if (n < 0)
            {
                error("\t[TCP/IP] ERROR reading from socket");
                break;
            }
            else if (n > 0)
            {
                printf("\t[TCP/IP] Received data[0]: 0x%02X\n", (char)buffer[0]);
                printf("\t[TCP/IP] Received data[1]: 0x%02X\n", (char)buffer[1]);
                printf("\t[TCP/IP] Received data[2]: 0x%02X\n", (char)buffer[2]);
                printf("\t[TCP/IP] Received data[3]: 0x%02X\n", (char)buffer[3]);

                if (buffer[0] == (char)0xAA)
                {
                    printf("\t[TCP/IP] tcp_debug_idx = 0x%02X\n", (char)buffer[3]);
                    tcp_debug_idx = buffer[3];
                }
            }
            else
            {
                printf("\t[TCP/IP] Wait TCP Data Attmpt %d\n", ++read_wait_time);
            }

            if (read_wait_time > 10)
            {
                printf("\t[TCP/IP] Read timeout (disconnect?)\n");
                break;
            }

            if (tcp_debug_result)
            {
                // write(newsockfd, tcp_debug_result, sizeof(tcp_debug_result));
                tcp_debug_result = 0;
            }

            sleep(1);
            fflush(stdout);
        }

        close(newsockfd);
    }
    close(sockfd);
    pthread_exit(NULL);
}

void *socketThreadControl(void *arg)
{
    int sockfd, newsockfd;
    char buffer[BUF_SIZE2];
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("\n\t [TCP/IP] ERROR opening socket \n");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_CONTROL);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("\n\t [TCP/IP] ERROR on binding \n");
    }

    if (listen(sockfd, 5))
    {
        error("\n\t [TCP/IP] ERROR on listening \n");
    }
    printf("\n\t [TCP/IP] Server is listening on port %d \n", PORT_CONTROL);

    while(1)
    {
        if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
        {
            printf("\n\t [TCP/IP] ERROR on accepting client \n");
            continue;
        }
        printf("\n\t [TCP/IP] Client connected from %s:%d \n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        memset(buffer, 0, BUF_SIZE2);
        if (read(newsockfd, buffer, BUF_SIZE2) == -1)
        {
            printf("\n\t [TCP/IP] ERROR on reading data from client \n");
            continue;
        }
    }
}

unsigned char check_uacDevice_exist()
{
    RtUac_CloseUSBDevice(); // If we don't call close here, an error will occur when replacing the DUT

    RtUac_Init();
	if (RtUac_OpenUSBDevice())
		return FALSE;
    else
        return TRUE;
}

int main(int argc, char **argv)
{
    int ret, result, testItem;
    // pthread_t tid_debug;
    pthread_t tid_control;

    initSig(); // Signal handler

    initGpio();

    copyToolFromSdCard(); // Copy data from SDCard to RTS3901 /tmp/ folder

    sleep(1);

    checkOnboardFwUpdate(); // To update the DUT FW when the RTS3901 is powered on, we should put the .rfw file in the SDCard.

    printf("\n ---- Debug %s %s ---- \n", __DATE__, __TIME__);

    // Load Test spec from txt file (EX: hp_2_line1_profile.txt)
    loadConfig();

    // Initialize loopback profile
    initLpbkProfile();

    /* For handling the issue of "RTS3901 needs to spend more than 100ms when using RtUac_OpenUSBDevice() for the first time" */
    RtUac_Init();
    if (RtUac_OpenUSBDevice())
        printf("\n No RTK uac device exist \n");
    else
        printf("\n Pre-open USB device successfully \n");
    /* ---------------------------------------------------------------------------------------------------------------------- */

    /* 
    if (pthread_create(&tid_debug, NULL, socketThreadDebug, NULL))
    {
        printf("Error creating socketThreadDebug.");
        return -1;
    } 
    */

    if (pthread_create(&tid_control, NULL, socketThreadControl, NULL))
    {
        printf("\n Error creating socketThreadControl \n");
        return -1;
    }

    while (1)
    {
        long size = 3047483647u;
        printf("\n %ld \n", sizeof(size));
        printf("\n %lu \n", size);

        FILE *fp;
        fp = fopen("/tmp/rt_UAC_rw", "rb");
        if(!fp)
        {
            printf("\n");
            perror(" fopen()");
            printf("\n");
            break;
        }

        // seek to end of file
        if (fseek(fp, 0, SEEK_END) == -1) {
            perror("fseek() SEEK_END");
        }

        // get file size
        size = ftell(fp);
        if (size == -1) {
            perror("ftell error");
        }

        // seek to end of file
        if (fseek(fp, 0, SEEK_SET) == -1) {
            perror("fseek() SEEK_SET");
        }
        printf("\n %lu \n", size);

        result = PASS;
        testItem = detectStar(); // Detect the test command from FT Machine (V50 or S100)
        
        if (check_uacDevice_exist() == FALSE)
        {
            transOutputAndAck(~testItem, testItem);
            result = FAIL;
            printf("\n No RTK uac device exist \n");
            goto sendPassFail;
        }


        switch (testItem)
        {
        case ENUM_CHECK_SIGNAL:                 // start1 signal
            transOutputAndAck(~testItem, testItem);
            result = usbEnumeration();
            printf("\n out usbEnumeration uactest \n");
            break;
        case CBJ_IPHONE_CHECK_SIGNAL:           // start2 signal
            transOutputAndAck(~testItem, testItem);
            result = iphoneTypeCheck();
            printf("\n out iphoneTypeCheck uactest \n");
            break;
        case GBTN_COMPARATOR_CHECK_SIGNAL:      // start3 signal
            transOutputAndAck(~testItem, testItem);
            result = gbtnComparatorCheck();
            printf("\n out gbtnComparatorCheck uactest \n");
            break;
        case MIC_DET_1BIT_CMP_CHECK_SIGNAL:     // start4 signal
            transOutputAndAck(~testItem, testItem);
            result = micDetect1BitCmpCheck();
            printf("\n out micDetect1BitCmpCheck uactest \n");
            break;
        case TEST_LPBK_HP_LINE1_AMIC_SIGNAL:    // start5 signal
            transOutputAndAck(~testItem, testItem);
            result = lpbkHP2Line1Amic();
            printf("\n out lpbkHP2Line1Amic uactest \n");
            break;
        case SETUP_CBJ_NORMAL_LINEIN_SIGNAL:    // start6 signal
            transOutputAndAck(~testItem, testItem);
            result = cbjNormalLineinSetup();
            printf("\n out cbjNormalLineinSetup uactest \n");
            break;
        case MEASURE_MICBIAS_123_SIGNAL:        // start7 signal
            transOutputAndAck(~testItem, testItem);
            result = micbias123Measure();
            printf("\n out micbias123Measure uactest \n");
            break;
        case JD_PLUG_CHECK_SIGNAL:              // start8 signal
            transOutputAndAck(~testItem, testItem);
            result = jdPlugCheck();
            printf("\n out jdPlugCheck uactest \n");
            break;
        case HP_CALIB_SIGNAL:                   // start9 signal
            transOutputAndAck(~testItem, testItem);
            result = hpCalibration();
            printf("\n out hpCalibration uactest \n");
            break;

        /*
        case TEST_LPBK_HP_NOKIA_AMIC_LINE1_SIGNAL: // start6 signal
            transOutputAndAck(~testItem, testItem);
            result = lpbkHP2NokiaAmicLine1();
            printf("\n out lpbkHP2NokiaAmicLine1 uactest \n");
            break;
        case TEST_LPBK_HP_IPHONE_AMIC_LINE2_SIGNAL: // start7 signal
            transOutputAndAck(~testItem, testItem);
            result = lpbkHP2IPhoneAmicLine2();
            printf("\n out lpbkHP2IPhoneAmicLine2 uactest \n");
            break;
        case OTP_CHECK_SIGNAL: // start8 signal
            transOutputAndAck(~testItem, testItem);
            result = otpCheck();
            printf("\n out otpCheck uactest \n");
            break;
        case CHECK_3901_FLASHID_SIGNAL: // start9 signal
            transOutputAndAck(~testItem, testItem);
            returnRts3901FlashId(RTS3901_FLASH_ID);
            printf("\n out returnRts3901FlashId uactest \n");
            break;
        case REMOTE_WAKEUP_SIGNAL: // start10 signal
            transOutputAndAck(~testItem, testItem);
            result = remoteWakeupTest();
            printf("\n out remoteWakeupTest uactest \n");
            break;
        case RESERVE_3_SIGNAL:
            transOutputAndAck(~testItem, testItem);
            result = PASS; // TODO: Not used
            printf("\n out xxxxxxx uactest \n");
            break;
        case CHECK_PID_4030_SIGNAL: // start12 signal
            transOutputAndAck(~testItem, testItem);
            result = PidCheck();
            printf("\n out PidCheck uactest \n");
            break; */
        }

sendPassFail:
        if (testItem != CHECK_3901_FLASHID_SIGNAL)
        {
            if (result == PASS)
            {
                sendPass();
            }
            else
            {
                sendFail();
            }
        }

        detectFinishAndTransInput();

#if (DEBUG3901 == 1)
        printf("\n in DEBUG3901 uactest \n");
        sleep(2);

        ret = rts_io_gpio_set_direction(rts_gpio7, GPIO_INPUT);
        if (ret != 0)
        {
            printf("\n main: rts_gpio7 set input fail uactest \n");
        }

        ret = rts_io_gpio_get_value(rts_gpio7);
        if (ret == 1)
        {
            break;
        }
#endif
    }

    rts_io_gpio_free(rts_UART2TXD_IO_0);
    rts_io_gpio_free(rts_UART0CTS_IO_1);
    rts_io_gpio_free(rts_UART0RTS_IO_2);
    rts_io_gpio_free(rts_UART0RXD_IO_3);
    rts_io_gpio_free(rts_UART0TXD_IO_4);
    rts_io_gpio_free(rts_gpio7);

    return 0;
}
