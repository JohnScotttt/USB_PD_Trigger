#include "event.h"
#include "phy.h"
#include "delay.h"
#include "usb/hid.h"
#include "control/status.h"
#include "control/keypad.h"

static PDO_t current_pdo[MAX_PDO_COUNT] = {0};
static uint8_t num_pdos = 0;
static RDO_t current_rdo = {0};
static mix_Message_Header_t current_msg_header = {0};
static mix_Message_Header_t last_msg_header = {0};
static uint8_t current_data[USB_PD_DATA_MAX_LEN] = {0}; // Includes sop, header, data objects and crc
static uint8_t last_ext_data[70] = {0};                 // Only data objects without extended header, but padding 3 bytes ahead
static usb_pd_event_status_type_t event_status = PD_WAITING;
static usb_pd_epr_status_type_t epr_status = EPR_UNSUPPORTED;
static usb_pd_keep_alive_type_t keep_alive_status = KEEP_ALIVE_NONE;
static uint32_t last_timestamp_ms = 0;
static uint8_t msg_id = 7;

#define CMD_QUEUE_SIZE 4
static usb_pd_cmd_t cmd_queue[CMD_QUEUE_SIZE];
static uint8_t cmd_queue_head = 0;
static uint8_t cmd_queue_tail = 0;

static bool cmd_queue_push(const usb_pd_cmd_t *cmd)
{
    uint8_t next = (cmd_queue_head + 1) % CMD_QUEUE_SIZE;
    if (next == cmd_queue_tail) return false; // full
    cmd_queue[cmd_queue_head] = *cmd;
    cmd_queue_head = next;
    return true;
}

static bool cmd_queue_pop(usb_pd_cmd_t *cmd)
{
    if (cmd_queue_tail == cmd_queue_head) return false; // empty
    *cmd = cmd_queue[cmd_queue_tail];
    cmd_queue_tail = (cmd_queue_tail + 1) % CMD_QUEUE_SIZE;
    return true;
}

#define NEXT_MESSAGE_ID() (msg_id = (msg_id + 1) % 8)
#define PDO_POS(x) ((x) + 1)
#define GET_PDO_POS(x) ((x) - 1)

// ============================================================================
// 按键修改模式 — 状态机
// ============================================================================

static key_mode_type_t key_mode = KEY_MODE_SELECT_PDO;
static modify_field_type_t modify_field = MODIFY_NONE;

// ============================================================================
// 步进值定义
// ============================================================================

#define FPDO_CURRENT_MIN_STEP_MA    10    /* FPDO/VPDO 电流最小步进 10mA */
#define FPDO_CURRENT_BIG_STEP_MA    (FPDO_CURRENT_MIN_STEP_MA * 10)  /* 100mA */

#define BPDO_POWER_MIN_STEP_CW      25    /* BPDO 功率最小步进 250mW (25cW) */
#define BPDO_POWER_BIG_STEP_CW      (BPDO_POWER_MIN_STEP_CW * 10)   /* 2.5W (250cW) */

#define PPS_VOLTAGE_MIN_STEP_MV     20    /* PPS 电压最小步进 20mV */
#define PPS_VOLTAGE_BIG_STEP_MV     (PPS_VOLTAGE_MIN_STEP_MV * 10)   /* 200mV */
#define PPS_CURRENT_MIN_STEP_MA     50    /* PPS 电流最小步进 50mA */
#define PPS_CURRENT_BIG_STEP_MA     (PPS_CURRENT_MIN_STEP_MA * 10)   /* 500mA */

#define AVS_VOLTAGE_MIN_STEP_MV     100   /* AVS 电压最小步进 100mV */
#define AVS_VOLTAGE_BIG_STEP_MV     (AVS_VOLTAGE_MIN_STEP_MV * 10)   /* 1000mV */
#define AVS_CURRENT_MIN_STEP_MA     50    /* AVS 电流最小步进 50mA */
#define AVS_CURRENT_BIG_STEP_MA     (AVS_CURRENT_MIN_STEP_MA * 10)   /* 500mA */

static void build_header(uint16_t *header, bool use_next_msg_id)
{
    *header |= (use_next_msg_id ? NEXT_MESSAGE_ID() : current_msg_header.msg_id) << 9;
    *header |= current_msg_header.revision << 6;
    *header |= !current_msg_header.pdr << 5;
}

static uint32_t build_rdo_msg(void)
{
    uint32_t data_obj = 0;
    data_obj |= PDO_POS(current_rdo.pos) << 28;
    data_obj |= current_pdo[0].epr << 22;
    switch (current_rdo.type)
    {
        case FPDO:
        case VPDO:
            data_obj |= current_rdo.current_mA / 10 << 10;
            data_obj |= current_rdo.current_mA / 10;
            break;
        case BPDO:
            data_obj |= current_rdo.pdp_cW / 25 << 10;
            data_obj |= current_rdo.pdp_cW / 25;
            break;
        case PPS_PDO:
            data_obj |= current_rdo.voltage_mV / 20 << 9;
            data_obj |= current_rdo.current_mA / 50;
            break;
        case EPR_AVS_PDO:
        case SPR_AVS_PDO:
            data_obj |= current_rdo.voltage_mV / 25 << 9;
            data_obj |= current_rdo.current_mA / 50;
            break;
    }
    return data_obj;
}

static mix_Message_Header_t parse_header(uint8_t sop, uint16_t header, uint16_t ext_header)
{
    mix_Message_Header_t msg_header = {0};
    msg_header.sop = sop;
    msg_header.extended = header >> 15;
    msg_header.num_objs = (header >> 12) & 0x7;
    msg_header.msg_id = (header >> 9) & 0x7;
    msg_header.ppr = (header >> 8) & 0x1;
    msg_header.revision = (header >> 6) & 0x3;
    msg_header.pdr = (header >> 5) & 0x1;
    msg_header.msg_type = header & 0x1F;
    if (msg_header.extended)
    {
        msg_header.chunked = ext_header >> 15;
        msg_header.chunked_num = (ext_header >> 11) & 0xF;
        msg_header.req_chunk = (ext_header >> 10) & 0x1;
        msg_header.data_size = ext_header & 0x1FF;
    }
    return msg_header;
}

