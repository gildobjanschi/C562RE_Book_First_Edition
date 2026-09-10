# file-format: 1.0.0
if(CMAKE_BUILD_TYPE STREQUAL "debug_GCC_NUCLEO-C562RE")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE main.c shared.h rng_task.h rng_task.c cordic_task.h cordic_task.c crc_task.h crc_task.c sha256_integrity_task.h sha256_integrity_task.c aes_cbc_enc_task.h aes_cbc_enc_task.c
  ../../Shared/shared_def.h
  ../../Shared/Faults/m33_it.h ../../Shared/Faults/m33_it.c
  ../../Shared/Debug/swd_printf.h ../../Shared/Debug/swd_printf.c ../../Shared/Debug/assert.c
  ../../Shared/Utils/error_handler.h ../../Shared/Utils/error_handler.c
  ../../Shared/RTT/SEGGER_RTT_Conf.h ../../Shared/RTT/SEGGER_RTT_printf.c ../../Shared/RTT/SEGGER_RTT.h ../../Shared/RTT/SEGGER_RTT.c)
endif()
