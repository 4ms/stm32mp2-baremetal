/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_cortex.c
  * @author  MCD Application Team
  * @brief   CORTEX HAL module driver.
  *          This file provides firmware functions to manage the following
  *          functionalities of the CORTEX:
  *           + Initialization and Configuration functions
  *           + Peripheral Control functions
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  @verbatim
  ==============================================================================
                        ##### How to use this driver #####
  ==============================================================================
    [..]
    *** How to configure Interrupts using CORTEX HAL driver ***
    ===========================================================
    [..]
    This section provides functions allowing to configure the NVIC interrupts (IRQ).
    The Cortex-M exceptions are managed by CMSIS functions.

    (#) Configure the NVIC Priority Grouping using HAL_NVIC_SetPriorityGrouping()
        function according to the following table.
    (#) Configure the priority of the selected IRQ Channels using HAL_NVIC_SetPriority().
    (#) Enable the selected IRQ Channels using HAL_NVIC_EnableIRQ().
    (#) please refer to programming manual for details in how to configure priority.

     -@- When the NVIC_PRIORITYGROUP_0 is selected, IRQ preemption is no more possible.
         The pending IRQ priority will be managed only by the sub priority.

     -@- IRQ priority order (sorted by highest to lowest priority):
        (+@) Lowest preemption priority
        (+@) Lowest sub priority
        (+@) Lowest hardware priority (IRQ number)

    [..]
    *** How to configure SysTick using CORTEX HAL driver ***
    ========================================================
    [..]
    Setup SysTick Timer for time base.

   (+) The HAL_SYSTICK_Config() function calls the SysTick_Config() function which
       is a CMSIS function that:
        (++) Configures the SysTick Reload register with value passed as function parameter.
        (++) Configures the SysTick IRQ priority to the lowest value.
        (++) Resets the SysTick Counter register.
        (++) Configures the SysTick Counter clock source to be Core Clock Source (HCLK).
        (++) Enables the SysTick Interrupt.
        (++) Starts the SysTick Counter.

   (+) You can change the SysTick Clock source to be HCLK_Div8 by calling the function
       HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK_DIV8) just after the
       HAL_SYSTICK_Config() function call.

   (+) You can change the SysTick IRQ priority by calling the
       HAL_NVIC_SetPriority(SysTick_IRQn,...) function just after the HAL_SYSTICK_Config() function
       call. The HAL_NVIC_SetPriority() call the NVIC_SetPriority() function which is a CMSIS function.

   (+) To adjust the SysTick time base, use the following formula:

       Reload Value = SysTick Counter Clock (Hz) x  Desired Time base (s)
       (++) Reload Value is the parameter to be passed for HAL_SYSTICK_Config() function
       (++) Reload Value should not exceed 0xFFFFFF

  @endverbatim
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32mp2xx_hal.h"

/** @addtogroup STM32MP2xx_HAL_Driver
  * @{
  */

/** @defgroup CORTEX CORTEX
  * @brief CORTEX HAL module driver
  * @{
  */

#ifdef HAL_CORTEX_MODULE_ENABLED

/* Private defines -----------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @defgroup CORTEX_Exported_Functions CORTEX Exported Functions
  * @{
  */


/** @defgroup CORTEX_Exported_Functions_Group1 Initialization and de-initialization functions
 *  @brief    Initialization and Configuration functions
 *
@verbatim
  ==============================================================================
              ##### Initialization and Configuration functions #####
  ==============================================================================
    [..]
      This section provides the CORTEX HAL driver functions allowing to configure Interrupts
      SysTick functionalities

@endverbatim
  * @{
  */

#ifdef CORE_CM33
/**
  * @brief  Set the priority grouping field (pre-emption priority and sub-priority)
  *         using the required unlock sequence.
  * @param  PriorityGrouping: indicator of the maximum Group Priority length in bit
  *                           (when it is not limited by __NVIC_PRIO_BITS value)
  *         This parameter can be one of the following values
  *         (given for __NVIC_PRIO_BITS = 4) :
  *         @arg NVIC_PRIORITYGROUP_0: 0 bit  for pre-emption priority
  *                                    4 bits for sub-priority
  *         @arg NVIC_PRIORITYGROUP_1: 1 bit  for pre-emption priority
  *                                    3 bits for sub-priority
  *         @arg NVIC_PRIORITYGROUP_2: 2 bits for pre-emption priority
  *                                    2 bits for sub-priority
  *         @arg NVIC_PRIORITYGROUP_3: 3 bits for pre-emption priority
  *                                    1 bit  for sub-priority
  *         @arg NVIC_PRIORITYGROUP_4: 4 bits for pre-emption priority,
  *                                    0 bit  for sub-priority
  * @note   When NVIC_PRIORITYGROUP_0 is selected, IRQ pre-emption is no more possible.
  *         The pending IRQ priority will be managed only by the sub-priority.
  * @retval None
  */
void HAL_NVIC_SetPriorityGrouping(uint32_t PriorityGrouping)
{
  /* Check input parameter */
  assert_param(IS_NVIC_PRIORITY_GROUPING(PriorityGrouping));

  /* Set the PRIGROUP[10:8] bits according to the PriorityGrouping parameter value */
  NVIC_SetPriorityGrouping(PriorityGrouping);

  return;
}
#endif /* CORE_CM33 */

/**
  * @brief  Set the priority of an interrupt.
  * @param  IRQn External interrupt number .
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to stm32mp2xx.h file)
  * @param  PreemptPriority The pre-emption priority for the IRQn channel.
  *         A lower priority value indicates a higher priority
  * @param  SubPriority the sub-priority level for the IRQ channel.
  *         A lower priority value indicates a higher priority.
  *         Note : It only exists on Cortex-M33 core
  *                It is dummy (and thus ignored) on Cortex-M0+ core
  * @retval None
  */
void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority)
{
  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));
  assert_param(IS_NVIC_PREEMPTION_PRIORITY(PreemptPriority));
  /* Note : SubPriority doesn't exist on M0+ */
  /*        Then, it shall be set to 0       */
  assert_param(IS_NVIC_SUB_PRIORITY(SubPriority));

#ifdef CORE_CM0PLUS
  /* Cortex-M0+ case : */
  NVIC_SetPriority(IRQn, PreemptPriority);
#endif /* CORE_CM0PLUS */
#ifdef CORE_CM33
  /* Cortex-M33 case : */
  uint32_t prioritygroup = NVIC_GetPriorityGrouping();
  NVIC_SetPriority(IRQn, NVIC_EncodePriority(prioritygroup, PreemptPriority, SubPriority));
#endif /* CORE_CM33 */
  return;
}

