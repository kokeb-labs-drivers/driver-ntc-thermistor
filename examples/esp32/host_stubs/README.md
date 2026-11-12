# Host stubs of the ESP-IDF headers

Just enough of ESP-IDF for `tests/esp32_port_check.cpp` to compile and run the port in `../main/` on a host computer
(CTest `ntc_thermistor_esp32_port_check`). The stubs return success and a raw ADC count of 750, accept only a 12-bit
channel, and can be told to fail `gpio_config()`; they model nothing else about the real ADC, so this guards the port's
own logic only (which hooks it declares, what it does with and without a bias pin, that a failed start-up can be
retried), not its use of ESP-IDF.
