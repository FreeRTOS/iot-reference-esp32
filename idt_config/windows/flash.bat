SET ESP_IDF_PATH=<PATH TO IDE e.g. C:\esp32\Espressif\esp-idf_x_y_z>
SET ESP_IDF_FRAMEWORK_PATH=<PATH TO FRAMEWORK e.g. C:\esp32\Espressif\esp-idf_x_y_z\frameworks\esp-idf-vx.y.z>
SET SOURCE_PATH=%1%
SET NUM_COMPORT=<serial port device e.g. COM3>

cd "%ESP_IDF_FRAMEWORK_PATH%"
call "%ESP_IDF_PATH%\idf_cmd_init.bat"
echo "esp-idf exported"
cd "%SOURCE_PATH%"

call idf.py -p "%NUM_COMPORT%" flash