# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "debug_GCC_STM32C542CCT6")
  file(REMOVE_RECURSE
  "I3C_Target.elf"
  "I3C_Target.map"
  )
endif()
