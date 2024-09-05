/**
 * @file M6020_Motor.c
 * @author Gsy 
 * @brief 
 * @version 1
 * @date 2022-07-01
 * 
 * @copyright Copyright (c) 2021
 * 
 */


#include "M6020_Motor.h"
#include <stdio.h>
#include "BSP_CAN.h"

M6020s_t M6020s_Yaw;
M6020s_t M6020s_Yaw_Follow;
M6020s_t M6020s_Pitch;
M6020s_t M6020s[4];
M6020s_t *M6020_Array[] = {&M6020s[0],&M6020s[1],&M6020s[2],&M6020s[3],&M6020s_Yaw}; //对应电机的ID必须为：索引+1

void M6020_getInfo(Can_Export_Data_t RxMessage)
{

    int32_t StdId;
    StdId = (int32_t)RxMessage.CAN_RxHeader.StdId - M6020_READID_START; //由0开始
    // if (IndexOutofBounds(StdId, M6020_Amount))
    // {
    // 	Device_setAlertType(Alert_Times_SoftWare);
    // 	return;
    // }

    //解包数据，数据格式详见C620电调说明书P33
    M6020_Array[StdId]->lastAngle = M6020_Array[StdId]->realAngle;
    M6020_Array[StdId]->realAngle = (uint16_t)(RxMessage.CANx_Export_RxMessage[0] << 8 | RxMessage.CANx_Export_RxMessage[1]);
    M6020_Array[StdId]->realSpeed = (int16_t)(RxMessage.CANx_Export_RxMessage[2] << 8 | RxMessage.CANx_Export_RxMessage[3]);
    M6020_Array[StdId]->realCurrent = (int16_t)(RxMessage.CANx_Export_RxMessage[4] << 8 | RxMessage.CANx_Export_RxMessage[5]);
    //M6020_Array[StdId]->temperture = RxMessage.CANx_Export_RxMessage[6];

    if (M6020_Array[StdId]->realAngle - M6020_Array[StdId]->lastAngle < -6500)
    {
        M6020_Array[StdId]->turnCount++;
    }

    if (M6020_Array[StdId]->lastAngle - M6020_Array[StdId]->realAngle < -6500)
    {
        M6020_Array[StdId]->turnCount--;
    }

    M6020_Array[StdId]->totalAngle = M6020_Array[StdId]->realAngle + (8192 * M6020_Array[StdId]->turnCount);

    //帧率统计，数据更新标志位
    M6020_Array[StdId]->InfoUpdateFrame++;
    M6020_Array[StdId]->InfoUpdateFlag = 1;
}
void Yaw_GetInfo(Can_Export_Data_t RxMessage)
{

    //解包数据，数据格式详见C620电调说明书P33
    M6020s_Yaw.lastAngle = M6020s_Yaw.realAngle;
    M6020s_Yaw.realAngle = (uint16_t)(RxMessage.CANx_Export_RxMessage[0] << 8 | RxMessage.CANx_Export_RxMessage[1]);
    M6020s_Yaw.realSpeed = (int16_t)(RxMessage.CANx_Export_RxMessage[2] << 8 | RxMessage.CANx_Export_RxMessage[3]);
    M6020s_Yaw.realCurrent = (int16_t)(RxMessage.CANx_Export_RxMessage[4] << 8 | RxMessage.CANx_Export_RxMessage[5]);
    //M6020_Array[StdId]->temperture = RxMessage.CANx_Export_RxMessage[6];

    if (M6020s_Yaw.realAngle - M6020s_Yaw.lastAngle < -6500)
    {             
        M6020s_Yaw.turnCount++;
    }             
									
    if (M6020s_Yaw.lastAngle - M6020s_Yaw.realAngle < -6500)
    {             
        M6020s_Yaw.turnCount--;
    }

    M6020s_Yaw.totalAngle = M6020s_Yaw.realAngle + (8192 * M6020s_Yaw.turnCount);

    //帧率统计，数据更新标志位
    M6020s_Yaw.InfoUpdateFrame++;
    M6020s_Yaw.InfoUpdateFlag = 1;
}
