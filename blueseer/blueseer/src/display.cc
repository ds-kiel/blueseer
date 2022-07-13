#include "module_config.h"

#ifdef WITH_DISPLAY
#include "display.h"
#include <drivers/display.h>
#include <lvgl.h>

//display
const struct device *display_dev;
static lv_obj_t *text;
static bool display_initalized;

	/*
	setup display and clear screen
	*/
	void initDisplay()
	{
		display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);

		if (display_dev == NULL) {
			#ifdef WITH_LOGGING
			printk("device not found.\n");
			#endif /* WITH_LOGGING */
			return;
		}

		if (IS_ENABLED(CONFIG_LVGL_POINTER_KSCAN)) {
			lv_obj_t *hello_world_button;

			hello_world_button = lv_btn_create(lv_scr_act(), NULL);
			lv_obj_align(hello_world_button, NULL, LV_ALIGN_CENTER, 0, 0);
			lv_btn_set_fit(hello_world_button, LV_FIT_TIGHT);
			text = lv_label_create(hello_world_button, NULL);
		} else {
			text = lv_label_create(lv_scr_act(), NULL);
		}

		lv_label_set_text(text, "");
		lv_obj_align(text, NULL, LV_ALIGN_CENTER, 0, 0);

		lv_task_handler();
		display_blanking_off(display_dev);

		display_initalized = true;
	}

	/*
	set text shown on display to txt
	*/
	void setDisplayText(char *txt)
	{
		if (text != NULL && display_initalized) {
			lv_label_set_text(text, txt);
			lv_obj_align(text, NULL, LV_ALIGN_CENTER, 0, 0);

			lv_task_handler();
		}
	}


#endif /* WITH_DISPLAY */