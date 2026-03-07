#include "def.h"

// ADC oversampling configuration
#define ADC_OVERSAMPLE_BITS 4                                                    // oversample 位数（要增加的位数）
#define ADC_BASE_BITS       12                                                   // adc 原始分辨率
#define ADC_CHANNEL_COUNT   1                                                    // adc 通道数
#define ADC_SAMPLE_COUNT    (1U << (2 * ADC_OVERSAMPLE_BITS))                    // adc 采样数
#define ADC_BUFFER_SIZE     (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)               // adc buffer 大小
#define ADC_MAX_VALUE       ((1U << (ADC_BASE_BITS + ADC_OVERSAMPLE_BITS)) - 1)  // adc 最大值

// ADC low-pass filter coefficient (0.0~1.0), 256 represents 1.0
#define ADC_LPF_ENABLE    0
#define ADC_LPF_ALPHA_NUM 32
#define ADC_LPF_ALPHA_DEN 256

// Calibration parameters
#define VBUS_CAL_ENABLE 0      // Enable/disable calibration
#define VDD_VOLTAGE     3300   // ADC reference voltage (mV)

#define VBUS_DIV_R1 150000
#define VBUS_DIV_R2 10000

// Calibration point data structure
typedef struct {
    float actual;    // Actual voltage value (mV)
    float measured;  // ADC measured value (mV)
} CalibrationPoint;

static const CalibrationPoint VBUS_CAL_POINTS[] = {
    {0, 0},
    {20000, 20000},
};

static const uint16_t VBUS_CAL_POINTS_COUNT = sizeof(VBUS_CAL_POINTS) / sizeof(CalibrationPoint);
static const float VBUS_CONVERT_SCALE = (float)VDD_VOLTAGE / (float)ADC_MAX_VALUE * (((float)VBUS_DIV_R1 + (float)VBUS_DIV_R2) / (float)VBUS_DIV_R2);

void in_adc_init();
uint32_t adc_get_in_raw(void);
uint16_t adc_in_raw_to_vbus_in_mv(uint32_t adc_raw);
uint16_t adc_get_vbus_in_mv(void);
// uint16_t adc_get_vdd_mv(void);