/**
  * @brief  Enable a device specific interrupt in the NVIC interrupt controller.
  * @param  IRQn External interrupt number.
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval None
  */
void HAL_NVIC_EnableIRQ(IRQn_Type IRQn)
{
  /* Check input parameter */
  assert_param(IS_NVIC_DEVICE_IRQ(IRQn));

  /* Enable interrupt */
  NVIC_EnableIRQ(IRQn);
}

/**
  * @brief  Disable a device specific interrupt in the NVIC interrupt controller.
  * @param  IRQn External interrupt number.
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval None
  */
void HAL_NVIC_DisableIRQ(IRQn_Type IRQn)
{
  /* Check input parameter */
  assert_param(IS_NVIC_DEVICE_IRQ(IRQn));

  /* Disable interrupt */
  NVIC_DisableIRQ(IRQn);
}

/**
  * @brief  Initiate a system reset request to reset the MCU.
  * @retval None
  */
void HAL_NVIC_SystemReset(void)
{
  /* System Reset */
  NVIC_SystemReset();
}

/**
  * @brief  Initializes the System Timer and its interrupt, and starts the System Tick Timer.
  *         Counter is in free running mode to generate periodic interrupts.
  * @param  TicksNumb: Specifies the ticks Number of ticks between two interrupts.
  * @retval status:  - 0  Function succeeded.
  *                  - 1  Function failed.
  */
uint32_t HAL_SYSTICK_Config(uint32_t TicksNumb)
{
   return SysTick_Config(TicksNumb);
}

/**
  * @brief  Configures the SysTick clock source.
  * @param  CLKSource: specifies the SysTick clock source.
  *          This parameter can be one of the following values:
  *             @arg SYSTICK_CLKSOURCE_HCLK_DIV8: AHB clock divided by 8 selected as SysTick clock source.
  *             @arg SYSTICK_CLKSOURCE_HCLK: AHB clock selected as SysTick clock source.
  * @retval None
  */
