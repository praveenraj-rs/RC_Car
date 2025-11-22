// Eval6 UART Tx

#include "stm32f4xx.h"
#include "string.h"

#define SysClk 25000000U 	// 25MHz HSE system clock freq

#define B_LED 13U 			// PC13 Built-in LED
#define Btn 0U 				// PA0 push-btn active low

#define Tx1	9				// PA9 Tx UART1
#define Rx1 10				// PA10 Rx UART1


// Function Prototyping
void SystemClock_Init(void);

void B_LED_Init(void);
void Btn_Init(void);

void TIM3_Delay(uint16_t delay_ms);

void UART1_Init(void);
void UART1_Send_Char(char c);
void UART1_Send_Str(char *str);
char UART1_Receive_Char(void);
void UART1_Receive_Str(char *str);

int main(void)
{
	SystemClock_Init(); // Selecting HSE 25MHz

	B_LED_Init();
	Btn_Init();

	UART1_Init();

	char master_cmd_on[]="M LED ON\n";
	char master_cmd_off[]="M LED OFF\n";

	char slave_res_on[] = "S LED ON\n";
	char slave_res_off[] = "S LED OFF\n";

	char buffer[20];

	while(1)
	{
		if (!(GPIOA->IDR & (1<<Btn)))
		{
			while(!(GPIOA->IDR & (1<<Btn))){}
			TIM3_Delay(20);

			if (!(GPIOC->ODR & 1<<B_LED)) // Check LED is ON
			{
				UART1_Send_Str(master_cmd_off);
				UART1_Receive_Str(buffer);

				if (strcmp(slave_res_off, buffer)==0)
				{
					GPIOC->BSRR |= (1<<(B_LED)); 	// Toggle LED OFF
				}
			}

			else if (GPIOC->ODR & 1<<B_LED) // Check LED is OFF
			{
				UART1_Send_Str(master_cmd_on);
				UART1_Receive_Str(buffer);

				if (strcmp(slave_res_on, buffer)==0)
				{
					GPIOC->BSRR |= (1<<(B_LED+16)); 	// Toggle LED ON
				}
			}
		}

		TIM3_Delay(100);
	}
}


void SystemClock_Init(void)
{
   // Enable HSE
   RCC->CR |= RCC_CR_HSEON;
   while (!(RCC->CR & RCC_CR_HSERDY)) { /* Wait until ready */ }

   // Set Flash latency for 25 MHz (0 WS is fine)
   FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_0WS;

   // Set AHB, APB1, and APB2 prescalers = 1
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
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; 	// Enable GPIOA clock
	GPIOC->MODER &= ~(3U << (B_LED * 2));	// Clear reg
	GPIOC->MODER |=  (1U << (B_LED * 2));  	// Output mode
	GPIOC->OTYPER &= ~(1U << B_LED);		// Push-pull
	GPIOC->PUPDR &= ~(3U << (B_LED * 2));	// No pull-up or pull-down
	GPIOC->ODR |= (1<<B_LED);				// LED off
}

void Btn_Init(void)
{
   RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
   GPIOA->MODER &= ~(3U << (Btn * 2));    // 00: Input mode
   GPIOA->PUPDR &= ~(3U << (Btn * 2));    // Clear reg
   GPIOA->PUPDR |= (1U << (Btn * 2));    // Pull-up config
}

void TIM3_Delay(uint16_t delay_ms)
{
	 RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

	 uint32_t prescaler = (SysClk / 1000) - 1;	// Timer clock = 1 KHz or 1ms
	 TIM3->PSC = prescaler - 1;					// Prescaler update

	 TIM3->ARR = delay_ms - 1;   			// Auto-reload 1 second delay (1ms * 999)
	 TIM3->CNT = 0;							// Reset the counter
	 TIM3->EGR |= (1 << 0);  				// Event generation

	 TIM3->SR &= ~ (1 << 0);  				// Clear update interrupt flag
	 TIM3->CR1 |= (1 << 0); 				// Enable Timer

	 while (!(TIM3->SR & (1 << 0))){} 		// Until UIF NEQ 1
	 TIM3->SR &= ~(1 << 0); 				//UIF is cleared manually
	 TIM3->CR1 &= ~(1 << 0); 				//CEN is cleared
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
	 USART1->BRR = 0xA2C;  // 9600 baud @25 MHz
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
