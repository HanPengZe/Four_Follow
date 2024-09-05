/**
 * @file Chassis_control.c
 * @author Gsy
 * @brief 
 * @version 1
 * @date 2022-07-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */
 #include "Chassis_control.h"
 #include "M6020_Motor.h"
 #include "M3508_Motor.h"
 #include "Monitor_RM_CAN.h"
 #include "DJI_IMU.h"
 #include "Control_Vision.h"
 #include "DR16_Remote.h"
 #include "SpeedRamp.h"
 #include "Cloud_control.h"
 #include "arm_math.h"
 #include <math.h>
/*******************************用户数据定义************************************/
float	start_imu;  
int a_start = 0;
int16_t speed[4];
int16_t angle[4];
int16_t Yaw_error;
int SpinDirection_Flag = 1; /* 小陀螺反向 */
int i_VOmega;
float Radius = 1.0f;  // 圆心距
float tan_to_sin = -0.707107f ;
float sin_90 = 1;
float cos_90 = 0;
int Chassis_tar = 2063;
int IMU_Init_Condition;
uint8_t Sup_Cap = 0;
/*******************************************************************************/

/**
 * @brief PID赋值
 * 
 * @param target 
 * @param value 
 * @return int 
 */
 
cloud_positionpid_t M3508s_FloowOPID = M3508s_FloowOPIDInit;
cloud_incrementalpid_t LFWheel_PID = LFWHEEL_PID_PARAM;
cloud_incrementalpid_t RFWheel_PID = RFWHEEL_PID_PARAM;
cloud_incrementalpid_t LBWheel_PID = LBWHEEL_PID_PARAM;
cloud_incrementalpid_t RBWheel_PID = RBWHEEL_PID_PARAM;
cloud_incrementalpid_t *Wheel_PID[] = {&LFWheel_PID, &RFWheel_PID, &LBWheel_PID, &RBWheel_PID};

cloud_positionpid_t RF_Ship_P_PID = RF_Ship_PID_P_PARAM;
cloud_positionpid_t LF_Ship_P_PID = LF_Ship_PID_P_PARAM;
cloud_positionpid_t LB_Ship_P_PID = LB_Ship_PID_P_PARAM;
cloud_positionpid_t RB_Ship_P_PID = RB_Ship_PID_P_PARAM;
cloud_positionpid_t *Ship_P_PID[] = {&RF_Ship_P_PID, &LF_Ship_P_PID, &LB_Ship_P_PID, &RB_Ship_P_PID};

cloud_positionpid_t RF_Ship_I_PID    = RF_Ship_I_PID_PARAM;
cloud_positionpid_t LF_Ship_I_PID    = LF_Ship_I_PID_PARAM;
cloud_positionpid_t LB_Ship_I_PID    = LB_Ship_I_PID_PARAM;
cloud_positionpid_t RB_Ship_I_PID    = RB_Ship_I_PID_PARAM;
cloud_positionpid_t *Ship_I_PID[] = {&RF_Ship_I_PID, &LF_Ship_I_PID, &LB_Ship_I_PID, &RB_Ship_I_PID};

cloud_positionpid_t Follow_I_PID    = Follow_I_PID_PARAM;
cloud_positionpid_t Follow_P_PID    = Follow_P_PID_PARAM;
/**
 * @brief  麦克纳姆轮速度解算
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				VOmega	自转速度
 * @param[out]	Speed	速度
 * @retval None
 */

void MecanumCalculate(float X_Move,float Y_Move,float Yaw ,int16_t *Speed)
{
    float Target_velocity[4]={0}; 
    float MaxSpeed = 0.0f;
    float Param = 1.0f;

	Target_velocity[0] = 	X_Move - Y_Move + Yaw;
	Target_velocity[1] =    X_Move + Y_Move + Yaw;
	Target_velocity[2] =   -X_Move + Y_Move + Yaw;
	Target_velocity[3]=    -X_Move - Y_Move + Yaw;	

    for(uint8_t i = 0 ; i<4 ; i ++)
    {
        if(abs(Target_velocity[i]) > MaxSpeed)
        {
            MaxSpeed = abs(Target_velocity[i]);
        }
    }

    if (MaxSpeed > WheelMaxSpeed)
    {
        Param = (float)WheelMaxSpeed / MaxSpeed;
    }

    Speed[0] = Target_velocity[0] * Param;
    Speed[1] = Target_velocity[1] * Param;
    Speed[2] = Target_velocity[2] * Param;
    Speed[3] = Target_velocity[3] * Param;

}

/**
 * @brief  舵轮运动解算(驱动轮)
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				VOmega	自转速度
 * @param[out]	Speed	速度
 * @retval None
 */

int8_t drct=1;//决定驱动电机正反转