static uint16_t current_goodcrc(void)
{
    uint16_t value = 0x0001;
    build_header(&value, false);
    return value;
}

static PDO_t parse_pdo_from_raw(uint32_t pdo_value, bool is_epr)
{
    PDO_t pdo = {0};
    uint8_t raw_type = (pdo_value >> 28) & PDO_MASK;
    pdo.type = (raw_type != 0b1100) ? raw_type : (pdo_value >> 28);
    pdo.epr = is_epr;
    pdo.raw = pdo_value;

    switch (pdo.type)
    {
        case FPDO:
            pdo.PDO.FPDO.voltage_mV = ((pdo_value >> 10) & 0x3FF) * 50;
            pdo.PDO.FPDO.current_mA = (pdo_value & 0x3FF) * 10;
            break;
        case BPDO:
            pdo.PDO.BPDO.max_voltage_mV = ((pdo_value >> 20) & 0x3FF) * 50;
            pdo.PDO.BPDO.min_voltage_mV = ((pdo_value >> 10) & 0x3FF) * 50;
            pdo.PDO.BPDO.PDP_cW         = (pdo_value & 0x3FF) * 25;
            break;
        case VPDO:
            pdo.PDO.VPDO.max_voltage_mV = ((pdo_value >> 20) & 0x3FF) * 50;
            pdo.PDO.VPDO.min_voltage_mV = ((pdo_value >> 10) & 0x3FF) * 50;
            pdo.PDO.VPDO.current_mA     = (pdo_value & 0x3FF) * 10;
            break;
        case PPS_PDO:
            pdo.PDO.PPS_PDO.max_voltage_mV = ((pdo_value >> 17) & 0xFF) * 100;
            pdo.PDO.PPS_PDO.min_voltage_mV = ((pdo_value >> 8) & 0xFF) * 100;
            pdo.PDO.PPS_PDO.current_mA     = (pdo_value & 0x7F) * 50;
            break;
        case EPR_AVS_PDO:
            pdo.PDO.EPR_AVS_PDO.max_voltage_mV = ((pdo_value >> 17) & 0x1FF) * 100;
            pdo.PDO.EPR_AVS_PDO.min_voltage_mV = ((pdo_value >> 8) & 0xFF) * 100;
            pdo.PDO.EPR_AVS_PDO.PDP_cW         = (pdo_value & 0xFF) * 100;
            break;
        case SPR_AVS_PDO:
            pdo.PDO.SPR_AVS_PDO.current_15v_mA = ((pdo_value >> 10) & 0x3FF) * 10;
            pdo.PDO.SPR_AVS_PDO.current_20v_mA = (pdo_value & 0x3FF) * 10;
            break;
    }

    return pdo;
}

static RDO_t parse_rdo_from_raw(uint32_t rdo_value, uint32_t copy_of_pdo)
{
    RDO_t rdo = {0};
    rdo.pos = GET_PDO_POS(rdo_value >> 28);
    if (!copy_of_pdo)
    {
        copy_of_pdo = current_pdo[rdo.pos].raw;
    }
    PDO_t pdo = parse_pdo_from_raw(copy_of_pdo, current_pdo[rdo.pos].epr);
    rdo.type = pdo.type;
    rdo.copy_of_pdo = copy_of_pdo;
    switch (rdo.type)
    {
        case FPDO:
        case VPDO:
        {
            rdo.voltage_mV = pdo.PDO.FPDO.voltage_mV;
            rdo.current_mA = ((rdo_value >> 10) & 0x3FF) * 10;
            break;
        }
        case BPDO:
        {
            rdo.pdp_cW = ((rdo_value >> 10) & 0x3FF) * 25;
            break;
        }
        case PPS_PDO:
        {
            rdo.voltage_mV = ((rdo_value >> 9) & 0xFFF) * 20;
            rdo.current_mA = (rdo_value & 0x7F) * 50;
            break;
        }
        case EPR_AVS_PDO:
        case SPR_AVS_PDO:
        {
            rdo.voltage_mV = ((rdo_value >> 9) & 0xFFF) * 25;
            rdo.current_mA = (rdo_value & 0x7F) * 50;
            break;
        }
    }

    return rdo;
}

// TODO: prop
static void parse_pdos(void)
{
    num_pdos = current_msg_header.extended ? (current_msg_header.data_size / 4) : current_msg_header.num_objs;
    num_pdos = (num_pdos > MAX_PDO_COUNT) ? MAX_PDO_COUNT : num_pdos;
    bool is_epr = false;
    for (uint8_t i = 0; i < num_pdos; i++)
    {
        uint8_t *pdos_data = current_msg_header.extended ? last_ext_data : current_data;
        uint32_t pdo_value = (pdos_data[6 + i * 4] << 24) |
                             (pdos_data[5 + i * 4] << 16) |
                             (pdos_data[4 + i * 4] << 8)  |
                              pdos_data[3 + i * 4];

        if (i == 0 && (pdo_value >> 23) & 0x1)
        {
            is_epr = true;
            if (get_usb_pd_sink_mode() == EPR_MODE && epr_status == EPR_UNSUPPORTED)
            {
                epr_status = EPR_NOT_READY;
            }
        }

        current_pdo[i] = parse_pdo_from_raw(pdo_value, is_epr);
    }

    // PDO 列表刷新后，重置按键修改模式，防止 pos / field 与新列表不匹配
    key_mode = KEY_MODE_SELECT_PDO;
    modify_field = MODIFY_NONE;
}

