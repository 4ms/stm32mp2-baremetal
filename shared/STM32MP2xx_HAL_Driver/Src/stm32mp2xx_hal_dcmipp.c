/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_dcmipp.c
  * @author  MCD Application Team
  * @brief   DCMIPP HAL module driver
  *          This file provides firmware functions to manage the following
  *          functionalities of the Digital Camera Interface (DCMIPP) peripheral:
  *           + Initialization and de-initialization functions
  *           + IO operation functions
  *           + Peripheral Control functions
  *           + Peripheral State and Error functions
  *
  @verbatim
  ==============================================================================
                        ##### How to use this driver #####
  ==============================================================================


  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32mp2xx_hal.h"

/** @addtogroup STM32MP2xx_HAL_Driver
  * @{
  */
/** @defgroup DCMIPP DCMIPP
  * @brief DCMIPP HAL module driver
  * @{
  */

/*
 * TODO : Add Lock/Unlock mechanism
 * TODO : Add Callback mechanism
 * TODO : Add read version macros in CMSIS/device include file
 * TODO : How to manage M0/M1 memory for Main/Ancillary pipes
 * TODO : Add CSI support
 */

#ifdef HAL_DCMIPP_MODULE_ENABLED

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/** @defgroup DCMIPP_interrupt_sources  DCMIPP interrupt sources
* @{
*/
#define DCMIPP_IPPLUG_AXI_IT_ERR       ((uint32_t)DCMIPP_CMIER_ATXERRIE)   /*!< IPPLUG AXI Transfer error interrupt                   */
#define DCMIPP_PARALLEL_IF_IT_ERR      ((uint32_t)DCMIPP_CMIER_PRERRIE)    /*!< Synchronization error interrupt on parallel interface */
#define DCMIPP_DUMP_PIPE_IT_FRAME      ((uint32_t)DCMIPP_CMIER_P0FRAMEIE)  /*!< Frame capture interrupt complete for Dump pipe        */
#define DCMIPP_DUMP_PIPE_IT_VSYNC      ((uint32_t)DCMIPP_CMIER_P0VSYNCIE)  /*!< Vertical synch interrupt for Dump pipe                */
#define DCMIPP_DUMP_PIPE_IT_LINE       ((uint32_t)DCMIPP_CMIER_P0LINEIE)   /*!< Multiline interrupt for Dump pipe                     */
#define DCMIPP_DUMP_PIPE_IT_LIMIT      ((uint32_t)DCMIPP_CMIER_P0LIMITIE)  /*!< Limit interrupt for Dump pipe                         */
#define DCMIPP_DUMP_PIPE_IT_OVR        ((uint32_t)DCMIPP_CMIER_P0OVRIE)    /*!< Overrun interrupt for Dump pipe                       */
#define DCMIPP_MAIN_PIPE_IT_LINE       ((uint32_t)DCMIPP_CMIER_P1LINEIE)   /*!< Multiline capture interrupt for Main pipe             */
#define DCMIPP_MAIN_PIPE_IT_FRAME      ((uint32_t)DCMIPP_CMIER_P1FRAMEIE)  /*!< Frame capture interrupt complete for Main pipe        */
#define DCMIPP_MAIN_PIPE_IT_VSYNC      ((uint32_t)DCMIPP_CMIER_P1VSYNCIE)  /*!< Vertical synch interrupt for Main pipe                */
#define DCMIPP_MAIN_PIPE_IT_OVR        ((uint32_t)DCMIPP_CMIER_P1OVRIE)    /*!< Overrun interrupt for Main pipe                       */
#define DCMIPP_ANCILLARY_PIPE_IT_LINE  ((uint32_t)DCMIPP_CMIER_P2LINEIE)   /*!< Multiline capture interrupt for ancilliary pipe       */
#define DCMIPP_ANCILLARY_PIPE_IT_FRAME ((uint32_t)DCMIPP_CMIER_P2FRAMEIE)  /*!< Frame capture interrupt complete for ancilliary pipe  */
#define DCMIPP_ANCILLARY_PIPE_IT_VSYNC ((uint32_t)DCMIPP_CMIER_P2VSYNCIE)  /*!< Vertical synch interrupt for ancilliary pipe          */
#define DCMIPP_ANCILLARY_PIPE_IT_OVR   ((uint32_t)DCMIPP_CMIER_P2OVRIE)    /*!< Overrun interrupt for ancilliary pipe                 */
/**
  * @}
  */

/** @defgroup DCMIPP_interrupt_sources  DCMIPP interrupt sources
* @{
*/
#define DCMIPP_IPPLUG_AXI_FLAG_ERR       ((uint32_t)DCMIPP_CMFCR_CATXERRF)   /*!< IPPLUG AXI Transfer error interrupt flag                   */
#define DCMIPP_PARALLEL_IF_FLAG_ERR      ((uint32_t)DCMIPP_CMFCR_CPRERRF)    /*!< Synchronization error interrupt on parallel interface flag */
#define DCMIPP_DUMP_PIPE_FLAG_FRAME      ((uint32_t)DCMIPP_CMFCR_CP0FRAMEF)  /*!< Frame capture interrupt complete for Dump pipe flag        */
#define DCMIPP_DUMP_PIPE_FLAG_VSYNC      ((uint32_t)DCMIPP_CMFCR_CP0VSYNCF)  /*!< Vertical synch interrupt for Dump pipe flag                */
#define DCMIPP_DUMP_PIPE_FLAG_LINE       ((uint32_t)DCMIPP_CMFCR_CP0LINEF)   /*!< Multiline interrupt for Dump pipe flag                     */
#define DCMIPP_DUMP_PIPE_FLAG_LIMIT      ((uint32_t)DCMIPP_CMFCR_CP0LIMITF)  /*!< Limit interrupt for Dump pipe flag                         */
#define DCMIPP_DUMP_PIPE_FLAG_OVR        ((uint32_t)DCMIPP_CMFCR_CP0OVRF)    /*!< Overrun interrupt for Dump pipe flag                       */
#define DCMIPP_MAIN_PIPE_FLAG_LINE       ((uint32_t)DCMIPP_CMFCR_CP1LINEF)   /*!< Multiline capture interrupt for Main pipe flag             */
#define DCMIPP_MAIN_PIPE_FLAG_FRAME      ((uint32_t)DCMIPP_CMFCR_CP1FRAMEF)  /*!< Frame capture interrupt complete for Main pipe flag        */
#define DCMIPP_MAIN_PIPE_FLAG_VSYNC      ((uint32_t)DCMIPP_CMFCR_CP1VSYNCF)  /*!< Vertical synch interrupt for Main pipe flag                */
#define DCMIPP_MAIN_PIPE_FLAG_OVR        ((uint32_t)DCMIPP_CMFCR_CP1OVRF)    /*!< Overrun interrupt for Main pipe flag                       */
#define DCMIPP_ANCILLARY_PIPE_FLAG_LINE  ((uint32_t)DCMIPP_CMFCR_CP2LINEF)   /*!< Multiline capture interrupt for ancilliary pipe flag       */
#define DCMIPP_ANCILLARY_PIPE_FLAG_FRAME ((uint32_t)DCMIPP_CMFCR_CP2FRAMEF)  /*!< Frame capture interrupt complete for ancilliary pipe flag  */
#define DCMIPP_ANCILLARY_PIPE_FLAG_VSYNC ((uint32_t)DCMIPP_CMFCR_CP2VSYNCF)  /*!< Vertical synch interrupt for ancilliary pipe flag          */
#define DCMIPP_ANCILLARY_PIPE_FLAG_OVR   ((uint32_t)DCMIPP_CMFCR_CP2OVRF)    /*!< Overrun interrupt for ancilliary pipe flag                 */
/**
  * @}
  */

/* Private macro -------------------------------------------------------------*/
#define IS_DCMIPP_PIPE_READY(__HANDLE__, __PIPE__) ((__HANDLE__)->a_PipeState[(__PIPE__)] == HAL_DCMIPP_PIPE_STATE_READY)
#define IS_DCMIPP_PIPE_STOP(__HANDLE__, __PIPE__) ((__HANDLE__)->a_PipeState[(__PIPE__)] == HAL_DCMIPP_PIPE_STATE_STOP)
#define IS_DCMIPP_PIPE_SUSPEND(__HANDLE__, __PIPE__) ((__HANDLE__)->a_PipeState[(__PIPE__)] == HAL_DCMIPP_PIPE_STATE_SUSPEND)
#define SET_DCMIPP_PIPE_STATE(__HANDLE__, __PIPE__, __STATE__) ((__HANDLE__)->a_PipeState[(__PIPE__)] =(__STATE__))
#define IS_DCMIPP_DUMP_PIPE(__PIPE__) ((__PIPE__) == HAL_DCMIPP_DUMP_PIPE)
#define IS_DUMP_PIPE_DISABLE(__HANDLE__) ((((__HANDLE__)->Instance->P0FSCR) & DCMIPP_P0FSCR_PIPEN) == 0)
#define IS_DCMIPP_MAIN_PIPE(__PIPE__) ((__PIPE__) == HAL_DCMIPP_MAIN_PIPE)
#define IS_DCMIPP_ANCILLARY_PIPE(__PIPE__) ((__PIPE__) == HAL_DCMIPP_ANCILLARY_PIPE)
#define IS_MAIN_PIPE_DISABLE(__HANDLE__) ((((__HANDLE__)->Instance->P1FSCR) & DCMIPP_P1FSCR_PIPEN) == 0)
#define IS_ANCILLARY_PIPE_DISABLE(__HANDLE__) ((((__HANDLE__)->Instance->P2FSCR) & DCMIPP_P2FSCR_PIPEN) == 0)

/**
* @brief  Enable the DCMI interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_DCMI_IF_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->PRCR |= DCMIPP_PRCR_ENABLE)

/**
* @brief  Disable the DCMI interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_DCMI_IF_DISABLE(__HANDLE__) ((__HANDLE__)->Instance->PRCR &= ~DCMIPP_PRCR_ENABLE)

/**
* @brief  Enable the Dump Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_DUMP_PIPE_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->P0FSCR |= DCMIPP_P0FSCR_PIPEN)

/**
* @brief  Disable the Dump Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_DUMP_PIPE_DISABLE(__HANDLE__) ((__HANDLE__)->Instance->P0FSCR &= ~DCMIPP_P0FSCR_PIPEN)

/**
* @brief  Enable the Main Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_MAIN_PIPE_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->P1FSCR |= DCMIPP_P1FSCR_PIPEN)

/**
* @brief  Enable the Ancillary Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_ANCILLARY_PIPE_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->P2FSCR |= DCMIPP_P2FSCR_PIPEN)

/**
* @brief  Disable the Main Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_MAIN_PIPE_DISABLE(__HANDLE__) ((__HANDLE__)->Instance->P1FSCR &= ~DCMIPP_P1FSCR_PIPEN)

/**
* @brief  Disable the Ancillary Pipe interface in DCMIPP.
* @param  __HANDLE__ DCMIPP handle
* @retval None
*/
#define __HAL_DCMIPP_ANCILLARY_PIPE_DISABLE(__HANDLE__) ((__HANDLE__)->Instance->P2FSCR &= ~DCMIPP_P2FSCR_PIPEN)

/**
  * @brief  Enable the specified DCMIPP interrupts.
  * @param  __HANDLE__    DCMIPP handle
  * @param  __INTERRUPT__ specifies the DCMIPP interrupt sources to be enabled.
  *         This parameter can be any combination of the following values:
  *            @arg DCMIPP_PARALLEL_IF_IT_ERR Limit interrupt for the parallel interface
  *            @arg DCMIPP_DUMP_PIPE_IT_LINE Multi-line capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_FRAME Frame capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_VSYNC Vertical sync interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_LIMIT Limit interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_OVR Overrun interrupt for the dump pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_LINE Multi-line capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_FRAME Frame capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_VSYNC Vertical sync interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_OVR Overrun interrupt for the main pipe
  *            @arg DCMIPP_ANC_PIPE_IT_LINE Multi-line capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_FRAME Frame capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_VSYNC Vertical sync interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_OVR Overrun interrupt for the ancillary pipe
  * @retval None
  */
#define __HAL_DCMIPP_ENABLE_IT(__HANDLE__, __INTERRUPT__) ((__HANDLE__)->Instance->CMIER |= (__INTERRUPT__))

/**
  * @brief  Disable the specified DCMIPP interrupts.
  * @param  __HANDLE__    DCMIPP handle
  * @param  __INTERRUPT__ specifies the DCMIPP interrupt sources to be disabled.
  *         This parameter can be any combination of the following values:
  *            @arg DCMIPP_PARALLEL_IF_IT_ERR Limit interrupt for the parallel interface
  *            @arg DCMIPP_DUMP_PIPE_IT_LINE Multi-line capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_FRAME Frame capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_VSYNC Vertical sync interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_LIMIT Limit interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_OVR Overrun interrupt for the dump pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_LINE Multi-line capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_FRAME Frame capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_VSYNC Vertical sync interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_OVR Overrun interrupt for the main pipe
  *            @arg DCMIPP_ANC_PIPE_IT_LINE Multi-line capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_FRAME Frame capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_VSYNC Vertical sync interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_OVR Overrun interrupt for the ancillary pipe
  * @retval None
  */
#define __HAL_DCMIPP_DISABLE_IT(__HANDLE__, __INTERRUPT__) ((__HANDLE__)->Instance->CMIER &= ~(__INTERRUPT__))

/**
  * @brief  Get the DCMIPP pending interrupt flags.
  * @param  __HANDLE__: DCMIPP handle
  * @param  __FLAG__: Get the specified flag.
  *         This parameter can be any combination of the following values:
  *            @arg DCMIPP_PARALLEL_FLAG_IT_ERR Limit interrupt flag for the parallel interface
  *            @arg DCMIPP_DUMP_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_LIMIT Limit interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_OVR Overrun interrupt flag for the dump pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_LINE Multi-line capture complete interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_OVR Overrun interrupt flag for the main pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_LINE Multi-line capture complete interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_OVR Overrun interrupt flag for the ancillary pipe
  * @retval The state of FLAG (SET or RESET).
  */
#define __HAL_DCMIPP_GET_FLAG(__HANDLE__, __FLAG__)  ((__HANDLE__)->Instance->CMSR2 & (__FLAG__))

/**
  * @brief  Clear the DCMIPP pending interrupt flags.
  * @param  __HANDLE__ DCMIPP handle
  * @param  __FLAG__ specifies the flag to clear.
  *         This parameter can be any combination of the following values:
  *            @arg DCMIPP_PARALLEL_FLAG_IT_ERR Limit interrupt flag for the parallel interface
  *            @arg DCMIPP_DUMP_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_LIMIT Limit interrupt flag for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_FLAG_OVR Overrun interrupt flag for the dump pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_LINE Multi-line capture complete interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_FLAG_OVR Overrun interrupt flag for the main pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_LINE Multi-line capture complete interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_FRAME Frame capture complete interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_VSYNC Vertical sync interrupt flag for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_FLAG_OVR Overrun interrupt flag for the ancillary pipe
  * @retval None
  */
