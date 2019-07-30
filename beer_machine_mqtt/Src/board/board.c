/*****************************************************************************
*  板级支持包                                                          
*  Copyright (C) 2019 wkxboot 1131204425@qq.com.                             
*                                                                            
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file     beer_machine.c                                                   
*  @brief    板级支持包                                                                                                                                                                                             
*  @author   wkxboot                                                      
*  @email    1131204425@qq.com                                              
*  @version  v1.0.0                                                  
*  @date     2019/1/11                                            
*  @license  GNU General Public License (GPL)                                
*                                                                            
*                                                                            
*****************************************************************************/

#include "spi.h"
#include "main.h"
#include "board.h"

void bsp_board_init(void)
{
    bsp_gsm_pwr_key_release();  
}


/*压缩机控制*/
void bsp_compressor_ctrl_pwr_on(void)
{
    HAL_GPIO_WritePin(BSP_COMPRESSOR_CTRL_POS_GPIO_Port, BSP_COMPRESSOR_CTRL_POS_Pin, BSP_CTRL_COMPRESSOR_ON); 
}
void bsp_compressor_ctrl_pwr_off(void)
{
    HAL_GPIO_WritePin(BSP_COMPRESSOR_CTRL_POS_GPIO_Port, BSP_COMPRESSOR_CTRL_POS_Pin, BSP_CTRL_COMPRESSOR_OFF); 
}

/*蜂鸣器控制*/
void bsp_buzzer_ctrl_pwr_on(void)
{
    HAL_GPIO_WritePin(BSP_BUFFER_CTRL_POS_GPIO_Port, BSP_BUFFER_CTRL_POS_Pin, BSP_CTRL_BUZZER_ON); 
}
void bsp_buzzer_ctrl_pwr_off(void)
{
    HAL_GPIO_WritePin(BSP_BUFFER_CTRL_POS_GPIO_Port, BSP_BUFFER_CTRL_POS_Pin, BSP_CTRL_BUZZER_OFF); 
}

/*GSM PWR控制*/
void bsp_gsm_pwr_key_press(void)
{
    HAL_GPIO_WritePin(BSP_2G_PWR_CTRL_POS_GPIO_Port, BSP_2G_PWR_CTRL_POS_Pin, BSP_GSM_PWR_ON);   
}

void bsp_gsm_pwr_key_release(void)
{
    HAL_GPIO_WritePin(BSP_2G_PWR_CTRL_POS_GPIO_Port, BSP_2G_PWR_CTRL_POS_Pin, BSP_GSM_PWR_OFF);   
}

bsp_gsm_pwr_status_t bsp_get_gsm_pwr_status(void)
{
    return HAL_GPIO_ReadPin(BSP_2G_STATUS_POS_GPIO_Port,BSP_2G_STATUS_POS_Pin) == GPIO_PIN_SET ? BSP_GSM_STATUS_PWR_ON : BSP_GSM_STATUS_PWR_OFF;
}

/*tm1629a cs控制*/
void bsp_tm1629a_cs_ctrl_set(void)
{
    HAL_GPIO_WritePin(BSP_TM1629A_CS_POS_GPIO_Port, BSP_TM1629A_CS_POS_Pin, BSP_TM1629A_CS_SET); 
}
void bsp_tm1629a_cs_ctrl_clr(void)
{
    HAL_GPIO_WritePin(BSP_TM1629A_CS_POS_GPIO_Port, BSP_TM1629A_CS_POS_Pin, BSP_TM1629A_CS_CLR); 
}

/*tm1629a interface*/
void bsp_tm1629a_write_byte(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi2,&byte,1,0xff); 
}
uint8_t bsp_tm1629a_read_byte(void)
{
    return 0;
}