void HAL_SYSTICK_CLKSourceConfig(uint32_t CLKSource)
{
  /* Check input parameter */
  assert_param(IS_SYSTICK_CLK_SOURCE(CLKSource));
  if (CLKSource == SYSTICK_CLKSOURCE_HCLK)
  {
    SysTick->CTRL |= SYSTICK_CLKSOURCE_HCLK;
  }
  else
  {
    SysTick->CTRL &= ~SYSTICK_CLKSOURCE_HCLK;
  }
}

/**
  * @}
  */

/** @defgroup CORTEX_Exported_Functions_Group2 Peripheral Control functions
 *  @brief   Cortex control functions
 *
@verbatim
  ==============================================================================
                      ##### Peripheral Control functions #####
  ==============================================================================
    [..]
      This subsection provides a set of functions allowing to control the CORTEX
      (NVIC, SYSTICK, MPU) functionalities.


@endverbatim
  * @{
  */
/**
  * @brief  Get the priority of an interrupt.
  * @param  IRQn: External interrupt number.
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @param  pPreemptPriority: Pointer on the Preemptive priority value (starting from 0).
  * @param  pSubPriority: Pointer on the Subpriority value (starting from 0).
  *                       Not existing on CM0+ (returned value set to 0).
  * @retval None
  */
void HAL_NVIC_GetPriority(IRQn_Type IRQn, uint32_t *pPreemptPriority, uint32_t *pSubPriority)
{
  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));

#ifdef CORE_CM0PLUS
  /* Cortex-M0+ case */
  /* Get priority for Cortex-M system or device specific interrupts */
  /* from appropriate NVIC register, when no "Priority Grouping" is implemented. */
  *pPreemptPriority = NVIC_GetPriority(IRQn);
  *pSubPriority = 0;
#endif /* CORE_CM0PLUS */
#ifdef CORE_CM33
  /* Cortex-M33 case */
  /* Get priority for Cortex-M system or device specific interrupts  */
  /* from appropriate NVIC register according to "Priority Grouping" */
  /* internal setting */
  /* as components "Group Priority" and "SubPriority" specified */
  /* in ARM reference documents */
  NVIC_DecodePriority( NVIC_GetPriority(IRQn),
                       NVIC_GetPriorityGrouping(),
                       pPreemptPriority, pSubPriority);
#endif /* CORE_CM33 */
  return;
}

/**
  * @brief  Set Pending bit of an external interrupt.
  * @param  IRQn External interrupt number
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval None
  */
void HAL_NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  /* For Cortex-M with Security Extension case in secure state, */
  /* Get interrupt target state (CMSIS function NVIC_GetTargetState) */
  /* - when interrupt is non-secure */
  if (NVIC_NONSECURE_INTERRUPT == NVIC_GetTargetState(IRQn))
  {
  /*   Set corresponding bit "SETPEND" in corresponding NVIC_ISPR<i>_NS register */
  /*   by using appropriate CMSIS function TZ_NVIC_<x>IRQ_NS */
    TZ_NVIC_SetPendingIRQ_NS(IRQn);
  }
  /* - when interrupt is secure */
  else
  {
  /*   Set corresponding bit "SETPEND" in corresponding NVIC_ISPR<i> register */
  /*   by using appropriate CMSIS function NVIC_<x>IRQ */
    NVIC_SetPendingIRQ(IRQn);
  }
#else
  /* Cortex-M with Security Extension case in non-secure state */
  /* and Cortex-M without Security Extension */
  /* Set corresponding bit "SETPEND" in corresponding NVIC_ISPR<i> register */
  /* by using appropriate CMSIS function NVIC_<x>IRQ */
  NVIC_SetPendingIRQ(IRQn);
#endif /* defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) */
  return;
}

/**
  * @brief  Get Pending Interrupt (read the pending register in the NVIC
  *         and return the pending bit for the specified interrupt).
  * @param  IRQn External interrupt number.
  *          This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval status: - 0  Interrupt status is not pending.
  *                 - 1  Interrupt status is pending.
  */
