/*
 * Badge Hardware Test App
 * Author: Uri Shaked
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr.h>
#include <device.h>
#include <gpio.h>
#include <stdio.h>
#include <led_strip.h>
#include <display/cfb.h>
#include <misc/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define LED_PORT LED0_GPIO_CONTROLLER
#define LED	LED0_GPIO_PIN

#define STRIP_NUM_LEDS 4
#define STRIP_DEV_NAME DT_WORLDSEMI_WS2812_0_LABEL

#define SLEEP_TIME 	500

#define DISPLAY_DRIVER		"DISPLAY"

static const struct led_rgb red = { .r = 0x20, .g = 0x00, .b = 0x00 };
static const struct led_rgb green = { .r = 0x0, .g = 0x20, .b = 0x0 };
static const struct led_rgb blue = { .r = 0x0, .g = 0x0, .b = 0x20 };
static const struct led_rgb purple = { .r = 0x10, .g = 0x0, .b = 0x20 };

static struct led_rgb strip_colors[STRIP_NUM_LEDS];

static uint16_t counter = 0;
static struct device *display;
static bool bluetooth_ready = false;
static const bt_addr_le_t *remote_device = NULL;

static char *bluetooth_mac_to_str(const bt_addr_le_t* addr) {
	static char buf[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, buf, sizeof(buf));
	char *space = strchr(buf, ' ');
	if (space) {
		*space = 0;
	}
	return buf;
}

void update_display() {
	char buf[64];
	cfb_framebuffer_clear(display, true);
	cfb_print(display, "*** Badge Self-Test ***", 0, 0);
	if (bluetooth_ready) {
		sprintf(buf, "Name: %s", bt_get_name());
		cfb_print(display, buf, 0, 16);

		bt_addr_le_t addr;
		size_t count = 1;
		bt_id_get(&addr, &count);
		if (count) {
			sprintf(buf, "Mac:  %s", bluetooth_mac_to_str(&addr));
			cfb_print(display, buf, 0, 32);
		}

		if (remote_device) {
			sprintf(buf, "Connected! %s", bluetooth_mac_to_str(remote_device));
			cfb_print(display, buf, 0, 48);
		}
	}

	// TODO display MP3 status
	// TODO display Flash status

	char *status = "\\|/-";
	sprintf(buf, "%c", status[counter % sizeof(status)]);
	cfb_print(display, buf, 0, 96);

	cfb_framebuffer_finalize(display);
}

/* Bluetooth */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static void connected(struct bt_conn *conn, u8_t err) {
	if (err) {
		LOG_ERR("Failed to connect to %s (%u)\n", bluetooth_mac_to_str(bt_conn_get_dst(conn)), err);
		return;
	}

	remote_device = bt_conn_get_dst(conn);
	LOG_INF("Connected %s\n", bluetooth_mac_to_str(remote_device));
}

static void disconnected(struct bt_conn *conn, u8_t reason) {
	LOG_INF("Disconnected from %s (reason %u)\n", bluetooth_mac_to_str(remote_device), reason);
	remote_device = NULL;
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err) {
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started\n");
	bluetooth_ready = true;
}

void main(void) {
	LOG_INF("Starting app...\n");

	// LED
	struct device *gpio = device_get_binding(LED_PORT);
	gpio_pin_configure(gpio, LED, GPIO_DIR_OUT);

	// Neopixels
	struct device *strip = device_get_binding(STRIP_DEV_NAME);
	if (!strip) {
		LOG_ERR("LED strip device %s not found", STRIP_DEV_NAME);
		return;
	}

	strip_colors[0] = purple;
	strip_colors[1] = purple;
	strip_colors[2] = purple;
	strip_colors[3] = purple;
	led_strip_update_rgb(strip, strip_colors, STRIP_NUM_LEDS);
	
	// Display
	display = device_get_binding(DISPLAY_DRIVER);
	if (display == NULL) {
		LOG_ERR("Display device not found");
		return;
	}

	if (display_set_pixel_format(display, PIXEL_FORMAT_MONO10) != 0) {
		LOG_ERR("Failed to set required pixel format");
		return;
	}

	if (cfb_framebuffer_init(display)) {
		LOG_ERR("Framebuffer initialization failed!");
		return;
	}

	cfb_framebuffer_clear(display, true);

	display_blanking_off(display);

	u16_t rows;
	u8_t ppt;
	u8_t font_width;
	u8_t font_height;
	rows = cfb_get_display_parameter(display, CFB_DISPLAY_ROWS);
	ppt = cfb_get_display_parameter(display, CFB_DISPLAY_PPT);
	for (int idx = 0; idx < 42; idx++) {
		if (cfb_get_font_size(display, idx, &font_width, &font_height)) {
			break;
		}
		cfb_framebuffer_set_font(display, idx);
		LOG_INF("font width %d, font height %d\n",
		       font_width, font_height);
	}

	update_display();

	// Bluetooth
	int err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)\n", err);
		return;
	}	

	while (1) {
		update_display();
		counter++;

		gpio_pin_write(gpio, LED, 0);
		strip_colors[0] = red;
		strip_colors[1] = green;
		strip_colors[2] = purple;
		strip_colors[3] = blue;
		led_strip_update_rgb(strip, strip_colors, STRIP_NUM_LEDS);
		k_sleep(SLEEP_TIME);

		update_display();
		counter++;

		gpio_pin_write(gpio, LED, 1);
		strip_colors[0] = green;
		strip_colors[1] = red;
		strip_colors[2] = blue;
		strip_colors[3] = purple;
		led_strip_update_rgb(strip, strip_colors, STRIP_NUM_LEDS);
		k_sleep(SLEEP_TIME);
	}
}