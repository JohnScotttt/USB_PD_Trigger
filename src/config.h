// ========================
//         引脚配置        
// ------------------------
//     VBUS_ADC: PA0  *
//      VBUS_EN: PA1
//    CC1_RA_EN: PB1
//    CC2_RA_EN: PB0
//        CC_EN: PB12 *
//         SW_L: PB3
//         SW_R: PB11
//     VBUS_CC1: PC14 *
//     VBUS_CC2: PC15 *
//        PC_DP: PC17 *
//        PC_DM: PC16 *
//      UART_TX: PA2  *
//      UART_RX: PA3  *
// ========================

// =================================================================
//                        Data Packet Formats                       
// -----------------------------------------------------------------
// |              Field | Full data | Standard data | Minimal data |
// |        Header (2B) |    0x_0FF |        0x_1FF |       0x_2FF |
// |          Time (4B) |         ✓ |             ✓ |            - |
// |       Counter (4B) |         ✓ |             ✓ |            - |
// |   Full Length (1B) |    ✓ (2B) |             ✓ |            ✓ |
// |    Raw Length (2B) |         ✓ |             - |            - |
// |     Data Type (1B) |         ✓ |             ✓ |            ✓ |
// |               Data |         ✓ |             ✓ |            ✓ |
// |  Raw Encoding (1B) |         ✓ |             - |            - |
// |                Raw |         ✓ |             - |            - |
// =================================================================
//
// Header: 0x[ST]FF, where S = Source, T = packet Type
//   Source: A = Device, 5 = Host
//   Type:   0 = Full,   1 = Standard, 2 = Minimal
//   e.g. 0xA0FF = Device Full, 0x51FF = Host Standard
//
// Full Length: The length from the byte immediately following the "Full Length" field to the end of the packet.
// Raw Length: The length of the "Raw" field.
//
// ======================      ======================      ==========================      ======================
//    Data Type Codes                SOP* Codes             Device System Data Codes         Raw Encoding Codes
// ----------------------      ----------------------      --------------------------      ----------------------
// |  Hex | Description |      |  Hex | Description |      |  Hex |     Description |      |  Hex | Description |
// | 0x00 |      USB PD |      | 0x00 |         SOP |      | 0x11 |      Attach CC1 |      | 0x00 |     HEX Raw |
// | 0x01 |      System |      | 0x01 |        SOP' |      | 0x21 |      Attach CC2 |      | 0x01 |       ASCII |
// | 0x02 |     Message |      | 0x02 |       SOP'' |      | 0x12 |      Detach CC1 |      | 0x02 |       UTF-8 |
// ======================      | 0x03 |  SOP'_DEBUG |      | 0x22 |      Detach CC2 |      ======================
//                             | 0x04 | SOP''_DEBUG |      | 0x31 |         VBUS On |
//                             | 0x05 |  Hard_Reset |      | 0x32 |        VBUS Off |
//                             | 0x06 | Cable_Reset |      | 0xEE |       CRC Error |
//                             ======================      ==========================
//
// ==============================================================================
//                             Host System Data Codes
// ------------------------------------------------------------------------------
//      Data = [Category (1B)] [Command (1B)] [Value (optional) (int32, 4B)]
//
// ======================      ==============================      ===========================
//     Cmd Categories                PD Cmd Codes (0x00)               SYS Cmd Codes (0x01)
// ----------------------      ------------------------------      ---------------------------
// |  Hex | Description |      |  Hex |         Description |      |  Hex |      Description |
// | 0x00 |      USB PD |      | 0x00 |          SELECT_PDO |      | 0x00 |       MCU Reboot |
// | 0x01 |      System |      | 0x01 |            NEXT_PDO |      | 0x01 |    USB PD Reboot |
// ======================      | 0x02 |            PREV_PDO |      | 0x02 |       Get Status |
//                             | 0x03 |      SET_VOLTAGE_MV |      | 0x11 |          VBUS On |
//                             | 0x04 |      SET_CURRENT_MA |      | 0x12 |         VBUS Off |
//                             | 0x05 |        SET_POWER_CW |      | 0x21 |     HID Priority |
//                             | 0x06 | INCREASE_VOLTAGE_MV |      | 0x22 |   Reply Priority |
//                             | 0x07 | DECREASE_VOLTAGE_MV |      | 0x31 |         SPR Mode |
//                             | 0x08 | INCREASE_CURRENT_MA |      | 0x32 |         EPR Mode |
//                             | 0x09 | DECREASE_CURRENT_MA |      | 0x33 |        Prop Mode |
//                             | 0x0A |   INCREASE_POWER_CW |      | 0x41 |   HID Report Std |
//                             | 0x0B |   DECREASE_POWER_CW |      | 0x42 |  HID Report Mini |
//                             ==============================      | 0x51 |   VBUS Always On |
//                                                                 | 0x52 |  VBUS Always Off |
//                                                                 | 0x53 |        VBUS Hold |
//                                                                 | 0x61 |  Trigger Hold On |
//                                                                 | 0x62 | Trigger Hold Off |
//                                                                 ===========================



#pragma once

#ifndef CONFIG_HW_VERSION_MAJOR
    #define CONFIG_HW_VERSION_MAJOR 1
#endif

#ifndef CONFIG_HW_VERSION_MINOR
    #define CONFIG_HW_VERSION_MINOR 6
#endif

#ifndef CONFIG_FW_VERSION_MAJOR
    #define CONFIG_FW_VERSION_MAJOR 0
#endif

#ifndef CONFIG_FW_VERSION_MINOR
    #define CONFIG_FW_VERSION_MINOR 2
#endif

#ifndef CONFIG_FW_VERSION_PATCH
    #define CONFIG_FW_VERSION_PATCH 2
#endif