static void process_msg(void)
{
    if (current_msg_header.extended && !current_msg_header.req_chunk) // save chunked data
    {
        if (current_msg_header.chunked_num == 0)
        {
            memset(last_ext_data, 0, sizeof(last_ext_data));
        }
        memcpy(&last_ext_data[current_msg_header.chunked_num * 26 + 3], &current_data[5], current_msg_header.num_objs * 4 - 2);
    }

    if (current_msg_header.extended) // extended msg
    {
        if (current_msg_header.data_size / 26 == current_msg_header.chunked_num)
        {
            switch (current_msg_header.msg_type)
            {
                case MSG_TYPE_EPR_Source_Capabilities:
                {
                    epr_status = EPR_ON;
                    parse_pdos();
                    break;
                }
            }
        }
    }
    else if (current_msg_header.num_objs > 0) // data msg
    {
        switch (current_msg_header.msg_type)
        {
            case MSG_TYPE_Source_Capabilities:
            {
                parse_pdos();
                break;
            }
        }
    }
    else // control msg
    {
        switch (current_msg_header.msg_type)
        {
            case MSG_TYPE_PS_RDY:
            {
                if (epr_status == EPR_NOT_READY)
                {
                    epr_status = EPR_READY;
                }
                if (epr_status == EPR_ON)
                {
                    keep_alive_status = KEEP_ALIVE_EPR;
                }
                if (current_rdo.type == PPS_PDO && keep_alive_status != KEEP_ALIVE_EPR)
                {
                    keep_alive_status = KEEP_ALIVE_PPS;
                }
                break;
            }
        }
    }
}

static void auto_reply(void)
{
    uint8_t reply[USB_PD_DATA_MAX_LEN] = {0};
    uint16_t header = 0;
    uint16_t ext_header = 0;
    if (current_msg_header.extended) // extended msg
    {
        switch (current_msg_header.msg_type)
        {
            case MSG_TYPE_EPR_Source_Capabilities:
            {
                if (current_msg_header.data_size / 26 > current_msg_header.chunked_num)
                {
                    header = 0x9000 | MSG_TYPE_EPR_Source_Capabilities;
                    build_header(&header, true);
                    ext_header = 0x8400 | ((current_msg_header.chunked_num + 1) << 11);
                    
                    memcpy(&reply[0], &header, 2);
                    memcpy(&reply[2], &ext_header, 2);

                    usb_pd_phy_send_data(reply, 6, UPD_SOP0);
                    break;
                }
                else
                {
                    header = 0x2000 | MSG_TYPE_EPR_Request;
                    build_header(&header, true);

                    uint32_t data_obj = 0x10000000;
                    data_obj |= current_pdo[0].epr << 22;
                    data_obj |= current_pdo[0].PDO.FPDO.current_mA / 10 << 10;
                    data_obj |= current_pdo[0].PDO.FPDO.current_mA / 10;

                    memcpy(&reply[0], &header, 2);
                    memcpy(&reply[2], &data_obj, 4);
                    memcpy(&reply[6], &current_pdo[0].raw, 4);

                    memset(&current_rdo, 0, sizeof(current_rdo));
                    current_rdo.pos = 0;
                    current_rdo.type = current_pdo[0].type;
                    current_rdo.voltage_mV = current_pdo[0].PDO.FPDO.voltage_mV;
                    current_rdo.current_mA = current_pdo[0].PDO.FPDO.current_mA;
                    current_rdo.copy_of_pdo = current_pdo[0].raw;

                    usb_pd_phy_send_data(reply, 10, UPD_SOP0);
                    break;
                }
            }
        }
    }
    else if (current_msg_header.num_objs > 0) // data msg
    {
        switch (current_msg_header.msg_type)
        {
            case MSG_TYPE_Source_Capabilities:
            {
                header = 0x1000 | MSG_TYPE_Request;
                build_header(&header, true);

                uint32_t data_obj = 0x10000000;
                data_obj |= current_pdo[0].epr << 22;
                data_obj |= current_pdo[0].PDO.FPDO.current_mA / 10 << 10;
                data_obj |= current_pdo[0].PDO.FPDO.current_mA / 10;

                memcpy(&reply[0], &header, 2);
                memcpy(&reply[2], &data_obj, 4);

                memset(&current_rdo, 0, sizeof(current_rdo));
                current_rdo.pos = 0;
                current_rdo.type = current_pdo[0].type;
                current_rdo.voltage_mV = current_pdo[0].PDO.FPDO.voltage_mV;
                current_rdo.current_mA = current_pdo[0].PDO.FPDO.current_mA;
                current_rdo.copy_of_pdo = current_pdo[0].raw;

                usb_pd_phy_send_data(reply, 6, UPD_SOP0);
                break;
            }
        }
    }
    else // control msg
    {
        switch (current_msg_header.msg_type)
        {
            case MSG_TYPE_Soft_Reset:
            {
                header = 0x0000 | MSG_TYPE_Accept;
                build_header(&header, true);

                memcpy(&reply[0], &header, 2);

                usb_pd_phy_send_data(reply, 2, UPD_SOP0);
                break; 
            }
            case MSG_TYPE_Get_Sink_Cap:
            {
                header = 0x2000 | MSG_TYPE_Sink_Capabilities;
                build_header(&header, true);

                uint32_t data_obj_1 = 0x0001912C;
                uint32_t data_obj_2 = 0xC0DC213C;

                memcpy(&reply[0], &header, 2);
                memcpy(&reply[2], &data_obj_1, 4);
                memcpy(&reply[6], &data_obj_2, 4);

                usb_pd_phy_send_data(reply, 10, UPD_SOP0);
                break;
            }
        }
    }
}

