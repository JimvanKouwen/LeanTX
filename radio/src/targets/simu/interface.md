# Simulator Plugin Interface

## Methods

```c

/**
 * Simu module functions
 */

/* Simulator plugin name */
const char* simu_get_name();

/* Start/stop */
void simuInit();
void simuStop(); // should be async but isn't
bool simuIsRunning();

/* Live data */
unsigned int simu_get_num_outputs();
const int16_t* simu_get_outputs();

unsigned int simu_get_num_channels();
const int16_t* simu_get_channels();


uint8_t simu_get_stick_mode();
uint16_t simu_get_trim_range();


/**
 * Callbacks / native functions
 */
void simu_read_analogs(uint16_t* analogs, int num_analogs);
uint32_t simu_read_keys();
uint32_t simu_read_trims();
void simu_flush_display(bool backlight, uint8_t* buffer, int buflen);
```
