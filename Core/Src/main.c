/* USER CODE BEGIN Header */

/*Лабораторна робота №3  Програмування порту USART (UART) мікроконтролера з
використанням бібліотек CMSIS на основі патерну «стан» з
урахуванням реального часу. */

	// Підключення: 1 GND - [-MORSE_LED+] - [резистор 325R] - PA0
    //              2 GND - [-STATE_LED+] - [резистор 325R] - PC13
	//              3 [USART2_TX] (PA2) - [USART1_RX] (PA10)
	//              4 [USART2_RX] (PA3) - [USART1_TX] (PA9) 

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
    MORSE_IDLE,       // Очікування нового символу для обробки
    MORSE_NEXT_CHAR,	// Підготовка наступного елемента (точки чи тире) у символі
    MORSE_SIGNAL_ON,	// Стан, коли світлодіод світиться
    MORSE_SIGNAL_OFF, // Пауза між точками/тире всередині однієї літери
    MORSE_WAIT_LETTER // Пауза після завершення всієї літери
} MorseState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t msg[] = "Test2"; 		// Пауза після завершення всієї літери
uint8_t tx_index = 0;  			// Індекс поточного символу для передачі
uint32_t last_tx_time = 0;  // Змінна для відстеження часу останньої передачі

// Буфер для прийому
uint8_t rx_buffer[16];			// Кільцевий буфер для зберігання прийнятих байтів
uint8_t rx_read_ptr = 0;		// Вказівник читання з буфера
uint8_t rx_write_ptr = 0; 	// Вказівник запису в буфер

// Налаштування таймінгів Морзе (в мс)
#define UNIT 200 													 // Базова одиниця часу (200 мс) для азбуки Морзе
MorseState_t currentState = MORSE_IDLE;		 // Початковий стан автомата
char const *currentPattern = ""; 					 // Поточна послідовність точок/тире
uint8_t patternIdx = 0;									   // Яку саме точку чи тире ми зараз відтворюємо
uint32_t stateTimer = 0;									 // Таймер для відліку тривалості спалахів та пауз

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Функція отримання кодів Морзе */
const char* GetMorseCode(char c) {
    switch (c) {
        case 'T': case 't': return "-";
        case 'E': case 'e': return ".";
        case 'S': case 's': return "...";
        case '2': return "..---";
        default: return "";
			/*при бажанні, можна додати весь алфавіт та цифри*/
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		
		/* --- 1. ПЕРЕДАЧА (USART2) за патерном Стан/Час --- */
        if (HAL_GetTick() - last_tx_time >= 500) {
            // Перевірка регістра стану (SR) через CMSIS: чи готовий передавач (TXE)
					if (USART2->SR & USART_SR_TXE) { // Прапор TXE показуэ чи порожній апаратний буфер передавача
                USART2->DR = msg[tx_index++]; // Запис у регістр даних
                if (tx_index >= sizeof(msg) - 1) { // Якщо передали весь рядок
                    tx_index = 0;	// Скидаємо індекс на початок
                    last_tx_time = HAL_GetTick(); // Пауза 500мс після всього рядка
                }
            }
        }

        /* --- 2. ПРИЙОМ (USART1) через CMSIS --- */
        // Перевірка чи прийшов байт (RXNE - Receive Not Empty)
        if (USART1->SR & USART_SR_RXNE) { // Якщо прапор RXNE = 1 (дані прийшли)
            uint8_t received = USART1->DR; // Зчитування з регістра даних автоматично скидає прапор RXNE, готуючи його до наступного символу.
            rx_buffer[rx_write_ptr++] = received; // Записуємо в наш буфер
            if (rx_write_ptr >= 16) rx_write_ptr = 0; // Зациклюємо буфер (кільцевий тип)
        }

        /* --- 3. АВТОМАТ СТАНІВ МОРЗЕ (PD13) --- */
        switch (currentState) {
            case MORSE_IDLE:
                if (rx_read_ptr != rx_write_ptr) { // Якщо в буфері є дані, беремо символ
                    HAL_GPIO_WritePin(STATE_LED_GPIO_Port, STATE_LED_Pin, GPIO_PIN_SET); 
									
										char c = rx_buffer[rx_read_ptr++]; // Беремо символ
                    if (rx_read_ptr >= 16) rx_read_ptr = 0;
                    currentPattern = GetMorseCode(c); // Отримуємо код (напр. "...")
                    patternIdx = 0;
                    currentState = MORSE_NEXT_CHAR; // Переходимо до обробки літери
                }
                break;

            case MORSE_NEXT_CHAR: 
                if (currentPattern[patternIdx] != '\0') { // Перевіряємо, чи є ще точки/тире в цій літері
                    HAL_GPIO_WritePin(MORSE_LED_GPIO_Port, MORSE_LED_Pin, GPIO_PIN_SET);
										stateTimer = HAL_GetTick(); // Фіксуємо час увімкнення 
                    currentState = MORSE_SIGNAL_ON;
								} else { // Якщо літера закінчилась  
									  stateTimer = HAL_GetTick(); // Скидаємо час
                    currentState = MORSE_WAIT_LETTER; // Та стан
                }
                break;

            case MORSE_SIGNAL_ON:
						{
                // Визначаємо тривалість: крапка = 1 UNIT, тире = 3 UNITs
                uint32_t duration = (currentPattern[patternIdx] == '.') ? UNIT : (UNIT * 3);
                if (HAL_GetTick() - stateTimer >= duration) {
                    HAL_GPIO_WritePin(MORSE_LED_GPIO_Port, MORSE_LED_Pin, GPIO_PIN_RESET);
                    stateTimer = HAL_GetTick();
                    currentState = MORSE_SIGNAL_OFF; // Режим паузи
                }
						}
            break;

            case MORSE_SIGNAL_OFF:
                // Пауза між сигналами однієї літери = 1 UNIT
                if (HAL_GetTick() - stateTimer >= UNIT) {
									patternIdx++; // Переходимо до наступної точки/тире
                    currentState = MORSE_NEXT_CHAR; 
                }
                break;

            case MORSE_WAIT_LETTER:
                // Пауза між літерами = 3 UNITs
                if (HAL_GetTick() - stateTimer >= (UNIT * 3)) {
										if (rx_read_ptr == rx_write_ptr) {
												HAL_GPIO_WritePin(STATE_LED_GPIO_Port, STATE_LED_Pin, GPIO_PIN_RESET); 
										}
                    currentState = MORSE_IDLE;
                }
                break;
        }
		
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STATE_LED_GPIO_Port, STATE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MORSE_LED_GPIO_Port, MORSE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : STATE_LED_Pin */
  GPIO_InitStruct.Pin = STATE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(STATE_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MORSE_LED_Pin */
  GPIO_InitStruct.Pin = MORSE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MORSE_LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