uint32_t HAL_NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
  uint32_t pending_irq_status;

  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  /* For Cortex-M with Security Extension case in secure state, */
  /* Get interrupt target state (CMSIS function NVIC_GetTargetState) */
  /* - when interrupt is non-secure */
  if (NVIC_NONSECURE_INTERRUPT == NVIC_GetTargetState(IRQn))
  {
  /*   Read corresponding bit "SETPEND" in corresponding NVIC_ISPR<i>_NS register */
  /*   (or corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i>_NS register) */
  /*   by using appropriate CMSIS function TZ_NVIC_<x>IRQ_NS */
    pending_irq_status = TZ_NVIC_GetPendingIRQ_NS(IRQn);
  }
  /* - when interrupt is secure */
  else
  {
  /*   Read corresponding bit "SETPEND" in corresponding NVIC_ISPR<i> register */
  /*   (or corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i> register) */
  /*   by using appropriate CMSIS function NVIC_<x>IRQ */
    pending_irq_status = NVIC_GetPendingIRQ(IRQn);
  }
#else
  /* Cortex-M with Security Extension case in non-secure state */
  /* and Cortex-M without Security Extension */
  /* Read corresponding bit "SETPEND" in corresponding NVIC_ISPR<i> register */
  /* (or corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i> register) */
  /* by using appropriate CMSIS function NVIC_<x>IRQ */
  pending_irq_status = NVIC_GetPendingIRQ(IRQn);
#endif /* defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) */
  return pending_irq_status;
}

/**
  * @brief  Clear the pending bit of an external interrupt.
  * @param  IRQn External interrupt number.
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval None
  */
void HAL_NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  /* For Cortex-M with Security Extension case in secure state, */
  /* Get interrupt target state (CMSIS function NVIC_GetTargetState) */
  /* - when interrupt is non-secure */
  if (NVIC_NONSECURE_INTERRUPT == NVIC_GetTargetState(IRQn))
  {
  /*   Set corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i>_NS register */
  /*   by using appropriate CMSIS function TZ_NVIC_<x>IRQ_NS */
    TZ_NVIC_ClearPendingIRQ_NS(IRQn);
  }
  /* - when interrupt is secure */
  else
  {
  /*   Set corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i> register */
  /*   by using appropriate CMSIS function NVIC_<x>IRQ */
    NVIC_ClearPendingIRQ(IRQn);
  }
#else
  /* Cortex-M with Security Extension case in non-secure state */
  /* and Cortex-M without Security Extension */
  /* Set corresponding bit "CLRPEND" in corresponding NVIC_ICPR<i> register */
  /* by using appropriate CMSIS function NVIC_<x>IRQ */
  NVIC_ClearPendingIRQ(IRQn);
#endif /* defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) */
  return;
}


#ifdef CORE_CM33
/* Cortex-M33 case only*/
/**
  * @brief  Get the priority grouping field from the NVIC Interrupt Controller.
  * @retval "Priority Grouping"
  */
uint32_t HAL_NVIC_GetPriorityGrouping(void)
{
  /* Get "Priority Grouping" value, aka SCB->AIRCR bits [10:8] "PRIGROUP" */
  return NVIC_GetPriorityGrouping();
}

/**
  * @brief Get active interrupt (read the active register in NVIC and return the active bit).
  * @param IRQn External interrupt number
  *         This parameter can be an enumerator of IRQn_Type enumeration
  *         (For the complete STM32 Devices IRQ Channels list, please refer to the appropriate CMSIS device file (stm32mp2xxxx.h))
  * @retval status: - 0  Interrupt status is not pending.
  *                 - 1  Interrupt status is pending.
  */
