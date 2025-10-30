#include "stm32f4xx.h"

#define B_LED 13 			// PC13 Builtin LED
#define Btn 0 				// PA0 push-btn active low

#define SysClk 16000000U 		// 16MHz system clock freq
#define Motor_PWM_Freq 1000U	// 1KHz motor PWM freq
#define Servo_PWM_Freq 50U		// 50Hz servo control freq

#define Motor 8U 			// PA8 Tim1Ch1 PWM
#define Servo 15U 			// PA15 Tim2Ch1 PWM

#define Tx1	9				// PA9 Tx UART1
#define Rx1 10				// PA10 Rx UART1



void B_LED_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; 	// Enable GPIOA clock
	GPIOC->MODER &= ~(3U << (B_LED * 2));	// Clear reg
	GPIOC->MODER |=  (1U << (B_LED * 2));  	// Output mode
	GPIOC->OTYPER |= (1U << B_LED);		// Open drain
	GPIOC->PUPDR &= ~(3U << (B_LED * 2));	// No pull-up or pull-down
}

void Btn_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~(3U << (Btn * 2));    // 00: Input mode
    GPIOA->PUPDR &= ~(3U << (Btn * 2));    // 00: No pull-up, no pull-down
}


void Motor_PWM_Init(void)
{
   // Enable clocks for GPIOA and TIM1
   RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
   RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

   // Configure Motor(PA8) as Alternate Function(AF1)
   GPIOA->MODER &= ~(3U << (Motor * 2));		// Clear register
   GPIOA->MODER |=  (2U << (Motor * 2));       	// Alternate function
   GPIOA->OTYPER &= ~(1U << Motor);            	// Push-pull
   GPIOA->OSPEEDR |= (3U << (Motor * 2));      	// Very high speed
   GPIOA->AFR[1] &= ~(0xFU << 0);				// Clear register
   GPIOA->AFR[1] |=  (1U << 0);            		// AF1 TIM1_CH1

   // Timer configuration
   // Timer frequency = sysclk / (PSC+1) / (ARR+1)
   uint32_t prescaler = (SysClk / 1000000) - 1;			// Timer clock = 1 MHz
   uint32_t period = (1000000 / Motor_PWM_Freq) - 1;	// ARR
   TIM1->PSC = prescaler;								// Prescaler update
   TIM1->ARR = period;									// ARR update
   TIM1->CCR1 = period / 2;  							// default 50% duty cycle

   // PWM mode 1, preload enable
   TIM1->CCMR1 &= ~(7U << 4);			// Clear reg
   TIM1->CCMR1 |= (6U << 4);        	// OC1M = PWM mode 1
   TIM1->CCMR1 |= TIM_CCMR1_OC1PE;    	// Preload enable

   // Enable CH1 output only (no complementary output)
   TIM1->CCER = 0;				// Clear register
   TIM1->CCER |= TIM_CCER_CC1E;	// Only main output

   // Dead-time and main output enable
   TIM1->BDTR = 0;				// Clear register
   TIM1->BDTR |= TIM_BDTR_MOE;	// Enable main output

   // Enable timer
   TIM1->CR1 |= TIM_CR1_ARPE;	// Auto-reload preload enable
   TIM1->CR1 |= TIM_CR1_CEN;	// Start timer
}

void Motor_PWM_SetDutyCycle(uint8_t duty_cycle)
{
   if(duty_cycle > 100) duty_cycle = 100;
   TIM1->CCR1 = ((TIM1->ARR + 1) * duty_cycle) / 100;
}

