include(${CMAKE_CURRENT_LIST_DIR}/gcc_arm.cmake)
set(COMMON_FLAGS "-Os -fdata-sections -ffunction-sections -fno-exceptions -ggdb -mcpu=cortex-m4 -mthumb -mthumb-interwork --specs=nano.specs")