#define __HAL_DCMIPP_CLEAR_FLAG(__HANDLE__, __FLAG__) ((__HANDLE__)->Instance->CMFCR = (__FLAG__))

/** @brief Set a bit in a DCMIPP register from HAL DCMIPP handle
  * @param  __HANDLE__ pointer to the DCMIPP handle.
  * @param  __FIELD__  specifies the DCMIPP register name.
  * @param  __BIT__  specifies the bit to be set.
  * @retval None
  */
#define __HAL_DCMIPP_SET_BIT(__HANDLE__, __FIELD__, __BIT__) ((__HANDLE__)->Instance->__FIELD__ |= (__BIT__))

/** @brief Clear a bit in a DCMIPP register from HAL DCMIPP handle
  * @param  __HANDLE__ pointer to the DCMIPP handle.
  * @param  __FIELD__  specifies the DCMIPP register name.
  * @param  __BIT__  specifies the bit to be clear.
  * @retval None
  */
#define __HAL_DCMIPP_CLEAR_BIT(__HANDLE__, __FIELD__, __BIT__) ((__HANDLE__)->Instance->__FIELD__ &= ~(__BIT__))

/**
  * @brief  Checks whether the specified DMA Channel interrupt is enabled or not.
  * @param  __HANDLE__: DMA handle
  * @param  __INTERRUPT__ specifies the DCMIPP interrupt sources to be checked.
  *         This parameter can be any combination of the following values:
  *            @arg DCMIPP_PARALLEL_IF_IT_ERR Limit interrupt for the parallel interface
  *            @arg DCMIPP_DUMP_PIPE_IT_LINE Multi-line capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_FRAME Frame capture complete interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_VSYNC Vertical sync interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_LIMIT Limit interrupt for the dump pipe
  *            @arg DCMIPP_DUMP_PIPE_IT_OVR Overrun interrupt for the dump pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_LINE Multi-line capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_FRAME Frame capture complete interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_VSYNC Vertical sync interrupt for the main pipe
  *            @arg DCMIPP_MAIN_PIPE_IT_OVR Overrun interrupt for the main pipe
  *            @arg DCMIPP_ANC_PIPE_IT_LINE Multi-line capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_FRAME Frame capture complete interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_VSYNC Vertical sync interrupt for the ancillary pipe
  *            @arg DCMIPP_ANC_PIPE_IT_OVR Overrun interrupt for the ancillary pipe
  * @retval The state of DCMIPP interrupt (SET or RESET).
  */
#define __HAL_DCMIPP_GET_IT_SOURCE(__HANDLE__, __INTERRUPT__)  (((__HANDLE__)->Instance->CMIER & (__INTERRUPT__)))

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef ConfigureDumpPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe);
static HAL_StatusTypeDef UnconfigureDumpPipe(DCMIPP_HandleTypeDef *pHdcmipp);
static HAL_StatusTypeDef ConfigureMainPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe);
static HAL_StatusTypeDef ConfigureAncillaryPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe);
static HAL_StatusTypeDef UnconfigureMainPipe(DCMIPP_HandleTypeDef *pHdcmipp);
static HAL_StatusTypeDef UnconfigureAncillaryPipe(DCMIPP_HandleTypeDef *pHdcmipp);
static HAL_StatusTypeDef DisableAllInterrupts(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef pipe);
/* Manage 1 lock per pipes  Need a structure with a field named "Lock" as the macros
 * __HAL_UNLOCK and __HAL_LOCK manage only Lock field (see implementation). It's a
 * way of managing several lock objects for 1 HAL
 */
/* TODO: Implement lock mechanism in DCMIPP HAL driver */
/* TODO: What about the re-entrency ????  :-(*/
static HAL_DCMIPP_PipeLockTypeDef Lock[HAL_DCMIPP_NUM_OF_PIPES];


/* Private functions ---------------------------------------------------------*/

/** @defgroup DCMIPP_Private_Functions DCMIPP Private Functions
  * @{

  */
/**
  * @brief  Configure the Dump Pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @param  pConfPipe pointer to the pipe configuration structure
  * @retval HAL status
  */
static HAL_StatusTypeDef ConfigureDumpPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe)
{
  uint32_t limit = 0;
  uint32_t * p_dest_buffer0 = NULL;

  /* Check pointers validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (pConfPipe == NULL)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NULL_POINTER;
    return HAL_ERROR;
  }

  /* Configure Dump Pipe */
  pHdcmipp->Instance->P0FCTCR |= (uint32_t)(pConfPipe->FrameRate);
  if (pHdcmipp->Mode == HAL_DCMIPP_MODE_CSI)
  {
    pHdcmipp->Instance->P0FSCR = (uint32_t)((pConfPipe->VcId) | \
                                             (pConfPipe->VcDtMode) | \
                                             (pConfPipe->VcDtIdA)  | \
                                             (pConfPipe->VcDtIdB));
  }

  /* Enable limit checking if required */
  limit = pConfPipe->Limit;
  if (limit != 0)
  {
    pHdcmipp->Instance->P0DCLMTR = limit & DCMIPP_P0DCLMTR_LIMIT_Msk;
    pHdcmipp->Instance->P0DCLMTR |= DCMIPP_P0DCLMTR_ENABLE;
  }
  else
  {
    pHdcmipp->Instance->P0DCLMTR &= ~DCMIPP_P0DCLMTR_ENABLE;
  }

  /* Set destination addresses */
  p_dest_buffer0 = pConfPipe->PixelPacker.pDestinationMemory0;
  if ( p_dest_buffer0 != NULL)
  {
    pHdcmipp->Instance->P0PPM0AR1 = (uint32_t)(p_dest_buffer0);
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NULL_BUFFER;
    return HAL_ERROR;
  }

  /* Set MultiLine configuration */
  pHdcmipp->Instance->P0PPCR &= ~DCMIPP_P0PPCR_LINEMULT_Msk;
  pHdcmipp->Instance->P0PPCR |= (uint32_t)pConfPipe->PixelPacker.MultiLine;


  /* Set the HeaderEN bit (only valid in CSI2 mode) */
  if (pConfPipe->PixelPacker.HeaderEN)
  {
    pHdcmipp->Instance->P0PPCR |= DCMIPP_P0PPCR_HEADEREN;
  }
  else
  {
    pHdcmipp->Instance->P0PPCR &= ~DCMIPP_P0PPCR_HEADEREN_Msk;
  }

  /* Set the PAD bit */
  if (pConfPipe->PixelPacker.PAD)
  {
    pHdcmipp->Instance->P0PPCR |= DCMIPP_P0PPCR_PAD;
  }
  else
  {
    pHdcmipp->Instance->P0PPCR &= ~DCMIPP_P0PPCR_PAD_Msk;
  }

  /* Pipe ready to be started */
  pHdcmipp->a_PipeState[HAL_DCMIPP_DUMP_PIPE] = HAL_DCMIPP_STATE_READY;

  return HAL_OK;
}

/**
  * @brief  Unconfigure the Dump Pipe. Release all the configurations done by
  *         ConfigureDumpPipe() function
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @retval HAL status
  */
static HAL_StatusTypeDef UnconfigureDumpPipe(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Check DCMIPP HAL state to see  if HAL_DCMIPP_Init() has been already called */
  if (pHdcmipp->State != HAL_DCMIPP_STATE_READY)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_STATE;
    return HAL_ERROR;
  }

  pHdcmipp->a_PipeState[HAL_DCMIPP_DUMP_PIPE] = HAL_DCMIPP_PIPE_STATE_RESET;

  /* Release */
  pHdcmipp->Instance->P0FSCR = 0;

  return HAL_OK;
}

/**
  * @brief  Configure the Main Pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @param  pConfPipe pointer to the pipe configuration structure
  * @retval HAL status
  */
static HAL_StatusTypeDef ConfigureMainPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe)
{
  uint32_t * p_dest_buffer0 = NULL;
  uint32_t * p_dest_buffer1 = NULL;
  uint32_t * p_dest_buffer2 = NULL;

  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (pConfPipe == NULL)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NULL_POINTER;
    return HAL_ERROR;
  }

  /* Configure Main Pipe */
  pHdcmipp->Instance->P1FCTCR |= (uint32_t)(pConfPipe->FrameRate);

  if (pHdcmipp->Mode == HAL_DCMIPP_MODE_CSI)
  {
    pHdcmipp->Instance->P1FSCR = (uint32_t)((pConfPipe->VcId) | \
                                             (pConfPipe->VcDtMode) | \
                                             (pConfPipe->VcDtIdA)  | \
                                             (pConfPipe->VcDtIdB));
  }

  /* At least 1 buffer is required */
  p_dest_buffer0 = pConfPipe->PixelPacker.pDestinationMemory0;
  p_dest_buffer1 = pConfPipe->PixelPacker.pDestinationMemory1;
  p_dest_buffer2 = pConfPipe->PixelPacker.pDestinationMemory2;
  if ((p_dest_buffer0 == NULL) && (p_dest_buffer1 == NULL) && (p_dest_buffer2 == NULL))
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NULL_BUFFER;
    return HAL_ERROR;
  }

  /* Set destination address */
  pHdcmipp->Instance->P1PPM0AR1 = (uint32_t)p_dest_buffer0;
  pHdcmipp->Instance->P1PPM1AR1 = (uint32_t)p_dest_buffer1;
  pHdcmipp->Instance->P1PPM2AR1 = (uint32_t)p_dest_buffer2;

  /* Configure the pixel packer */
  pHdcmipp->Instance->P1PPCR &= ~DCMIPP_P1PPCR_FORMAT_Msk;
  pHdcmipp->Instance->P1PPCR |= pConfPipe->PixelPacker.Format;
  pHdcmipp->Instance->P1PPM0PR = pConfPipe->PixelPacker.Pitch;
  if ((pConfPipe->PixelPacker.Format == DCMIPP_PIXEL_PACKER_FORMAT_YUV422_2) ||
      (pConfPipe->PixelPacker.Format == DCMIPP_PIXEL_PACKER_FORMAT_YUV420_2))
  {
    pHdcmipp->Instance->P1PPM1PR = pConfPipe->PixelPacker.Pitch;
  }
  else if (pConfPipe->PixelPacker.Format == DCMIPP_PIXEL_PACKER_FORMAT_YUV420_3)
  {
    pHdcmipp->Instance->P1PPM1PR = pConfPipe->PixelPacker.Pitch / 2;
  }

  if (pConfPipe->PixelPacker.SwapRB)
  {
    pHdcmipp->Instance->P1PPCR |= DCMIPP_P1PPCR_SWAPRB;
  }
  else
  {
    pHdcmipp->Instance->P1PPCR &= ~DCMIPP_P1PPCR_SWAPRB_Msk;
  }

  /* Set MultiLine configuration */
  pHdcmipp->Instance->P1PPCR &= ~DCMIPP_P1PPCR_LINEMULT_Msk;
  pHdcmipp->Instance->P1PPCR |= (uint32_t)pConfPipe->PixelPacker.MultiLine;

  /* Pipe ready to be started */
  pHdcmipp->a_PipeState[HAL_DCMIPP_MAIN_PIPE] = HAL_DCMIPP_STATE_READY;

  return HAL_OK;
}

/**
  * @brief  Configure the Ancillary Pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @param  pConfPipe pointer to the pipe configuration structure
  * @retval HAL status
  */
static HAL_StatusTypeDef ConfigureAncillaryPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe)
{
  uint32_t * p_dest_buffer0 = NULL;

  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Configure ancillary Pipe */
  pHdcmipp->Instance->P2FCTCR |= (uint32_t)(pConfPipe->FrameRate);

  if (pHdcmipp->Mode == HAL_DCMIPP_MODE_CSI)
  {
    pHdcmipp->Instance->P2FSCR = (uint32_t)((pConfPipe->VcId) | \
                                             (pConfPipe->VcDtMode) | \
                                             (pConfPipe->VcDtIdA)  | \
                                             (pConfPipe->VcDtIdB));
    if (pConfPipe->PipeSrc == HAL_DCMIPP_PIPE_SRC_SHARED)
    {
      pHdcmipp->Instance->P1FSCR &= ~DCMIPP_P1FSCR_PIPEDIFF_Msk;
    }
    else
    {
      pHdcmipp->Instance->P1FSCR |= DCMIPP_P1FSCR_PIPEDIFF;
    }
  }

  /* Configuration is over. Pipe is ready */
  pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_PIPE_STATE_READY;

  /* Set destination address */
  p_dest_buffer0 = pConfPipe->PixelPacker.pDestinationMemory0;
  if ( p_dest_buffer0 != NULL)
  {
    pHdcmipp->Instance->P2PPM0AR1 = (uint32_t)(p_dest_buffer0);
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NULL_BUFFER;
    return HAL_ERROR;
   }

  /* Configure the pixel packer */
  pHdcmipp->Instance->P2PPCR &= ~DCMIPP_P2PPCR_FORMAT_Msk;
  pHdcmipp->Instance->P2PPCR |= pConfPipe->PixelPacker.Format;
  pHdcmipp->Instance->P2PPM0PR = pConfPipe->PixelPacker.Pitch;

  if (pConfPipe->PixelPacker.SwapRB)
  {
    pHdcmipp->Instance->P2PPCR |= DCMIPP_P2PPCR_SWAPRB;
  }
  else
  {
    pHdcmipp->Instance->P2PPCR &= ~DCMIPP_P2PPCR_SWAPRB_Msk;
  }

  /* Set MultiLine configuration */
  pHdcmipp->Instance->P2PPCR &= ~DCMIPP_P2PPCR_LINEMULT_Msk;
  pHdcmipp->Instance->P2PPCR |= (uint32_t)pConfPipe->PixelPacker.MultiLine;

  /* Pipe ready to be started */
  pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_STATE_READY;

  return HAL_OK;
}

/**
  * @brief  Unconfigure the Main Pipe. Release all the configurations done by
  *         ConfigureMainPipe() function
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @retval HAL status
  */
static HAL_StatusTypeDef UnconfigureMainPipe(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Check DCMIPP HAL state to see  if HAL_DCMIPP_Init() has been already called */
  if (pHdcmipp->State != HAL_DCMIPP_STATE_READY)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_STATE;
    return HAL_ERROR;
  }

  pHdcmipp->a_PipeState[HAL_DCMIPP_MAIN_PIPE] = HAL_DCMIPP_PIPE_STATE_RESET;

  /* Release */
  pHdcmipp->Instance->P1FSCR = 0;

  return HAL_OK;
}

/**
  * @brief  Unconfigure the Ancillary Pipe. Release all the configurations done by
  *         ConfigureMainPipe() function
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @retval HAL status
  */
static HAL_StatusTypeDef UnconfigureAncillaryPipe(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Check DCMIPP HAL state to see  if HAL_DCMIPP_Init() has been already called */
  if (pHdcmipp->State != HAL_DCMIPP_STATE_READY)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_STATE;
    return HAL_ERROR;
  }

  pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_PIPE_STATE_RESET;

  /* Release */
  pHdcmipp->Instance->P2FSCR = 0;

  return HAL_OK;
}

