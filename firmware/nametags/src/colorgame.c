#include <logging/log.h>
LOG_MODULE_REGISTER(colorgame);
#include <stdlib.h>

#include <bluetooth/bluetooth.h>

#include "drivers/neopixels.h"
#include "drivers/vibration_motor.h"
#include "colorgame.h"


#define PACKET_VENDOR_ID (0x1337)
#define RSSI_THRESHOLD (-60)
#define BLAST_TIMEOUT (5000)

#define COLOR_ID 4

static u32_t last_blast;
static struct nvs_fs * g_fs;

struct __attribute__((__packed__)) colorgame_packet
{
  u16_t vendor_id;
  u8_t r;
  u8_t g;
  u8_t b;
};

static const struct led_rgb colors[] = {
  //red
  {
    .r = 0x20,
    .g = 0,
    .b = 0
  },
  //green
  {
    .r = 0,
    .g = 0x20,
    .b = 0
  },
  //blue
  {
    .r = 0,
    .g = 0,
    .b = 0x20
  },
  // yellow
  {
    .r = 0x20,
    .g = 0x20,
    .b = 0
  },
  //purple
    {
    .r = 0x20,
    .g = 0,
    .b = 0x20
  },
  //cyan
    {
    .r = 0,
    .g = 0x20,
    .b = 0x20
  },

};


static struct led_rgb my_color;

void colorgame_packet_handler(void *buf, u8_t len, s8_t rssi)
{
  struct colorgame_packet *packet = buf;
  if (len >= sizeof(packet) && packet->vendor_id == PACKET_VENDOR_ID)
  {
    LOG_INF("Incoming blast, RSSI=%d - %s", rssi, rssi >= RSSI_THRESHOLD ? "accepted" : "rejected");
    if (rssi >= RSSI_THRESHOLD)
    {
      my_color.r = packet->r;
      my_color.g = packet->g;
      my_color.b = packet->b;
      nvs_delete(g_fs, COLOR_ID);
      nvs_write(g_fs, COLOR_ID, &my_color , sizeof(my_color));
      pulse_vibration_motor(400);
      k_sleep(500);
      write_neopixels_all(my_color, true);
    }
  }
}

void colorgame_blast(bool is_advertising)
{
  const struct colorgame_packet data_packet = {
      .vendor_id = PACKET_VENDOR_ID,
      .r = my_color.r,
      .g = my_color.g,
      .b = my_color.b,
  };

  const struct bt_data ad[] = {
      BT_DATA(BT_DATA_MANUFACTURER_DATA, &data_packet, sizeof(data_packet)),
  };

  s32_t delta = k_uptime_get_32() - last_blast;
  if (delta < BLAST_TIMEOUT) {
    return;
  }

  if (is_advertising) {
    bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
  } else {
    bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
  }

  last_blast = k_uptime_get_32();
}

void colorgame_init(struct nvs_fs * fs)
{
  last_blast = k_uptime_get_32();
  g_fs = fs;
  int rc = nvs_read(g_fs, COLOR_ID, &my_color , sizeof(my_color));
  if(rc <= 0) {
      u32_t index = sys_rand32_get() % (sizeof(colors) / sizeof(colors[0]));
      my_color = colors[index];
      nvs_write(g_fs, COLOR_ID, &my_color , sizeof(my_color));
  }

}