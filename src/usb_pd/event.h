#pragma once

#include "def.h"

#define MAX_PDO_COUNT 15
#define PDO_MASK 0b1100

typedef enum usb_pd_control_message_type_t
{
    MSG_TYPE_GoodCRC                      = 0b00001,     // Source, Sink or Cable Plug
    MSG_TYPE_GoToMin                      = 0b00010,     // Deprecated
    MSG_TYPE_Accept                       = 0b00011,     // Source, Sink or Cable Plug
    MSG_TYPE_Reject                       = 0b00100,     // Source, Sink or Cable Plug
    MSG_TYPE_Ping                         = 0b00101,     // Deprecated
    MSG_TYPE_PS_RDY                       = 0b00110,     // Source or Sink
    MSG_TYPE_Get_Source_Cap               = 0b00111,     // Sink or DRP
    MSG_TYPE_Get_Sink_Cap                 = 0b01000,     // Source or DRP
    MSG_TYPE_DR_Swap                      = 0b01001,     // Source or Sink
    MSG_TYPE_PR_Swap                      = 0b01010,     // Source or Sink
    MSG_TYPE_VCONN_Swap                   = 0b01011,     // Source or Sink
    MSG_TYPE_Wait                         = 0b01100,     // Source or Sink
    MSG_TYPE_Soft_Reset                   = 0b01101,     // Source or Sink
    MSG_TYPE_Data_Reset                   = 0b01110,     // Source or Sink
    MSG_TYPE_Data_Reset_Complete          = 0b01111,     // Source or Sink
    MSG_TYPE_Not_Supported                = 0b10000,     // Source, Sink or Cable Plug
    MSG_TYPE_Get_Source_Cap_Extended      = 0b10001,     // Sink or DRP
    MSG_TYPE_Get_Status                   = 0b10010,     // Source or Sink
    MSG_TYPE_FR_Swap                      = 0b10011,     // Sink
    MSG_TYPE_Get_PPS_Status               = 0b10100,     // Sink
    MSG_TYPE_Get_Country_Codes            = 0b10101,     // Source or Sink
    MSG_TYPE_Get_Sink_Cap_Extended        = 0b10110,     // Source or DRP
    MSG_TYPE_Get_Source_Info              = 0b10111,     // Sink or DRP
    MSG_TYPE_Get_Revision                 = 0b11000,     // Source or Sink
} usb_pd_control_message_type_t;

typedef enum usb_pd_data_message_type_t
{
    MSG_TYPE_Source_Capabilities          = 0b00001,     // Source or DRP
    MSG_TYPE_Request                      = 0b00010,     // Sink
    MSG_TYPE_BIST                         = 0b00011,     // Tester, Source or Sink
    MSG_TYPE_Sink_Capabilities            = 0b00100,     // Sink or DRP
    MSG_TYPE_Battery_Status               = 0b00101,     // Source or Sink
    MSG_TYPE_Alert                        = 0b00110,     // Source or Sink
    MSG_TYPE_Get_Country_Info             = 0b00111,     // Source or Sink
    MSG_TYPE_Enter_USB                    = 0b01000,     // DFP
    MSG_TYPE_EPR_Request                  = 0b01001,     // Sink
    MSG_TYPE_EPR_Mode                     = 0b01010,     // Source or Sink
    MSG_TYPE_Source_Info                  = 0b01011,     // Source
    MSG_TYPE_Revision                     = 0b01100,     // Source, Sink or Cable Plug
    MSG_TYPE_Vendor_Defined               = 0b01111,     // Source, Sink or Cable Plug
} usb_pd_data_message_type_t;

typedef enum usb_pd_extended_data_message_type_t
{
    MSG_TYPE_Source_Capabilities_Extended = 0b00001,     // Source or DRP
    MSG_TYPE_Status                       = 0b00010,     // Source, Sink or C
    MSG_TYPE_Get_Battery_Cap              = 0b00011,     // Source or Sink
    MSG_TYPE_Get_Battery_Status           = 0b00100,     // Source or Sink
    MSG_TYPE_Battery_Capabilities         = 0b00101,     // Source or Sink
    MSG_TYPE_Get_Manufacturer_Info        = 0b00110,     // Source or Sink
    MSG_TYPE_Manufacturer_Info            = 0b00111,     // Source, Sink or Cable Plug
    MSG_TYPE_Security_Request             = 0b01000,     // Source or Sink
    MSG_TYPE_Security_Response            = 0b01001,     // Source, Sink or Cable Plug
    MSG_TYPE_Firmware_Update_Request      = 0b01010,     // Source or Sink
    MSG_TYPE_Firmware_Update_Response     = 0b01011,     // Source, Sink or Cable Plug
    MSG_TYPE_PPS_Status                   = 0b01100,     // Source
    MSG_TYPE_Country_Info                 = 0b01101,     // Source or Sink
    MSG_TYPE_Country_Codes                = 0b01110,     // Source or Sink
    MSG_TYPE_Sink_Capabilities_Extended   = 0b01111,     // Sink or DRP
    MSG_TYPE_Extended_Control             = 0b10000,     // Source or Sink
    MSG_TYPE_EPR_Source_Capabilities      = 0b10001,     // Source or DRP
    MSG_TYPE_EPR_Sink_Capabilities        = 0b10010,     // Sink or DRP
    MSG_TYPE_Vendor_Defined_Extended      = 0b11110,     // Source, Sink or Cable Plug
} usb_pd_extended_data_message_type_t;