/**
  * @brief  Disable all the required interrupts for a given pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @param  pipe Pipe where interrupts have to be switched off
  * @retval HAL status
  */
static HAL_StatusTypeDef DisableAllInterrupts(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef pipe)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Configure specific pipes  */
  if (pipe == HAL_DCMIPP_DUMP_PIPE)
  {
    __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_DUMP_PIPE_IT_FRAME | DCMIPP_DUMP_PIPE_IT_VSYNC | DCMIPP_DUMP_PIPE_IT_LINE | \
                            DCMIPP_DUMP_PIPE_IT_LIMIT | DCMIPP_DUMP_PIPE_IT_OVR | DCMIPP_IPPLUG_AXI_IT_ERR);

    /* Clear potential pending interrupt in snapshot mode */
    /* TODO : loop until no more pending interrupts???? */
    __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_IT_FRAME | DCMIPP_DUMP_PIPE_IT_VSYNC | DCMIPP_DUMP_PIPE_IT_LINE | \
                            DCMIPP_DUMP_PIPE_IT_LIMIT | DCMIPP_DUMP_PIPE_IT_OVR | DCMIPP_IPPLUG_AXI_IT_ERR);
  }
  else if (pipe == HAL_DCMIPP_MAIN_PIPE)
  {
    __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_MAIN_PIPE_IT_FRAME | DCMIPP_MAIN_PIPE_IT_LINE | DCMIPP_MAIN_PIPE_IT_OVR \
                           | DCMIPP_MAIN_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);
    __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_IT_FRAME | DCMIPP_MAIN_PIPE_IT_LINE | DCMIPP_MAIN_PIPE_IT_OVR \
                           | DCMIPP_MAIN_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);
  }
  else if (pipe == HAL_DCMIPP_ANCILLARY_PIPE)
  {
    __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_FRAME | DCMIPP_ANCILLARY_PIPE_IT_LINE | DCMIPP_ANCILLARY_PIPE_IT_OVR \
                           | DCMIPP_ANCILLARY_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);
    __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_FRAME | DCMIPP_ANCILLARY_PIPE_IT_LINE | DCMIPP_ANCILLARY_PIPE_IT_OVR \
                           | DCMIPP_ANCILLARY_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);
  }

//FIXME, rework this with refcnt
  /* If all pipes are off then turn off DCMIPP parallel interface interrupts */
  if (IS_DUMP_PIPE_DISABLE(pHdcmipp) && IS_MAIN_PIPE_DISABLE(pHdcmipp) && IS_ANCILLARY_PIPE_DISABLE(pHdcmipp))
    __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_PARALLEL_IF_IT_ERR);

  return HAL_OK;
}

/* Exported functions --------------------------------------------------------*/

/** @defgroup DCMIPP_Exported_Functions DCMIPP Exported Functions
  * @{
  */

/** @defgroup DCMIPP_Exported_Functions_Group1 Initialization and Configuration functions
 *  @brief   Initialization and Configuration functions
 *
@verbatim
 ===============================================================================
                ##### Initialization and Configuration functions #####
 ===============================================================================
    [..]  This section provides functions allowing to:
      (+) Initialize and configure the DCMIPP
      (+) De-initialize the DCMIPP

@endverbatim
  * @{
  */

/**
  * @brief  Initializes the DCMIPP according to the specified
  *         parameters in the DCMIPP_InitTypeDef and create the associated handle.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *                  the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Init(DCMIPP_HandleTypeDef *pHdcmipp)
{
  uint32_t pipe;

  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)

#else
  /* Check HAL state */
  if (pHdcmipp->State == HAL_DCMIPP_STATE_RESET)
  {
    /* Init the low level hardware */
    HAL_DCMIPP_MspInit(pHdcmipp);
  }
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */

  /* Change the DCMIPP state */
  pHdcmipp->State = HAL_DCMIPP_STATE_BUSY;

  for (pipe = 0; pipe < HAL_DCMIPP_NUM_OF_PIPES; pipe++)
  {
    /* Allocate lock resources */
    pHdcmipp->p_Lock[pipe] = &Lock[pipe];
    __HAL_UNLOCK(pHdcmipp->p_Lock[pipe]);

    /* Set the pipe to its initial state */
    pHdcmipp->a_PipeState[pipe] = HAL_DCMIPP_PIPE_STATE_RESET;
  }

  if (pHdcmipp->Init.DCMIControl.ExtendedDataMode != DCMIPP_INTERFACE_CSI)
  {
    /* Configure the parallel interface */
      pHdcmipp->Instance->CMCR |= pHdcmipp->Init.DCMIControl.SwapRB;
      pHdcmipp->Instance->PRCR |= (uint32_t)((pHdcmipp->Init.DCMIControl.Format)          | \
                                           (pHdcmipp->Init.DCMIControl.VPolarity)         | \
                                           (pHdcmipp->Init.DCMIControl.HPolarity)         | \
                                           (pHdcmipp->Init.DCMIControl.PCKPolarity)       | \
                                           (pHdcmipp->Init.DCMIControl.ExtendedDataMode)  | \
                                           (pHdcmipp->Init.DCMIControl.EmbeddedSynchro)   | \
                                           (pHdcmipp->Init.DCMIControl.SwapCycles)        | \
                                           (pHdcmipp->Init.DCMIControl.SwapBits));
    if (pHdcmipp->Init.DCMIControl.EmbeddedSynchro == DCMIPP_EMBEDDED_SYNCHRO)
    {
      uint32_t embeddedSynchroCodes = (pHdcmipp->Init.DCMIControl.EmbeddedSynchroCodes.FEC << DCMIPP_PRESCR_FEC_Pos) | \
                                      (pHdcmipp->Init.DCMIControl.EmbeddedSynchroCodes.LEC << DCMIPP_PRESCR_LEC_Pos) | \
                                      (pHdcmipp->Init.DCMIControl.EmbeddedSynchroCodes.FSC << DCMIPP_PRESCR_FSC_Pos) | \
                                      (pHdcmipp->Init.DCMIControl.EmbeddedSynchroCodes.LSC << DCMIPP_PRESCR_LSC_Pos);
      pHdcmipp->Instance->PRESCR = embeddedSynchroCodes;

      /* TODO: All codes are unmasked: more accurate config must be managed */
      pHdcmipp->Instance->PRESUR = 0xFFFFFFFF;
    }

    pHdcmipp->Mode = HAL_DCMIPP_MODE_DCMI;

    /* Activate the DCMIPP parallel interface */
    __HAL_DCMIPP_ENABLE_IT(pHdcmipp, DCMIPP_PARALLEL_IF_IT_ERR);
    __HAL_DCMIPP_DCMI_IF_ENABLE(pHdcmipp);
  }
  else
  {
    /* Select the serial CSI interface */
    __HAL_DCMIPP_SET_BIT(pHdcmipp, CMCR, DCMIPP_CMCR_INSEL);
    pHdcmipp->Mode = HAL_DCMIPP_MODE_CSI;
  }

  pHdcmipp->State = HAL_DCMIPP_STATE_READY;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP frame counter
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure
  * @param  HAL_DCMIPP_PipeTypeDef pipe to which the frame counter is associated
  * @param  HAL_DCMIPP_FCResetTypedef frame counter reset condition
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Config(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe,
                                                 HAL_DCMIPP_FCResetTypedef Reset)
{
  uint32_t cmcr_tmp;

  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if ((Pipe != HAL_DCMIPP_DUMP_PIPE) && (Pipe != HAL_DCMIPP_MAIN_PIPE) && (Pipe != HAL_DCMIPP_ANCILLARY_PIPE))
  {
    return HAL_ERROR;
  }

  cmcr_tmp = pHdcmipp->Instance->CMCR;
  cmcr_tmp &= ~DCMIPP_CMCR_PSFC_Msk;
  cmcr_tmp |= (Pipe << DCMIPP_CMCR_PSFC_Pos);
#ifdef STM32MP25XX_SI_CUT1_X
  cmcr_tmp &= ~DCMIPP_CMCR_FCRS_Msk;
  cmcr_tmp |= (Reset << DCMIPP_CMCR_FCRS_Pos);
#endif

  pHdcmipp->Instance->CMCR = cmcr_tmp;

  return HAL_OK;
}

/**
  * @brief  Reset the DCMIPP frame counter
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Reset(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->CMCR |= DCMIPP_CMCR_CFC;

  return HAL_OK;
}

/**
  * @brief  Read the DCMIPP frame counter
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure
  * @param  pointer to store the value of the frame counter
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Read(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCounter)
{
  /* Check pointer validity */
  if ((pHdcmipp == NULL) || (pCounter == NULL))
  {
    return HAL_ERROR;
  }

  *pCounter = pHdcmipp->Instance->CMFRCR;

  return HAL_OK;
}

/**
  * @brief  Enables DCMIPP capture on the specified pipe: pHdcmipp->Pipe. Only one pipe can be started at a time
  * @param  pHdcmipp    Pointer to a DCMIPP_HandleTypeDef structure that contains
  *                     the configuration information for DCMIPP.
  * @param  CaptureMode DCMIPP capture mode for the pipe : snapshot or continuous grab.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Start(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_CaptureModeTypeDef CaptureMode)
{
  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (pHdcmipp->State != HAL_DCMIPP_STATE_READY)
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_STATE;
    return HAL_ERROR;
  }

  /* ------------------------ DUMP PIPE ------------------------------------- */
  if (IS_DCMIPP_DUMP_PIPE(Pipe))
  {
    if (pHdcmipp->a_PipeState[HAL_DCMIPP_DUMP_PIPE] != (HAL_DCMIPP_PipeStateTypeDef)HAL_DCMIPP_STATE_READY)
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_STATE;
      return HAL_ERROR;
    }

    /* Set the required capture mode */
    if (CaptureMode == HAL_DCMIPP_MODE_SNAPSHOT)
    {
      pHdcmipp->Instance->P0FCTCR |= DCMIPP_P0FCTCR_CPTMODE;
    }
    else if (CaptureMode == HAL_DCMIPP_MODE_CONTINUOUS)
    {
      pHdcmipp->Instance->P0FCTCR &= ~DCMIPP_P0FCTCR_CPTMODE;
    }
    else
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_CAPTURE_MODE;
      return HAL_ERROR;
    }

    /* Now the pipe is busy */
    pHdcmipp->a_PipeState[HAL_DCMIPP_DUMP_PIPE] = HAL_DCMIPP_STATE_BUSY;

    /* Enable all required interrupts lines for the dump pipe */
    __HAL_DCMIPP_ENABLE_IT(pHdcmipp, DCMIPP_DUMP_PIPE_IT_FRAME | DCMIPP_DUMP_PIPE_IT_VSYNC | DCMIPP_DUMP_PIPE_IT_LINE | \
                           DCMIPP_DUMP_PIPE_IT_LIMIT | DCMIPP_DUMP_PIPE_IT_OVR | DCMIPP_IPPLUG_AXI_IT_ERR);

    /* Activate the Pipe */
    __HAL_DCMIPP_DUMP_PIPE_ENABLE(pHdcmipp);

    /* Start the capture */
    pHdcmipp->Instance->P0FCTCR |= DCMIPP_P0FCTCR_CPTREQ;
  }

  /* TODO : Factorize .... ! */
  /* ------------------------ MAIN PIPE ------------------------------------- */
  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    if (pHdcmipp->a_PipeState[HAL_DCMIPP_MAIN_PIPE] != (HAL_DCMIPP_PipeStateTypeDef)HAL_DCMIPP_STATE_READY)
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_STATE;
      return HAL_ERROR;
    }

    /* Set the required capture mode */
    if (CaptureMode == HAL_DCMIPP_MODE_SNAPSHOT)
    {
      pHdcmipp->Instance->P1FCTCR |= DCMIPP_P1FCTCR_CPTMODE;
    }
    else if (CaptureMode == HAL_DCMIPP_MODE_CONTINUOUS)
    {
      pHdcmipp->Instance->P1FCTCR &= ~DCMIPP_P1FCTCR_CPTMODE;
    }
    else
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_CAPTURE_MODE;
      return HAL_ERROR;
    }

    /* Now the pipe is busy */
    pHdcmipp->a_PipeState[HAL_DCMIPP_MAIN_PIPE] = HAL_DCMIPP_STATE_BUSY;

    /* Enable all required interrupts lines for the main pipe */
    __HAL_DCMIPP_ENABLE_IT(pHdcmipp, DCMIPP_MAIN_PIPE_IT_FRAME | DCMIPP_MAIN_PIPE_IT_LINE | DCMIPP_MAIN_PIPE_IT_OVR \
                           | DCMIPP_MAIN_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);
    /* Activate the Pipe */
    __HAL_DCMIPP_MAIN_PIPE_ENABLE(pHdcmipp);

    /* Start the capture */
    pHdcmipp->Instance->P1FCTCR |= DCMIPP_P1FCTCR_CPTREQ;
  }

  /* ------------------------ ANCILLARY PIPE ------------------------------------- */
  if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    if (pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] != (HAL_DCMIPP_PipeStateTypeDef)HAL_DCMIPP_STATE_READY)
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_STATE;
      return HAL_ERROR;
    }

    /* Set the required capture mode */
    if (CaptureMode == HAL_DCMIPP_MODE_SNAPSHOT)
    {
      pHdcmipp->Instance->P2FCTCR |= DCMIPP_P2FCTCR_CPTMODE;
    }
    else if (CaptureMode == HAL_DCMIPP_MODE_CONTINUOUS)
    {
      pHdcmipp->Instance->P2FCTCR &= ~DCMIPP_P2FCTCR_CPTMODE;
    }
    else
    {
      pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_CAPTURE_MODE;
      return HAL_ERROR;
    }

    /* Now the pipe is busy */
    pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_STATE_BUSY;

    /* Enable all required interrupts lines for the ancillary pipe */
    __HAL_DCMIPP_ENABLE_IT(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_FRAME | DCMIPP_ANCILLARY_PIPE_IT_LINE | DCMIPP_ANCILLARY_PIPE_IT_OVR \
                           | DCMIPP_ANCILLARY_PIPE_IT_VSYNC | DCMIPP_IPPLUG_AXI_IT_ERR);

    /* Activate the Pipe */
    __HAL_DCMIPP_ANCILLARY_PIPE_ENABLE(pHdcmipp);

    /* Start the capture */
    pHdcmipp->Instance->P2FCTCR |= DCMIPP_P2FCTCR_CPTREQ;

    pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_STATE_READY;
  }

  return HAL_OK;
}