static void hid_forward_pd(void)
{
    const uint8_t *data = NULL;
    if (!hid_rx_buf_peek_pd(&data))
    {
        return;
    }

    uint8_t buf[HIDRAW_OUT_EP_SIZE] = {0};
    memcpy(buf, data, HIDRAW_OUT_EP_SIZE);
    hid_rx_buf_pop_pd();

    // Determine payload base offset based on packet format
    // Standard: ...| FullLength [10] | DataType [11] | SOP [12] | PD...
    // Mini:     ...| FullLength [2]  | DataType [3]  | SOP [4]  | PD...
    uint8_t d = (buf[0] == HID_CMD_HEADER_0_STD)
              ? (HID_CMD_STD_DATA_TYPE_OFFSET + 1)    // 12
              : (HID_CMD_MINI_DATA_TYPE_OFFSET + 1);  // 4

    uint8_t length = buf[d - 2] - 2; // FullLength minus DataType and SOP
    uint8_t sop = (buf[d] == SOP)         ? UPD_SOP0
                : (buf[d] == SOP1)        ? UPD_SOP1
                : (buf[d] == SOP2)        ? UPD_SOP2
                : (buf[d] == HARD_RESET)  ? UPD_HARD_RESET
                : (buf[d] == CABLE_RESET) ? UPD_CABLE_RESET
                                          : 0xFF;

    uint16_t header = (buf[d + 2] << 8) | buf[d + 1];
    header &= 0xF11F;
    build_header(&header, true);
    memcpy(&buf[d + 1], &header, 2);
    uint16_t if_ext_header = (buf[d + 4] << 8) | buf[d + 3];
    mix_Message_Header_t msg_header = parse_header(buf[d], header, if_ext_header);
    if (msg_header.num_objs > 0 && msg_header.msg_type == MSG_TYPE_Request)
    {
        uint32_t rdo_value = (buf[d + 6] << 24) | (buf[d + 5] << 16) | (buf[d + 4] << 8) | buf[d + 3];
        current_rdo = parse_rdo_from_raw(rdo_value, 0);
    }
    else if (msg_header.num_objs > 0 && !msg_header.extended && msg_header.msg_type == MSG_TYPE_EPR_Request)
    {
        uint32_t rdo_value = (buf[d + 6] << 24) | (buf[d + 5] << 16) | (buf[d + 4] << 8) | buf[d + 3];
        uint32_t copy_of_pdo = (buf[d + 10] << 24) | (buf[d + 9] << 16) | (buf[d + 8] << 8) | buf[d + 7];
        current_rdo = parse_rdo_from_raw(rdo_value, copy_of_pdo);
    }

    if (sop != 0xFF)
    {
        usb_pd_phy_send_data(&buf[d + 1], length, sop);
    }
}

static void send_current_rdo(void)
{
    uint8_t request[USB_PD_DATA_MAX_LEN] = {0};
    uint32_t data_obj = build_rdo_msg();
    if (epr_status == EPR_ON)
    {
        uint16_t header = 0x2000 | MSG_TYPE_EPR_Request;
        build_header(&header, true);

        memcpy(&request[0], &header, 2);
        memcpy(&request[2], &data_obj, 4);
        memcpy(&request[6], &current_rdo.copy_of_pdo, 4);
        usb_pd_phy_send_data(request, 10, UPD_SOP0);
    }
    else
    {
        uint16_t header = 0x1000 | MSG_TYPE_Request;
        build_header(&header, true);

        memcpy(&request[0], &header, 2);
        memcpy(&request[2], &data_obj, 4);
        usb_pd_phy_send_data(request, 6, UPD_SOP0);
    }
}

// 查找下一个有效PDO位置（跳过 raw == 0 的空槽）
// direction: +1 向后, -1 向前
static uint8_t find_valid_pdo_pos(int8_t direction)
{
    uint8_t pos = current_rdo.pos;
    for (uint8_t i = 0; i < num_pdos; i++)
    {
        pos = (pos + num_pdos + direction) % num_pdos;
        if (current_pdo[pos].raw != 0)
        {
            return pos;
        }
    }
    // 找不到有效PDO，返回当前位置
    return current_rdo.pos;
}

static void select_and_send_rdo(uint8_t pos)
{
    current_rdo.pos = pos;
    current_rdo.type = current_pdo[pos].type;
    current_rdo.copy_of_pdo = current_pdo[pos].raw;
    switch (current_pdo[pos].type)
    {
        case FPDO:
            current_rdo.voltage_mV = current_pdo[pos].PDO.FPDO.voltage_mV;
            current_rdo.current_mA = current_pdo[pos].PDO.FPDO.current_mA;
            current_rdo.pdp_cW     = 0;
            break;
        case BPDO:
            current_rdo.voltage_mV = current_pdo[pos].PDO.BPDO.min_voltage_mV;
            current_rdo.current_mA = 0;
            current_rdo.pdp_cW     = current_pdo[pos].PDO.BPDO.PDP_cW;
            break;
        case VPDO:
            current_rdo.voltage_mV = current_pdo[pos].PDO.VPDO.min_voltage_mV;
            current_rdo.current_mA = current_pdo[pos].PDO.VPDO.current_mA;
            current_rdo.pdp_cW     = 0;
            break;
        case PPS_PDO:
            current_rdo.voltage_mV = current_pdo[pos].PDO.PPS_PDO.min_voltage_mV;
            current_rdo.current_mA = current_pdo[pos].PDO.PPS_PDO.current_mA;
            current_rdo.pdp_cW     = 0;
            break;
        case EPR_AVS_PDO:
        {
            current_rdo.voltage_mV = current_pdo[pos].PDO.EPR_AVS_PDO.min_voltage_mV;
            current_rdo.pdp_cW     = current_pdo[pos].PDO.EPR_AVS_PDO.PDP_cW;
            // 暂定 5A，但不超过 PDP / V
            uint16_t i_max_epr = 5000;
            if (current_rdo.voltage_mV > 0)
            {
                uint16_t i_pdp = (uint16_t)((uint32_t)current_rdo.pdp_cW * 10000 / current_rdo.voltage_mV);
                if (i_pdp < i_max_epr) i_max_epr = i_pdp;
            }
            current_rdo.current_mA = i_max_epr;
            break;
        }
        case SPR_AVS_PDO:
            current_rdo.voltage_mV = 9000;
            current_rdo.current_mA = current_pdo[pos].PDO.SPR_AVS_PDO.current_15v_mA;
            current_rdo.pdp_cW     = 0;
            break;
    }

    send_current_rdo();
}

