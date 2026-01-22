#ifndef _ST7701S_H
#define _ST7701S_H

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"

#define SPI_METHOD 1
#define IOEXPANDER_METHOD 0

//类结构体
typedef struct{
    char method_select;
    
    //SPI config_t
    spi_device_handle_t spi_device;
    spi_bus_config_t spi_io_config_t;
    spi_device_interface_config_t st7701s_protocol_config_t;

    //I2C config_t
}vernon_st7701s;

typedef vernon_st7701s * vernon_st7701s_handle;


/*Public Function*/

//创建新的对象
vernon_st7701s_handle st7701s_new_object(int sda, int scl, int cs, char channel_select, char method_select);

//屏幕初始化
void st7701s_screen_init(vernon_st7701s_handle st7701s_handlev, unsigned char type);

//删除对象
void st7701s_delObject(vernon_st7701s_handle st7701s_handle);


/*Private Function*/

//SPI写指令
void st7701s_write_cmd(vernon_st7701s_handle st7701s_handle, uint8_t cmd);

//SPI写地址
void st7701s_write_data(vernon_st7701s_handle st7701s_handle, uint8_t data);

#endif

