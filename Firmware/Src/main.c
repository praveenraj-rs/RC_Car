#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define SysClk 25000000U 	// 25MHz HSE system clock frequency

#define B_LED 13U 			// PC13 Built-in LED
#define Btn 0U 				// PA0 push-btn active low

#define Motor 8U			// PA8 Tim1Ch1 PWM
#define Servo 15U 			// PA15 Tim2Ch1 PWM

#define Motor_PWM_Freq 1000	// 1KHz motor PWM frequency
#define Servo_PWM_Freq 50	// 50Hz servo control frequency

#define Tx1	9				// PA9 Tx UART1
#define Rx1 10				// PA10 Rx UART1

#define Motor_DC1	12		// PB12 Motor Direction Control
#define Motor_DC2	13		// PB13 Motor Direction Control

#define Car_Reset_Steer_Angle	60	// Straight facing
#define Car_Reset_Throttle		0	// No Throttle
#define Car_Reset_Direction 	0 	// Stop Condition


// Function Prototyping
void SystemClock_Init(void);

void B_LED_Init(void);
void Btn_Init(void);

void TIM3_Delay(uint16_t delay_ms);

void Motor_TIM1_PWM_Init(void);
void Motor_TIM1_PWM_SetDutyCycle(uint8_t duty_cycle);

void Servo_TIM2_PWM_Init(void);
void Servo_TIM2_PWM_SetDutyCycle(uint8_t duty_cycle);
void Servo_TIM2_PWM_SetAngle(uint8_t angle);

void UART1_Init(void);
void UART1_Send_Char(char c);
void UART1_Send_Str(char *str);
char UART1_Receive_Char(void);
void UART1_Receive_Str(char *str);
bool UART1_Receive_Packet(uint8_t *steer, uint8_t *throttle, uint8_t *dir);

void Motor_Direction_Control_Init(void);
void Motor_Direction_Control(uint8_t Direction);

void Car_Control(uint8_t Steer,uint8_t Throttle, uint8_t Dir);

// Checking/Testing Functions
void CK_LED_Blink(void);	// LED blink
void CK_LED_Btn(void); 		// Toggle LED with button press
void CK_Servo(void);		// Check 9g servo

// Checking/Testing Car Functions
void CK_Car_Servo(void);	// Car servo check

int main(void)
{
	// Initialization
	SystemClock_Init(); 				// Selecting HSE 25MHz
	Motor_TIM1_PWM_Init();				// Motor PWM initialization
	Servo_TIM2_PWM_Init();				// Motor PWM initialization
	UART1_Init();						// UART1 initialization
	Motor_Direction_Control_Init();		// Motor Direction GPIO Initialization

	// Reset Condition
	Car_Control(Car_Reset_Steer_Angle, Car_Reset_Throttle, Car_Reset_Direction);

	uint8_t steer, throttle, dir;		// Initializing the variables

	while(1)
	{
	    if (UART1_Receive_Packet(&steer, &throttle, &dir))	// Checking Control Commands
	    {
	        Car_Control(steer, throttle, dir);				// Controlling the Car
	    }
	}
}


void SystemClock_Init(void)
{
   // Enable HSE
   RCC->CR |= RCC_CR_HSEON;
   while (!(RCC->CR & RCC_CR_HSERDY)) { /* Wait until ready */ }

   // Set Flash latency for 25 MHz (0 WS is fine)
   FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_0WS;

   // Set AHB, APB1, and APB2 pre-scalers = 1
   RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
   RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;

   // Select HSE as system clock
   RCC->CFGR &= ~RCC_CFGR_SW;
   RCC->CFGR |= RCC_CFGR_SW_HSE;

   // Wait until HSE is used as system clock
   while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE) { /* Wait */ }
}

void B_LED_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; 	// Enable GPIOC clock
	GPIOC->MODER &= ~(3U << (B_LED * 2));	// Clear register
	GPIOC->MODER |=  (1U << (B_LED * 2));  	// 01: Output mode
	GPIOC->OTYPER &= ~(1U << B_LED);		// Push-pull
	GPIOC->PUPDR &= ~(3U << (B_LED * 2));	// No pull-up or pull-down
	GPIOC->ODR |= (1<<B_LED);				// LED off
}