typedef enum PDO_type_t
{
    FPDO        = 0b0000,
    BPDO        = 0b0100,
    VPDO        = 0b1000,
    PPS_PDO     = 0b1100,
    EPR_AVS_PDO = 0b1101,
    SPR_AVS_PDO = 0b1110,
} PDO_type_t;

typedef enum usb_pd_event_status_type_t
{
    PD_GOODCRC,
    PD_REPLY,
    PD_WAITING,
} usb_pd_event_status_type_t;

typedef enum usb_pd_epr_status_type_t
{
    EPR_UNSUPPORTED,
    EPR_NOT_READY,
    EPR_READY,
    EPR_WAITING,
    EPR_ON,
} usb_pd_epr_status_type_t;

typedef enum usb_pd_keep_alive_type_t
{
    KEEP_ALIVE_NONE,
    KEEP_ALIVE_PPS,
    KEEP_ALIVE_EPR,
} usb_pd_keep_alive_type_t;

typedef struct FPDO_t
{
    uint16_t voltage_mV;
    uint16_t current_mA;
} FPDO_t;

typedef struct BPDO_t
{
    uint16_t min_voltage_mV;
    uint16_t max_voltage_mV;
    uint16_t PDP_cW;
} BPDO_t;

typedef struct VPDO_t
{
    uint16_t min_voltage_mV;
    uint16_t max_voltage_mV;
    uint16_t current_mA;
} VPDO_t;

typedef struct PPS_PDO_t
{
    uint16_t min_voltage_mV;
    uint16_t max_voltage_mV;
    uint16_t current_mA;
} PPS_PDO_t;

typedef struct EPR_AVS_PDO_t
{
    uint16_t min_voltage_mV;
    uint16_t max_voltage_mV;
    uint16_t PDP_cW;
} EPR_AVS_PDO_t;

typedef struct SPR_AVS_PDO_t
{
    uint16_t current_15v_mA;
    uint16_t current_20v_mA;
} SPR_AVS_PDO_t;

typedef struct PDO_t
{
    PDO_type_t type;
    bool epr;
    union PDO
    {
        FPDO_t FPDO;
        BPDO_t BPDO;
        VPDO_t VPDO;
        PPS_PDO_t PPS_PDO;
        EPR_AVS_PDO_t EPR_AVS_PDO;
        SPR_AVS_PDO_t SPR_AVS_PDO;
    } PDO;
    uint32_t raw;
} PDO_t;

typedef struct RDO_t
{
    uint8_t pos;
    PDO_type_t type;
    uint16_t voltage_mV;
    uint16_t current_mA;
    uint16_t pdp_cW;
    uint32_t copy_of_pdo;
} RDO_t;

typedef struct mix_Message_Header_t
{
    uint8_t sop;
    bool extended;
    uint8_t num_objs;
    uint8_t msg_id;
    uint8_t ppr;
    uint8_t revision;
    uint8_t pdr;
    uint8_t msg_type;
    bool chunked;
    uint8_t chunked_num;
    bool req_chunk;
    uint16_t data_size;
} mix_Message_Header_t;

typedef enum key_mode_type_t
{
    KEY_MODE_SELECT_PDO,     // 默认: 左右键切换PDO
    KEY_MODE_SELECT_FIELD,   // PPS/AVS: 左键=电压, 右键=电流, 双击右键确认
    KEY_MODE_MODIFY_VALUE,   // 修改值模式
} key_mode_type_t;

typedef enum modify_field_type_t
{
    MODIFY_NONE,
    MODIFY_VOLTAGE,
    MODIFY_CURRENT,
    MODIFY_POWER,
} modify_field_type_t;

typedef enum usb_pd_cmd_type_t
{
    USB_PD_CMD_SELECT_PDO,        // value = PDO position (0-based index)
    USB_PD_CMD_NEXT_PDO,          // 选择下一个有效 PDO
    USB_PD_CMD_PREV_PDO,          // 选择上一个有效 PDO
    USB_PD_CMD_SET_VOLTAGE_MV,    // value = 目标电压 (mV)
    USB_PD_CMD_SET_CURRENT_MA,    // value = 目标电流 (mA)
    USB_PD_CMD_SET_POWER_CW,      // value = 目标功率 (cW, 即 10mW)
    USB_PD_CMD_ADJUST_VOLTAGE_MV, // value = 电压增量 (±mV)
    USB_PD_CMD_ADJUST_CURRENT_MA, // value = 电流增量 (±mA)
    USB_PD_CMD_ADJUST_POWER_CW,   // value = 功率增量 (±cW)
} usb_pd_cmd_type_t;

typedef struct usb_pd_cmd_t
{
    usb_pd_cmd_type_t type;
    int32_t value;
} usb_pd_cmd_t;

void usb_pd_cmd_execute(const usb_pd_cmd_t *cmd);

void usb_pd_event_save_rx(uint8_t status, const uint8_t *data, uint16_t len);
void usb_pd_event_save_connect_change(void);

void usb_pd_event_process_next(void);
