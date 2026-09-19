# file-format: 1.0.0
if(CMAKE_BUILD_TYPE STREQUAL "debug_GCC_NUCLEO-C562RE")
  target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC tinyusb)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE main.c shared.h app_task.h app_task.c
    ##### GIL #####
    # common
    tinyusb/usb_descriptors.c
    tinyusb/tusb.c
    tinyusb/tusb_config.h
    tinyusb/common/tusb_fifo.c

    # device
    tinyusb/device/usbd.c
    tinyusb/class/cdc/cdc_device.c

    #portable
    tinyusb/portable/st/stm32_fsdev/fsdev_common.c
    tinyusb/portable/st/stm32_fsdev/dcd_stm32_fsdev.c

    # bsp
    tinyusb/bsp/board_api.h
    tinyusb/bsp/board.c
    ##########
  ../../Shared/shared_def.h
  ../../Shared/Faults/m33_it.h ../../Shared/Faults/m33_it.c
  ../../Shared/Debug/swd_printf.h ../../Shared/Debug/swd_printf.c ../../Shared/Debug/assert.c
  ../../Shared/Utils/error_handler.h ../../Shared/Utils/error_handler.c
  ../../Shared/RTT/SEGGER_RTT_Conf.h ../../Shared/RTT/SEGGER_RTT_printf.c ../../Shared/RTT/SEGGER_RTT.h ../../Shared/RTT/SEGGER_RTT.c)
endif()