/**
  * @brief  Stop DCMIPP capture on the specified pipe: pHdcmipp->Pipe. Only one pipe can be stopped at a time
  * @param  pHdcmipp    Pointer to a DCMIPP_HandleTypeDef structure that contains
  *                     the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Stop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check pointer validity */
  if ((pHdcmipp == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_DUMP_PIPE(Pipe))
  {
    /* Stop the capture */
    pHdcmipp->Instance->P0FCTCR &= ~DCMIPP_P0FCTCR_CPTREQ;

    /* TODO : Check that the pipe is effectively stopped */

    /* Disable the pipe */
    __HAL_DCMIPP_DUMP_PIPE_DISABLE(pHdcmipp);

    /* Disable all interrupts for this pipe */
    DisableAllInterrupts(pHdcmipp, HAL_DCMIPP_DUMP_PIPE);

    /* Change the dump pipe DCMIPP state */
    pHdcmipp->a_PipeState[HAL_DCMIPP_DUMP_PIPE] = HAL_DCMIPP_PIPE_STATE_READY;
  }
  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    /* Stop the capture */
    pHdcmipp->Instance->P1FCTCR &= ~DCMIPP_P1FCTCR_CPTREQ;

    /* TODO : Check that the pipe is effectively stopped */

    /* Disable the pipe */
    __HAL_DCMIPP_MAIN_PIPE_DISABLE(pHdcmipp);

    /* Disable all interrupts for this pipe */
    DisableAllInterrupts(pHdcmipp, HAL_DCMIPP_MAIN_PIPE);

    /* Change the dump pipe DCMIPP state */
    pHdcmipp->a_PipeState[HAL_DCMIPP_MAIN_PIPE] = HAL_DCMIPP_PIPE_STATE_READY;
  }

  if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    /* Stop the capture */
    pHdcmipp->Instance->P2FCTCR &= ~DCMIPP_P2FCTCR_CPTREQ;

    /* TODO : Check that the pipe is effectively stopped */

    /* Disable the pipe */
    __HAL_DCMIPP_ANCILLARY_PIPE_DISABLE(pHdcmipp);

    /* Disable all interrupts for this pipe */
    DisableAllInterrupts(pHdcmipp, HAL_DCMIPP_ANCILLARY_PIPE);

    /* Change the dump pipe DCMIPP state */
    pHdcmipp->a_PipeState[HAL_DCMIPP_ANCILLARY_PIPE] = HAL_DCMIPP_PIPE_STATE_READY;
  }

  return HAL_OK;
}

/**
  * @brief  Configure individual pipes
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for the DCMIPP.
  * @param  pConfPipe pointer to pipe configuration
  * @param  Pipe  pipe to be configured
  * @retval None
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe, HAL_DCMIPP_PipeTypeDef Pipe)
{
  HAL_StatusTypeDef err = HAL_ERROR;

  if (IS_DCMIPP_DUMP_PIPE(Pipe))
  {
    err = ConfigureDumpPipe(pHdcmipp, pConfPipe);
  }
  else if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    err = ConfigureMainPipe(pHdcmipp, pConfPipe);
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    err = ConfigureAncillaryPipe(pHdcmipp, pConfPipe);
  }
  else
  {
    err = HAL_ERROR;
  }

  return err;
}

/**
  * @brief  Read the amount of word transfered into memory by Pipe0
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for the DCMIPP.
  * @param  pointer to amount of word transfered
  * @retval Status
  */
HAL_StatusTypeDef HAL_DCMIPP_Get_Pipe0_Transfered_size(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCounter)
{
  uint32_t limit;
  uint32_t counter;

  /* Check pointer validity */
  if ((pHdcmipp == NULL) || (pCounter == NULL))
  {
    return HAL_ERROR;
  }

  limit = pHdcmipp->Instance->P0DCLMTR;/* in words */
  counter = pHdcmipp->Instance->P0DCCNTR;/* in bytes */

  if (limit & DCMIPP_P0DCLMTR_ENABLE)
  {
    if (counter < ((limit & DCMIPP_P0DCLMTR_LIMIT_Msk) * 4))
    {
      *pCounter = counter;
    }
    else
    {
      *pCounter = (limit & DCMIPP_P0DCLMTR_LIMIT_Msk) * 4;
    }
  }
  else
  {
    *pCounter = counter;
  }

  return HAL_OK;
}

/**
  * @brief  Set embedded synchronization delimiters unmasks.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for the DCMIPP.
  * @param  SyncUnmask pointer to a DCMIPP_SyncUnmaskTypeDef structure that contains
  *                    the embedded synchronization delimiters unmasks.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigSyncUnmask(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_EmbeddedSyncUnmaskTypeDef *SyncUnmask)
{
  /* Check pointer validity */
  if ((pHdcmipp == NULL) || (SyncUnmask == NULL))
  {
    return HAL_ERROR;
  }

  /* Write DCMIPP embedded synchronization unmask register */
  pHdcmipp->Instance->PRESUR = (((uint32_t)SyncUnmask->FSU << DCMIPP_PRESUR_FSU_Pos) | \
                           ((uint32_t)SyncUnmask->LSU << DCMIPP_PRESUR_LSU_Pos) | \
                           ((uint32_t)SyncUnmask->LEU << DCMIPP_PRESUR_LEU_Pos) | \
                           ((uint32_t)SyncUnmask->FEU << DCMIPP_PRESUR_FEU_Pos));

  return HAL_OK;
}

/**
  * @brief  Handles DCMIPP interrupt request.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for the DCMIPP.
  * @retval None
  */
void HAL_DCMIPP_IRQHandler(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Should not be null at this stage : handle tested to avoid system crash */
  if (pHdcmipp == NULL)
  {
    return;
  }

  /* =================== PARALLEL INTERFACE INTERRUPTS ====================== */


  /* Synchronization error on the parallel interface  ************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_PARALLEL_IF_FLAG_ERR) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_PARALLEL_IF_IT_ERR) != RESET)
    {
      /* Clear the synchronization error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_PARALLEL_IF_FLAG_ERR);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_SYNC;

      /* Change DCMIPP state */
      pHdcmipp->State = HAL_DCMIPP_STATE_ERROR;

      /* VSYNC Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->SyncErrorEventCallback(pHdcmipp);
#else
      HAL_DCMIPP_SyncErrorEventCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* =================== IPPLUG INTERRUPTS ====================== */


  /* AXI transfer error on IPPLUG  ************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_IPPLUG_AXI_FLAG_ERR) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_IPPLUG_AXI_IT_ERR) != RESET)
    {
      /* Clear the AXI transfer error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_IPPLUG_AXI_FLAG_ERR);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_AXI;

      /* Change DCMIPP state */
      pHdcmipp->State = HAL_DCMIPP_STATE_ERROR;
    }
  }

  /* ========================= DUMP PIPE INTERRUPTS ==================== */


  /* Overrun error on the Dump pipe **************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_OVR) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_OVR) != RESET)
    {
      /* Clear the overrun error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_OVR);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_OVR_DUMP_PIPE;

      /* Change DCMIPP state */
      pHdcmipp->State = HAL_DCMIPP_STATE_ERROR;
    }
  }
  /* Limit error on the Dump pipe ********************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_LIMIT) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_DUMP_PIPE_IT_LIMIT) != RESET)
    {
      /* Clear the synchronization error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_LIMIT);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_LIMIT_DUMP_PIPE;

     /* LIMIT Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->LimitEventDumpPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_LimitEventDumpPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* VSYNC interrupt management **********************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_VSYNC) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_DUMP_PIPE_IT_VSYNC) != RESET)
    {
      /* Clear the VSYNC flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_VSYNC);

      /* VSYNC Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->VsyncEventDumpPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_VsyncEventDumpPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* FRAME interrupt management ****************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_FRAME) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_DUMP_PIPE_IT_FRAME) != RESET)
    {
      /* When snapshot mode or when stop is requested by the user then disable Vsync,
       * Error, Overrun and limit interrupts
       */
      if (READ_BIT(pHdcmipp->Instance->P0FCTCR, DCMIPP_P0FCTCR_CPTMODE))
      {
        HAL_DCMIPP_Stop(pHdcmipp, HAL_DCMIPP_DUMP_PIPE);
      }

      /* Clear the End of Frame flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_FRAME);

      /* Frame Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      pHdcmipp->FrameEventDumpPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_FrameEventDumpPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* LINE interrupt management **********************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_LINE) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_LINE) != RESET)
    {
      /* Clear the LINE flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_DUMP_PIPE_FLAG_LINE);

      /* LINE Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->LineEventDumpPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_LineEventDumpPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* ========================= MAIN PIPE INTERRUPTS ==================== */
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_LINE) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_LINE) != RESET)
    {
      /* Clear the End of Frame flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_LINE);

      /* Frame Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      pHdcmipp->LineEventMainPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_LineEventMainPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* VSYNC interrupt management **********************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_VSYNC) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_MAIN_PIPE_IT_VSYNC) != RESET)
    {
      /* Clear the VSYNC flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_VSYNC);

      /* VSYNC Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->VsyncEventMainPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_VsyncEventMainPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_FRAME) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_MAIN_PIPE_IT_FRAME) != RESET)
    {
      /* When snapshot mode or when stop is requested by the user then disable Vsync,
       * Error and overrun interrupts
       */
      if (READ_BIT(pHdcmipp->Instance->P1FCTCR, DCMIPP_P1FCTCR_CPTMODE))
      {
        __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_MAIN_PIPE_IT_FRAME | DCMIPP_MAIN_PIPE_IT_LINE | \
                                DCMIPP_MAIN_PIPE_IT_VSYNC | DCMIPP_MAIN_PIPE_IT_OVR | DCMIPP_PARALLEL_IF_IT_ERR);

        HAL_DCMIPP_Stop(pHdcmipp, HAL_DCMIPP_MAIN_PIPE);
      }

      /* Clear the End of Frame flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_FRAME);

      /* Frame Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      pHdcmipp->FrameEventMainPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_FrameEventMainPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* Overrun error on the Main pipe **************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_OVR) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_OVR) != RESET)
    {
      /* Clear the overrun error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_MAIN_PIPE_FLAG_OVR);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_OVR_MAIN_PIPE;

      /* Change DCMIPP state */
      pHdcmipp->State = HAL_DCMIPP_STATE_ERROR;
    }
  }
  /* ========================= ANCILLARY PIPE INTERRUPTS ==================== */
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_LINE) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_LINE) != RESET)
    {
      /* Clear the End of Line flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_LINE);

      /* Frame Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      pHdcmipp->LineEventAncillaryPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_LineEventAncillaryPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  /* VSYNC interrupt management **********************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_VSYNC) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_VSYNC) != RESET)
    {
      /* Clear the VSYNC flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_VSYNC);

      /* VSYNC Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      hdcmi->VsyncEventAncillaryPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_VsyncEventAncillaryPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }

  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_FRAME) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_FRAME) != RESET)
    {
      /* When snapshot mode or when stop is requested by the user then disable Vsync,
       * Error and Overrun interrupts
       */
      if (READ_BIT(pHdcmipp->Instance->P2FCTCR, DCMIPP_P2FCTCR_CPTMODE))
      {
        __HAL_DCMIPP_DISABLE_IT(pHdcmipp, DCMIPP_ANCILLARY_PIPE_IT_FRAME | DCMIPP_ANCILLARY_PIPE_IT_LINE | \
                                DCMIPP_ANCILLARY_PIPE_IT_VSYNC | DCMIPP_ANCILLARY_PIPE_IT_OVR | DCMIPP_PARALLEL_IF_IT_ERR);

        HAL_DCMIPP_Stop(pHdcmipp, HAL_DCMIPP_ANCILLARY_PIPE);
      }

      /* Clear the End of Frame flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_FRAME);

      /* Frame Callback */
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
      pHdcmipp->FrameEventAncillaryPipeCallback(pHdcmipp);
#else
      HAL_DCMIPP_FrameEventAncillaryPipeCallback(pHdcmipp);
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
    }
  }
  /* Overrun error on the Ancillary pipe **************************************/
  if (__HAL_DCMIPP_GET_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_OVR) != RESET)
  {
    if (__HAL_DCMIPP_GET_IT_SOURCE(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_OVR) != RESET)
    {
      /* Clear the overrun error flag */
      __HAL_DCMIPP_CLEAR_FLAG(pHdcmipp, DCMIPP_ANCILLARY_PIPE_FLAG_OVR);

      /* Update error code */
      pHdcmipp->ErrorCode |= HAL_DCMIPP_ERROR_OVR_AUX_PIPE;

      /* Change DCMIPP state */
      pHdcmipp->State = HAL_DCMIPP_STATE_ERROR;
    }
  }

  /* ERROR management either for the pipes or for the interfaces ***********/
  /* TODO: Manage error cases */
}

/**
  * @brief  De-initializes the DCMIPP peripheral registers to their default reset
  *         values.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */

HAL_StatusTypeDef HAL_DCMIPP_DeInit(DCMIPP_HandleTypeDef *pHdcmipp)
{
  uint32_t pipe;

  /* Check pointer validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Lock all the pipes as the DCMIPP is going to be deactivated and de-initialized */
  for (pipe = 0; pipe < HAL_DCMIPP_NUM_OF_PIPES; pipe++)
  {
    /* Allocate lock resource */
    pHdcmipp->p_Lock[pipe] = &Lock[pipe];

    /* Reset all the pipe state */
    pHdcmipp->a_PipeState[pipe] = HAL_DCMIPP_PIPE_STATE_RESET;
  }

  /* Update error code */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  /* Initialize the DCMIPP state*/
  pHdcmipp->State = HAL_DCMIPP_STATE_RESET;

  /* Deactivate all the DCMIPP interfaces */
  __HAL_DCMIPP_DCMI_IF_DISABLE(pHdcmipp);
  __HAL_DCMIPP_DUMP_PIPE_DISABLE(pHdcmipp);
  __HAL_DCMIPP_MAIN_PIPE_DISABLE(pHdcmipp);
  __HAL_DCMIPP_ANCILLARY_PIPE_DISABLE(pHdcmipp);

  /* un-configure Pipe configurations */
  UnconfigureDumpPipe(pHdcmipp);
  UnconfigureMainPipe(pHdcmipp);
  UnconfigureAncillaryPipe(pHdcmipp);

#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
  if (pHdcmipp->MspDeInitCallback == NULL)
  {
    pHdcmipp->MspDeInitCallback = HAL_DCMIPP_MspDeInit;
  }

  /* DeInit the low level hardware */
  pHdcmipp->MspDeInitCallback(pHdcmipp);
#else
  /* DeInit the low level hardware */
  HAL_DCMIPP_MspDeInit(pHdcmipp);
#endif /* USE_HAL_DCMI_REGISTER_CALLBACKS */

  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
* @brief  Initializes the DCMIPP MSP.
* @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
*               the configuration information for DCMIPP.
* @retval None
*/
__weak void HAL_DCMIPP_MspInit(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(pHdcmipp);

  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_MspInit could be implemented in the user file
   */
}

/**
  * @brief  Frame Event callback on Dump pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_FrameEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_FrameEventDumpPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Vsync Event callback on Dump pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_VsyncEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_VsyncEventDumpPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Frame Event callback on Main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_FrameEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_FrameEventMainPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Line Event callback on Dump pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_LineEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_LineEventDumpPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Line Event callback on Main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_LineEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_LineEventMainPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Vsync Event callback on Main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_VsyncEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_VsyncEventMainPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Frame Event callback on Ancillary pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_FrameEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_FrameEventAncillaryPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Line Event callback on Ancillary pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_LineEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_FrameEventAncillaryPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Vsync Event callback on Ancillary pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_VsyncEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_VsyncEventAncillaryPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Synchronization Error callback on Parallel DCMIPP interface
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_SyncErrorEventCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_SyncErrorEventDumpPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}

/**
  * @brief  Limit callback on Parallel/CSI DCMIPP interface
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval None
  */
__weak void HAL_DCMIPP_LimitEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_DCMIPP_SyncErrorEventDumpPipeCallback could be implemented in the user file
   */
  UNUSED(pHdcmipp);
}