void Btn_Init(void)
{
   RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;		// Enable GPIOA clock
   GPIOA->MODER &= ~(3U << (Btn * 2));    	// 00: Input mode
   GPIOA->PUPDR &= ~(3U << (Btn * 2));    	// Clear register
   GPIOA->PUPDR |= (1U << (Btn * 2));    	// Pull-up configuration
}

void TIM3_Delay(uint16_t delay_ms)
{
	 RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;		// Enable TIM3 Clock

	 uint32_t prescaler = (SysClk / 1000) - 1;	// Timer clock = 1 KHz or 1ms
	 TIM3->PSC = prescaler - 1;					// Pre-scaler update

	 TIM3->ARR = delay_ms - 1;   			// Auto-reload 1 second delay (1ms * 999)
	 TIM3->CNT = 0;							// Reset the counter
	 TIM3->EGR |= (1 << 0);  				// Event generation

	 TIM3->SR &= ~ (1 << 0);  				// Clear update interrupt flag
	 TIM3->CR1 |= (1 << 0); 				// Enable Timer

	 while (!(TIM3->SR & (1 << 0))){} 		// Until UIF NEQ 1
	 TIM3->SR &= ~(1 << 0); 				//UIF is cleared manually
	 TIM3->CR1 &= ~(1 << 0); 				//CEN is cleared
}

void Motor_TIM1_PWM_Init(void)
{
  // Enable clocks for GPIOA and TIM1
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

  // Configure Motor(PA8) as Alternate Function(AF1)
  GPIOA->MODER &= ~(3U << (Motor * 2));			// 00: Clear register
  GPIOA->MODER |=  (2U << (Motor * 2));       	// 10: Alternate function
  GPIOA->OTYPER &= ~(1U << Motor);            	// 01: Push-pull
  GPIOA->OSPEEDR |= (3U << (Motor * 2));      	// 11: Very high speed
  GPIOA->AFR[1] &= ~(0xFU << 0);				// 00: Clear register
  GPIOA->AFR[1] |=  (1U << 0);            		// 01: AF1 TIM1_CH1

  // Timer configuration
  // Timer frequency = sysclk / (PSC+1) / (ARR+1)
  uint32_t prescaler = (SysClk / 1000000) - 1;			// Timer clock = 1 MHz
  uint32_t period = (1000000 / Motor_PWM_Freq) - 1;		// ARR
  TIM1->PSC = prescaler;								// Pre-scaler update
  TIM1->ARR = period;									// ARR update
  TIM1->CCR1 = 0;  										// 0% duty cycle

  // PWM mode 1, pre-load enable
  TIM1->CCMR1 &= ~(7U << 4);			// Clear register
  TIM1->CCMR1 |= (6U << 4);        		// OC1M = PWM mode 1
  TIM1->CCMR1 |= TIM_CCMR1_OC1PE;    	// Pre-load enable

  // Enable CH1 output only (no complementary output)
  TIM1->CCER = 0;				// Clear register
  TIM1->CCER |= TIM_CCER_CC1E;	// Only main output

  // Dead-time and main output enable
  TIM1->BDTR = 0;				// Clear register
  TIM1->BDTR |= TIM_BDTR_MOE;	// Enable main output

  // Enable timer
  TIM1->CR1 |= TIM_CR1_ARPE;	// Auto-reload pre-load enable
  TIM1->CR1 |= TIM_CR1_CEN;		// Start timer
}

void Motor_TIM1_PWM_SetDutyCycle(uint8_t duty_cycle)
{
  if(duty_cycle > 100) duty_cycle = 100;
  TIM1->CCR1 = ((TIM1->ARR + 1) * duty_cycle) / 100;
}

