SET ESP_IDF_PATH=<PATH TO IDE e.g. C:\esp32\Espressif\esp-idf_x_y_z>
SET ESP_IDF_FRAMEWORK_PATH=<PATH TO FRAMEWORK e.g. C:\esp32\Espressif\esp-idf_x_y_z\frameworks\esp-idf-vx.y.z>
SET SOURCE_PATH=%1%

cd "%ESP_IDF_FRAMEWORK_PATH%"
call "%ESP_IDF_PATH%\idf_cmd_init.bat"
echo "esp-idf exported"
call rm "%SOURCE_PATH%\build\FeaturedFreeRTOSIoTIntegration.bin"
cd "%SOURCE_PATH%"
for /l %%a in (1 1 10) do (
    if exist "%SOURCE_PATH%\build\FeaturedFreeRTOSIoTIntegration.bin" (
        rem file exists
        echo "file exists"
        goto :break
    ) else (
        rem file doesn't exist
        echo "file doesn't exist"
        call idf.py build
        sleep 1
    )
)

:break
