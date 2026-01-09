#include "interrupt.h"

#include "app_config.h"
#include "gpioexp.h"
#include "keyboard.h"
#include "reg.h"
#include "touchpad.h"

#include <pico/stdlib.h>
#include <hardware/sync.h>

// Non-blocking interrupt pulse generation
// Instead of busy_wait_ms which blocks in interrupt context,
// we use an alarm to release the interrupt pin after the pulse duration
static struct
{
	alarm_id_t pending_alarm;
	volatile bool int_pin_low;
} self;

static int64_t int_release_alarm_cb(alarm_id_t id, void *user_data)
{
	(void)id;
	(void)user_data;

	gpio_put(PIN_INT, 1);
	self.int_pin_low = false;
	self.pending_alarm = 0;

	return 0; // don't reschedule
}

// Non-blocking interrupt pulse - pulls INT low and schedules release
static void trigger_int_pulse(void)
{
	// If interrupt pin is already low, extend the pulse by cancelling and rescheduling
	if (self.pending_alarm != 0) {
		cancel_alarm(self.pending_alarm);
		self.pending_alarm = 0;
	}

	gpio_put(PIN_INT, 0);
	self.int_pin_low = true;

	// Schedule the release - use microseconds for more precision
	// REG_ID_IND is in milliseconds, convert to microseconds
	const uint32_t pulse_us = reg_get_value(REG_ID_IND) * 1000;
	self.pending_alarm = add_alarm_in_us(pulse_us, int_release_alarm_cb, NULL, true);
}

static void key_cb(char key, enum key_state state)
{
	(void)key;
	(void)state;

	if (!reg_is_bit_set(REG_ID_CFG, CFG_KEY_INT))
		return;

	reg_set_bit(REG_ID_INT, INT_KEY);

	trigger_int_pulse();
}
static struct key_callback key_callback = { .func = key_cb };

static void key_lock_cb(bool caps_changed, bool num_changed)
{
	bool do_int = false;

	if (caps_changed && reg_is_bit_set(REG_ID_CFG, CFG_CAPSLOCK_INT)) {
		reg_set_bit(REG_ID_INT, INT_CAPSLOCK);
		do_int = true;
	}

	if (num_changed && reg_is_bit_set(REG_ID_CFG, CFG_NUMLOCK_INT)) {
		reg_set_bit(REG_ID_INT, INT_NUMLOCK);
		do_int = true;
	}

	if (do_int) {
		trigger_int_pulse();
	}
}
static struct key_lock_callback key_lock_callback = { .func = key_lock_cb };

static void touch_cb(int8_t x, int8_t y)
{
	(void)x;
	(void)y;

	if (!reg_is_bit_set(REG_ID_CF2, CF2_TOUCH_INT))
		return;

	reg_set_bit(REG_ID_INT, INT_TOUCH);

	trigger_int_pulse();
}
static struct touch_callback touch_callback = { .func = touch_cb };

static void gpioexp_cb(uint8_t gpio, uint8_t gpio_idx)
{
	(void)gpio;

	if (!reg_is_bit_set(REG_ID_GIC, (1 << gpio_idx)))
		return;

	reg_set_bit(REG_ID_INT, INT_GPIO);
	reg_set_bit(REG_ID_GIN, (1 << gpio_idx));

	trigger_int_pulse();
}
static struct gpioexp_callback gpioexp_callback = { .func = gpioexp_cb };

void interrupt_init(void)
{
	gpio_init(PIN_INT);
	gpio_set_dir(PIN_INT, GPIO_OUT);
	gpio_pull_up(PIN_INT);
	gpio_put(PIN_INT, true);

	keyboard_add_key_callback(&key_callback);
	keyboard_add_lock_callback(&key_lock_callback);

	touchpad_add_touch_callback(&touch_callback);

	gpioexp_add_int_callback(&gpioexp_callback);
}