void Servo_TIM2_PWM_Init(void)
{
  // Enable clocks for GPIOA and TIM1
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

  // Configure Servo(PA15) as Alternate Function(AF1)
  GPIOA->MODER &= ~(3U << (Servo * 2));			// Clear register
  GPIOA->MODER |=  (2U << (Servo * 2));       	// Alternate function
  GPIOA->OTYPER &= ~(1U << Servo);            	// Push-pull
  GPIOA->OSPEEDR |= (3U << (Servo * 2));      	// Very high speed
  GPIOA->AFR[1] &= ~(0xFU << (4*7));			// Clear register
  GPIOA->AFR[1] |=  (1U << (4*7));            	// AF1 TIM2_CH1

  // Timer configuration
  // Timer frequency = sysclk / (PSC+1) / (ARR+1)
  uint32_t prescaler = (SysClk / 1000000) - 1;			// Timer clock = 1MHz
  uint32_t period = (1000000 / Servo_PWM_Freq) - 1;		// ARR
  TIM2->PSC = prescaler;								// Pre-scaler update
  TIM2->ARR = period;									// ARR update
  TIM2->CCR1 = period / 2;  							// default 50% duty cycle

  // PWM mode 1, pre-load enable
  TIM2->CCMR1 &= ~(7U << 4);			// Clear register
  TIM2->CCMR1 |= (6U << 4);        		// OC1M = PWM mode 1
  TIM2->CCMR1 |= TIM_CCMR1_OC1PE;    	// Pre-load enable

  // Enable CH1 output only (no complementary output)
  TIM2->CCER = 0;				// Clear register
  TIM2->CCER |= TIM_CCER_CC1E;	// Only main output
  // Enable timer
  TIM2->CR1 |= TIM_CR1_ARPE;	// Auto-reload pre-load enable
  TIM2->CR1 |= TIM_CR1_CEN;		// Start timer
}

void Servo_TIM2_PWM_SetDutyCycle(uint8_t duty_cycle)
{
	if(duty_cycle > 100) duty_cycle = 100;
	TIM2->CCR1 = ((TIM2->ARR + 1) * duty_cycle) / 100;
}

void Servo_TIM2_PWM_SetAngle(uint8_t angle)
{

//	#define MIN_PULSE_WIDTH       544     // the shortest pulse sent to a servo
//	#define MAX_PULSE_WIDTH      2400     // the longest pulse sent to a servo
//	#define DEFAULT_PULSE_WIDTH  1500     // default pulse width when servo is attached
//	#define REFRESH_INTERVAL    20000     // minimum time to refresh servos in microseconds

	if(angle < 0) 	angle = 0;
	if(angle > 180) angle = 180;

	// CCR =  MIN_PULSE_WIDTH + ((MAX_PULSE_WIDTH - MIN_PULSE_WIDTH)/180)*angle
	// TIM2->CCR1 = 544 + (10.312*angle);
	 TIM2->CCR1 = 544 + (((2400-544)/180)*angle);
}

void UART1_Init(void)
{
	 // Enable clocks for GPIOA and USART1
	 RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;   // GPIOA clock enable
	 RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // USART1 clock enable

	 // Configure PA9 (TX1) and PA10 (RX1) as Alternate Function 7 (AF7)
	 GPIOA->MODER &= ~( (3U << (Tx1 * 2)) | (3U << (Rx1 * 2)) );  					// 00: Clear register
	 GPIOA->MODER |=  ( (2U << (Tx1 * 2)) | (2U << (Rx1 * 2)) );  					// 10: AF mode
	 GPIOA->AFR[1] &= ~( (0xFU << ((Tx1 - 8) * 4)) | (0xFU << ((Rx1 - 8) * 4)) );	// 00: Clear register
	 GPIOA->AFR[1] |=  ( (7U << ((Tx1 - 8) * 4)) | (7U << ((Rx1 - 8) * 4)) );  		// AF7 for USART1
	 GPIOA->OSPEEDR |= (3U << (Tx1 * 2)) | (3U << (Rx1 * 2));  						// High speed

	 // Configure USART1
	 USART1->CR1 = 0;  								// Disable before configuration
	 USART1->BRR = 0xA2C;  							// 9600 baud @25 MHz
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
   } while (c != '\n' && c != '\r');  	// until newline or carriage return
   buffer[i] = '\0';  					// null terminate
}

bool UART1_Receive_Packet(uint8_t *steer, uint8_t *throttle, uint8_t *dir)
{
	// Packet: <S,45,0,0>
    static char buffer[32];
    static uint8_t index = 0;

    while (USART1->SR & USART_SR_RXNE)      // Check data available
    {
        char c = USART1->DR & 0xFF;

        if (c == '<') {
            index = 0;
        }
        else if (c == '>') {
            buffer[index] = '\0';

            // Parse: S,steer,throttle,direction
            char *tok = strtok(buffer, ","); // skip 'S'
            tok = strtok(NULL, ","); *steer = atoi(tok);
            tok = strtok(NULL, ","); *throttle = atoi(tok);
            tok = strtok(NULL, ","); *dir = atoi(tok);

            return 1;   // Packet ready
        }
        else {
            if (index < 31)
                buffer[index++] = c;
        }
    }

    return 0; // No complete packet yet
}