// ============================================================================
// 按键修改模式 — 辅助函数
// ============================================================================

static bool pdo_type_has_dual_fields(PDO_type_t type)
{
    return (type == PPS_PDO || type == EPR_AVS_PDO || type == SPR_AVS_PDO);
}

static void get_step_values(PDO_type_t type, modify_field_type_t field, uint16_t *min_step, uint16_t *big_step)
{
    switch (type)
    {
        case FPDO:
        case VPDO:
            *min_step = FPDO_CURRENT_MIN_STEP_MA;
            *big_step = FPDO_CURRENT_BIG_STEP_MA;
            break;
        case BPDO:
            *min_step = BPDO_POWER_MIN_STEP_CW;
            *big_step = BPDO_POWER_BIG_STEP_CW;
            break;
        case PPS_PDO:
            if (field == MODIFY_VOLTAGE)
            {
                *min_step = PPS_VOLTAGE_MIN_STEP_MV;
                *big_step = PPS_VOLTAGE_BIG_STEP_MV;
            }
            else
            {
                *min_step = PPS_CURRENT_MIN_STEP_MA;
                *big_step = PPS_CURRENT_BIG_STEP_MA;
            }
            break;
        case EPR_AVS_PDO:
        case SPR_AVS_PDO:
            if (field == MODIFY_VOLTAGE)
            {
                *min_step = AVS_VOLTAGE_MIN_STEP_MV;
                *big_step = AVS_VOLTAGE_BIG_STEP_MV;
            }
            else
            {
                *min_step = AVS_CURRENT_MIN_STEP_MA;
                *big_step = AVS_CURRENT_BIG_STEP_MA;
            }
            break;
    }
}

static void get_value_bounds(modify_field_type_t field, uint16_t *min_val, uint16_t *max_val)
{
    PDO_t *pdo = &current_pdo[current_rdo.pos];
    switch (current_rdo.type)
    {
        case FPDO:
            *min_val = 0;
            *max_val = pdo->PDO.FPDO.current_mA;
            break;
        case VPDO:
            *min_val = 0;
            *max_val = pdo->PDO.VPDO.current_mA;
            break;
        case BPDO:
            *min_val = 0;
            *max_val = pdo->PDO.BPDO.PDP_cW;
            break;
        case PPS_PDO:
            if (field == MODIFY_VOLTAGE)
            {
                *min_val = pdo->PDO.PPS_PDO.min_voltage_mV;
                *max_val = pdo->PDO.PPS_PDO.max_voltage_mV;
            }
            else
            {
                *min_val = 0;
                *max_val = pdo->PDO.PPS_PDO.current_mA;
            }
            break;
        case EPR_AVS_PDO:
            if (field == MODIFY_VOLTAGE)
            {
                *min_val = pdo->PDO.EPR_AVS_PDO.min_voltage_mV;
                *max_val = pdo->PDO.EPR_AVS_PDO.max_voltage_mV;
            }
            else
            {
                // EPR_AVS 电流上限 = PDP / 当前电压
                *min_val = 0;
                if (current_rdo.voltage_mV > 0)
                {
                    // PDP_cW 单位 cW(10mW), voltage_mV 单位 mV
                    // I_max(mA) = PDP(cW) * 10(mW/cW) * 1000(mA/A) / V(mV) = PDP_cW * 10000 / V_mV
                    *max_val = (uint16_t)((uint32_t)pdo->PDO.EPR_AVS_PDO.PDP_cW * 10000 / current_rdo.voltage_mV);
                }
                else
                {
                    *max_val = 0;
                }
            }
            break;
        case SPR_AVS_PDO:
            if (field == MODIFY_VOLTAGE)
            {
                *min_val = 9000;
                *max_val = 20000;
            }
            else
            {
                // 9-15V 区间对应 current_15v, 15-20V 区间对应 current_20v
                *min_val = 0;
                if (current_rdo.voltage_mV <= 15000)
                {
                    *max_val = pdo->PDO.SPR_AVS_PDO.current_15v_mA;
                }
                else
                {
                    *max_val = pdo->PDO.SPR_AVS_PDO.current_20v_mA;
                }
            }
            break;
    }
}

static uint16_t* get_modifiable_value_ptr(modify_field_type_t field)
{
    switch (field)
    {
        case MODIFY_VOLTAGE: return &current_rdo.voltage_mV;
        case MODIFY_CURRENT: return &current_rdo.current_mA;
        case MODIFY_POWER:   return &current_rdo.pdp_cW;
        default:             return NULL;
    }
}

static void clamp_spr_avs_current(modify_field_type_t field)
{
    // SPR_AVS: 电压跨越 15V 边界时，电流上限不同，需要钳位
    if (current_rdo.type != SPR_AVS_PDO || field != MODIFY_VOLTAGE)
        return;

    PDO_t *pdo = &current_pdo[current_rdo.pos];
    uint16_t i_max = (current_rdo.voltage_mV <= 15000)
                   ? pdo->PDO.SPR_AVS_PDO.current_15v_mA
                   : pdo->PDO.SPR_AVS_PDO.current_20v_mA;

    if (current_rdo.current_mA > i_max)
    {
        current_rdo.current_mA = i_max;
    }
}