uint32_t HAL_NVIC_GetActive(IRQn_Type IRQn)
{
  /* Check input parameter */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));

  /* Return 1 if active else 0 */
  return NVIC_GetActive(IRQn);
}

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
void HAL_NVIC_ConfigInterruptSecurity(IRQn_Type IRQn, uint32_t IRQSecurityState)
{
  /* Check input parameters */
  assert_param(IS_NVIC_IRQN_TYPE(IRQn));
  assert_param(IS_NVIC_IRQ_SECURITY_STATE(IRQSecurityState));

  /* When secure state is selected, */
  if (NVIC_SECURE_INTERRUPT == IRQSecurityState)
  {
  /* then clear corresponding bit "ITNS" in corresponding NVIC_ITNSx register */
  /* (see #4.4.7 in [ARM 100235_0003_00_en]) */
    NVIC_ClearTargetState(IRQn);
  }
  /* When non-secure state is selected, */
  else
  {
  /* then set corresponding bit "ITNS" in corresponding NVIC_ITNSx register */
  /* (see #4.4.7 in [ARM 100235_0003_00_en]) */
    NVIC_SetTargetState(IRQn);
  }
  return;
}
#endif /* defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) */
#endif /* CORE_CM33 */

/**
  * @brief  This function handles SYSTICK interrupt request.
  * @retval None
  */
void HAL_SYSTICK_IRQHandler(void)
{
  HAL_SYSTICK_Callback();
}

/**
  * @brief  SYSTICK callback.
  * @retval None
  */
__weak void HAL_SYSTICK_Callback(void)
{
  /* NOTE : This function Should not be modified, when the callback is needed,
            the HAL_SYSTICK_Callback could be implemented in the user file
   */
}

#if (__MPU_PRESENT == 1U)
/* [MPU/FUNCTIONS] M0+ (ARMv6-M) / M33 (ARMv8-M) common implementations */
/**
  * @brief  Disables the MPU
  * @retval None
  */
void HAL_MPU_Disable(void)
{
  __DMB(); /* Data Memory Barrier operation to force any outstanding writes to memory before disabling the MPU */

#ifdef CORE_CM33
  /* Disable fault exceptions on Cortex-M33 (not present on Cortex-M0+) */
  SCB->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;
#endif /* CORE_CM33 */

  /* Disable MPU by clearing control register                   */
  /* see Table 4-52 page 323 in [ARM 100234_0003_00]            */
  /* ("ARM Cortex-M33 Processor User Guide Reference Material") */
  /* see Table 4-28 page 115 in [ARM DUI0605B]                  */
  /* ("Cortex-M0+ User Guide Reference Material")               */
  MPU->CTRL = 0x0;

  /* Follow ARM recommendation with */
  /* Data Synchronization and Instruction Synchronization Barriers to ensure MPU configuration */
  __DSB(); /* Ensure that the subsequent instruction is executed only after the write to memory */
  __ISB(); /* Flush and refill pipeline with updated MPU configuration settings */
}

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
/**
  * @brief  Disable the non-secure MPU.
  * @retval None
  */
void HAL_MPU_Disable_NS(void)
{
  __DMB(); /* Data Memory Barrier operation to force any outstanding writes to memory before disabling the MPU */

  /* Disable fault exceptions */
  SCB_NS->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;

#ifdef CORE_CM33
  /* Disable the MPU */
  MPU_NS->CTRL = 0x0;
#endif /* CORE_CM33 */

  /* Follow ARM recommendation with */
  /* Data Synchronization and Instruction Synchronization Barriers to ensure MPU configuration */
  __DSB(); /* Ensure that the subsequent instruction is executed only after the write to memory */
  __ISB(); /* Flush and refill pipeline with updated MPU configuration settings */
}
#endif /* __ARM_FEATURE_CMSE */

/**
  * @brief  Enable the MPU.
  * @param  MPU_Control: Specifies the control mode of the MPU during hard fault,
  *          NMI, FAULTMASK and privileged access to the default memory
  *          This parameter shall be one of the following values:
  *            @arg MPU_HFNMI_PRIVDEF_NONE
  *            @arg MPU_HARDFAULT_NMI
  *            @arg MPU_PRIVILEGED_DEFAULT
  *            @arg MPU_HFNMI_PRIVDEF
  * @retval None
  */