void Motor_Direction_Control_Init(void)
{
	// Motor_DC1 - PB12, Motor_DC2 - PB13
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; 	// Enable GPIOB clock

	GPIOB->MODER &= ~(3U << (Motor_DC1 * 2));	// 00: Clear register
	GPIOB->MODER &= ~(3U << (Motor_DC2 * 2));	// 00: Clear register

	GPIOB->MODER |=  (1U << (Motor_DC1 * 2));  	// 01: Output mode
	GPIOB->MODER |=  (1U << (Motor_DC2 * 2));  	// 01: Output mode

	GPIOB->OTYPER &= ~(1U << Motor_DC1);		// Push-pull
	GPIOB->OTYPER &= ~(1U << Motor_DC2);		// Push-pull

	GPIOB->PUPDR &= ~(3U << (Motor_DC1 * 2));	// No pull-up or pull-down
	GPIOB->PUPDR &= ~(3U << (Motor_DC2 * 2));	// No pull-up or pull-down

	// Motor in OFF Condition
	// PB12 - Low && PB13 - Low
	GPIOB->ODR &= ~(1<<Motor_DC1);				// Motor_DC1 low
	GPIOB->ODR &= ~(1<<Motor_DC2);				// Motor_DC2 low
}


void Motor_Direction_Control(uint8_t Direction)
{
	// Directions
	// 0 = Stop
	// 1 = Forward
	// 2 = Backward

	if (Direction==0)
	{
		GPIOB->ODR &= ~(1<<Motor_DC1);				// Motor_DC1 low
		GPIOB->ODR &= ~(1<<Motor_DC2);				// Motor_DC2 low
	}

	else if (Direction==1)
	{
		GPIOB->ODR |= (1<<Motor_DC1);				// Motor_DC1 high
		GPIOB->ODR &= ~(1<<Motor_DC2);				// Motor_DC2 low
	}

	else if (Direction==2)
	{
		GPIOB->ODR &= ~(1<<Motor_DC1);				// Motor_DC1 low
		GPIOB->ODR |= (1<<Motor_DC2);				// Motor_DC2 high
	}
}


// ----------------------------------------------------
// Testing Functions
// ----------------------------------------------------

void CK_LED_Blink(void)
{
	GPIOC->ODR ^= (1<<B_LED);
	TIM3_Delay(1000);
}

void CK_LED_Btn(void)
{
	if (!(GPIOA->IDR & (1<<Btn)))
	{
		while(!(GPIOA->IDR & (1<<Btn))){}
		TIM3_Delay(20);
		GPIOC->ODR ^= (1<<B_LED);
	}
}

void CK_Servo(void)
{
	for (int angle = 0 ; angle<=180; angle+=30)
	{
		Servo_TIM2_PWM_SetAngle(angle);
		TIM3_Delay(50);
	}

	TIM3_Delay(500);

	for (int angle = 180; angle>=0; angle-=30)
	{
		Servo_TIM2_PWM_SetAngle(angle);
		TIM3_Delay(50);
	}
	TIM3_Delay(500);
}

void CK_Car_Servo(void)
{
	// 0  - Go Left
	// 45 - Go Straight
	// 90 -	Go Right

	int max_angle = 90;

	for (int angle = 0 ; angle<=max_angle; angle+=30)
	{
		Servo_TIM2_PWM_SetAngle(angle);
		TIM3_Delay(50);
	}

	TIM3_Delay(500);

	for (int angle = max_angle; angle>=0; angle-=30)
	{
		Servo_TIM2_PWM_SetAngle(angle);
		TIM3_Delay(50);
	}
	TIM3_Delay(500);
}


void Car_Control(uint8_t Steer,uint8_t Throttle, uint8_t Dir)
{
	// Steer: 		0-90, 	0-Left, 45-Straight, 90-Right
	// Throttle: 	0-100
	// Direction:	0-Stop, 1-Forward, 2-Backward

	Servo_TIM2_PWM_SetAngle(Steer);
	Motor_Direction_Control(Dir);
	Motor_TIM1_PWM_SetDutyCycle(Throttle);
}




