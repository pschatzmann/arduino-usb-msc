#include "USBMsc_config.h"
#if USBHOSTMSC_COMPILE
/*
 * STM32 (STM32duino core) native USB Host bring-up: RCC clock, GPIO AF and
 * NVIC/ISR wiring for the OTG_FS peripheral's D+/D- pins, which TinyUSB's
 * own dwc2 host driver (hcd_dwc2.c / dwc2_stm32.h) expects to already be
 * done before tusb_init() runs -- unlike RP2040/ESP32-S3, where the core
 * itself (or Adafruit_USBH_Host) handles this.
 *
 * Requires Adafruit_TinyUSB_Arduino to be built with -DCFG_TUH_ENABLED=1;
 * see docs/STM32.md for why, and for the (small, non-breaking)
 * patch that needs to be applied to that library's STM32 port files.
 *
 * Only plain "OTG_FS on PA11/PA12" parts are covered: STM32F4, and STM32F7
 * parts other than the FS-less H7-style ones. STM32WB (FSDEV, device-only
 * silicon) cannot do USB host at the hardware level, at all. STM32H7 parts
 * without a separate OTG_FS instance, STM32WBA, and STM32U5 would need
 * their own bring-up (different AF/clock-enable macros and/or HS PHY
 * setup) that hasn't been added yet.
 */
#if defined(ARDUINO_ARCH_STM32)

#include "USBHostMSC.h"

#ifndef USE_HAL_DRIVER
#define USE_HAL_DRIVER
#endif
#include "Arduino.h"

#if defined(STM32H7xx)
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_rcc.h"
#elif defined(STM32F7xx)
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_rcc.h"
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_rcc.h"
#else
#error \
    "usb-msc's STM32 host support currently only covers F4/F7 (and " \
    "FS-capable H7) OTG_FS parts."
#endif

#include "tusb.h"

#if !CFG_TUH_ENABLED
#error \
    "usb-msc on STM32 requires TinyUSB host mode, which is off by " \
    "default in Adafruit_TinyUSB_Arduino for this architecture. Build " \
    "with -DCFG_TUH_ENABLED=1 -- see docs/STM32.md."
#endif

#if defined(STM32H7xx) && !defined(USB2_OTG_FS)
#error \
    "This STM32H7 part has no separate OTG_FS instance (only OTG_HS); " \
    "usb-msc doesn't cover that bring-up yet."
#elif defined(STM32H7xx)
#define USBHOSTMSC_STM32_FS_IRQn OTG_FS_IRQn
#define USBHOSTMSC_STM32_FS_IRQHandler OTG_FS_IRQHandler
#define USBHOSTMSC_STM32_FS_AF GPIO_AF10_OTG2_FS
#define USBHOSTMSC_STM32_FS_CLK_ENABLE() __HAL_RCC_USB2_OTG_FS_CLK_ENABLE()
#else
#define USBHOSTMSC_STM32_FS_IRQn OTG_FS_IRQn
#define USBHOSTMSC_STM32_FS_IRQHandler OTG_FS_IRQHandler
#define USBHOSTMSC_STM32_FS_AF GPIO_AF10_OTG_FS
#define USBHOSTMSC_STM32_FS_CLK_ENABLE() __HAL_RCC_USB_OTG_FS_CLK_ENABLE()
#endif

extern "C" {
void USBHOSTMSC_STM32_FS_IRQHandler(void) { tuh_int_handler(0, true); }
}

void usbhostmsc_stm32_hw_init(void) {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  USBHOSTMSC_STM32_FS_CLK_ENABLE();

#if defined(STM32H7xx)
  // H7's USB PHY is powered from a separate VDD33USB supply.
  HAL_PWREx_EnableUSBVoltageDetector();
#endif

  // OTG_FS DM/DP are fixed to PA11/PA12 on every part in this bucket.
  GPIO_InitTypeDef GPIO_InitStruct = {};
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = USBHOSTMSC_STM32_FS_AF;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  NVIC_SetPriority(USBHOSTMSC_STM32_FS_IRQn, 0);
  NVIC_EnableIRQ(USBHOSTMSC_STM32_FS_IRQn);

  // NOT handled here: driving 5V VBUS out to the attached USB device.
  // Boards with a software-controlled power switch (a GPIO-enabled load
  // switch IC) need that GPIO driven high before a device will enumerate;
  // add it in your sketch's setup() before msc.begin() if your board has
  // one. Simple dev boards (e.g. BlackPill-style) typically have no such
  // switch -- confirm 5V reaches the connector's VBUS pin by your board's
  // own design.
}

#endif // ARDUINO_ARCH_STM32

#endif // USBHOSTMSC_COMPILE
