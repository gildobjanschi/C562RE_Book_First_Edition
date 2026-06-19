# file-format: 1.0.0
if(CMAKE_BUILD_TYPE STREQUAL "debug_GCC_NUCLEO-C562RE")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE main.c shared.h output_pwm_dma.h output_pwm_dma.c input_capture_dma.h input_capture_dma.c app_task.h app_task.c
  ../../Shared/shared_def.h
  ../../Shared/Utils/error_handler.h ../../Shared/Utils/error_handler.c
  ../../Shared/Faults/m33_it.h ../../Shared/Faults/m33_it.c
  ../../Shared/Debug/swd_printf.h ../../Shared/Debug/swd_printf.c ../../Shared/Debug/assert.c
  ../../Shared/RTT/SEGGER_RTT_Conf.h ../../Shared/RTT/SEGGER_RTT_printf.c ../../Shared/RTT/SEGGER_RTT.h ../../Shared/RTT/SEGGER_RTT.c)
endif()
