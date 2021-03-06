/// <reference types="web-bluetooth" />

import { Injectable } from '@angular/core';
import { displayWidth, displayHeight, badgePrefix } from './badge-consts';

const DISPLAY_SERVICE = 0xfeef;
const DISPLAY_CHARACTERISTIC = 0xfeee;
const BATTERY_VOLTAGE_CHARACTERISTIC = '34ad4113-1259-ff85-4d44-1fb0c6d0249a';

@Injectable({
  providedIn: 'root',
})
export class BadgeGattService {
  private device: BluetoothDevice | null = null;
  private displayCharacteristic: BluetoothRemoteGATTCharacteristic | null = null;
  private lastImage = this.createImage();

  public connected = false;
  private displayUpdating = false;
  public batteryVoltage: string | null = null;

  constructor() {}

  async connect() {
    const queryParams = new URLSearchParams(window.location.search);
    const badgeId = queryParams.get('b');
    let filter: BluetoothRequestDeviceFilter = { services: [DISPLAY_SERVICE] };
    if (badgeId) {
      filter.name = badgePrefix + badgeId;
    }
    this.device = await navigator.bluetooth.requestDevice({
      filters: [filter],
    });
    this.device.addEventListener('gattserverdisconnected', () => {
      this.connected = false;
    });
    await this.device.gatt.connect();
    const svc = await this.device.gatt.getPrimaryService(DISPLAY_SERVICE);
    this.displayCharacteristic = await svc.getCharacteristic(DISPLAY_CHARACTERISTIC);
    console.log('Connected :-)');
    this.connected = true;

    const batteryCharacteristic = await svc.getCharacteristic(BATTERY_VOLTAGE_CHARACTERISTIC);
    this.batteryVoltage = new TextDecoder().decode(await batteryCharacteristic.readValue());

    // Clear screen to white:
    await this.displayCharacteristic.writeValue(new Uint8Array([0xff]));
    for (let i = 0; i < this.lastImage.length; i++) {
      this.lastImage[i] = 0xff;
    }
  }

  disconnect() {
    if (this.device) {
      this.device.gatt.disconnect();
      this.device = null;
      this.connected = false;
    }
  }

  get updating() {
    return this.displayUpdating;
  }

  async updateDisplay(ctx: CanvasRenderingContext2D) {
    if (this.displayUpdating || !this.displayCharacteristic) {
      return;
    }
    this.displayUpdating = true;
    try {
      let updated = false;
      do {
        updated = false;
        const currentImage = this.readCanvas(ctx);
        for (let i = 0; i < currentImage.length; i++) {
          if (this.lastImage[i] != currentImage[i]) {
            const data = [i & 0xff, (i >> 8) | 0x80, ...currentImage.slice(i, i + 18)];
            await this.displayCharacteristic.writeValue(new Uint8Array(data));
            for (let j = 0; j < 18; j++) {
              this.lastImage[i + j] = currentImage[i + j];
            }
            updated = true;
            break;
          }
        }
      } while (updated);
    } finally {
      this.displayUpdating = false;
    }
  }

  private createImage() {
    return new Uint8Array((displayWidth * displayHeight) / 8);
  }

  private readCanvas(ctx: CanvasRenderingContext2D) {
    const { data } = ctx.getImageData(0, 0, displayWidth, displayHeight);
    const bitmap = this.createImage();
    for (let y = 0; y < displayHeight; y++) {
      for (let x = 0; x < displayWidth; x++) {
        const point = data[(y * displayWidth + x) * 4];
        if (point > 0x80) {
          const bitmapOffs = Math.floor(y / 8) * displayWidth + x;
          const bit = 7 - (y % 8);
          bitmap[bitmapOffs] |= 1 << bit;
        }
      }
    }
    return bitmap;
  }
}
