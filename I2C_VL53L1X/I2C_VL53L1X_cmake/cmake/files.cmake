# file-format: 1.0.0
if(CMAKE_BUILD_TYPE STREQUAL "debug_GCC_NUCLEO-C562RE")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE main.c shared.h app_input.h app_input.c vl53l1x.h vl53l1x.c i2c1.h i2c1.c app_task.h app_task.c
  ../../Shared/shared_def.h
  ../../Shared/Input/input.h ../../Shared/Input/input.c
  ../../Shared/Faults/m33_it.h ../../Shared/Faults/m33_it.c
  ../../Shared/Debug/swd_printf.h ../../Shared/Debug/swd_printf.c ../../Shared/Debug/assert.c
  ../../Shared/Utils/error_handler.h ../../Shared/Utils/error_handler.c
  ../../Shared/RTT/SEGGER_RTT_Conf.h ../../Shared/RTT/SEGGER_RTT_printf.c ../../Shared/RTT/SEGGER_RTT.h ../../Shared/RTT/SEGGER_RTT.c)
endif()
