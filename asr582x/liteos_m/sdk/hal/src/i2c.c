/*
 * Copyright (c) 2022 ASR Microelectronics (Shanghai) Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdint.h>
#include "duet_cm4.h"
#include "duet_i2c.h"
#include "hal/soc/soc.h"
#include "lega_rhino.h"

lega_semaphore_t g_duet_i2c_sem[DUET_I2C_NUM];
lega_queue_t g_duet_i2c_queue[DUET_I2C_NUM];

char *g_duet_i2c_queue_name[DUET_I2C_NUM] =
{
    "duet_i2c0_queue",
    "duet_i2c1_queue"
};

void hal_i2c0_slv_tx_callback(void)
{
    lega_rtos_set_semaphore(&g_duet_i2c_sem[I2C_DEVICE0]);
}

void hal_i2c0_slv_rx_callback(uint8_t data)
{
    lega_rtos_push_to_queue(&g_duet_i2c_queue[I2C_DEVICE0],&data,LEGA_NEVER_TIMEOUT);
}

void hal_i2c1_slv_tx_callback(void)
{
    lega_rtos_set_semaphore(&g_duet_i2c_sem[I2C_DEVICE1]);
}

void hal_i2c1_slv_rx_callback(uint8_t data)
{
    lega_rtos_push_to_queue(&g_duet_i2c_queue[I2C_DEVICE1],&data,LEGA_NEVER_TIMEOUT);
}

/**
 * Initialises an I2C interface
 * Prepares an I2C hardware interface for communication as a master or slave
 *
 * @param[in]  i2c  the device for which the i2c port should be initialised
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_init(i2c_dev_t *i2c)
{
    int32_t ret;
    ret = duet_i2c_init((duet_i2c_dev_t *)i2c);
    if(!ret && (I2C_MODE_SLAVE == i2c->config.mode))
    {
        ret = lega_rtos_init_semaphore(&g_duet_i2c_sem[i2c->port], 0);
        if(!ret)
        {
            ret = lega_rtos_init_queue(&g_duet_i2c_queue[i2c->port],g_duet_i2c_queue_name[i2c->port],1,I2C_BUF_QUEUE_SIZE);
        }
    }
    return ret;
}

/**
 * I2c master send
 *
 * @param[in]  i2c       the i2c device
 * @param[in]  dev_addr  device address
 * @param[in]  data      i2c send data
 * @param[in]  size      i2c send data size
 * @param[in]  timeout   timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                       if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_master_send(i2c_dev_t *i2c, uint16_t dev_addr, const uint8_t *data,
                            uint16_t size, uint32_t timeout)
{
    return duet_i2c_master_send((duet_i2c_dev_t *)i2c, dev_addr, data, size, timeout);
}

/**
 * I2c master recv
 *
 * @param[in]   i2c       the i2c device
 * @param[in]   dev_addr  device address
 * @param[out]  data      i2c receive data
 * @param[in]   size      i2c receive data size
 * @param[in]   timeout   timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                        if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_master_recv(i2c_dev_t *i2c, uint16_t dev_addr, uint8_t *data,
                            uint16_t size, uint32_t timeout)
{
    return duet_i2c_master_recv((duet_i2c_dev_t *)i2c, dev_addr, data, size, timeout);
}

/**
 * I2c slave send
 *
 * @param[in]  i2c      the i2c device
 * @param[in]  data     i2c slave send data
 * @param[in]  size     i2c slave send data size
 * @param[in]  timeout  timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                      if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_slave_send(i2c_dev_t *i2c, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    uint16_t i = 0;
    int32_t ret;
    if((NULL == i2c) || (NULL == data) || (0 == size))
    {
        return -1;
    }
    if(HAL_WAIT_FOREVER == timeout)
    {
        timeout = LEGA_NEVER_TIMEOUT;
    }
    while(i < size)
    {
        if(!lega_rtos_get_semaphore(&g_duet_i2c_sem[i2c->port], timeout))
        {
            ret = duet_i2c_tx_data((duet_i2c_dev_t *)i2c, data[i++]);
            if(ret)
            {
                return ret;
            }
        }
    }
    return 0;
}

/**
 * I2c slave receive
 *
 * @param[in]   i2c      tthe i2c device
 * @param[out]  data     i2c slave receive data
 * @param[in]   size     i2c slave receive data size
 * @param[in]  timeout   timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                       if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_slave_recv(i2c_dev_t *i2c, uint8_t *data, uint16_t size, uint32_t timeout)
{
    uint16_t i = 0;
    int32_t ret;
    if((NULL == i2c) || (NULL == data))
    {
        return -1;
    }

    if(i2c->port >= DUET_I2C_NUM)
    {
        return -1;
    }

    if(HAL_WAIT_FOREVER == timeout)
    {
        timeout = LEGA_NEVER_TIMEOUT;
    }
    while(i < size)
    {
        ret = lega_rtos_pop_from_queue(&g_duet_i2c_queue[i2c->port],(data+i++),timeout);
        if(ret)
        {
            return ret;
        }
    }
    return 0;
}

/**
 * I2c mem write
 *
 * @param[in]  i2c            the i2c device
 * @param[in]  dev_addr       device address
 * @param[in]  mem_addr       mem address
 * @param[in]  mem_addr_size  mem address
 * @param[in]  data           i2c master send data
 * @param[in]  size           i2c master send data size
 * @param[in]  timeout        timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                            if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_mem_write(i2c_dev_t *i2c, uint16_t dev_addr, uint16_t mem_addr,
                          uint16_t mem_addr_size, const uint8_t *data, uint16_t size,
                          uint32_t timeout)
{
    return duet_i2c_mem_write((duet_i2c_dev_t *)i2c, dev_addr, mem_addr, mem_addr_size, data, size, timeout);
}

/**
 * I2c master mem read
 *
 * @param[in]   i2c            the i2c device
 * @param[in]   dev_addr       device address
 * @param[in]   mem_addr       mem address
 * @param[in]   mem_addr_size  mem address
 * @param[out]  data           i2c master send data
 * @param[in]   size           i2c master send data size
 * @param[in]  timeout         timeout in milisecond, set this value to HAL_WAIT_FOREVER
 *                             if you want to wait forever
 *
 * @return  0 : on success, EIO : if an error occurred during initialisation
 */
int32_t hal_i2c_mem_read(i2c_dev_t *i2c, uint16_t dev_addr, uint16_t mem_addr,
                         uint16_t mem_addr_size, uint8_t *data, uint16_t size,
                         uint32_t timeout)
{
    return duet_i2c_mem_read((duet_i2c_dev_t *)i2c, dev_addr, mem_addr, mem_addr_size, data, size, timeout);
}

/**
 * Deinitialises an I2C device
 *
 * @param[in]  i2c  the i2c device
 *
 * @return  0 : on success, EIO : if an error occurred during deinitialisation
 */
int32_t hal_i2c_finalize(i2c_dev_t *i2c)
{
    return duet_i2c_finalize((duet_i2c_dev_t *)i2c);
}