/** @defgroup DCMI_Exported_Functions_Group4 Peripheral State functions
 *  @brief    Peripheral State functions
 *
@verbatim
 ===============================================================================
               ##### Peripheral State and Errors functions #####
 ===============================================================================
    [..]
    This subsection provides functions allowing to
      (+) Check the DCMIPP state.
      (+) Get the specific DCMIPP error flag.

@endverbatim
  * @{
  */

/**
  * @brief  Return the DCMIPP state
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL state
  */
HAL_DCMIPP_StateTypeDef HAL_DCMIPP_GetState(DCMIPP_HandleTypeDef *pHdcmipp)
{
  return pHdcmipp->State;
}

/**
 * @brief  Return the DCMIPP error code
 * @param  pHdcmipp  pointer to a DCMIPP_HandleTypeDef structure that contains
 *              the configuration information for DCMIPP.
 * @retval DCMIPP Error Code
 */
uint32_t HAL_DCMIPP_GetError(DCMIPP_HandleTypeDef *pHdcmipp)
{
  return pHdcmipp->ErrorCode;
}

/** @defgroup DCMI_Exported_Functions_Group4 Peripheral Control functions
 *  @brief    Peripheral State functions
 *
@verbatim
 ===============================================================================
               ##### Peripheral Control functions #####
 ===============================================================================
    [..]
    This subsection provides functions allowing to
      (+) Configure Crop window.

@endverbatim
  * @{
  */

/**
  * @brief  Configure the DCMIPP Pipe0 CROP coordinate.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  X0    DCMIPP window X offset (this is expressed in word and not pixel !!)
  * @param  Y0    DCMIPP window Y offset
  * @param  XSize DCMIPP Word per line
  * @param  YSize DCMIPP Line number
  * @param  In    Indicate if area inside or outside the window should be taken
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_ConfigCrop(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize, uint32_t In)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P0SCSTR = (((X0 << DCMIPP_P0SCSTR_HSTART_Pos) & DCMIPP_P0SCSTR_HSTART_Msk) |
                                 ((Y0 << DCMIPP_P0SCSTR_VSTART_Pos) & DCMIPP_P0SCSTR_VSTART_Msk));
  pHdcmipp->Instance->P0SCSZR = (((XSize << DCMIPP_P0SCSZR_HSIZE_Pos) & DCMIPP_P0SCSZR_HSIZE_Msk) |
                                 ((YSize << DCMIPP_P0SCSZR_VSIZE_Pos) & DCMIPP_P0SCSZR_VSIZE_Msk));
  if (In)
  {
    pHdcmipp->Instance->P0SCSZR &= ~(uint32_t)DCMIPP_P0SCSZR_POSNEG;
  }
  else
  {
    pHdcmipp->Instance->P0SCSZR |= (uint32_t)DCMIPP_P0SCSZR_POSNEG;
  }

  return HAL_OK;
}

/**
  * @brief  Disable the Crop feature of Pipe0
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_DisableCrop(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P0SCSZR &= ~(uint32_t)DCMIPP_P0SCSZR_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Crop feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_EnableCrop(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P0SCSZR |= (uint32_t)DCMIPP_P0SCSZR_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP CROP coordinate.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where crop is configured (Dump, Main or Ancillary)
  * @param  X0    DCMIPP window X offset (!! In case of Dump pipe, this is expressed in word and not pixel !!)
  * @param  Y0    DCMIPP window Y offset
  * @param  XSize DCMIPP Pixel per line (!! In case of Dump pipe, this is amount of word and not pixel !!)
  * @param  YSize DCMIPP Line number
  * @param  In    (for DUMP only) Indicate if area inside or outside the window should be taken
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CRSTR = ((X0 << DCMIPP_P1CRSTR_HSTART_Pos) | (Y0 << DCMIPP_P1CRSTR_VSTART_Pos));
    pHdcmipp->Instance->P1CRSZR = ((XSize << DCMIPP_P1CRSZR_HSIZE_Pos) | (YSize << DCMIPP_P1CRSZR_VSIZE_Pos));
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2CRSTR = ((X0 << DCMIPP_P2CRSTR_HSTART_Pos) | (Y0 << DCMIPP_P2CRSTR_VSTART_Pos));
    pHdcmipp->Instance->P2CRSZR = ((XSize << DCMIPP_P2CRSZR_HSIZE_Pos) | (YSize << DCMIPP_P2CRSZR_VSIZE_Pos));
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Crop feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where crop is disable (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CRSZR &= ~(uint32_t)DCMIPP_P1CRSZR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2CRSZR &= ~(uint32_t)DCMIPP_P2CRSZR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Crop feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where crop is enable (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_DUMP_PIPE(Pipe))
  {
    pHdcmipp->Instance->P0SCSZR |= (uint32_t)DCMIPP_P0SCSZR_ENABLE;
  }
  else if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CRSZR |= (uint32_t)DCMIPP_P1CRSZR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2CRSZR |= (uint32_t)DCMIPP_P2CRSZR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}


/**
  * @}
  */
/**
  * @brief  Configure the DCMIPP Pipe0 Decimation parameters.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  pDownsizeConfig pointer to Downsize parameters
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_P0_LinesDecimationModeTypeDef LinesMode, HAL_DCMIPP_P0_BytesDecimationModeTypeDef BytesMode)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Lines mode */
  pHdcmipp->Instance->P0PPCR &= ~(DCMIPP_P0PPCR_LSM_Msk | DCMIPP_P0PPCR_OELS_Msk);
  pHdcmipp->Instance->P0PPCR |= (uint32_t)LinesMode;

  /* Bytes mode */
  pHdcmipp->Instance->P0PPCR &= ~(DCMIPP_P0PPCR_BSM_Msk | DCMIPP_P0PPCR_OEBS_Msk);
  pHdcmipp->Instance->P0PPCR |= (uint32_t)BytesMode;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP AXI master memory IP-Plug.
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigIPPlug(DCMIPP_HandleTypeDef *pHdcmipp, int MemoryPage, int Client,
                                          int Traffic, int Outstanding, int DpregStart, int DpregEnd,
                                          int Wlru)
{
  int cnt;

  /* Check handle validity */
  if (pHdcmipp == NULL)
    return HAL_ERROR;

#define MEMORY_PAGE_DEFAULT 0x02
#if defined(STM32MP135Cxx)
#define TRAFFIC_DEFAULT     0x03
#elif defined(STM32MP257Cxx)
#define TRAFFIC_DEFAULT     0x04
#endif
#define OTR_DEFAULT   0x00
#define WLRU_DEFAULT  0x01
#define DPREG_START_0 0x00
#if defined(STM32MP135Cxx)
#define DPREG_END_0   0x3F
#elif defined(STM32MP257Cxx)
#define DPREG_END_0   0x7F
#define DPREG_START_1 0x80
#define DPREG_END_1   0x13F
#define DPREG_START_2 0x140
#define DPREG_END_2   0x18F
#define DPREG_START_3 0x190
#define DPREG_END_3   0x1BF
#define DPREG_START_4 0x1C0
#define DPREG_END_4   0x27F
#endif

  /* Lock & put IP-PLUG in idle to allow configuration */
  cnt=10;
  pHdcmipp->Instance->IPGR2 = DCMIPP_IPGR2_PSTART;
  while ((pHdcmipp->Instance->IPGR3 != DCMIPP_IPGR3_IDLE) && cnt) {
    HAL_Delay(1);
    cnt--;
  }
  if (!cnt)
    return HAL_ERROR;

  if (MemoryPage != -1)
    pHdcmipp->Instance->IPGR1 = MemoryPage << DCMIPP_IPGR1_MEMORYPAGE_Pos;
  else
    pHdcmipp->Instance->IPGR1 = MEMORY_PAGE_DEFAULT << DCMIPP_IPGR1_MEMORYPAGE_Pos;

  switch (Client)
  {
  /* Pipe Dump */
  case 1:
    if (Traffic != -1)
      pHdcmipp->Instance->IPC1R1 = Traffic << DCMIPP_IPC1R1_TRAFFIC_Pos;
    else
      pHdcmipp->Instance->IPC1R1 = TRAFFIC_DEFAULT << DCMIPP_IPC1R1_TRAFFIC_Pos;

    if (Outstanding != -1)
      pHdcmipp->Instance->IPC1R1 |= Outstanding << DCMIPP_IPC1R1_OTR_Pos;
    else
      pHdcmipp->Instance->IPC1R1 |= OTR_DEFAULT << DCMIPP_IPC1R1_OTR_Pos;

    if (DpregStart != -1)
      pHdcmipp->Instance->IPC1R3 = DpregStart << DCMIPP_IPC1R3_DPREGSTART_Pos;
    else
      pHdcmipp->Instance->IPC1R3 = DPREG_START_0 << DCMIPP_IPC1R3_DPREGSTART_Pos;

    if (DpregEnd != -1)
      pHdcmipp->Instance->IPC1R3 |= DpregEnd << DCMIPP_IPC1R3_DPREGEND_Pos;
    else
      pHdcmipp->Instance->IPC1R3 |= DPREG_END_0 << DCMIPP_IPC1R3_DPREGEND_Pos;

    if (Wlru != -1)
      pHdcmipp->Instance->IPC1R2 = Wlru << DCMIPP_IPC1R2_WLRU_Pos;
    else
      pHdcmipp->Instance->IPC1R2 = WLRU_DEFAULT << DCMIPP_IPC1R2_WLRU_Pos;
    break;
  /* Pipe Main */
  case 2:
    if (Traffic != -1)
      pHdcmipp->Instance->IPC2R1 = Traffic << DCMIPP_IPC2R1_TRAFFIC_Pos;
    else
      pHdcmipp->Instance->IPC2R1 = TRAFFIC_DEFAULT << DCMIPP_IPC2R1_TRAFFIC_Pos;

    if (Outstanding != -1)
      pHdcmipp->Instance->IPC2R1 |= Outstanding << DCMIPP_IPC2R1_OTR_Pos;
    else
      pHdcmipp->Instance->IPC2R1 |= OTR_DEFAULT << DCMIPP_IPC2R1_OTR_Pos;

    if (DpregStart != -1)
      pHdcmipp->Instance->IPC2R3 = DpregStart << DCMIPP_IPC2R3_DPREGSTART_Pos;
    else
      pHdcmipp->Instance->IPC2R3 = DPREG_START_1 << DCMIPP_IPC2R3_DPREGSTART_Pos;

    if (DpregEnd != -1)
      pHdcmipp->Instance->IPC2R3 |= DpregEnd << DCMIPP_IPC2R3_DPREGEND_Pos;
    else
      pHdcmipp->Instance->IPC2R3 |= DPREG_END_1 << DCMIPP_IPC2R3_DPREGEND_Pos;

    if (Wlru != -1)
      pHdcmipp->Instance->IPC2R2 = Wlru << DCMIPP_IPC2R2_WLRU_Pos;
    else
      pHdcmipp->Instance->IPC2R2 = WLRU_DEFAULT << DCMIPP_IPC2R2_WLRU_Pos;
    break;
  case 3:
    if (Traffic != -1)
      pHdcmipp->Instance->IPC3R1 = Traffic << DCMIPP_IPC3R1_TRAFFIC_Pos;
    else
      pHdcmipp->Instance->IPC3R1 = TRAFFIC_DEFAULT << DCMIPP_IPC3R1_TRAFFIC_Pos;

    if (Outstanding != -1)
      pHdcmipp->Instance->IPC3R1 |= Outstanding << DCMIPP_IPC3R1_OTR_Pos;
    else
      pHdcmipp->Instance->IPC3R1 |= OTR_DEFAULT << DCMIPP_IPC3R1_OTR_Pos;

    if (DpregStart != -1)
      pHdcmipp->Instance->IPC3R3 = DpregStart << DCMIPP_IPC3R3_DPREGSTART_Pos;
    else
      pHdcmipp->Instance->IPC3R3 = DPREG_START_2 << DCMIPP_IPC3R3_DPREGSTART_Pos;

    if (DpregEnd != -1)
      pHdcmipp->Instance->IPC3R3 |= DpregEnd << DCMIPP_IPC3R3_DPREGEND_Pos;
    else
      pHdcmipp->Instance->IPC3R3 |= DPREG_END_2 << DCMIPP_IPC3R3_DPREGEND_Pos;

    if (Wlru != -1)
      pHdcmipp->Instance->IPC3R2 = Wlru << DCMIPP_IPC3R2_WLRU_Pos;
    else
      pHdcmipp->Instance->IPC3R2 = WLRU_DEFAULT << DCMIPP_IPC3R2_WLRU_Pos;
    break;
  case 4:
    if (Traffic != -1)
      pHdcmipp->Instance->IPC4R1 = Traffic << DCMIPP_IPC4R1_TRAFFIC_Pos;
    else
      pHdcmipp->Instance->IPC4R1 = TRAFFIC_DEFAULT << DCMIPP_IPC4R1_TRAFFIC_Pos;

    if (Outstanding != -1)
      pHdcmipp->Instance->IPC4R1 |= Outstanding << DCMIPP_IPC4R1_OTR_Pos;
    else
      pHdcmipp->Instance->IPC4R1 |= OTR_DEFAULT << DCMIPP_IPC4R1_OTR_Pos;

    if (DpregStart != -1)
      pHdcmipp->Instance->IPC4R3 = DpregStart << DCMIPP_IPC4R3_DPREGSTART_Pos;
    else
      pHdcmipp->Instance->IPC4R3 = DPREG_START_3 << DCMIPP_IPC4R3_DPREGSTART_Pos;

    if (DpregEnd != -1)
      pHdcmipp->Instance->IPC4R3 |= DpregEnd << DCMIPP_IPC4R3_DPREGEND_Pos;
    else
      pHdcmipp->Instance->IPC4R3 |= DPREG_END_3 << DCMIPP_IPC4R3_DPREGEND_Pos;

    if (Wlru != -1)
      pHdcmipp->Instance->IPC4R2 = Wlru << DCMIPP_IPC4R2_WLRU_Pos;
    else
      pHdcmipp->Instance->IPC4R2 = WLRU_DEFAULT << DCMIPP_IPC4R2_WLRU_Pos;
    break;
  /* Pipe AUX */
  case 5:
    if (Traffic != -1)
      pHdcmipp->Instance->IPC5R1 = Traffic << DCMIPP_IPC5R1_TRAFFIC_Pos;
    else
      pHdcmipp->Instance->IPC5R1 = TRAFFIC_DEFAULT << DCMIPP_IPC5R1_TRAFFIC_Pos;

    if (Outstanding != -1)
      pHdcmipp->Instance->IPC5R1 |= Outstanding << DCMIPP_IPC5R1_OTR_Pos;
    else
      pHdcmipp->Instance->IPC5R1 |= OTR_DEFAULT << DCMIPP_IPC5R1_OTR_Pos;

    if (DpregStart != -1)
      pHdcmipp->Instance->IPC5R3 = DpregStart << DCMIPP_IPC5R3_DPREGSTART_Pos;
    else
      pHdcmipp->Instance->IPC5R3 = DPREG_START_4 << DCMIPP_IPC5R3_DPREGSTART_Pos;

    if (DpregEnd != -1)
      pHdcmipp->Instance->IPC5R3 |= DpregEnd << DCMIPP_IPC5R3_DPREGEND_Pos;
    else
      pHdcmipp->Instance->IPC5R3 |= DPREG_END_4 << DCMIPP_IPC5R3_DPREGEND_Pos;

    if (Wlru != -1)
      pHdcmipp->Instance->IPC5R2 = Wlru << DCMIPP_IPC5R2_WLRU_Pos;
    else
      pHdcmipp->Instance->IPC5R2 = WLRU_DEFAULT << DCMIPP_IPC5R2_WLRU_Pos;
    break;
  }

  /* Unlock IP-PLUG */
  pHdcmipp->Instance->IPGR2 = 0;

  return HAL_OK;
}

