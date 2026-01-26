#include "backlight.h"
#include "reg.h"

#include <hardware/pwm.h>
#include <pico/stdlib.h>

void backlight_sync(void)
{
#ifdef BACKLIGHT_INVERT
	uint8_t inverted = 255 - reg_get_value(REG_ID_BKL);
	pwm_set_gpio_level(PIN_BKL, inverted * 0x80);
#else
	pwm_set_gpio_level(PIN_BKL, reg_get_value(REG_ID_BKL) * 0x80);
#endif
}

void backlight_init(void)
{
	gpio_set_function(PIN_BKL, GPIO_FUNC_PWM);

	const uint slice_num = pwm_gpio_to_slice_num(PIN_BKL);

	pwm_config config = pwm_get_default_config();
	pwm_init(slice_num, &config, true);

	backlight_sync();
}