void Wheel_calc(float X_Move,float Y_Move,float Yaw ,int16_t *Speed) 
{
	float Target_velocity[4]={0}; 
	  //3508目标速度计算
    float MaxSpeed = 9000.0f;
    float Param = 1.0f;
    Target_velocity[0] = sqrt(pow(X_Move - Yaw*tan_to_sin,2) + pow(Y_Move + Yaw*tan_to_sin,2))  ;
    Target_velocity[1] = sqrt(pow(X_Move - Yaw*tan_to_sin,2) + pow(Y_Move - Yaw*tan_to_sin,2)) ;
    Target_velocity[2] = sqrt(pow(X_Move + Yaw*tan_to_sin,2) + pow(Y_Move - Yaw*tan_to_sin,2)) ; 
    Target_velocity[3] = sqrt(pow(X_Move + Yaw*tan_to_sin,2) + pow(Y_Move + Yaw*tan_to_sin,2)) ;
	  
    for(uint8_t i = 0 ; i<4 ; i ++)
    {
        if(abs(Target_velocity[i]) > MaxSpeed)
        {
            MaxSpeed = abs(Target_velocity[i]);
        }
    }

    if (MaxSpeed > WheelMaxSpeed)
    {
        Param = (float)WheelMaxSpeed / MaxSpeed;
    }

    Speed[0] = Target_velocity[0] * Param;
    Speed[1] = Target_velocity[1] * Param;
    Speed[2] = Target_velocity[2] * Param;
    Speed[3] = Target_velocity[3] * Param;
}

/**
 * @brief  舵轮运动解算(转向轮)
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				VOmega	自转速度
 * @param[out]	Speed	速度
 * @retval None
 */
void Ship_calc(float X_Move,float Y_Move,float Yaw ,int16_t *Angle) 
{
	  float Target_velocity[4]={0}; 
	  //3508目标速度计算
//    float MaxSpeed = 0.0f;
//    float Param = 1.0f;
    Target_velocity[0] = atan2(X_Move - Yaw*Radius*tan_to_sin, Y_Move + Yaw*Radius*tan_to_sin)*(180/PI);
    Target_velocity[1] = atan2(X_Move - Yaw*Radius*tan_to_sin, Y_Move - Yaw*Radius*tan_to_sin)*(180/PI);
    Target_velocity[2] = atan2(X_Move + Yaw*Radius*tan_to_sin, Y_Move - Yaw*Radius*tan_to_sin)*(180/PI);
    Target_velocity[3] = atan2(X_Move + Yaw*Radius*tan_to_sin, Y_Move + Yaw*Radius*tan_to_sin)*(180/PI);
	  
		
    Angle[0] = Target_velocity[0];
    Angle[1] = Target_velocity[1];
    Angle[2] = Target_velocity[2];
    Angle[3] = Target_velocity[3];
}

/**
 * @brief  通过6020机械角度的方式获取云台Yaw旋转的角度（偏移车正前方的角度-中心点）
 * @param[in]  None
 * @retval 360度的角度值。
 */
float Cloud_getYawAngleWithCenter(void)
{
    return (M6020s_Yaw.totalAngle - Cloud_Yaw_Center) / M6020_mAngleRatio;
}


/**
 * @brief  全向移动公式
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				VOmega	自转速度
 * @param[out]	Speed	速度
 * @retval None
 */
static void Omnidirectional_Formula(float *Vx, float *Vy)
{
    float RadRaw = 0.0f;
    float temp_Vx = 0.0f;

    float angle = Cloud_getYawAngleWithCenter(); //机械角度偏差
    RadRaw = angle * DEG_TO_RAD;                           //弧度偏差
    temp_Vx = *Vx;
    *Vx = *Vx * cos(RadRaw) - *Vy * sin(RadRaw);
    *Vy = *Vy * cos(RadRaw) + temp_Vx * sin(RadRaw);
}


/**
 * @brief 过零处理
 * 
 * @param target 
 * @param value 
 * @return int 
 */
 int ComputeMinOffset(int target, int value) //计算最小偏差，底盘跟随应该往哪个方向去完成跟随动作。
{
    int err =target - value  ;

    if (err > 4096)
    {
        err -= 8191;
    }
    else if (err < -4096)
    {
        err += 8191;
    }
    return err;
}
//#define Center_0 2063
//#define Center_1 4026
//#define Center_2 6185
//#define Center_3 8178

int Follow_Judge(int angle)
{
	uint8_t i;
	int err[5],Err;
err[0] =  ComputeMinOffset(angle,Center_0);
err[1] =  ComputeMinOffset(angle,Center_1);
err[2] =  ComputeMinOffset(angle,Center_2);
err[3] =  ComputeMinOffset(angle,Center_3);
	
err[4] = abs(err[0])<abs(err[1])?err[0]:err[1];
err[4] = abs(err[4])<abs(err[2])?err[4]:err[2];
err[4] = abs(err[4])<abs(err[3])?err[4]:err[3];
	
	return err[4];
	
}


/**
 * @brief 获取开机后的陀螺仪位置，并设为基准
 * 
 * @param target 
 * @param value 
 * @return int 
 */

