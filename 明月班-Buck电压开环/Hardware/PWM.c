#include "stm32f10x.h"                  // Device header

void PWM_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);//使能Time1定时器
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);//使能GPIOA
	
	//配置GPIOA8引脚
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;//复用推挽输出模式
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;		//使用PA8引脚
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//输出速度配置
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIOA
	
	//配置Time1使用内部时钟源
	TIM_InternalClockConfig(TIM1);
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;//配置定时器时基参数
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;//时钟分频-不分频
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;//向上计数模式
	TIM_TimeBaseInitStructure.TIM_Period =  2880 - 1 ;		//ARR自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler = 0 ;		//PSC预分频器1分频
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;//不启用重复计数
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);//初始化Time1时基
	
	//配置PWM输出模式
	TIM_OCInitTypeDef TIM_OCInitStructure;//
	TIM_OCStructInit(&TIM_OCInitStructure);//默认值初始化结构体
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;//PWM模式1
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;//输出极性为高
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;//使能输出
	TIM_OCInitStructure.TIM_Pulse = 0 ;//设置初始占空比
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);//初始化Time1通道1
	
	TIM_Cmd(TIM1, ENABLE);//使能定时器
	TIM_CtrlPWMOutputs(TIM1, ENABLE);  // Time1高级定时器额外使能
}

/**
  * @brief  设置PWM占空比
  * @param  Compare: 比较值,范围0-2880
  *         0 = 0%?, 1440 = 50%, 2880 = 100%
  * @retval None
  */
void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM1, Compare);
}
