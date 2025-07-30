/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void);

typedef enum {
    SLOT_A = 0,
    SLOT_B = 1
} app_slot_t;

typedef struct {
	uint32_t is_valid;    // 0xA5A5A5A5 if metadata is valid
	uint32_t version;
    uint32_t active_slot; // SLOT_A or SLOT_B
    uint32_t crc;         // Expected CRC32 of active image
    uint32_t image_size;  // Size of active image in bytes
    uint32_t reserved[2]; // Reserved for future use
} BootMetadata_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
extern uint32_t _estack;  // Defined in linker script
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define SLOT_A_ADDR   0x08010000
#define SLOT_B_ADDR   0x08040000
#define METADATA_ADDR 0x0800C000

// STM32F446RE RAM range: 0x20000000 - 0x2001FFFF (128KB)
#define RAM_START     0x20000000
#define RAM_END       0x20020000
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern int _bflag;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void jump_to_application(uint32_t app_address);
uint32_t calculate_crc32(uint32_t *data, uint32_t length_words);
uint32_t calculate_flash_crc(uint32_t start_addr, uint32_t size_bytes);
uint8_t validate_crc(uint32_t app_addr, uint32_t expected_crc, uint32_t image_size);
uint8_t is_valid_application(uint32_t app_addr);
HAL_StatusTypeDef initialize_first_boot_metadata();
void check_and_clear_flash_protection();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void log(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

// Note: Metadata is now dynamically managed in flash
// Initial metadata should be written by OTA process or flash programmer

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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  log("Bootloader started...\r\n");
  
  // Check and clear any flash protection that might interfere with programming
  check_and_clear_flash_protection();

  // Read metadata from flash (not from const section)
  BootMetadata_t *metadata = (BootMetadata_t *)METADATA_ADDR;
  
  char debug_buf[150];
  sprintf(debug_buf, "Reading metadata from 0x%08X\r\n", (unsigned int)METADATA_ADDR);
  log(debug_buf);

  if (metadata->is_valid != 0xA5A5A5A5) {
	  sprintf(debug_buf, "Metadata invalid! Read value: 0x%08X (expected: 0xA5A5A5A5)\r\n", 
	          (unsigned int)metadata->is_valid);
	  log(debug_buf);
	  
	  // First boot - initialize metadata by scanning for valid applications
	  HAL_StatusTypeDef init_status = initialize_first_boot_metadata();
	  if (init_status != HAL_OK) {
		  log("Failed to initialize metadata! Defaulting to Slot A\r\n");
		  jump_to_application(SLOT_A_ADDR);
	  }
	  
	  // Re-read metadata after initialization
	  metadata = (BootMetadata_t *)METADATA_ADDR;
	  if (metadata->is_valid != 0xA5A5A5A5) {
		  log("Metadata still invalid after initialization! Defaulting to Slot A\r\n");
		  jump_to_application(SLOT_A_ADDR);
	  }
	  
	  log("Metadata initialized successfully, continuing with normal boot...\r\n");
  }

  char log_buf[100];
  sprintf(log_buf, "Metadata: ver=0x%08X, slot=%lu, crc=0x%08X, size=%lu\r\n", 
          (unsigned int)metadata->version, metadata->active_slot, 
          (unsigned int)metadata->crc, metadata->image_size);
  log(log_buf);
  
  uint32_t target_address = (metadata->active_slot == SLOT_B) ? SLOT_B_ADDR : SLOT_A_ADDR;
  sprintf(log_buf, "Target slot: %s (0x%08X)\r\n", 
          (metadata->active_slot == SLOT_B) ? "SLOT_B" : "SLOT_A", 
          (unsigned int)target_address);
  log(log_buf);

  // CRC validation of target slot
  if (!validate_crc(target_address, metadata->crc, metadata->image_size)) {
	  log("CRC validation failed! Falling back to Slot A\r\n");
	  target_address = SLOT_A_ADDR;
	  
	  // Also validate Slot A as fallback (skip CRC if different from target)
	  if (metadata->active_slot == SLOT_B) {
		  if (!validate_crc(SLOT_A_ADDR, 0xFFFFFFFF, 0)) {
			  log("Fallback validation failed! Boot may fail.\r\n");
		  }
	  }
  }

  sprintf(log_buf, "Final target: 0x%08X\r\n", (unsigned int)target_address);
  log(log_buf);
  log("Jumping to application...\r\n");
  jump_to_application(target_address);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
  huart2.Init.BaudRate = 115200;
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void jump_to_application(uint32_t app_address)
{
    // Read vector table from application
    uint32_t sp = *(volatile uint32_t *)app_address;
    uint32_t reset_handler = *(volatile uint32_t *)(app_address + 4);

    char log_buf[100];

    //App jump debug logs
    log("Reading vector table:\r\n");
    sprintf(log_buf, "  App Address: 0x%08X\r\n", (unsigned int)app_address);
    log(log_buf);
    sprintf(log_buf, "  Stack Pointer: 0x%08X\r\n", (unsigned int)sp);
    log(log_buf);
    sprintf(log_buf, "  Reset Handler: 0x%08X\r\n", (unsigned int)reset_handler);
    log(log_buf);


    // Validate the stack pointer (should be in RAM range)
    if ((sp < RAM_START) || (sp > RAM_END)) {
    	log("Invalid stack pointer! Using bootloader stack.\r\n");
        sp = (uint32_t)&_estack;  // Use bootloader's stack if app stack is invalid
        sprintf(log_buf, "  Using bootloader stack: 0x%08X\r\n", (unsigned int)sp);
        log(log_buf);
    }

    // Validate reset handler (should be in flash range and odd for Thumb)
    if ((reset_handler < 0x08000000) || (reset_handler >= 0x08080000) || ((reset_handler & 0x1) == 0)) {
    	log("Invalid reset handler address!\r\n");
        Error_Handler();
    }

    log("Preparing for jump:\r\n");
    
    // Disable all interrupts
    __disable_irq();
    
    // Deinitialize HAL and peripherals
    HAL_UART_DeInit(&huart2);
    HAL_DeInit();

    // Clear all pending interrupts
    for (int i = 0; i < 8; i++) {
    	NVIC->ICER[i] = 0xFFFFFFFF;  // Disable interrupts
        NVIC->ICPR[i] = 0xFFFFFFFF;   // Clear pending interrupts
    }

    // Data Synchronization Barrier
    __DSB();
    
    // Set Vector Table Offset Register to point to application
    SCB->VTOR = app_address;
    
    // Instruction Synchronization Barrier
    __ISB();

    // Set Main Stack Pointer
    __set_MSP(sp);
    
    // Jump to application
    pFunction app_entry = (pFunction)reset_handler;
    app_entry();
    
    // Should never reach here
    while(1);
}

uint32_t calculate_crc32(uint32_t *data, uint32_t length_words)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length_words; i++) {
        uint32_t word = data[i];
        
        // Process each byte in the word
        for (int byte = 0; byte < 4; byte++) {
            uint8_t current_byte = (word >> (byte * 8)) & 0xFF;
            crc ^= current_byte;
            
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320; // CRC32 polynomial
                } else {
                    crc >>= 1;
                }
            }
        }
    }
    
    return ~crc;
}

