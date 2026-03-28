#include "keypad.h"
#include "delay.h"

// ============================================================================
// Ring Buffer (参考 logger.c 的 log_buf_* 实现)
// ============================================================================

static key_event_buf_t key_buffer = {0};

#define KEY_RING_INC(x) (((x) + 1) % KEY_BUFFER_SIZE)

bool key_buf_is_full(void)
{
    return (KEY_RING_INC(key_buffer.head) == key_buffer.tail);
}

bool key_buf_has_data(void)
{
    return (key_buffer.head != key_buffer.tail);
}

void key_buf_clear(void)
{
    key_buffer.head = 0;
    key_buffer.tail = 0;
}

uint32_t key_buf_get_overflow_count(void)
{
    return key_buffer.overflow_count;
}

uint32_t key_buf_get_count(void)
{
    return (key_buffer.head >= key_buffer.tail)
               ? (key_buffer.head - key_buffer.tail)
               : (KEY_BUFFER_SIZE - key_buffer.tail + key_buffer.head);
}

static void key_buf_push(key_event_type_t event)
{
    if (event == KEY_NONE)
    {
        return;
    }

    // 检查是否已满
    if (key_buf_is_full())
    {
        key_buffer.overflow_count++;
        // 覆盖最旧的事件，移动尾指针
        key_buffer.tail = KEY_RING_INC(key_buffer.tail);
    }

    // 写入事件
    key_buffer.events[key_buffer.head] = event;

    // 更新头指针
    key_buffer.head = KEY_RING_INC(key_buffer.head);
}

void key_buf_pop(void)
{
    // 检查是否为空
    if (!key_buf_has_data())
    {
        return;
    }

    // 更新尾指针
    key_buffer.tail = KEY_RING_INC(key_buffer.tail);
}

bool key_buf_peek(key_event_type_t *event)
{
    if (event == NULL)
    {
        return false;
    }

    // 检查是否为空
    if (!key_buf_has_data())
    {
        return false;
    }

    // 返回队首事件
    *event = key_buffer.events[key_buffer.tail];

    return true;
}

// ============================================================================
// GPIO 读取
// ============================================================================

static inline bool read_sw_l(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, SW_L_GPIO_PIN) == Bit_RESET); // 低电平有效
}

static inline bool read_sw_r(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, SW_R_GPIO_PIN) == Bit_RESET); // 低电平有效
}

// ============================================================================
// 按键状态机
// ============================================================================

static key_fsm_t fsm_left  = {0};
static key_fsm_t fsm_right = {0};

// "同时按下"检测标志
static bool both_press_fired = false;

// 同时按下回调
static key_both_press_cb_t both_press_cb = NULL;

void keypad_set_both_press_callback(key_both_press_cb_t cb)
{
    both_press_cb = cb;
}

// ============================================================================
// 单个按键状态机更新
// ============================================================================

static void key_fsm_update(key_fsm_t *fsm, bool pressed, uint32_t now,
                           key_event_type_t click_evt, key_event_type_t dbl_evt,
                           key_event_type_t long_evt, key_event_type_t long_hold_evt,
                           key_event_type_t long_release_evt)
{
    switch (fsm->state)
    {
    case KEY_STATE_IDLE:
        if (pressed)
        {
            fsm->press_time = now;
            fsm->state = KEY_STATE_DEBOUNCE;
        }
        break;

    case KEY_STATE_DEBOUNCE:
        if (!pressed)
        {
            // 抖动，回到空闲
            fsm->state = KEY_STATE_IDLE;
        }
        else if ((now - fsm->press_time) >= KEY_DEBOUNCE_MS)
        {
            fsm->state = KEY_STATE_PRESSED;
        }
        break;

    case KEY_STATE_PRESSED:
        if (!pressed)
        {
            // 释放：记录释放时刻，进入双击等待
            fsm->release_time = now;
            fsm->state = KEY_STATE_WAIT_2ND_CLICK;
        }
        else if ((now - fsm->press_time) >= KEY_LONG_PRESS_MS)
        {
            // 长按触发
            key_buf_push(long_evt);
            fsm->is_long_press = true;
            fsm->hold_time = now;
            fsm->state = KEY_STATE_WAIT_RELEASE;
        }
        break;

    case KEY_STATE_WAIT_RELEASE:
        if (!pressed)
        {
            if (fsm->is_long_press)
            {
                key_buf_push(long_release_evt);
                fsm->is_long_press = false;
            }
            fsm->state = KEY_STATE_IDLE;
        }
        else if (fsm->is_long_press && (now - fsm->hold_time) >= KEY_LONG_HOLD_MS)
        {
            // 长按持续信号
            fsm->hold_time = now;
            key_buf_push(long_hold_evt);
        }
        break;

    case KEY_STATE_WAIT_2ND_CLICK:
        if (pressed)
        {
            fsm->press_time = now;
            fsm->state = KEY_STATE_DEBOUNCE_2ND;
        }
        else if ((now - fsm->release_time) >= KEY_DOUBLE_CLICK_MS)
        {
            // 超时未再次按下 → 单击
            key_buf_push(click_evt);
            fsm->state = KEY_STATE_IDLE;
        }
        break;

    case KEY_STATE_DEBOUNCE_2ND:
        if (!pressed)
        {
            // 抖动，回到等待双击
            fsm->state = KEY_STATE_WAIT_2ND_CLICK;
        }
        else if ((now - fsm->press_time) >= KEY_DEBOUNCE_MS)
        {
            // 第二次按下确认 → 双击
            key_buf_push(dbl_evt);
            fsm->is_long_press = false;
            fsm->state = KEY_STATE_WAIT_RELEASE;
        }
        break;

    default:
        fsm->state = KEY_STATE_IDLE;
        break;
    }
}