void read_start_imu (void)  //获取开机后的陀螺仪位置，并设为基准
{
	if (a_start == 0)
	{	
		for(int i = 0;i<10;++i)
		{
			M6020s_Yaw.targetAngle   = start_imu ;
			M6020s_Pitch.targetAngle =  pitch_start;
			start_imu =  DJI_C_IMU.total_yaw * 22.7527f;
			pitch_start = Cloud_Pitch_Center;
			a_start = 1 ;
			M6020s_Yaw.rc_targetAngle   = 0;
			M6020s_Pitch.rc_targetAngle = 0;
		}
	}
}
/**
 * @brief 摇杆死区设置
 * 
 * @param target 
 * @param value 
 * @return int 
 */

void rc_dead(float *rc_dead)
{
	if(*rc_dead >= 0 && *rc_dead <= 50)
	{
		*rc_dead = 0;
	}
	else if(*rc_dead <= 0 && *rc_dead >= -50)
	{
		*rc_dead = 0;
	}
}


/**
 * @brief 底盘跟随云台运动
 * 
 * @param target 
 * @param value 
 * @return int 
 */

void ChassisWorkMode_Follow(float Vx, float Vy, float VOmega)
{
//	if(Robot.ChassisWorkMode == ChassisWorkMode_CloudFollow)
//	{
//	if(rc.roll>200&&rc.sw1 != 1)
//	{
//	Omnidirectional_Formula(&Vx,&Vy);
//  i_VOmega *= SpinDirection_Flag;
//	i_VOmega = rc.roll * 10.0f * SpinDirection_Flag;
//	}
//	else
//	{
//		i_VOmega = 0;
//	}
//	Yaw_error = ComputeMinOffset(Chassis_tar,M6020s_Yaw.realAngle);
//	M6020s_Yaw_Follow.p_outCurrent = M3508s_FloowOPID.CLOUD_Position_PID(&M3508s_FloowOPID,Yaw_error,0);
//	for(int i = 0; i<4; i++)
//	{
//		if(rc.roll>200&&rc.sw1 != 1)
//		MecanumCalculate(8*Vx,8*Vy,  i_VOmega,speed);
//		else
//		MecanumCalculate(10*Vx,10*Vy, M6020s_Yaw_Follow.p_outCurrent  ,speed);
//		M3508s[i].i_outCurrent = Wheel_PID[i]->CLOUD_Incremental_PID(Wheel_PID[i], speed[i], M3508s[i].realSpeed); 
//	}
//	Motor_0x200_SendData(M3508s[0].i_outCurrent,M3508s[1].i_outCurrent,M3508s[2].i_outCurrent,M3508s[3].i_outCurrent);
//	}
//	else
//	Motor_0x200_SendData(0,0,0,0);
}


/**
 * @brief 底盘跟随云台运动
 * 
 * @param target 
 * @param value 
 * @return int 
 */
int Ship_time = 0;
int16_t Ship_ready_dead[4] ;
int16_t Wheel_Ramp[4];
float Follow_error;
float Follow_I_Out;
void Ship_ChassisWorkMode_Follow(float Vx, float Vy, float VOmega)
{
}


/**
 * @brief  底盘控制处理-跟随云台
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				Omega	偏向角
*				mode	模式 - 除Status_ControlOFF外，其他正常控制
 * @retval None
 */
void Chassis_processing(float Vx, float Vy, float VOmega ,float VPitch)
{
	//rc_dead(&Vx);  不需要这个死区了
	//rc_dead(&Vy);
	//rc_dead(&VOmega);
	//rc_dead(&VPitch);
	//ChassisWorkMode_Follow(Vx,Vy,VOmega);
	Ship_ChassisWorkMode_Follow(Vx,Vy,VOmega);
	
}
/**
 * @brief  底盘控制处理-传统控制
 * @param[in]  Vx		x轴速度
 *				Vy		y轴速度
 *				Omega	偏向角
*				mode	模式 - 传统
 * @retval None
 */
void Cloud_processing(float Vx, float Vy, float VOmega ,float VPitch)
{
	//rc_dead(&Vx);  
	//rc_dead(&Vy);
	//rc_dead(&VOmega);
	//rc_dead(&VPitch);
	YawWorkMode_Follow(Vx,Vy,VOmega,VPitch);
	
}

void Chassis_Init(void)
{
  M6020s[0].Init_angle = -32.25;
  M6020s[1].Init_angle = -88.9;
  M6020s[2].Init_angle = 34.27;
  M6020s[3].Init_angle = +151.69;
}

void Chassis_Dead_Init(void)
{
  M6020s[0].Dead_angle = (-45)-32.25;
  M6020s[1].Dead_angle = (+45)-88.9;
  M6020s[2].Dead_angle = (-45)+34.27;
  M6020s[3].Dead_angle = (+45)+151.69;
}

void Robot_Init(void)
{
		Robot.ChassisWorkMode = ChassisWorkMode_Disable;
	  Robot.Device_FrictMode=FrictWorkMode_Disable;
}