uint32_t calculate_flash_crc(uint32_t start_addr, uint32_t size_bytes)
{
    // Ensure size is word-aligned
    uint32_t size_words = (size_bytes + 3) / 4;
    uint32_t *flash_ptr = (uint32_t *)start_addr;
    
    char log_buf[100];
    sprintf(log_buf, "Calculating CRC: addr=0x%08X, size=%lu bytes (%lu words)\r\n", 
            (unsigned int)start_addr, size_bytes, size_words);
    log(log_buf);
    
    return calculate_crc32(flash_ptr, size_words);
}

uint8_t is_valid_application(uint32_t app_addr)
{
    // Read the stack pointer and reset handler from the application's vector table
    uint32_t sp = *(volatile uint32_t *)app_addr;
    uint32_t reset_handler = *(volatile uint32_t *)(app_addr + 4);
    
    // Validate stack pointer (should be in RAM range)
    if ((sp < RAM_START) || (sp > RAM_END)) {
        return 0;
    }
    
    // Validate reset handler (should be in flash range and odd for Thumb mode)
    if ((reset_handler < 0x08000000) || (reset_handler >= 0x08080000) || ((reset_handler & 0x1) == 0)) {
        return 0;
    }
    
    // Check if the application area is not all 0xFF (erased flash)
    uint32_t *app_data = (uint32_t *)app_addr;
    uint8_t all_erased = 1;
    for (int i = 0; i < 64; i++) { // Check first 256 bytes
        if (app_data[i] != 0xFFFFFFFF) {
            all_erased = 0;
            break;
        }
    }
    
    return !all_erased;
}

