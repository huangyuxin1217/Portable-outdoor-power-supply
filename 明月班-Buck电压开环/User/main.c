#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "PWM.h"

uint8_t i;

int main(void)
{
	OLED_Init();
	PWM_Init();
	
	while (1)
	{
		
			PWM_SetCompare1(2880/2);
			Delay_ms(10);
		
	}
}