// ============================================================================
// 初始化
// ============================================================================

void keypad_init(void)
{
    RCC_APB2PeriphClockCmd(SW_GPIO_CLK, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin   = SW_L_GPIO_PIN | SW_R_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SW_GPIO_PORT, &GPIO_InitStructure);

    memset(&fsm_left,  0, sizeof(key_fsm_t));
    memset(&fsm_right, 0, sizeof(key_fsm_t));
    both_press_fired = false;

    key_buf_clear();
}

// ============================================================================
// 按键扫描（需在主循环中周期性调用）
// ============================================================================

void keypad_scan(void)
{
    uint32_t now = millis();
    bool l_pressed = read_sw_l();
    bool r_pressed = read_sw_r();

    // —— 同时按下检测 ——
    if (l_pressed && r_pressed)
    {
        if (!both_press_fired)
        {
            // 两个键都已经过了消抖才算有效同时按下
            bool l_stable = (fsm_left.state  >= KEY_STATE_PRESSED) ||
                            (fsm_left.state  == KEY_STATE_DEBOUNCE &&
                             (now - fsm_left.press_time) >= KEY_DEBOUNCE_MS);
            bool r_stable = (fsm_right.state >= KEY_STATE_PRESSED) ||
                            (fsm_right.state == KEY_STATE_DEBOUNCE &&
                             (now - fsm_right.press_time) >= KEY_DEBOUNCE_MS);

            // 两键按下时间差必须在窗口内
            uint32_t l_press = fsm_left.press_time;
            uint32_t r_press = fsm_right.press_time;
            uint32_t diff = (l_press >= r_press) ? (l_press - r_press) : (r_press - l_press);
            bool in_window = (diff <= KEY_BOTH_PRESS_MS);

            if (l_stable && r_stable && in_window)
            {
                if (both_press_cb)
                {
                    both_press_cb();
                }
                both_press_fired = true;

                // 重置两个状态机，避免再触发单键事件
                fsm_left.state  = KEY_STATE_WAIT_RELEASE;
                fsm_right.state = KEY_STATE_WAIT_RELEASE;
                return;
            }
        }
    }
    else
    {
        // 只要有一个松开就允许下一次同时按下检测
        if (!l_pressed && !r_pressed)
        {
            both_press_fired = false;
        }
    }

    // —— 独立按键状态机 ——
    if (!both_press_fired || fsm_left.state != KEY_STATE_WAIT_RELEASE)
    {
        key_fsm_update(&fsm_left, l_pressed, now,
                       KEY_LEFT_CLICK, KEY_LEFT_DOUBLE_CLICK,
                       KEY_LEFT_LONG_PRESS, KEY_LEFT_LONG_HOLD, KEY_LEFT_LONG_RELEASE);
    }
    else if (!l_pressed)
    {
        fsm_left.state = KEY_STATE_IDLE;
    }

    if (!both_press_fired || fsm_right.state != KEY_STATE_WAIT_RELEASE)
    {
        key_fsm_update(&fsm_right, r_pressed, now,
                       KEY_RIGHT_CLICK, KEY_RIGHT_DOUBLE_CLICK,
                       KEY_RIGHT_LONG_PRESS, KEY_RIGHT_LONG_HOLD, KEY_RIGHT_LONG_RELEASE);
    }
    else if (!r_pressed)
    {
        fsm_right.state = KEY_STATE_IDLE;
    }
}