static void clamp_epr_avs_pdp(modify_field_type_t field)
{
    // EPR_AVS: 确保 V × I ≤ PDP，超限时钳位未被调整的那一侧
    if (current_rdo.type != EPR_AVS_PDO)
        return;

    PDO_t *pdo = &current_pdo[current_rdo.pos];
    uint32_t pdp_mW = (uint32_t)pdo->PDO.EPR_AVS_PDO.PDP_cW * 10; // cW → mW
    uint32_t power_mW = (uint32_t)current_rdo.voltage_mV * current_rdo.current_mA / 1000; // mV × mA / 1000 = mW

    if (power_mW > pdp_mW && current_rdo.voltage_mV > 0 && current_rdo.current_mA > 0)
    {
        if (field == MODIFY_VOLTAGE)
        {
            // 调电压 → 钳位电流
            current_rdo.current_mA = (uint16_t)(pdp_mW * 1000 / current_rdo.voltage_mV);
        }
        else
        {
            // 调电流 → 钳位电压
            current_rdo.voltage_mV = (uint16_t)(pdp_mW * 1000 / current_rdo.current_mA);
            // 电压须对齐 100mV (AVS RDO 最低两位为零,步进 25mV×4)
            current_rdo.voltage_mV = (current_rdo.voltage_mV / 100) * 100;
        }
    }
}

static void apply_step(modify_field_type_t field, int16_t step)
{
    uint16_t *val = get_modifiable_value_ptr(field);
    if (!val) return;

    uint16_t min_val, max_val;
    get_value_bounds(field, &min_val, &max_val);

    int32_t new_val = (int32_t)*val + step;
    if (new_val < (int32_t)min_val) new_val = min_val;
    if (new_val > (int32_t)max_val) new_val = max_val;
    *val = (uint16_t)new_val;

    // 电压/电流变化后检查是否越限
    clamp_spr_avs_current(field);
    clamp_epr_avs_pdp(field);
}

// ============================================================================
// usb_pd_cmd_execute — 统一命令接口
//
// 所有输入源 (按键 / HID 命令 / UART 命令) 通过此接口控制 PD 请求参数。
// ============================================================================

static modify_field_type_t cmd_to_field(usb_pd_cmd_type_t type)
{
    switch (type)
    {
        case USB_PD_CMD_SET_VOLTAGE_MV:
        case USB_PD_CMD_ADJUST_VOLTAGE_MV:
            return MODIFY_VOLTAGE;
        case USB_PD_CMD_SET_CURRENT_MA:
        case USB_PD_CMD_ADJUST_CURRENT_MA:
            return MODIFY_CURRENT;
        case USB_PD_CMD_SET_POWER_CW:
        case USB_PD_CMD_ADJUST_POWER_CW:
            return MODIFY_POWER;
        default:
            return MODIFY_NONE;
    }
}

static bool is_field_valid_for_pdo(modify_field_type_t field, PDO_type_t type)
{
    switch (type)
    {
        case FPDO:
        case VPDO:
            return field == MODIFY_CURRENT;
        case BPDO:
            return field == MODIFY_POWER;
        case PPS_PDO:
        case EPR_AVS_PDO:
        case SPR_AVS_PDO:
            return field == MODIFY_VOLTAGE || field == MODIFY_CURRENT;
        default:
            return false;
    }
}

static void cmd_execute_immediate(const usb_pd_cmd_t *cmd)
{
    if (num_pdos == 0) return;

    // 校验: set/adjust 命令的字段必须与当前 PDO 类型匹配
    modify_field_type_t field = cmd_to_field(cmd->type);
    if (field != MODIFY_NONE && !is_field_valid_for_pdo(field, current_rdo.type))
        return;

    switch (cmd->type)
    {
        case USB_PD_CMD_SELECT_PDO:
        {
            uint8_t pos = GET_PDO_POS((uint8_t)cmd->value);
            if (pos < num_pdos && current_pdo[pos].raw != 0)
            {
                select_and_send_rdo(pos);
            }
            break;
        }
        case USB_PD_CMD_NEXT_PDO:
            select_and_send_rdo(find_valid_pdo_pos(+1));
            break;
        case USB_PD_CMD_PREV_PDO:
            select_and_send_rdo(find_valid_pdo_pos(-1));
            break;

        case USB_PD_CMD_SET_VOLTAGE_MV:
        {
            uint16_t min_val, max_val;
            get_value_bounds(MODIFY_VOLTAGE, &min_val, &max_val);
            int32_t v = cmd->value;
            if (v < (int32_t)min_val) v = min_val;
            if (v > (int32_t)max_val) v = max_val;
            current_rdo.voltage_mV = (uint16_t)v;
            clamp_spr_avs_current(MODIFY_VOLTAGE);
            clamp_epr_avs_pdp(MODIFY_VOLTAGE);
            send_current_rdo();
            break;
        }
        case USB_PD_CMD_SET_CURRENT_MA:
        {
            uint16_t min_val, max_val;
            get_value_bounds(MODIFY_CURRENT, &min_val, &max_val);
            int32_t v = cmd->value;
            if (v < (int32_t)min_val) v = min_val;
            if (v > (int32_t)max_val) v = max_val;
            current_rdo.current_mA = (uint16_t)v;
            clamp_epr_avs_pdp(MODIFY_CURRENT);
            send_current_rdo();
            break;
        }
        case USB_PD_CMD_SET_POWER_CW:
        {
            uint16_t min_val, max_val;
            get_value_bounds(MODIFY_POWER, &min_val, &max_val);
            int32_t v = cmd->value;
            if (v < (int32_t)min_val) v = min_val;
            if (v > (int32_t)max_val) v = max_val;
            current_rdo.pdp_cW = (uint16_t)v;
            send_current_rdo();
            break;
        }

        case USB_PD_CMD_ADJUST_VOLTAGE_MV:
            apply_step(MODIFY_VOLTAGE, (int16_t)cmd->value);
            send_current_rdo();
            break;
        case USB_PD_CMD_ADJUST_CURRENT_MA:
            apply_step(MODIFY_CURRENT, (int16_t)cmd->value);
            send_current_rdo();
            break;
        case USB_PD_CMD_ADJUST_POWER_CW:
            apply_step(MODIFY_POWER, (int16_t)cmd->value);
            send_current_rdo();
            break;
    }
}