void HAL_MPU_Enable(uint32_t MPU_Control)
{
  __DMB(); /* Data Memory Barrier operation to force any outstanding writes to memory before enabling the MPU */

  /* Check input configuration */
  assert_param(IS_MPU_HFNMI_PRIVDEF_CONTROL(MPU_Control));

  /* Enable the MPU */
  MPU->CTRL = MPU_Control | MPU_CTRL_ENABLE_Msk;

#ifdef CORE_CM33
  /* Enable memory fault exceptions on Cortex-M33 (not present on Cortex-M0+) */
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif /* CORE_CM33 */

  /* Follow ARM recommendation with */
  /* Data Synchronization and Instruction Synchronization Barriers to ensure MPU configuration */
  __DSB(); /* Ensure that the subsequent instruction is executed only after the write to memory */
  __ISB(); /* Flush and refill pipeline with updated MPU configuration settings */
}

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
/**
  * @brief  Enable the non-secure MPU.
  * @param  MPU_Control: Specifies the control mode of the MPU during hard fault,
  *          NMI, FAULTMASK and privileged access to the default memory
  *          This parameter can be one of the following values:
  *            @arg MPU_HFNMI_PRIVDEF_NONE
  *            @arg MPU_HARDFAULT_NMI
  *            @arg MPU_PRIVILEGED_DEFAULT
  *            @arg MPU_HFNMI_PRIVDEF
  * @retval None
  */
void HAL_MPU_Enable_NS(uint32_t MPU_Control)
{
  __DMB(); /* Data Memory Barrier operation to force any outstanding writes to memory before disabling the MPU */

  /* Check input configuration */
  assert_param(IS_MPU_HFNMI_PRIVDEF_CONTROL(MPU_Control));

  /* Enable the MPU */
  MPU_NS->CTRL   = MPU_Control | MPU_CTRL_ENABLE_Msk;

#ifdef CORE_CM33
  /* Enable fault exceptions */
  SCB_NS->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif /* CORE_CM33 */

  /* Follow ARM recommendation with */
  /* Data Synchronization and Instruction Synchronization Barriers to ensure MPU configuration */
  __DSB(); /* Ensure that the subsequent instruction is executed only after the write to memory */
  __ISB(); /* Flush and refill pipeline with updated MPU configuration settings */
}
#endif /* __ARM_FEATURE_CMSE */

#ifdef CORE_CM0PLUS
/* [MPU/FUNCTIONS] M0+ (ARMv6-M) specific implementations */
/**
  * @brief  Initializes and configures the Region and the memory to be protected.
  * @param  MPU_Init: Pointer to a MPU_Region_InitTypeDef structure that contains
  *                   the initialization and configuration information.
  * @retval None
  */
void HAL_MPU_ConfigRegion(MPU_Region_InitTypeDef *MPU_Init)
{
  /* Check input parameter */
  assert_param(IS_MPU_REGION_NUMBER(MPU_Init->Number));
  assert_param(IS_MPU_REGION_ENABLE(MPU_Init->Enable));

  /* Set the Region number */
  MPU->RNR = MPU_Init->Number;

  /* Follow ARM recommendation with Data Memory Barrier prior to MPU configuration */
  __DMB();

  if ((MPU_Init->Enable) != 0U)
  {
    /* Check the parameters */
    assert_param(IS_MPU_INSTRUCTION_ACCESS(MPU_Init->DisableExec));
    assert_param(IS_MPU_REGION_PERMISSION_ATTRIBUTE(MPU_Init->AccessPermission));
    assert_param(IS_MPU_ACCESS_SHAREABLE(MPU_Init->IsShareable));
    assert_param(IS_MPU_ACCESS_CACHEABLE(MPU_Init->IsCacheable));
    assert_param(IS_MPU_ACCESS_BUFFERABLE(MPU_Init->IsBufferable));
    assert_param(IS_MPU_REGION_SIZE(MPU_Init->Size));

    MPU->RBAR = MPU_Init->BaseAddress & MPU_RBAR_ADDR_Msk;
    MPU->RASR = ((uint32_t)MPU_Init->DisableExec      << MPU_RASR_XN_Pos)   |
                ((uint32_t)MPU_Init->AccessPermission << MPU_RASR_AP_Pos)   |
                ((uint32_t)MPU_Init->Reserved         << MPU_RASR_TEX_Pos)  |
                ((uint32_t)MPU_Init->IsShareable      << MPU_RASR_S_Pos)    |
                ((uint32_t)MPU_Init->IsCacheable      << MPU_RASR_C_Pos)    |
                ((uint32_t)MPU_Init->IsBufferable     << MPU_RASR_B_Pos)    |
                ((uint32_t)MPU_Init->SubRegionDisable << MPU_RASR_SRD_Pos)  |
                ((uint32_t)MPU_Init->Size             << MPU_RASR_SIZE_Pos) |
                ((uint32_t)MPU_Init->Enable           << MPU_RASR_ENABLE_Pos);
  }
  else
  {
    MPU->RBAR = 0x00;
    MPU->RASR = 0x00;
  }
}
#endif /* CORE_CM0PLUS */