void Servo_PWM_Init(void)
{
   // Enable clocks for GPIOA and TIM1
   RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
   RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

   // Configure Servo(PA15) as Alternate Function(AF1)
   GPIOA->MODER &= ~(3U << (Servo * 2));		// Clear register
   GPIOA->MODER |=  (2U << (Servo * 2));       	// Alternate function
   GPIOA->OTYPER &= ~(1U << Servo);            	// Push-pull
   GPIOA->OSPEEDR |= (3U << (Servo * 2));      	// Very high speed
   GPIOA->AFR[1] &= ~(0xFU << (4*7));			// Clear register
   GPIOA->AFR[1] |=  (1U << (4*7));            	// AF1 TIM2_CH1

   // Timer configuration
   // Timer frequency = sysclk / (PSC+1) / (ARR+1)
   uint32_t prescaler = (SysClk / 1000) - 1;			// Timer clock = 1 KHz
   uint32_t period = (1000 / Servo_PWM_Freq) - 1;		// ARR
   TIM2->PSC = prescaler;								// Prescaler update
   TIM2->ARR = period;									// ARR update
   TIM2->CCR1 = period / 2;  							// default 50% duty cycle

   // PWM mode 1, preload enable
   TIM2->CCMR1 &= ~(7U << 4);			// Clear reg
   TIM2->CCMR1 |= (6U << 4);        	// OC1M = PWM mode 1
   TIM2->CCMR1 |= TIM_CCMR1_OC1PE;    	// Preload enable

   // Enable CH1 output only (no complementary output)
   TIM2->CCER = 0;				// Clear register
   TIM2->CCER |= TIM_CCER_CC1E;	// Only main output

   // Enable timer
   TIM2->CR1 |= TIM_CR1_ARPE;	// Auto-reload preload enable
   TIM2->CR1 |= TIM_CR1_CEN;	// Start timer
}

void Servo_PWM_SetDutyCycle(uint8_t duty_cycle)
{
   if(duty_cycle > 100) duty_cycle = 100;
   TIM2->CCR1 = ((TIM2->ARR + 1) * duty_cycle) / 100;
}

void UART1_Init(void)
{
	 // Enable clocks for GPIOA and USART1
	 RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;   // GPIOA clock enable
	 RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // USART1 clock enable

	 // Configure PA9 (TX1) and PA10 (RX1) as Alternate Function 7 (AF7)
	 GPIOA->MODER &= ~( (3U << (Tx1 * 2)) | (3U << (Rx1 * 2)) );  // clear
	 GPIOA->MODER |=  ( (2U << (Tx1 * 2)) | (2U << (Rx1 * 2)) );  // AF mode
	 GPIOA->AFR[1] &= ~( (0xFU << ((Tx1 - 8) * 4)) | (0xFU << ((Rx1 - 8) * 4)) );
	 GPIOA->AFR[1] |=  ( (7U << ((Tx1 - 8) * 4)) | (7U << ((Rx1 - 8) * 4)) );  // AF7 for USART1
	 GPIOA->OSPEEDR |= (3U << (Tx1 * 2)) | (3U << (Rx1 * 2));  // High speed

	 // Configure USART1
	 USART1->CR1 = 0;  // disable before config
	 USART1->BRR = 0x0683;  // 9600 baud @16 MHz

	 USART1->CR1 |= (USART_CR1_TE | USART_CR1_RE);  // Enable TX, RX
	 USART1->CR1 |= USART_CR1_UE;                   // Enable USART1
}

void UART1_Send_Char(char c)
{
	while (!(USART1->SR & USART_SR_TXE));  // wait until TX buffer empty
	USART1->DR = (c & 0xFF);
}

void UART1_Send_Str(char *str)
{
	 while(*str)
	 {
		 UART1_Send_Char(*str++);
	 }
}

char UART1_Receive_Char(void)
{
    while (!(USART1->SR & USART_SR_RXNE));  // wait until data received
    return (char)(USART1->DR & 0xFF);
}

void UART1_Receive_Str(char *buffer)
{
    char c;
    uint16_t i = 0;
    do {
        c = UART1_Receive_Char();
        buffer[i++] = c;
    } while (c != '\n' && c != '\r');  // until newline or carriage return
    buffer[i] = '\0';  // null terminate
}

int main(void)
{
//   Motor_PWM_Init();			// Motor PWM init
//   Motor_PWM_SetDutyCycle(50);  // 50% duty cycle
//
//   Servo_PWM_Init();			// Motor PWM init
//   Servo_PWM_SetDutyCycle(80);  // 80% duty cycle

	B_LED_Init();
	Btn_Init();
	while(1)
	{
		if (!(GPIOA->IDR & (1U << Btn)))  // Active LOW
		{
			while (!(GPIOA->IDR & (1U << Btn)));  // Wait for release
			GPIOC->ODR ^= (1U << B_LED);          // Toggle LED (active LOW)
			for (volatile int i = 0; i < 50000; i++); // Debounce
		}
		for (volatile int i = 0; i < 90000; i++); // Debounce
	}
}