void usb_pd_cmd_execute(const usb_pd_cmd_t *cmd)
{
    cmd_queue_push(cmd);
}

// ============================================================================
// key_forward_pd — 按键交互状态机
//
// KEY_MODE_SELECT_PDO:   左/右键切换PDO, 双击右键进入修改
// KEY_MODE_SELECT_FIELD: (PPS/AVS) 左键=电压, 右键=电流, 双击右键确认
// KEY_MODE_MODIFY_VALUE: 左键-小步进, 右键+小步进, 长按持续=大步进, 双击左键返回
// ============================================================================

static void key_forward_pd(void)
{
    key_event_type_t key_event;
    if (!key_buf_peek(&key_event))
    {
        return;
    }

    if (num_pdos == 0)
    {
        key_buf_pop();
        return;
    }

    switch (key_mode)
    {
        // ── 选择 PDO 模式 ──────────────────────────────────
        case KEY_MODE_SELECT_PDO:
        {
            switch (key_event)
            {
                case KEY_LEFT_CLICK:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_PREV_PDO, 0});
                    break;
                case KEY_RIGHT_CLICK:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_NEXT_PDO, 0});
                    break;
                case KEY_RIGHT_DOUBLE_CLICK:
                    key_buf_pop();
                    if (pdo_type_has_dual_fields(current_rdo.type))
                    {
                        // PPS/AVS: 需要先选择修改字段
                        key_mode = KEY_MODE_SELECT_FIELD;
                        modify_field = MODIFY_VOLTAGE; // 默认选中电压
                    }
                    else
                    {
                        // FPDO/VPDO → 直接修改电流, BPDO → 直接修改功率
                        key_mode = KEY_MODE_MODIFY_VALUE;
                        modify_field = (current_rdo.type == BPDO) ? MODIFY_POWER : MODIFY_CURRENT;
                    }
                    break;
                default:
                    key_buf_pop();
                    break;
            }
            break;
        }

        // ── 选择修改字段模式 (PPS/AVS) ─────────────────────
        case KEY_MODE_SELECT_FIELD:
        {
            switch (key_event)
            {
                case KEY_LEFT_CLICK:
                    key_buf_pop();
                    modify_field = MODIFY_VOLTAGE;
                    break;
                case KEY_RIGHT_CLICK:
                    key_buf_pop();
                    modify_field = MODIFY_CURRENT;
                    break;
                case KEY_RIGHT_DOUBLE_CLICK:
                    key_buf_pop();
                    // 确认选择, 进入修改值模式
                    key_mode = KEY_MODE_MODIFY_VALUE;
                    break;
                case KEY_LEFT_DOUBLE_CLICK:
                    key_buf_pop();
                    // 返回 PDO 选择模式
                    key_mode = KEY_MODE_SELECT_PDO;
                    modify_field = MODIFY_NONE;
                    break;
                default:
                    key_buf_pop();
                    break;
            }
            break;
        }

        // ── 修改值模式 ─────────────────────────────────────
        case KEY_MODE_MODIFY_VALUE:
        {
            uint16_t min_step, big_step;
            get_step_values(current_rdo.type, modify_field, &min_step, &big_step);
            usb_pd_cmd_type_t adj_cmd = (modify_field == MODIFY_VOLTAGE) ? USB_PD_CMD_ADJUST_VOLTAGE_MV
                                      : (modify_field == MODIFY_POWER)   ? USB_PD_CMD_ADJUST_POWER_CW
                                                                         : USB_PD_CMD_ADJUST_CURRENT_MA;

            switch (key_event)
            {
                case KEY_LEFT_CLICK:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){adj_cmd, -(int32_t)min_step});
                    break;
                case KEY_RIGHT_CLICK:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){adj_cmd, (int32_t)min_step});
                    break;
                case KEY_LEFT_LONG_PRESS:
                case KEY_LEFT_LONG_HOLD:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){adj_cmd, -(int32_t)big_step});
                    break;
                case KEY_RIGHT_LONG_PRESS:
                case KEY_RIGHT_LONG_HOLD:
                    key_buf_pop();
                    usb_pd_cmd_execute(&(usb_pd_cmd_t){adj_cmd, (int32_t)big_step});
                    break;
                case KEY_LEFT_LONG_RELEASE:
                case KEY_RIGHT_LONG_RELEASE:
                    key_buf_pop();
                    break;
                case KEY_LEFT_DOUBLE_CLICK:
                    key_buf_pop();
                    if (pdo_type_has_dual_fields(current_rdo.type))
                    {
                        // 返回字段选择
                        key_mode = KEY_MODE_SELECT_FIELD;
                    }
                    else
                    {
                        // 返回 PDO 选择
                        key_mode = KEY_MODE_SELECT_PDO;
                        modify_field = MODIFY_NONE;
                    }
                    break;
                default:
                    key_buf_pop();
                    break;
            }
            break;
        }
    }
}

