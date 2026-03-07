#pragma once

#include "def.h"

// ============================================================================
// 按键参数配置
// ============================================================================

#define KEY_DEBOUNCE_MS       20    // 消抖时间 (ms)
#define KEY_LONG_PRESS_MS     500   // 长按判定时间 (ms)
#define KEY_LONG_HOLD_MS      200   // 长按持续信号间隔 (ms)
#define KEY_DOUBLE_CLICK_MS   150   // 双击间隔时间 (ms)
#define KEY_BOTH_PRESS_MS     100   // 同时按下判定窗口 (ms)
#define KEY_BUFFER_SIZE       16    // 按键事件环形缓冲区大小

// ============================================================================
// 按键事件类型
// ============================================================================

typedef enum key_event_type_t
{
    KEY_NONE = 0,
    KEY_LEFT_CLICK,          // 左键单击
    KEY_RIGHT_CLICK,         // 右键单击
    KEY_LEFT_DOUBLE_CLICK,   // 左键双击
    KEY_RIGHT_DOUBLE_CLICK,  // 右键双击
    KEY_LEFT_LONG_PRESS,     // 左键长按
    KEY_LEFT_LONG_HOLD,      // 左键长按持续信号
    KEY_LEFT_LONG_RELEASE,   // 左键长按松开
    KEY_RIGHT_LONG_PRESS,    // 右键长按
    KEY_RIGHT_LONG_HOLD,     // 右键长按持续信号
    KEY_RIGHT_LONG_RELEASE,  // 右键长按松开
    KEY_BOTH_PRESS,          // 左右键同时按下
} key_event_type_t;

// ============================================================================
// 按键事件环形缓冲区
// ============================================================================

typedef struct key_event_buf_t
{
    volatile uint32_t overflow_count;          // 溢出计数
    volatile uint16_t head;                    // 写指针
    volatile uint16_t tail;                    // 读指针
    key_event_type_t events[KEY_BUFFER_SIZE];       // 事件数组
} key_event_buf_t;

// ============================================================================
// 按键状态机
// ============================================================================

typedef enum key_state_type_t
{
    KEY_STATE_IDLE = 0,        // 空闲
    KEY_STATE_DEBOUNCE,        // 消抖确认
    KEY_STATE_PRESSED,         // 已按下
    KEY_STATE_WAIT_RELEASE,    // 等待释放（长按已触发）
    KEY_STATE_WAIT_2ND_CLICK,  // 等待第二次点击（双击检测）
    KEY_STATE_DEBOUNCE_2ND,    // 第二次点击消抖
} key_state_type_t;

typedef struct key_fsm_t
{
    key_state_type_t state;
    uint32_t    press_time;    // 按下时刻
    uint32_t    release_time;  // 释放时刻
    uint32_t    hold_time;     // 上次 hold 信号时刻
    bool        is_long_press; // 是否为长按进入的等待释放
} key_fsm_t;

// ============================================================================
// 同时按下回调（不经过环形缓冲区，直接在 scan 中调用）
// ============================================================================

typedef void (*key_both_press_cb_t)(void);
void keypad_set_both_press_callback(key_both_press_cb_t cb);

// ============================================================================
// 初始化与扫描
// ============================================================================

void keypad_init(void);
void keypad_scan(void);

// ============================================================================
// 环形缓冲区接口
// ============================================================================

bool key_buf_is_full(void);
bool key_buf_has_data(void);
void key_buf_clear(void);
uint32_t key_buf_get_overflow_count(void);
uint32_t key_buf_get_count(void);

void key_buf_pop(void);
bool key_buf_peek(key_event_type_t *event);