/**
  * @}
  */
/**
  * @brief  Configure the DCMIPP Pipe1 Decimation parameters.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe1_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_P1_VerticalDecimationModeTypeDef vdec, HAL_DCMIPP_P1_HorizontalDecimationModeTypeDef hdec)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Vertical decimation */
  pHdcmipp->Instance->P1DECR &= ~(DCMIPP_P1DECR_VDEC_Msk);
  pHdcmipp->Instance->P1DECR |= (uint32_t)vdec;

  /* Horizontal decimation */
  pHdcmipp->Instance->P1DECR &= ~(DCMIPP_P1DECR_HDEC_Msk);
  pHdcmipp->Instance->P1DECR |= (uint32_t)hdec;

  /* Enable decimation */
  pHdcmipp->Instance->P1DECR |= DCMIPP_P1DECR_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @}
  */
/**
  * @brief  Get DCMIPP Pipe1 Decimation parameters.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  pvdec pointer to receive the vertical decimation
  * @param  phdec pointer to receive the horizontal decimation
  * @param  pEnable pointer receiving the status of the Decimation function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_Pipe1_GetConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp,
                                                       HAL_DCMIPP_P1_VerticalDecimationModeTypeDef *pvdec,
                                                       HAL_DCMIPP_P1_HorizontalDecimationModeTypeDef *phdec,
                                                       uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pvdec == NULL) || (phdec == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  *pEnable = (pHdcmipp->Instance->P1DECR & DCMIPP_P1DECR_ENABLE_Msk) >> DCMIPP_P1DECR_ENABLE_Pos;
  *pvdec = pHdcmipp->Instance->P1DECR & DCMIPP_P1DECR_VDEC_Msk;
  *phdec = pHdcmipp->Instance->P1DECR & DCMIPP_P1DECR_HDEC_Msk;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @}
  */
/**
  * @brief  Configure the DCMIPP Decimation bloc parameters.
  *         There are several blocs available within the DCMIPP depending on the IP version
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_DecimationBlocTypeDef Bloc, HAL_DCMIPP_VerticalDecimationModeTypeDef vdec, HAL_DCMIPP_HorizontalDecimationModeTypeDef hdec)
{
  HAL_StatusTypeDef ret = HAL_OK;

  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  switch(Pipe)
  {
  case HAL_DCMIPP_DUMP_PIPE:
    /* Pipe0 Decimation is still to be done via the HAL_DCMIPP_Pipe0ConfigDecimation function */
    ret = HAL_ERROR;
    break;
  case HAL_DCMIPP_MAIN_PIPE:
    if (Bloc == DCMIPP_ISP_DECIMATION)
    {
      /* Vertical decimation */
      pHdcmipp->Instance->P1DECR &= ~(DCMIPP_P1DECR_VDEC_Msk);
      pHdcmipp->Instance->P1DECR |= (uint32_t)vdec;

      /* Horizontal decimation */
      pHdcmipp->Instance->P1DECR &= ~(DCMIPP_P1DECR_HDEC_Msk);
      pHdcmipp->Instance->P1DECR |= (uint32_t)hdec;

      /* Enable decimation */
      pHdcmipp->Instance->P1DECR |= DCMIPP_P1DECR_ENABLE;
#ifdef STM32MP25XX_SI_CUT1_X
    } else {
      ret = HAL_ERROR;
    }
#else
    } else {
      /* Vertical decimation */
      pHdcmipp->Instance->P1DCCR &= ~(DCMIPP_P1DCCR_VDEC_Msk);
      pHdcmipp->Instance->P1DCCR |= (uint32_t)vdec;

      /* Horizontal decimation */
      pHdcmipp->Instance->P1DCCR &= ~(DCMIPP_P1DCCR_HDEC_Msk);
      pHdcmipp->Instance->P1DCCR |= (uint32_t)hdec;

      /* Enable decimation */
      pHdcmipp->Instance->P1DCCR |= DCMIPP_P1DCCR_ENABLE;
    }
#endif
    break;
  case HAL_DCMIPP_ANCILLARY_PIPE:
#ifdef STM32MP25XX_SI_CUT1_X
    ret = HAL_ERROR;
#else
    if (Bloc == DCMIPP_POSTPROC_DECIMATION)
    {
      /* Vertical decimation */
      pHdcmipp->Instance->P2DCCR &= ~(DCMIPP_P2DCCR_VDEC_Msk);
      pHdcmipp->Instance->P2DCCR |= (uint32_t)vdec;

      /* Horizontal decimation */
      pHdcmipp->Instance->P2DCCR &= ~(DCMIPP_P2DCCR_HDEC_Msk);
      pHdcmipp->Instance->P2DCCR |= (uint32_t)hdec;

      /* Enable decimation */
      pHdcmipp->Instance->P2DCCR |= DCMIPP_P2DCCR_ENABLE;
    } else {
      ret = HAL_ERROR;
    }
#endif
    break;
  default:
    ret = HAL_ERROR;
    break;
  }

  return ret;
}