void usb_pd_event_save_rx(uint8_t status, const uint8_t *data, uint16_t len)
{
    memset(current_data, 0, sizeof(current_data));
    uint8_t sop = status & BMC_AUX_Mask;
    if (status & IF_RX_RESET)
    {
        current_data[0] = (sop == PD_RX_SOP1_HRST) ? HARD_RESET : CABLE_RESET;
    }
    else
    {
        current_data[0] = (sop == PD_RX_SOP0)      ? SOP
                        : (sop == PD_RX_SOP1_HRST) ? SOP1
                        : (sop == PD_RX_SOP2_CRST) ? SOP2
                                                   : 0xFF;
    }
    
    memcpy(&current_data[1], data, len);
    event_status = PD_GOODCRC;
}

void usb_pd_event_save_connect_change(void)
{
    memset(current_pdo, 0, sizeof(current_pdo));
    memset(&current_rdo, 0, sizeof(current_rdo));
    memset(&current_msg_header, 0, sizeof(current_msg_header));
    memset(&last_msg_header, 0, sizeof(last_msg_header));
    memset(current_data, 0, sizeof(current_data));
    memset(last_ext_data, 0, sizeof(last_ext_data));
    event_status = PD_WAITING;
    epr_status = EPR_UNSUPPORTED;
    keep_alive_status = KEEP_ALIVE_NONE;
    key_mode = KEY_MODE_SELECT_PDO;
    modify_field = MODIFY_NONE;
    msg_id = 7;
    num_pdos = 0;
    cmd_queue_head = 0;
    cmd_queue_tail = 0;
}

void usb_pd_event_process_next(void)
{
    switch (event_status)
    {
        case PD_GOODCRC:
        {
            memcpy(&last_msg_header, &current_msg_header, sizeof(mix_Message_Header_t));
            memset(&current_msg_header, 0, sizeof(current_msg_header));

            uint16_t header = (current_data[2] << 8) | current_data[1];
            uint16_t if_ext_header = (current_data[4] << 8) | current_data[3];
            current_msg_header = parse_header(current_data[0], header, if_ext_header);

            if (current_msg_header.sop != SOP || (current_msg_header.msg_type == MSG_TYPE_GoodCRC && current_msg_header.num_objs == 0))
            {
                event_status = PD_WAITING;
                return;
            }
            else if (current_msg_header.sop == HARD_RESET || current_msg_header.sop == CABLE_RESET)
            {
                usb_pd_event_save_connect_change();
                return;
            }

            uint16_t goodcrc = current_goodcrc();
            usb_pd_phy_send_data((uint8_t *)&goodcrc, 2, UPD_SOP0);
            event_status = PD_REPLY;
            break;
        }
        case PD_REPLY:
        {
            process_msg();

            if (get_usb_pd_msg_priority() == REPLY_PRIORITY)
            {
                auto_reply();
            }
            else if (get_usb_pd_msg_priority() == HID_PRIORITY)
            {
                if (hid_rx_buf_has_pd())
                {
                    hid_forward_pd();
                }
            }

            event_status = PD_WAITING;
            break; 
        }
        case PD_WAITING:
        {
            if (epr_status == EPR_READY)
            {
                uint8_t request[USB_PD_DATA_MAX_LEN] = {0};
                uint16_t header = 0x1000 | MSG_TYPE_EPR_Mode;
                build_header(&header, true);

                uint32_t data_obj = 0x01000000;

                memcpy(&request[0], &header, 2);
                memcpy(&request[2], &data_obj, 4);

                epr_status = EPR_WAITING;
                usb_pd_phy_send_data(request, 6, UPD_SOP0);
            }

            if (keep_alive_status == KEEP_ALIVE_EPR && millis() - last_timestamp_ms > 375)
            {
                last_timestamp_ms = millis();
                uint8_t request[USB_PD_DATA_MAX_LEN] = {0};
                uint16_t header = 0x9000 | MSG_TYPE_Extended_Control;
                build_header(&header, true);

                uint16_t ext_header = 0x8002;
                uint16_t data_obj = 0x0300;

                memcpy(&request[0], &header, 2);
                memcpy(&request[2], &ext_header, 2);
                memcpy(&request[4], &data_obj, 2);

                usb_pd_phy_send_data(request, 6, UPD_SOP0);
            }
            else if (keep_alive_status == KEEP_ALIVE_PPS && millis() - last_timestamp_ms > 8000)
            {
                last_timestamp_ms = millis();
                uint8_t request[USB_PD_DATA_MAX_LEN] = {0};
                uint16_t header = 0x1000 | MSG_TYPE_Request;
                build_header(&header, true);

                uint32_t data_obj = 0;
                data_obj |= PDO_POS(current_rdo.pos) << 28;
                data_obj |= current_pdo[0].epr << 22;
                data_obj |= current_rdo.voltage_mV / 20 << 9;
                data_obj |= current_rdo.current_mA / 50;

                memcpy(&request[0], &header, 2);
                memcpy(&request[2], &data_obj, 4);

                usb_pd_phy_send_data(request, 6, UPD_SOP0);
            }

            if (hid_rx_buf_has_pd())
            {
                hid_forward_pd();
            }
            
            if (key_buf_has_data())
            {
                key_forward_pd();
            }

            usb_pd_cmd_t cmd;
            if (cmd_queue_pop(&cmd))
            {
                cmd_execute_immediate(&cmd);
            }
        }
    }

    if (current_rdo.type != PPS_PDO && keep_alive_status != KEEP_ALIVE_EPR)
    {
        keep_alive_status = KEEP_ALIVE_NONE;
    }
    else if (keep_alive_status == KEEP_ALIVE_EPR && epr_status != EPR_ON)
    {
        keep_alive_status = KEEP_ALIVE_NONE;
    }
}