cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/"
    )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/main")

project(firmware)

# Path to the signing script
set(SIGN_SCRIPT "${CMAKE_SOURCE_DIR}/main/tools/sign_firmware.py")

# Command to convert ELF to BIN
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/firmware.bin
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND esptool.py --chip esp32s3 elf2image -o ${CMAKE_BINARY_DIR}/firmware.bin ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Converting ELF to BIN"
)

# Command to execute the signing script after BIN creation
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/firmware.sig
    DEPENDS ${CMAKE_BINARY_DIR}/firmware.bin
    COMMAND python3 ${SIGN_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Signing firmware binary using AWS KMS"
)

# Define a custom target that depends on the signature
add_custom_target(
    firmware_signed ALL
    DEPENDS ${CMAKE_BINARY_DIR}/firmware.sig
)

# Add the partition table for firmware.sig
set(FIRMWARE_SIG_PATH "${CMAKE_BINARY_DIR}/firmware.sig")

# Custom target to flash the firmware.sig and firmware.hsh files into their respective partitions
add_custom_target(flash_firmware_sig ALL
    COMMAND ${ESPTOOLPY} --chip esp32s3 write_flash 0x351000 ${CMAKE_BINARY_DIR}/firmware.sig
    COMMAND ${ESPTOOLPY} --chip esp32s3 write_flash 0x352000 ${CMAKE_BINARY_DIR}/firmware.hsh
    DEPENDS firmware_signed
    COMMENT "Flashing firmware.sig and firmware.hsh to their respective partitions"
)

# Remove the duplicate rule for copying firmware.sig to the build directory
# as it is already being managed by firmware_signed.

# Add dependency for the flash target
add_dependencies(flash flash_firmware_sig)