#ifdef CORE_CM33
/* [MPU/FUNCTIONS] M33 (ARMv8-M) specific implementations */
/**
  * @brief  Initialize and configure the Region and the memory to be protected.
  * @param  MPU_Init: Pointer to a MPU_Region_InitTypeDef structure that contains
  *                the initialization and configuration information.
  * @retval None
  */
void HAL_MPU_ConfigRegion(MPU_Region_InitTypeDef *MPU_RegionInit)
{
  /* Check the parameters */
  assert_param(IS_MPU_REGION_NUMBER(MPU_RegionInit->Number));
  assert_param(IS_MPU_REGION_ENABLE(MPU_RegionInit->Enable));

  /* Follow ARM recommendation with Data Memory Barrier prior to MPU configuration */
  __DMB();

  /* Set the Region number */
  MPU->RNR = MPU_RegionInit->Number;

  if (MPU_RegionInit->Enable != MPU_REGION_DISABLE)
  {
    /* Check the parameters */
    assert_param(IS_MPU_INSTRUCTION_ACCESS(MPU_RegionInit->DisableExec));
    assert_param(IS_MPU_REGION_PERMISSION_ATTRIBUTE(MPU_RegionInit->AccessPermission));
    assert_param(IS_MPU_ACCESS_SHAREABLE(MPU_RegionInit->IsShareable));

    MPU->RBAR = ((MPU_RegionInit->BaseAddress & MPU_RBAR_BASE_Msk)            |
                 (MPU_RegionInit->IsShareable           << MPU_RBAR_SH_Pos)   |
                 (MPU_RegionInit->AccessPermission      << MPU_RBAR_AP_Pos)   |
                 (MPU_RegionInit->DisableExec           << MPU_RBAR_XN_Pos));

    MPU->RLAR = ((MPU_RegionInit->LimitAddress & MPU_RLAR_LIMIT_Msk)              |
                 (MPU_RegionInit->AttributesIndex       << MPU_RLAR_AttrIndx_Pos) |
                 ((uint32_t)MPU_RegionInit->Enable      << MPU_RLAR_EN_Pos));
  }
  else
  {
    MPU->RBAR = 0U;
    MPU->RLAR = 0U;
  }
}

/**
  * @brief  Initialize and configure the memory attributes.
  * @param  MPU_AttributesInit: Pointer to a MPU_Attributes_InitTypeDef structure that contains
  *                the initialization and configuration information.
  * @retval None
  */
void HAL_MPU_ConfigMemoryAttributes(MPU_Attributes_InitTypeDef *MPU_AttributesInit)
{
  __IO uint32_t *mair;
  uint32_t      attr_values;
  uint32_t      attr_number;

  /* Check the parameters */
  assert_param(IS_MPU_ATTRIBUTES_NUMBER(MPU_AttributesInit->Number));
  /* No need to check Attributes value as all 0x0..0xFF possible */

  /* Follow ARM recommendation with Data Memory Barrier prior to MPU configuration */
  __DMB();

  if(MPU_AttributesInit->Number < MPU_ATTRIBUTES_NUMBER4)
  {
    /* Program MPU_MAIR0 */
    mair = &(MPU->MAIR0);
    attr_number = MPU_AttributesInit->Number;
  }
  else
  {
    /* Program MPU_MAIR1 */
    mair = &(MPU->MAIR1);
    attr_number = MPU_AttributesInit->Number-4;
  }

  attr_values = *(mair);
  attr_values &=  ~(0xFFU << (attr_number * 8));
  *(mair) = attr_values | (MPU_AttributesInit->Attributes << (attr_number * 8));
}
#endif /* CORE_CM33 */
#endif /* __MPU_PRESENT */

/**
  * @}
  */

/**
  * @}
  */

#endif /* HAL_CORTEX_MODULE_ENABLED */
/**
  * @}
  */

/**
  * @}
  */