HAL_StatusTypeDef initialize_first_boot_metadata()
{
    char log_buf[150];
    log("First boot detected - scanning for valid application...\r\n");
    
    uint8_t slot_a_valid = is_valid_application(SLOT_A_ADDR);
    uint8_t slot_b_valid = is_valid_application(SLOT_B_ADDR);
    
    sprintf(log_buf, "Slot A (0x%08X): %s\r\n", (unsigned int)SLOT_A_ADDR, 
            slot_a_valid ? "VALID" : "INVALID");
    log(log_buf);
    sprintf(log_buf, "Slot B (0x%08X): %s\r\n", (unsigned int)SLOT_B_ADDR, 
            slot_b_valid ? "VALID" : "INVALID");
    log(log_buf);
    
    // Create default metadata structure
    BootMetadata_t new_metadata;
    new_metadata.is_valid = 0xA5A5A5A5;
    new_metadata.version = 0x00010001;
    new_metadata.active_slot = SLOT_A;  // Default to Slot A
    new_metadata.crc = 0xFFFFFFFF;      // No CRC check initially
    new_metadata.image_size = 0;        // Unknown size
    new_metadata.reserved[0] = 0;
    new_metadata.reserved[1] = 0;
    
    // Determine which slot to use
    if (slot_a_valid && !slot_b_valid) {
        new_metadata.active_slot = SLOT_A;
        log("Using Slot A as active slot\r\n");
    } else if (!slot_a_valid && slot_b_valid) {
        new_metadata.active_slot = SLOT_B;
        log("Using Slot B as active slot\r\n");
    } else if (slot_a_valid && slot_b_valid) {
        new_metadata.active_slot = SLOT_A; // Prefer Slot A if both valid
        log("Both slots valid - defaulting to Slot A\r\n");
    } else {
        log("No valid application found! Defaulting to Slot A\r\n");
        new_metadata.active_slot = SLOT_A;
    }
    
    // Write metadata to flash
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log("Flash unlock failed for metadata initialization\r\n");
        return status;
    }
    
    // Erase metadata sector
    FLASH_EraseInitTypeDef eraseInit = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        .Sector = FLASH_SECTOR_3,
        .NbSectors = 1
    };
    
    uint32_t sectorError;
    status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    if (status != HAL_OK) {
        sprintf(log_buf, "Metadata sector erase failed: %d\r\n", status);
        log(log_buf);
        HAL_FLASH_Lock();
        return status;
    }
    
    // Write metadata word by word
    uint32_t *metadata_ptr = (uint32_t *)&new_metadata;
    uint32_t metadata_words = sizeof(BootMetadata_t) / 4;
    
    for (uint32_t i = 0; i < metadata_words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                                   METADATA_ADDR + (i * 4), 
                                   metadata_ptr[i]);
        if (status != HAL_OK) {
            sprintf(log_buf, "Metadata write failed at word %lu\r\n", i);
            log(log_buf);
            HAL_FLASH_Lock();
            return status;
        }
    }
    
    HAL_FLASH_Lock();
    log("First boot metadata initialized successfully\r\n");
    return HAL_OK;
}

void check_and_clear_flash_protection()
{
    // Check current RDP level
    uint8_t rdp_level = (FLASH->OPTCR & FLASH_OPTCR_RDP) >> FLASH_OPTCR_RDP_Pos;
    
    char log_buf[100];
    sprintf(log_buf, "Flash RDP level: 0x%02X\r\n", rdp_level);
    log(log_buf);
    
    // Check write protection status
    uint32_t wrp_sectors = (FLASH->OPTCR & FLASH_OPTCR_nWRP) >> FLASH_OPTCR_nWRP_Pos;
    sprintf(log_buf, "Write protection: 0x%03X (0=protected, 1=unprotected)\r\n", wrp_sectors);
    log(log_buf);
    
    // Only clear protection if there are issues (avoid unnecessary flash cycles)
    if (rdp_level != 0xAA || wrp_sectors != 0xFFF) {
        log("Flash protection detected - will be cleared during OTA operations\r\n");
    } else {
        log("Flash protection: OK\r\n");
    }
}

uint8_t validate_crc(uint32_t app_addr, uint32_t expected_crc, uint32_t image_size)
{
    if (expected_crc == 0xFFFFFFFF) {
        log("CRC validation skipped (no expected CRC)\r\n");
        return 1; // Skip validation if no CRC is set
    }
    
    // Use provided image size, or default to 192KB if not specified
    uint32_t app_size = (image_size > 0 && image_size <= (192 * 1024)) ? 
                        image_size : (192 * 1024);
    
    uint32_t calculated_crc = calculate_flash_crc(app_addr, app_size);
    
    char log_buf[100];
    sprintf(log_buf, "CRC check: expected=0x%08X, calculated=0x%08X (size=%lu)\r\n", 
            (unsigned int)expected_crc, (unsigned int)calculated_crc, app_size);
    log(log_buf);
    
    if (calculated_crc == expected_crc) {
        log("CRC validation: PASS\r\n");
        return 1;
    } else {
        log("CRC validation: FAIL\r\n");
        return 0;
    }
}
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

#ifdef  USE_FULL_ASSERT
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