/**
  * @brief  Configure the DCMIPP Downsize parameters.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where downsize is configured (Main or Ancillary)
  * @param  pDownsizeConfig pointer to Downsize parameters
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_DownsizeTypeDef *pDownsizeConfig)
{
  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pDownsizeConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1DSCR = ((pDownsizeConfig->HorizontalDivFactor << DCMIPP_P1DSCR_HDIV_Pos) | (pDownsizeConfig->VerticalDivFactor << DCMIPP_P1DSCR_VDIV_Pos));
    pHdcmipp->Instance->P1DSRTIOR = ((pDownsizeConfig->HorizontalRatio << DCMIPP_P1DSRTIOR_HRATIO_Pos) | (pDownsizeConfig->VerticalRatio << DCMIPP_P1DSRTIOR_VRATIO_Pos));
    pHdcmipp->Instance->P1DSSZR = ((pDownsizeConfig->HorizontalSize << DCMIPP_P1DSSZR_HSIZE_Pos) | (pDownsizeConfig->VerticalSize << DCMIPP_P1DSSZR_VSIZE_Pos));
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2DSCR = ((pDownsizeConfig->HorizontalDivFactor << DCMIPP_P2DSCR_HDIV_Pos) | (pDownsizeConfig->VerticalDivFactor << DCMIPP_P2DSCR_VDIV_Pos));
    pHdcmipp->Instance->P2DSRTIOR = ((pDownsizeConfig->HorizontalRatio << DCMIPP_P2DSRTIOR_HRATIO_Pos) | (pDownsizeConfig->VerticalRatio << DCMIPP_P2DSRTIOR_VRATIO_Pos));
    pHdcmipp->Instance->P2DSSZR = ((pDownsizeConfig->HorizontalSize << DCMIPP_P2DSSZR_HSIZE_Pos) | (pDownsizeConfig->VerticalSize << DCMIPP_P2DSSZR_VSIZE_Pos));
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the downsize feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where downsize is enable (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1DSCR |= (uint32_t)DCMIPP_P1DSCR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2DSCR |= (uint32_t)DCMIPP_P2DSCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the downsize feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where downsize is enable (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1DSCR &= ~(uint32_t)DCMIPP_P1DSCR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2DSCR &= ~(uint32_t)DCMIPP_P2DSCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Gamma Conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe to which apply the gamma conversion
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableGammaConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1GMCR |= (uint32_t)DCMIPP_P1GMCR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2GMCR |= (uint32_t)DCMIPP_P2GMCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Gamma Conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe to which apply the gamma conversion
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableGammaConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1GMCR &= ~(uint32_t)DCMIPP_P1GMCR_ENABLE;
  }
  else if (IS_DCMIPP_ANCILLARY_PIPE(Pipe))
  {
    pHdcmipp->Instance->P2GMCR &= ~(uint32_t)DCMIPP_P2GMCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP Raw Bayer to RGB parameters for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  pRawBayer2RGBConfig pointer to RawBayer to RGB parameters
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_RawBayer2RGBTypeDef *pRawBayer2RGBConfig)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pRawBayer2RGBConfig == NULL))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1DMCR = ((pRawBayer2RGBConfig->Type << DCMIPP_P1DMCR_TYPE_Pos) & DCMIPP_P1DMCR_TYPE_Msk)   | \
                               ((pRawBayer2RGBConfig->Peak << DCMIPP_P1DMCR_PEAK_Pos) & DCMIPP_P1DMCR_PEAK_Msk)   | \
                               ((pRawBayer2RGBConfig->LineV << DCMIPP_P1DMCR_LINEV_Pos) & DCMIPP_P1DMCR_LINEV_Msk) | \
                               ((pRawBayer2RGBConfig->LineH << DCMIPP_P1DMCR_LINEH_Pos) & DCMIPP_P1DMCR_LINEH_Msk) | \
                               ((pRawBayer2RGBConfig->Edge << DCMIPP_P1DMCR_EDGE_Pos) & DCMIPP_P1DMCR_EDGE_Msk);

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Raw Bayer to RGB feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1DMCR |= (uint32_t)DCMIPP_P1DMCR_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Raw Bayer to RGB feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1DMCR &= ~(uint32_t)DCMIPP_P1DMCR_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the DCMIPP Raw Bayer to RGB parameters for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  pRawBayer2RGBConfig pointer receiving the RawBayer to RGB parameters
  * @param  pEnable pointer receiving the status of the Raw Bayer to RGB function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_RawBayer2RGBTypeDef *pRawBayer2RGBConfig, uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pRawBayer2RGBConfig == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  *pEnable = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_ENABLE_Msk) >> DCMIPP_P1DMCR_ENABLE_Pos;
  pRawBayer2RGBConfig->Type = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_TYPE_Msk) >> DCMIPP_P1DMCR_TYPE_Pos;
  pRawBayer2RGBConfig->Peak = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_PEAK_Msk) >> DCMIPP_P1DMCR_PEAK_Pos;
  pRawBayer2RGBConfig->LineV = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_LINEV_Msk) >> DCMIPP_P1DMCR_LINEV_Pos;
  pRawBayer2RGBConfig->LineH = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_LINEH_Msk) >> DCMIPP_P1DMCR_LINEH_Pos;
  pRawBayer2RGBConfig->Edge = (pHdcmipp->Instance->P1DMCR & DCMIPP_P1DMCR_EDGE_Msk) >> DCMIPP_P1DMCR_EDGE_Pos;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Stat Removal feature
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  *         NbHeadLines number of lines to skip at the top of the image
  *         NbValidLines number of valid image line to keep after the skipped
  *                      head lines
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t NbHeadLines, uint32_t NbValidLines)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if ((NbHeadLines > (DCMIPP_P1SRCR_FIRSTLINEDEL_Msk >> DCMIPP_P1SRCR_FIRSTLINEDEL_Pos)) || (NbValidLines > (DCMIPP_P1SRCR_LASTLINE_Msk >> DCMIPP_P1SRCR_LASTLINE_Pos)))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1SRCR = (uint32_t)((NbHeadLines << DCMIPP_P1SRCR_FIRSTLINEDEL_Pos) | (NbValidLines << DCMIPP_P1SRCR_LASTLINE_Pos) | DCMIPP_P1SRCR_CROPEN);

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Stat Removal feature
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1SRCR &= ~(uint32_t)(DCMIPP_P1SRCR_CROPEN_Msk);

  return HAL_OK;
}

/**
  * @brief  Get the Stat Removal parameters
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  *         pNbHeadLines pointer receiving the number of lines to skip at the top of the image
  *         pNbValidLines pointer receiving the number of valid image line to keep after the skipped
  *                      head lines
  *         pEnable pointer receiving the status of the statistic removal function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *NbHeadLines, uint32_t *NbValidLines, uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (NbHeadLines == NULL) || (NbValidLines == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  *pEnable =  (pHdcmipp->Instance->P1SRCR & DCMIPP_P1SRCR_CROPEN_Msk) >> DCMIPP_P1SRCR_CROPEN_Pos;
  *NbHeadLines = (pHdcmipp->Instance->P1SRCR & DCMIPP_P1SRCR_FIRSTLINEDEL_Msk) >> DCMIPP_P1SRCR_FIRSTLINEDEL_Pos;
  *NbValidLines = (pHdcmipp->Instance->P1SRCR & DCMIPP_P1SRCR_LASTLINE_Msk) >> DCMIPP_P1SRCR_LASTLINE_Pos;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Bad Pixel Removal feature
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  *         Strength value representing the strength of the removal algorithm
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t Strength)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (Strength > 7)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1BPRCR = (uint32_t)((Strength << DCMIPP_P1BPRCR_STRENGTH_Pos) | DCMIPP_P1BPRCR_ENABLE);

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Bad Pixel Removal feature
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handles validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1BPRCR &= ~(uint32_t)(DCMIPP_P1BPRCR_ENABLE_Msk);

  return HAL_OK;
}

/**
  * @brief  Return the number of corrected Bad Pixel in the last frame
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  *         pCount pointer receiving the number of corrected bad pixels
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetBadPixelCount(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCount)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pCount == NULL))
  {
    return HAL_ERROR;
  }

  *pCount = (pHdcmipp->Instance->P1BPRSR & DCMIPP_P1BPRSR_BADCNT_Msk) >> DCMIPP_P1BPRSR_BADCNT_Pos;

  return HAL_OK;
}

/**
  * @brief  Get Bad Pixel Removal strength
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  *         pStrength pointer receiving the strength of the removal algorithm
  *         pEnable pointer receiving the status of the bad pixel function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pStrength, uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pStrength == NULL))
  {
    return HAL_ERROR;
  }

  *pEnable =  (pHdcmipp->Instance->P1BPRCR & DCMIPP_P1BPRCR_ENABLE_Msk) >> DCMIPP_P1BPRCR_ENABLE_Pos;
  *pStrength = (pHdcmipp->Instance->P1BPRCR & DCMIPP_P1BPRCR_STRENGTH_Msk) >> DCMIPP_P1BPRCR_STRENGTH_Pos;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP color Conversion for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where color conversion is configured (Main or Ancillary)
  * @param  pColorConversionConfig pointer to color conversion parameters
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_ColorConversionTypeDef *pColorConversionConfig)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pColorConversionConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CCCR = ((pColorConversionConfig->Clamp << DCMIPP_P1CCCR_CLAMP_Pos) & DCMIPP_P1CCCR_CLAMP_Msk) | \
                                 ((pColorConversionConfig->Type << DCMIPP_P1CCCR_TYPE_Pos) & DCMIPP_P1CCCR_TYPE_Msk);

    pHdcmipp->Instance->P1CCRR1 = ((pColorConversionConfig->RR << DCMIPP_P1CCRR1_RR_Pos) & DCMIPP_P1CCRR1_RR_Msk) | \
                                  ((pColorConversionConfig->RG << DCMIPP_P1CCRR1_RG_Pos) & DCMIPP_P1CCRR1_RG_Msk);

    pHdcmipp->Instance->P1CCRR2 = ((pColorConversionConfig->RB << DCMIPP_P1CCRR2_RB_Pos) & DCMIPP_P1CCRR2_RB_Msk) | \
                                  ((pColorConversionConfig->RA << DCMIPP_P1CCRR2_RA_Pos) & DCMIPP_P1CCRR2_RA_Msk);

    pHdcmipp->Instance->P1CCGR1 = ((pColorConversionConfig->GR << DCMIPP_P1CCGR1_GR_Pos) & DCMIPP_P1CCGR1_GR_Msk) | \
                                  ((pColorConversionConfig->GG << DCMIPP_P1CCGR1_GG_Pos) & DCMIPP_P1CCGR1_GG_Msk);

    pHdcmipp->Instance->P1CCGR2 = ((pColorConversionConfig->GB << DCMIPP_P1CCGR2_GB_Pos) & DCMIPP_P1CCGR2_GB_Msk) | \
                                  ((pColorConversionConfig->GA << DCMIPP_P1CCGR2_GA_Pos) & DCMIPP_P1CCGR2_GA_Msk);

    pHdcmipp->Instance->P1CCBR1 = ((pColorConversionConfig->BR << DCMIPP_P1CCBR1_BR_Pos) & DCMIPP_P1CCBR1_BR_Msk) | \
                                  ((pColorConversionConfig->BG << DCMIPP_P1CCBR1_BG_Pos) & DCMIPP_P1CCBR1_BG_Msk);

    pHdcmipp->Instance->P1CCBR2 = ((pColorConversionConfig->BB << DCMIPP_P1CCBR2_BB_Pos) & DCMIPP_P1CCBR2_BB_Msk) | \
                                  ((pColorConversionConfig->BA << DCMIPP_P1CCBR2_BA_Pos) & DCMIPP_P1CCBR2_BA_Msk);
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the color conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where color conversion is enable (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CCCR |= (uint32_t)DCMIPP_P1CCCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the color conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where the color conversion is to be disabled (Main or Ancillary)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1CCCR &= ~(uint32_t)DCMIPP_P1CCCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the DCMIPP color Conversion configuration for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where color conversion is configured (Main or Ancillary)
  * @param  pColorConversionConfig pointer receiving the color conversion parameters
  * @param  pEnable pointer receiving the status of the color conversion function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigColorConversion(DCMIPP_HandleTypeDef *pHdcmipp,
                                                      HAL_DCMIPP_PipeTypeDef Pipe,
                                                      DCMIPP_ColorConversionTypeDef *pColorConversionConfig,
                                                      uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pColorConversionConfig == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
	*pEnable = (pHdcmipp->Instance->P1CCCR & DCMIPP_P1CCCR_ENABLE_Msk) >> DCMIPP_P1CCCR_ENABLE_Pos;
    pColorConversionConfig->Clamp = (pHdcmipp->Instance->P1CCCR & DCMIPP_P1CCCR_CLAMP_Msk) >> DCMIPP_P1CCCR_CLAMP_Pos;
    pColorConversionConfig->Type = (pHdcmipp->Instance->P1CCCR & DCMIPP_P1CCCR_TYPE_Msk) >> DCMIPP_P1CCCR_TYPE_Pos;
    pColorConversionConfig->RR = (pHdcmipp->Instance->P1CCRR1 & DCMIPP_P1CCRR1_RR_Msk) >> DCMIPP_P1CCRR1_RR_Pos;
    pColorConversionConfig->RG = (pHdcmipp->Instance->P1CCRR1 & DCMIPP_P1CCRR1_RG_Msk) >> DCMIPP_P1CCRR1_RG_Pos;
    pColorConversionConfig->RB = (pHdcmipp->Instance->P1CCRR2 & DCMIPP_P1CCRR2_RB_Msk) >> DCMIPP_P1CCRR2_RB_Pos;
    pColorConversionConfig->RA = (pHdcmipp->Instance->P1CCRR2 & DCMIPP_P1CCRR2_RA_Msk) >> DCMIPP_P1CCRR2_RA_Pos;
    pColorConversionConfig->GR = (pHdcmipp->Instance->P1CCGR1 & DCMIPP_P1CCGR1_GR_Msk) >> DCMIPP_P1CCGR1_GR_Pos;
    pColorConversionConfig->GG = (pHdcmipp->Instance->P1CCGR1 & DCMIPP_P1CCGR1_GG_Msk) >> DCMIPP_P1CCGR1_GG_Pos;
    pColorConversionConfig->GB = (pHdcmipp->Instance->P1CCGR2 & DCMIPP_P1CCGR2_GB_Msk) >> DCMIPP_P1CCGR2_GB_Pos;
    pColorConversionConfig->GA = (pHdcmipp->Instance->P1CCGR2 & DCMIPP_P1CCGR2_GA_Msk) >> DCMIPP_P1CCGR2_GA_Pos;
    pColorConversionConfig->BR = (pHdcmipp->Instance->P1CCBR1 & DCMIPP_P1CCBR1_BR_Msk) >> DCMIPP_P1CCBR1_BR_Pos;
    pColorConversionConfig->BG = (pHdcmipp->Instance->P1CCBR1 & DCMIPP_P1CCBR1_BG_Msk) >> DCMIPP_P1CCBR1_BG_Pos;
    pColorConversionConfig->BB = (pHdcmipp->Instance->P1CCBR2 & DCMIPP_P1CCBR2_BB_Msk) >> DCMIPP_P1CCBR2_BB_Pos;
    pColorConversionConfig->BA = (pHdcmipp->Instance->P1CCBR2 & DCMIPP_P1CCBR2_BA_Msk) >> DCMIPP_P1CCBR2_BA_Pos;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP YUV Conversion for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where YUV conversion is configured (Main)
  * @param  pColorConversionConfig pointer to color conversion parameters
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_ColorConversionTypeDef *pColorConversionConfig)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pColorConversionConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1YUVCR = ((pColorConversionConfig->Clamp << DCMIPP_P1YUVCR_CLAMP_Pos) & DCMIPP_P1YUVCR_CLAMP_Msk) | \
                                 ((pColorConversionConfig->Type << DCMIPP_P1YUVCR_TYPE_Pos) & DCMIPP_P1YUVCR_TYPE_Msk);

    pHdcmipp->Instance->P1YUVRR1 = ((pColorConversionConfig->RR << DCMIPP_P1YUVRR1_RR_Pos) & DCMIPP_P1YUVRR1_RR_Msk) | \
                                  ((pColorConversionConfig->RG << DCMIPP_P1YUVRR1_RG_Pos) & DCMIPP_P1YUVRR1_RG_Msk);

    pHdcmipp->Instance->P1YUVRR2 = ((pColorConversionConfig->RB << DCMIPP_P1YUVRR2_RB_Pos) & DCMIPP_P1YUVRR2_RB_Msk) | \
                                  ((pColorConversionConfig->RA << DCMIPP_P1YUVRR2_RA_Pos) & DCMIPP_P1YUVRR2_RA_Msk);

    pHdcmipp->Instance->P1YUVGR1 = ((pColorConversionConfig->GR << DCMIPP_P1YUVGR1_GR_Pos) & DCMIPP_P1YUVGR1_GR_Msk) | \
                                  ((pColorConversionConfig->GG << DCMIPP_P1YUVGR1_GG_Pos) & DCMIPP_P1YUVGR1_GG_Msk);

    pHdcmipp->Instance->P1YUVGR2 = ((pColorConversionConfig->GB << DCMIPP_P1YUVGR2_GB_Pos) & DCMIPP_P1YUVGR2_GB_Msk) | \
                                  ((pColorConversionConfig->GA << DCMIPP_P1YUVGR2_GA_Pos) & DCMIPP_P1YUVGR2_GA_Msk);

    pHdcmipp->Instance->P1YUVBR1 = ((pColorConversionConfig->BR << DCMIPP_P1YUVBR1_BR_Pos) & DCMIPP_P1YUVBR1_BR_Msk) | \
                                  ((pColorConversionConfig->BG << DCMIPP_P1YUVBR1_BG_Pos) & DCMIPP_P1YUVBR1_BG_Msk);

    pHdcmipp->Instance->P1YUVBR2 = ((pColorConversionConfig->BB << DCMIPP_P1YUVBR2_BB_Pos) & DCMIPP_P1YUVBR2_BB_Msk) | \
                                  ((pColorConversionConfig->BA << DCMIPP_P1YUVBR2_BA_Pos) & DCMIPP_P1YUVBR2_BA_Msk);
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the YUV conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where YUV conversion is enable (Main)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1YUVCR |= (uint32_t)DCMIPP_P1YUVCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the YUV conversion feature.
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where the YUV conversion is to be disabled (Main)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1YUVCR &= ~(uint32_t)DCMIPP_P1YUVCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the DCMIPP Black Level Calibration for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where black level calibration is configured (Main only)
  * @param  pBlackLevelConfig pointer
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_BlackLevelTypeDef *pBlackLevelConfig)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pBlackLevelConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1BLCCR = (((pBlackLevelConfig->BLCR << DCMIPP_P1BLCCR_BLCR_Pos) & DCMIPP_P1BLCCR_BLCR_Msk) | \
                                   ((pBlackLevelConfig->BLCG << DCMIPP_P1BLCCR_BLCG_Pos) & DCMIPP_P1BLCCR_BLCG_Msk) | \
                                   ((pBlackLevelConfig->BLCB << DCMIPP_P1BLCCR_BLCB_Pos) & DCMIPP_P1BLCCR_BLCB_Msk));
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the DCMIPP Black Level Calibration for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where black level calibration is configured (Main only)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1BLCCR |= (uint32_t)DCMIPP_P1BLCCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the DCMIPP Black Level Calibration for the main pipe
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *               the configuration information for DCMIPP.
  * @param  Pipe pipe where black level calibration is configured (Main only)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->Instance->P1BLCCR &= ~(uint32_t)DCMIPP_P1BLCCR_ENABLE;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get black level calibration value on each component
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pBlackLevelConfig pointer to receive the calibration values
  * @param  pEnable pointer receiving the status of the black level function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp,
                                                            HAL_DCMIPP_PipeTypeDef Pipe,
                                                            DCMIPP_BlackLevelTypeDef *pBlackLevelConfig,
                                                            uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pBlackLevelConfig == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    *pEnable =  (pHdcmipp->Instance->P1BLCCR & DCMIPP_P1BLCCR_ENABLE_Msk) >> DCMIPP_P1BLCCR_ENABLE_Pos;
    pBlackLevelConfig->BLCR = (pHdcmipp->Instance->P1BLCCR & DCMIPP_P1BLCCR_BLCR_Msk) >> DCMIPP_P1BLCCR_BLCR_Pos;
    pBlackLevelConfig->BLCG = (pHdcmipp->Instance->P1BLCCR & DCMIPP_P1BLCCR_BLCG_Msk) >> DCMIPP_P1BLCCR_BLCG_Pos;
    pBlackLevelConfig->BLCB = (pHdcmipp->Instance->P1BLCCR & DCMIPP_P1BLCCR_BLCB_Msk) >> DCMIPP_P1BLCCR_BLCB_Pos;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the statistic extraction module
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  moduleId (1,2 or 3), there are 3 modules available
  * @param  pStatExtractionConfig configuration structure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId, HAL_DCMIPP_StatConfigTypeDef *pStatExtractionConfig)
{
  uint32_t tmp = 0;

  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pStatExtractionConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe) || (moduleId > 2))
  {
    return HAL_ERROR;
  }

  tmp = pStatExtractionConfig->mode |
        pStatExtractionConfig->src |
        pStatExtractionConfig->bins;

  switch(moduleId) {
  case 0:
    pHdcmipp->Instance->P1ST1CR = tmp;
    break;
  case 1:
    pHdcmipp->Instance->P1ST2CR = tmp;
    break;
  case 2:
    pHdcmipp->Instance->P1ST3CR = tmp;
    break;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the statistic extraction module configuration
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  moduleId (1,2 or 3), there are 3 modules available
  * @param  pStatExtractionConfig pointer receiving the configuration structure
  * @param  pEnable pointer receiving the status of the statistic extraction module configuration
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp,
                                                     HAL_DCMIPP_PipeTypeDef Pipe,
                                                     uint32_t moduleId,
                                                     HAL_DCMIPP_StatConfigTypeDef *pStatExtractionConfig,
                                                     uint8_t *pEnable)
{
  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pStatExtractionConfig == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe) || (moduleId > 2))
  {
    return HAL_ERROR;
  }

  switch(moduleId) {
  case 0:
    *pEnable = pHdcmipp->Instance->P1ST1CR & DCMIPP_P1ST1CR_ENABLE_Msk;
    pStatExtractionConfig->mode = pHdcmipp->Instance->P1ST1CR & DCMIPP_P1ST1CR_MODE_Msk;
    pStatExtractionConfig->src = pHdcmipp->Instance->P1ST1CR & DCMIPP_P1ST1CR_SRC_Msk;
    pStatExtractionConfig->bins = pHdcmipp->Instance->P1ST1CR & DCMIPP_P1ST1CR_BINS_Msk;
    break;
  case 1:
    *pEnable = pHdcmipp->Instance->P1ST2CR & DCMIPP_P1ST2CR_ENABLE_Msk;
    pStatExtractionConfig->mode = pHdcmipp->Instance->P1ST2CR & DCMIPP_P1ST1CR_MODE_Msk;
    pStatExtractionConfig->src = pHdcmipp->Instance->P1ST2CR & DCMIPP_P1ST1CR_SRC_Msk;
    pStatExtractionConfig->bins = pHdcmipp->Instance->P1ST2CR & DCMIPP_P1ST1CR_BINS_Msk;
    break;
  case 2:
    *pEnable = pHdcmipp->Instance->P1ST3CR & DCMIPP_P1ST3CR_ENABLE_Msk;
    pStatExtractionConfig->mode = pHdcmipp->Instance->P1ST3CR & DCMIPP_P1ST1CR_MODE_Msk;
    pStatExtractionConfig->src = pHdcmipp->Instance->P1ST3CR & DCMIPP_P1ST1CR_SRC_Msk;
    pStatExtractionConfig->bins = pHdcmipp->Instance->P1ST3CR & DCMIPP_P1ST1CR_BINS_Msk;
    break;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Configure the statistic extraction area (common for all modules)
            if all values are 0, then area cropping is disabled
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  X0 top left position
  * @param  Y0 top left position
  * @param  XSize width of the area
  * @param  YSize height of the area
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigStatExtractionArea(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  if ((X0 == 0) && (Y0 == 0) && (XSize == 0) && (YSize == 0))
  {
    pHdcmipp->Instance->P1STSTR = 0;
    pHdcmipp->Instance->P1STSZR = 0;
  }

  /* Disable the CROP before updating the values */
  pHdcmipp->Instance->P1STSZR &= ~DCMIPP_P1STSZR_CROPEN_Msk;
  pHdcmipp->Instance->P1STSTR = ((X0 << DCMIPP_P1STSTR_HSTART_Pos) & DCMIPP_P1STSTR_HSTART_Msk) |
                                ((Y0 << DCMIPP_P1STSTR_VSTART_Pos) & DCMIPP_P1STSTR_VSTART_Msk);
  pHdcmipp->Instance->P1STSZR = ((XSize << DCMIPP_P1STSZR_HSIZE_Pos) & DCMIPP_P1STSZR_HSIZE_Msk) |
                                ((YSize << DCMIPP_P1STSZR_VSIZE_Pos) & DCMIPP_P1STSZR_VSIZE_Msk) |
                                DCMIPP_P1STSZR_CROPEN;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the statistic extraction area configuration(common for all modules)
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  X0 top left position
  * @param  Y0 top left position
  * @param  XSize width of the area
  * @param  YSize height of the area
  * @param  pEnable pointer receiving the status of the statistic area configuration
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatExtractionArea(DCMIPP_HandleTypeDef *pHdcmipp,
                                                      HAL_DCMIPP_PipeTypeDef Pipe,
                                                      uint32_t *pX0,
                                                      uint32_t *pY0,
                                                      uint32_t *pXSize,
                                                      uint32_t *pYSize,
                                                      uint8_t *pEnable)
{
  /* Check handles validity */
  if ((pHdcmipp == NULL) || (pX0 == NULL) || (pY0 == NULL) || (pXSize == NULL) || (pYSize == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  *pEnable = (pHdcmipp->Instance->P1STSZR & DCMIPP_P1STSZR_CROPEN_Msk) >> DCMIPP_P1STSZR_CROPEN_Pos;
  *pX0 = (pHdcmipp->Instance->P1STSTR & DCMIPP_P1STSTR_HSTART_Msk) >> DCMIPP_P1STSTR_HSTART_Pos;
  *pY0 = (pHdcmipp->Instance->P1STSTR & DCMIPP_P1STSTR_VSTART_Msk) >> DCMIPP_P1STSTR_VSTART_Pos;
  *pXSize = (pHdcmipp->Instance->P1STSZR & DCMIPP_P1STSZR_HSIZE_Msk) >> DCMIPP_P1STSZR_HSIZE_Pos;
  *pYSize = (pHdcmipp->Instance->P1STSZR & DCMIPP_P1STSZR_VSIZE_Msk) >> DCMIPP_P1STSZR_VSIZE_Pos;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the statistic extraction module
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  moduleId (1,2 or 3), there are 3 modules available
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe) || (moduleId > 2))
  {
    return HAL_ERROR;
  }

  switch(moduleId) {
  case 0:
    pHdcmipp->Instance->P1ST1CR |= DCMIPP_P1ST1CR_ENABLE;
    break;
  case 1:
    pHdcmipp->Instance->P1ST2CR |= DCMIPP_P1ST2CR_ENABLE;
    break;
  case 2:
    pHdcmipp->Instance->P1ST3CR |= DCMIPP_P1ST3CR_ENABLE;
    break;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the statistic extraction module
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  moduleId (1,2 or 3), there are 3 modules available
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe) || (moduleId > 2))
  {
    return HAL_ERROR;
  }

  switch(moduleId) {
  case 0:
    pHdcmipp->Instance->P1ST1CR &= ~DCMIPP_P1ST1CR_ENABLE_Msk;
    break;
  case 1:
    pHdcmipp->Instance->P1ST2CR &= ~DCMIPP_P1ST2CR_ENABLE_Msk;
    break;
  case 2:
    pHdcmipp->Instance->P1ST3CR &= ~DCMIPP_P1ST3CR_ENABLE_Msk;
    break;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get Statistic accumulated value for a specific module
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  moduleId (1,2 or 3), there are 3 modules available
  * @param  pAccu pointer to receive the accumulated value
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetStatAccuValue(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId, uint32_t *pAccu)
{
  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pAccu == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe) || (moduleId > 2))
  {
    return HAL_ERROR;
  }

  switch(moduleId) {
  case 0:
    *pAccu = pHdcmipp->Instance->P1ST1SR & DCMIPP_P1ST1SR_ACCU_Msk;
    break;
  case 1:
    *pAccu = pHdcmipp->Instance->P1ST2SR & DCMIPP_P1ST2SR_ACCU_Msk;
    break;
  case 2:
    *pAccu = pHdcmipp->Instance->P1ST3SR & DCMIPP_P1ST3SR_ACCU_Msk;
    break;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Config the Exposure Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pExposureControl pointer to the structure of config for exposure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ExposureControlTypeDef *pExposureControl)
{
  uint32_t p1excr1, p1excr2;

  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pExposureControl == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  p1excr1 = pHdcmipp->Instance->P1EXCR1 & DCMIPP_P1EXCR1_ENABLE_Msk;
  p1excr1 |= ((pExposureControl->R.shift << DCMIPP_P1EXCR1_SHFR_Pos) |
              (pExposureControl->R.multiply << DCMIPP_P1EXCR1_MULTR_Pos));
  p1excr2 = (pExposureControl->G.shift << DCMIPP_P1EXCR2_SHFG_Pos) |
            (pExposureControl->G.multiply << DCMIPP_P1EXCR2_MULTG_Pos) |
            (pExposureControl->B.shift << DCMIPP_P1EXCR2_SHFB_Pos) |
            (pExposureControl->B.multiply << DCMIPP_P1EXCR2_MULTB_Pos);

  pHdcmipp->Instance->P1EXCR1 = p1excr1;
  pHdcmipp->Instance->P1EXCR2 = p1excr2;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Enable the Exposure Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1EXCR1 |= DCMIPP_P1EXCR1_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Exposure Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1EXCR1 &= ~DCMIPP_P1EXCR1_ENABLE_Msk;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the Exposure Control settings
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pExposureControl pointer receiving the structure of config for exposure
  * @param  pEnable pointer receiving the status of the exposure function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigExposureControl(DCMIPP_HandleTypeDef *pHdcmipp,
                                                      HAL_DCMIPP_PipeTypeDef Pipe,
                                                      HAL_DCMIPP_ExposureControlTypeDef *pExposureControl,
                                                      uint8_t *pEnable)
{
  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pExposureControl == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }

  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    *pEnable = (pHdcmipp->Instance->P1EXCR1 & DCMIPP_P1EXCR1_ENABLE_Msk) >> DCMIPP_P1EXCR1_ENABLE_Pos;
    pExposureControl->R.shift = (pHdcmipp->Instance->P1EXCR1 & DCMIPP_P1EXCR1_SHFR_Msk) >> DCMIPP_P1EXCR1_SHFR_Pos;
    pExposureControl->R.multiply = (pHdcmipp->Instance->P1EXCR1 & DCMIPP_P1EXCR1_MULTR_Msk) >> DCMIPP_P1EXCR1_MULTR_Pos;
    pExposureControl->G.shift = (pHdcmipp->Instance->P1EXCR2 & DCMIPP_P1EXCR2_SHFG_Msk) >> DCMIPP_P1EXCR2_SHFG_Pos;
    pExposureControl->G.multiply = (pHdcmipp->Instance->P1EXCR2 & DCMIPP_P1EXCR2_MULTG_Msk) >> DCMIPP_P1EXCR2_MULTG_Pos;
    pExposureControl->B.shift = (pHdcmipp->Instance->P1EXCR2 & DCMIPP_P1EXCR2_SHFB_Msk) >> DCMIPP_P1EXCR2_SHFB_Pos;
    pExposureControl->B.multiply = (pHdcmipp->Instance->P1EXCR2 & DCMIPP_P1EXCR2_MULTB_Msk) >> DCMIPP_P1EXCR2_MULTB_Pos;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Config the Contrast Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pContrastControl pointer to the contrast control structure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_ConfigContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ContrastControlTypeDef *pContrastControl)
{
  uint32_t p1ctcr1, p1ctcr2, p1ctcr3;

  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pContrastControl == NULL))
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  p1ctcr1 = pHdcmipp->Instance->P1CTCR1 & DCMIPP_P1CTCR1_ENABLE_Msk;
  p1ctcr1 |= pContrastControl->LUM_0 << DCMIPP_P1CTCR1_LUM0_Pos;
  p1ctcr2 = (pContrastControl->LUM_32 << DCMIPP_P1CTCR2_LUM1_Pos) |
            (pContrastControl->LUM_64 << DCMIPP_P1CTCR2_LUM2_Pos) |
            (pContrastControl->LUM_96 << DCMIPP_P1CTCR2_LUM3_Pos) |
            (pContrastControl->LUM_128 << DCMIPP_P1CTCR2_LUM4_Pos);
  p1ctcr3 = (pContrastControl->LUM_160 << DCMIPP_P1CTCR3_LUM5_Pos) |
            (pContrastControl->LUM_192 << DCMIPP_P1CTCR3_LUM6_Pos) |
            (pContrastControl->LUM_224 << DCMIPP_P1CTCR3_LUM7_Pos) |
            (pContrastControl->LUM_256 << DCMIPP_P1CTCR3_LUM8_Pos);

  pHdcmipp->Instance->P1CTCR1 = p1ctcr1;
  pHdcmipp->Instance->P1CTCR2 = p1ctcr2;
  pHdcmipp->Instance->P1CTCR3 = p1ctcr3;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Config the Contrast Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pContrastControl pointer to the contrast control structure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1CTCR1 |= DCMIPP_P1CTCR1_ENABLE;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Config the Contrast Control
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pContrastControl pointer to the contrast control structure
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  if (!IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->P1CTCR1 &= ~DCMIPP_P1CTCR1_ENABLE_Msk;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Get the Contrast Control configuration
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Pipe pipe where statistic extraction is performed (Main only)
  * @param  pContrastControl pointer receiving the contrast control structure
  * @param  pEnable pointer receiving the status of the contrast function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_GetConfigContrastControl(DCMIPP_HandleTypeDef *pHdcmipp,
                                                      HAL_DCMIPP_PipeTypeDef Pipe,
                                                      HAL_DCMIPP_ContrastControlTypeDef *pContrastControl,
                                                      uint8_t *pEnable)
{
  /* Check handle validity */
  if ((pHdcmipp == NULL) || (pContrastControl == NULL) || (pEnable == NULL))
  {
    return HAL_ERROR;
  }


  if (IS_DCMIPP_MAIN_PIPE(Pipe))
  {
    *pEnable = (pHdcmipp->Instance->P1CTCR1 & DCMIPP_P1CTCR1_ENABLE_Msk) >> DCMIPP_P1CTCR1_ENABLE_Pos;
    pContrastControl->LUM_0 = (pHdcmipp->Instance->P1CTCR1 & DCMIPP_P1CTCR1_LUM0_Msk) >> DCMIPP_P1CTCR1_LUM0_Pos;
    pContrastControl->LUM_32 = (pHdcmipp->Instance->P1CTCR2 & DCMIPP_P1CTCR2_LUM1_Msk) >> DCMIPP_P1CTCR2_LUM1_Pos;
    pContrastControl->LUM_64 = (pHdcmipp->Instance->P1CTCR2 & DCMIPP_P1CTCR2_LUM2_Msk) >> DCMIPP_P1CTCR2_LUM2_Pos;
    pContrastControl->LUM_96 = (pHdcmipp->Instance->P1CTCR2 & DCMIPP_P1CTCR2_LUM3_Msk) >> DCMIPP_P1CTCR2_LUM3_Pos;
    pContrastControl->LUM_128 = (pHdcmipp->Instance->P1CTCR2 & DCMIPP_P1CTCR2_LUM4_Msk) >> DCMIPP_P1CTCR2_LUM4_Pos;
    pContrastControl->LUM_160 = (pHdcmipp->Instance->P1CTCR3 & DCMIPP_P1CTCR3_LUM5_Msk) >> DCMIPP_P1CTCR3_LUM5_Pos;
    pContrastControl->LUM_192 = (pHdcmipp->Instance->P1CTCR3 & DCMIPP_P1CTCR3_LUM6_Msk) >> DCMIPP_P1CTCR3_LUM6_Pos;
    pContrastControl->LUM_224 = (pHdcmipp->Instance->P1CTCR3 & DCMIPP_P1CTCR3_LUM7_Msk) >> DCMIPP_P1CTCR3_LUM7_Pos;
    pContrastControl->LUM_256 = (pHdcmipp->Instance->P1CTCR3 & DCMIPP_P1CTCR3_LUM8_Msk) >> DCMIPP_P1CTCR3_LUM8_Pos;
  }
  else
  {
    pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_PIPE_ID;
    return HAL_ERROR;
  }

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}



/**
  * @brief  Config and Enable the Test Pattern Generator
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @param  Config Config of the generated data
  * @param  Flags Additional informations
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_EnableTestPatternGenerator(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_TPGConfigTypeDef *Config)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  /* Set Width & Height */
  if ((Config->Width > (DCMIPP_CMTPGCR1_WIDTH_Msk >> DCMIPP_CMTPGCR1_WIDTH_Pos)) ||
      (Config->Height > (DCMIPP_CMTPGCR1_HEIGHT_Msk >> DCMIPP_CMTPGCR1_HEIGHT_Pos)))
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->CMTPGCR1 = (Config->Width << DCMIPP_CMTPGCR1_WIDTH_Pos) |
	  			 (Config->Height << DCMIPP_CMTPGCR1_HEIGHT_Pos);

  pHdcmipp->Instance->CMTPGCR2 = Config->Mode | DCMIPP_CMTPGCR2_TPGEN;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @brief  Disable the Test Pattern Generator
  * @param  pHdcmipp pointer to a DCMIPP_HandleTypeDef structure that contains
  *         the configuration information for DCMIPP.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DCMIPP_DisableTestPatternGenerator(DCMIPP_HandleTypeDef *pHdcmipp)
{
  /* Check handle validity */
  if (pHdcmipp == NULL)
  {
    return HAL_ERROR;
  }

  pHdcmipp->Instance->CMTPGCR2 &= ~DCMIPP_CMTPGCR2_TPGEN_Msk;

  /* Process Unlocked */
  pHdcmipp->ErrorCode = HAL_DCMIPP_ERROR_NONE;

  return HAL_OK;
}

/**
  * @}
  */
#endif /* HAL_DCMIPP_MODULE_ENABLED */
/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
