/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_bsec.c
  * @author  MCD Application Team
  * @brief   BSEC HAL module driver.
  *          This file provides firmware functions to manage the following
  *          functionalities of Boot and SECurity (BSEC):
  *           + Initialization and de-initialization functions
  *
  @verbatim
  ==============================================================================
                        ##### How to use this driver #####
  ==============================================================================
    [..]
    The BSEC HAL driver can be used as follows:



    *** High level operation ***
    =================================


    *** Low level operation ***
    =================================


  @endverbatim
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32mp2xx_hal.h"
#include "stm32mp2xx_hal_bsec.h"

/** @addtogroup STM32MP2xx_HAL_Driver
  * @{
  */

#if defined(BSEC) && defined(HAL_BSEC_MODULE_ENABLED)

/** @defgroup BSEC BSEC
  * @brief BSEC HAL module driver.
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/** @defgroup BSEC_Private_Define BSEC Private Define
  * @{
  */
/**
  * @}
  */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/** @defgroup BSEC_Private_Functions BSEC Private Functions
  * @{
  */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @defgroup BSEC_Exported_Functions BSEC Exported Functions
  * @{
  */

/** @defgroup BSEC_Exported_Functions_Group1 Initialization and de-initialization functions
 *  @brief   Initialization and de-initialization functions
 *
@verbatim
 ===============================================================================
             ##### Initialization and de-initialization functions  #####
 ===============================================================================
    [..]  This subsection provides a set of functions allowing to initialize and
          deinitialize the BSECx peripheral:

      (+) User must implement HAL_BSEC_MspInit() function in which he configures
          all related peripherals resources (CLOCK, IT and NVIC ).

      (+) Call the function HAL_BSEC_Init() to configure the selected device with
          the selected configuration:
        (++) Security level

      (+) Call the function HAL_BSEC_DeInit() to restore the default configuration
          of the selected BSECx peripheral.

@endverbatim
  * @{
  */

/**
  * @brief  Initialize the BSEC according to the specified
  *         parameters in the BSEC_InitTypeDef and initialize the associated handle.
  * @param  hbsec BSEC handle
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_BSEC_Init(BSEC_HandleTypeDef *hbsec)
{
  HAL_StatusTypeDef err = HAL_OK;

  /* Check the BSEC handle allocation */
  if (hbsec != NULL)
  {
    /* Check the parameters */
    assert_param(IS_BSEC_ALL_INSTANCE(hbsec->Instance));

    if (hbsec->State == HAL_BSEC_STATE_RESET)
    {

#if (USE_HAL_BSEC_REGISTER_CALLBACKS == 1)

#else
      /* Init the low level hardware */
      HAL_BSEC_MspInit(hbsec);
#endif /* USE_HAL_BSEC_REGISTER_CALLBACKS */
    }

    /* Set the state to busy */
    hbsec->State = HAL_BSEC_STATE_BUSY;

  }
  else
  {
    err = HAL_ERROR;
  }

  return err;
}

/**
  * @brief  DeInitialize the BSEC peripheral.
  * @param  hbsec BSEC handle
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_BSEC_DeInit(BSEC_HandleTypeDef *hbsec)
{
  HAL_StatusTypeDef err = HAL_OK;

  /* Check the BSEC handle allocation */
  if (hbsec != NULL)
  {
    /* Check the parameters */
    assert_param(IS_BSEC_ALL_INSTANCE(hbsec->Instance));

    /* Set the state to busy */
    hbsec->State = HAL_BSEC_STATE_BUSY;

#if (USE_HAL_BSEC_REGISTER_CALLBACKS == 1)
    if (hbsec->MspDeInitCallback == NULL)
    {
      hbsec->MspDeInitCallback = HAL_BSEC_MspDeInit; /* Legacy weak MspDeInit  */
    }

    /* DeInit the low level hardware: GPIO, CLOCK, NVIC */
    hbsec->MspDeInitCallback(hbsec);
#else
    /* DeInit the low level hardware: CLOCK, NVIC */
    HAL_BSEC_MspDeInit(hbsec);
#endif /* USE_HAL_BSEC_REGISTER_CALLBACKS */

    /* Reset the state */
    hbsec->State = HAL_BSEC_STATE_RESET;
  }
  else
  {
    err = HAL_ERROR;
  }

  return err;
}

/**
  * @brief  Initialize the BSEC MSP.
  * @param  hbsec BSEC handle
  * @retval None
  */
__weak void HAL_BSEC_MspInit(BSEC_HandleTypeDef *hbsec)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hbsec);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_BSEC_MspInit can be implemented in the user file
   */
}

/**
  * @brief  DeInitialize the BSEC MSP.
  * @param  hbsec BSEC handle
  * @retval None
  */
__weak void HAL_BSEC_MspDeInit(BSEC_HandleTypeDef *hbsec)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hbsec);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_BSEC_MspDeInit can be implemented in the user file
   */
}

#if (USE_HAL_BSEC_REGISTER_CALLBACKS == 1)
/**
  * @brief  Register a User BSEC Callback
  *         To be used instead of the weak predefined callback
  * @param  hbsec Pointer to a BSEC_HandleTypeDef structure that contains
  *                the configuration information for the specified BSEC.
  * @param  CallbackID ID of the callback to be registered
  *         This parameter can be one of the following values:
  *          @arg @ref HAL_BSEC_OPERATION_COMPLETE_CB_ID End of operation callback ID
  *          @arg @ref HAL_BSEC_ERROR_CB_ID Error callback ID
  *          @arg @ref HAL_BSEC_MSPINIT_CB_ID MspInit callback ID
  *          @arg @ref HAL_BSEC_MSPDEINIT_CB_ID MspDeInit callback ID
  * @param  pCallback pointer to the Callback function
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_BSEC_RegisterCallback(BSEC_HandleTypeDef *hbsec, HAL_BSEC_CallbackIDTypeDef CallbackID, pBSEC_CallbackTypeDef pCallback)
{

  HAL_StatusTypeDef status = HAL_OK;
#if 0
  if (pCallback == NULL)
  {
    /* Update the error code */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;
    return HAL_ERROR;
  }

  if (HAL_BSEC_STATE_READY == hbsec->State)
  {
    switch (CallbackID)
    {
      case HAL_BSEC_OPERATION_COMPLETE_CB_ID :
        hbsec->OperationCpltCallback = pCallback;
        break;

      case HAL_BSEC_ERROR_CB_ID :
        hbsec->ErrorCallback = pCallback;
        break;

      case HAL_BSEC_MSPINIT_CB_ID :
        hbsec->MspInitCallback = pCallback;
        break;

      case HAL_BSEC_MSPDEINIT_CB_ID :
        hbsec->MspDeInitCallback = pCallback;
        break;

      default :
        /* Update the error code */
        hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = HAL_ERROR;
        break;
    }
  }
  else if (HAL_BSEC_STATE_RESET == hbsec->State)
  {
    switch (CallbackID)
    {
      case HAL_BSEC_MSPINIT_CB_ID :
        hbsec->MspInitCallback = pCallback;
        break;

      case HAL_BSEC_MSPDEINIT_CB_ID :
        hbsec->MspDeInitCallback = pCallback;
        break;

      default :
        /* Update the error code */
        hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = HAL_ERROR;
        break;
    }
  }
  else
  {
    /* Update the error code */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status =  HAL_ERROR;
  }
#endif
  return status;
}

/**
  * @brief  Unregister a BSEC Callback
  *         BSEC callback is redirected to the weak predefined callback
  * @param  hbsec Pointer to a BSEC_HandleTypeDef structure that contains
  *                the configuration information for the specified BSEC.
  * @param  CallbackID ID of the callback to be unregistered
  *         This parameter can be one of the following values:
  *          @arg @ref HAL_BSEC_OPERATION_COMPLETE_CB_ID End of operation callback ID
  *          @arg @ref HAL_BSEC_ERROR_CB_ID Error callback ID
  *          @arg @ref HAL_BSEC_MSPINIT_CB_ID MspInit callback ID
  *          @arg @ref HAL_BSEC_MSPDEINIT_CB_ID MspDeInit callback ID
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_BSEC_UnRegisterCallback(BSEC_HandleTypeDef *hbsec, HAL_BSEC_CallbackIDTypeDef CallbackID)
{
  HAL_StatusTypeDef status = HAL_OK;

  if (HAL_BSEC_STATE_READY == hbsec->State)
  {
    switch (CallbackID)
    {
      case HAL_BSEC_OPERATION_COMPLETE_CB_ID :
        hbsec->OperationCpltCallback = HAL_BSEC_OperationCpltCallback; /* Legacy weak OperationCpltCallback */
        break;

      case HAL_BSEC_ERROR_CB_ID :
        hbsec->ErrorCallback = HAL_BSEC_ErrorCallback;                 /* Legacy weak ErrorCallback        */
        break;

      case HAL_BSEC_MSPINIT_CB_ID :
        hbsec->MspInitCallback = HAL_BSEC_MspInit;                     /* Legacy weak MspInit              */
        break;

      case HAL_BSEC_MSPDEINIT_CB_ID :
        hbsec->MspDeInitCallback = HAL_BSEC_MspDeInit;                 /* Legacy weak MspDeInit            */
        break;

      default :
        /* Update the error code */
        hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status =  HAL_ERROR;
        break;
    }
  }
  else if (HAL_BSEC_STATE_RESET == hbsec->State)
  {
    switch (CallbackID)
    {
      case HAL_BSEC_MSPINIT_CB_ID :
        hbsec->MspInitCallback = HAL_BSEC_MspInit;                   /* Legacy weak MspInit              */
        break;

      case HAL_BSEC_MSPDEINIT_CB_ID :
        hbsec->MspDeInitCallback = HAL_BSEC_MspDeInit;               /* Legacy weak MspDeInit            */
        break;

      default :
        /* Update the error code */
        hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status =  HAL_ERROR;
        break;
    }
  }
  else
  {
    /* Update the error code */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status =  HAL_ERROR;
  }

  return status;
}

#endif /* USE_HAL_BSEC_REGISTER_CALLBACKS */


/**
  * @brief  BSEC Wait Busy Clear With a Timeout
  * @param hbsec      BSEC handle.
  * @param Timeout   Timeout duration
  * @retval HAL status
  */
HAL_StatusTypeDef BSEC_WaitBusyClearWithTimeout(BSEC_HandleTypeDef *hbsec, uint32_t Timeout)
{
  uint32_t timerValInit;

  /* Get timer current value at start of loop */
  timerValInit = HAL_GetTick();

  /* Wait until flag is set */
  while(LL_BSEC_OtpStatus_Is_Busy(hbsec->Instance) != 0)
  {
    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((HAL_GetTick() - timerValInit) > Timeout) || (Timeout == 0U))
      {
        hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
        return HAL_TIMEOUT;
      }
    }
  }
  return HAL_OK;
}

/**
  * @brief  Get information whether OTP word 'otpWordNb' is shadowed or not in SoC
  *
  * @ Note  This information is defined by netlist ties and is SoC dependent.
  *
  * @param  BSECx          : pointer on BSEC HW instance
  * @param  otpWordNb      : OTP word number from which we want to know if is shadowed or not.
  * @param  isShadowed     : output value
  *                             1 if OTP 'otpWordNb' is effectively shadowed in SoC
  *                             0 if OTP word 'otpWordNb' is not shadowed in SoC.
  * \return  HAL_StatusTypeDef : Returned value is either :
  *          HAL_OK if no error was encountered
  *          or HAL_ERROR in case one or several errors where encountered.
  */
HAL_StatusTypeDef HAL_BSEC_Is_Shadowed(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pIsShadowed)
{
  /** Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));
  assert_param(pIsShadowed != NULL);

  *pIsShadowed = LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx);

  return(HAL_OK);
}

/**
* \brief   This function analyze OTP Manual Reload errors. To be called after a fuse
*          Manual Reload operation of 'otpWordIdx' (ie OTP read from SAFMEM/XPM fuse memory).
* @param   [in] otpWordIdx : OTP word from which we want to know OTP Reload errors status.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_Analyze_OtpManualReloadErrors(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx)
{
    HAL_StatusTypeDef retVal = HAL_ERROR; /*!< Initialize return value to HAL_ERROR */
    uint32_t errorFound = 0;            /*!< Initialize errorFound to No Error found */

    /** Verify input parameters validity */
    assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

    /* Note : Do not consider 'BSEC_OTPSR.SECF' as a real error,
     * this is a single error that have been corrected */

    if(LL_BSEC_OtpStatus_Is_Ded_Error(hbsec->Instance) != 0)
    {
        errorFound = 1;
    }

    if(LL_BSEC_OtpStatus_Is_AddressMismatch_Error(hbsec->Instance) != 0)
    {
#if (BSEC_IGNORE_AMEF_ERRORS == 0)
        errorFound = 1;
#endif /* HAL_BSEC_IGNORE_AMEF_ERRORS == 0 */
    }

    if(LL_BSEC_OtpStatus_Is_Disturbed_Error(hbsec->Instance) != 0)
    {
        errorFound = 1;
    }

    /** Have errors been found ? */
    if(errorFound == 0)
    {
        /** NO : return good status on exit */
        retVal = HAL_OK;
    }

    return(retVal);
}

/**
* \brief   This function analyze only automatic Reload errors. To be called following
*          a BSEC Boot to get information on errors impacting word 'otpWordIdx' following
*          a BSEC boot (ie automatic reload following a system reset).
* @param   [in] otpWordIdx    : OTP word from which we want to know OTP Reload errors status.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_Analyze_AutomaticReloadErrors(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx)
{
  HAL_StatusTypeDef retVal = HAL_ERROR; /*!< Initialize return value to HAL_ERROR */
  uint32_t errorFound = 0;            /*!< Initialize errorFound to No Error found */

  /* Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

  /* We care about automatic reload validity of BSEC shadow contents for words */
  /* that are shadowed only of course                                          */
  if(LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx) != 0)
  {
    /** Global OTP error detected following BSEC boot (ie automatic reload process)
    *  following a BSEC scratch (aka 'cold') or warm reset ?
    *  Only 'BSEC_OTPSR.OTP_ERR' is considered as 'BSEC_OTPSR.OTP_SEC' is a single error
    *  that has been corrected and is therefore not a real error.
    */
    if(LL_BSEC_OtpStatus_Is_OtpErr_Error(hbsec->Instance) != 0)
    {
      /** 'BSEC_OTPSR.OTP_ERR' = 1b1' there is a global error
      *  Does it impact specifically 'otpWordIdx' ?
      *  Check for that bit HAL_BSEC_OTPVLDx[y] value : 0 means invalid value in shadow
      *  for otpWordIdx = 32*x + y while shadowing this word from automatic reload process
      *  (following a scratch or warm BSEC reset).
      *  A possible corrective action for user will consists in manual reloading this otpWordIdx.
      */
      if(LL_BSEC_Is_ShadowedOtpReloadedInShadowValid(hbsec->Instance, otpWordIdx) == 0)
      {
/* If No Ignore of Automatic Reload errors */
#if (BSEC_IGNORE_AUTO_RELOAD_ERRORS == 0)
        /** Yes it does : shadowed word auto-reload did not succeeded (not valid) ... */
        errorFound = 1;
#endif /* HAL_BSEC_IGNORE_AUTO_RELOAD_ERRORS == 0 */
      } /* of if(HAL_BSEC_Is_ShadowedOtpReloadedInShadowValid(BSEC, otpWordIdx) == 0) */

    } /* of if(HAL_BSEC_Is_OtpErr_Error(BSEC) != 0) ... */

  } /* of if(LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx) != 0) */

  /** Have errors been found impacting specifically 'otpWordIdx' ? */
  if(errorFound == 0)
  {
    /** NO : return good status on exit */
    retVal = HAL_OK;
  }

  return(retVal);
}

/**
* \brief   This function analyze either errors linked to
*          automatic reload to be used for OTP that are shadowed or linked to manual reload only
*          for OTP that are known to be non-shadowed.
* @param   [in]     otpWordIdx        : OTP word from which we want to know OTP Reload errors status.
* @param   [in]     analyzeErrorTypes : type of BSEC errors to analyze
*                                       (auto and manual reload OR only manual reload errors).
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_AnalyzeErrors(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, HAL_BSEC_AnalyzeErrorTypeDef analyzeErrorTypes)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef statusWait;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  if(analyzeErrorTypes == HAL_BSEC_ANALYZE_MANUAL_RELOAD_ERRORS)
  {
    status = HAL_BSEC_Analyze_OtpManualReloadErrors(hbsec,otpWordIdx);
  }
  else
  {
    /** Necessarily HAL_BSEC_ANALYZE_AUTO_RELOAD_ERRORS */
    status = HAL_BSEC_Analyze_AutomaticReloadErrors(hbsec,otpWordIdx);
  }

  return(status);
}


/**
* \brief   This function execute an OTP Manual Reload operation from fuse 'otpWordIdx'
*          in fuse memory XPM/SAFMEM Kilopass to the corresponding BSEC Shadow
*          register HAL_BSEC_FVRx, by forcing a Manual Reload operation via BSEC register.
* @param   [in]     otpWordIdx    : OTP word from which we want to read the shadow register.
* @param   [in,out] pOtpShadowVal : pointer to get read fuse value, which is always via
*                                   the corresponding BSEC shadow register HAL_BSEC_FVRx (x = 'otpWordIdx').
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_OtpManualReload(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pOtpShadowVal)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX);
  assert_param(pOtpShadowVal != (uint32_t *)NULL);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is Global Sticky Write lock set in 'BSEC_LOCKR.GWLOCK' to lock write
   *  to the bsec_corekp1 and bsec_core1 ?
   *  This makes BSEC Shadow register write, OTP Manual Reload operation and
   *  OTP programming operation impossible (ie without effect).
   */
  if(LL_BSEC_Is_Global_StickyWriteLocked(hbsec->Instance) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_GLOBAL_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is 'otpWordIdx' in OTP upper area ? */
  if(otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX)
  {
    /** Yes, Test 'BSEC_OTPSR.HIDEUP' current value : error if upper is currently hidden */
    if(LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0)
    {
      /** Upper area is hidden, so BSEC reloaded value will have no meaning and should not
       *  be considered as valid => raise an error and abort further processing.
       */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
      return(HAL_ERROR); /* Early exit */

    } /* of if(LL_BSEC_OtpStatus_Is_HideUpper(BSEC) != 0) */

  } /* of if(otpWordIdx >= HAL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) */

  /* Proceed with the OTP Manual Reload operation.*/
  LL_BSEC_Set_OtpCtrl(hbsec->Instance,
                      otpWordIdx,
                      LL_BSEC_OTPCR_PROG_BIT_IS_READ_REQ,
                      LL_BSEC_OTPCR_LOCK_BIT_IS_NOLOCK_REQ);

  /** Wait BUSY bit set by operation above is cleared (with timeout) */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Analyze eventual OTP reload errors */
  status = HAL_BSEC_AnalyzeErrors(hbsec, otpWordIdx, HAL_BSEC_ANALYZE_MANUAL_RELOAD_ERRORS);
  if(status != HAL_OK)
  {
    /** Errors have been found : they have been added to 'pErrorMask' */
    return(HAL_ERROR); /* Early exit */
  }

  /** If we arrive here, there were no OTP Reload errors ...
   *  Read finally the just updated BSEC shadow register
   *  it contains the reloaded fuse value.
   */
  * pOtpShadowVal = LL_BSEC_Read_OtpShadow(hbsec->Instance, otpWordIdx);

  return (HAL_OK);
}

/*!
 * \fn HAL_StatusTypeDef HAL_BSEC_GetSecurityStatus(BSEC_ChipSecurityTypeDef * pSecStatus)
 * \brief This function gets the security status of the SoC sample
*         as given by 'BSEC_SR.STATE' register.
*
* @param   [in,out] pSecStatus : pointer to get the SoC sample security status
*          The returned security status can take values from @ref HAL_BSEC_ChipSecurityTypeDef.
*
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
*/
HAL_StatusTypeDef HAL_BSEC_GetSecurityStatus(BSEC_HandleTypeDef *hbsec, BSEC_ChipSecurityTypeDef * pSecStatus)
{
  uint32_t SecStatus;
  uint32_t value;
  uint32_t SecMask = 0x00000001;
  uint32_t bootrom_config_9_OTP18 = 18U;
  HAL_StatusTypeDef statusWait;

  /** Check input parameters validity */
  assert_param((pSecStatus != (BSEC_ChipSecurityTypeDef *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /* Ensure 'BSEC_OTPSR.FUSEOK' is set so that 'BSEC_SR.STATE' have been loaded */
  if(LL_BSEC_OtpStatus_Is_FuseOk(hbsec->Instance) != 0)
  {
    /* 'BSEC_OTPSR.FUSEOK' = 1 => HAL_BSEC_SR.STATE is then valid, get it */
    SecStatus = LL_BSEC_Get_SecurityState(hbsec->Instance);

    if(SecStatus == LL_BSEC_SECURITY_STATE_OPEN)
    {
      * pSecStatus = BSEC_INVALID_STATE;
    }
    else if ((SecStatus == LL_BSEC_SECURITY_STATE_CLOSED))
    {
      /* Get BSEC UNSECURE/SECURE state */
      HAL_BSEC_OtpRead(hbsec, bootrom_config_9_OTP18, &value);
      if ((value & SecMask) == 0)
      {
        /* BSEC is UNSECURED CLOSE */
        * pSecStatus = BSEC_UNSECURED_CLOSE_STATE;
      }
      else
      {
        /* BSEC is SECURED CLOSE */
        * pSecStatus = BSEC_SECURED_CLOSE_STATE;
      }
    }
    else
    {
      /* None of the known security states -> major error to be trapped */
      * pSecStatus = BSEC_INVALID_STATE;
    }

    return (HAL_OK);

  } /* of if(HAL_BSEC_Is_FuseOk(hbsec) != 0) */
  else
  {
    /* 'BSEC_OTPSR.FUSEOK' = 0 : this is very unusual situation */
    /* should block in bootROM such situation asap              */
    * pSecStatus = BSEC_INVALID_STATE;
    return (HAL_ERROR);
  }
}

/**
* \brief   This function reads a given OTP word (32 bits) shadow register of index given in first
*          parameter 'otpWordIdx'.
*
* @Note : WARNING : This function can be called both either after a BSEC boot (ie automatic reload following
*                   a system reset) for a word 'otpWordIdx' known to be shadowed already
*                   ('isPostManualReload=0')
*                   OR after a Manual reload request : parameter 'isPostManualReload=1'
*                   have to be specified according to context, to avoid caring about automatic
*                   reload specific errors in case we just handled a manual reload.
*
* @param   [in]     otpWordIdx    : OTP word from which we want to read the shadow register.
* @param   [in,out] pOtpShadowVal : pointer to get the OTP word shadow read value.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_OtpShadowRead(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pOtpShadowVal)
{
  HAL_StatusTypeDef analyzeErrorsStatus;
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX);
  assert_param(pOtpShadowVal != (uint32_t *)NULL);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /** Early exit */
  }

  /** Is 'otpWordIdx' in OTP upper area ? */
  if(otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX)
  {
    /** Yes, Test 'BSEC_OTPSR.HIDEUP' current value : error if upper is currently hidden */
    if(LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0)
    {
      /**
       * Upper area is hidden, so BSEC shadow value have no meaning and should not
       * be considered as valid => raise an error and abort further processing
       */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
      return(HAL_ERROR); /** Early exit */
    } /* end of if(LL_BSEC_OtpStatus_Is_HideUpper(BSEC) != 0) */
  } /* end of if(otpWordIdx >= HAL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) */

  if(LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx) == 0)
  {
    /* If OTP 'otpWordIdx' is not shadowed and we count an an automatic reload following BSEC boot */
    /* this is an error as shadow will never have been updated by Hw in that case                  */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_OTP_NOT_SHADOWED;
    return(HAL_ERROR); /* Early exit */
  }

  /* We want to read back the shadow just after a Manual Reload operation 'BSEC_OtpManualReload()' */
  /* then do not take into consideration the BSEC boot (automatic reload errors)...                */
  /* Are there Manual reload errors in HAL_BSEC_OTPSR impacting this 'otpWordIdx' ?                */
  /* ALSO the 'otpWordIdx' can be un-shadowed in that case and this does not cause an error        */
  /* because we have just before Manual Reloaded the 'otpWordIdx' in common shadow register in     */
  /* that case ...                                                                                 */
  analyzeErrorsStatus = HAL_BSEC_AnalyzeErrors(hbsec, otpWordIdx, HAL_BSEC_ANALYZE_MANUAL_RELOAD_ERRORS);

  if(analyzeErrorsStatus != HAL_OK)
  {
    return(HAL_ERROR); /* Early exit */
  }

  /* If we are here there were no errors found yet                                    */
  /* Proceed with the OTP shadow read itself                                          */
  /* @Note : it can be a dedicated shadow register if 'otpWordIdx' is Shadowed in SoC */
  /* or a common shadow register if 'otpWordIdx' is not Shadowed in SoC               */
  * pOtpShadowVal = LL_BSEC_Read_OtpShadow(hbsec->Instance, otpWordIdx);

  return (HAL_OK);
}

/**
* \brief   This function writes the OTP word 'otpWordIdx' with value 'otpWordVal'.
*          The write affects one of the shadow registers in the range
*          HAL_BSEC_FVR0..BSEC_FVR383.
*
*          @Note : This function is meant to be used on 'otpWordIdx' that is shadowed in SoC only.
*
*          Error status such as : OTP word 'otpWordIdx'
*          -shadow sticky write locked or
*          -word not shadowed in Soc
*          are checked.
*
* @param   [in]     otpWordIdx : OTP word shadow register index to be written to
* @param   [in]     otpWordVal : Value to be written in OTP Word shadow register.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_OtpShadowWrite(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t otpWordVal)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /* Is 'otpWordIdx' shadowed in SoC ? Read netlist ties to get information */
  if(LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx) == 0)
  {
    /* If OTP 'otpWordIdx' is not shadowed then we should not use the current function */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_OTP_NOT_SHADOWED;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is Global Sticky Write lock set in 'BSEC_LOCKR.GWLOCK' to lock write
   *  to the bsec_corekp1 and bsec_core1 ?
   *  This makes BSEC Shadow register write, OTP Reload operation and
   *  OTP programming operation impossible (ie without effect)
   */
  if(LL_BSEC_Is_Global_StickyWriteLocked(hbsec->Instance) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_STICKY_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /** Get information whether the 'otpWordIdx' is shadow sticky write locked */
  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_WRITE_LOCKED) != 0)
  {
    /* The 'otpWordIdx' have been effectively sticky lock write shadow */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_STICKY_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /** If we are here, no error : proceed with the shadow register write */
  LL_BSEC_Write_OtpShadow(hbsec->Instance, otpWordIdx, otpWordVal);

  return (HAL_OK);
}


/**
* \brief   This function Sticky Locks until next BSEC 'scratch' (aka 'cold') or 'warm' BSEC IP block
*          reset the OTP word of index 'otpWordIdx' with a lock depending on sticky lock command parameter ('stickyLockCmd')
*          Parameter 'stickyLockCmd' can take a value as a combination (OR) of
*          several values from 'BSEC_OtpStickyLockTypeDef'. This allows to apply in a single call multiple sticky lock types
*          on the same 'otpWordIdx'. The kind of sticky locks that can be applied are single or combination of :
*          - OTP programming lock command (No more possible to Program fuse word in fuse memory)
*          - OTP shadow register write lock command (No more possible to Write OTP word relative BSEC shadow register)
*          - OTP read lock command (No more possible to handle a Read operation in fuse memory on OTP word to reload the related shadow register).
*          Note : In implementation below Sticky locking is applied even if OTP word is already sticky locked
*                 for the same sticky function and same word.
*                 So no error is returned in that case and sticky lock is re-applied.
* @param   [in] otpWordIdx     : OTP word index for which the sticky lock command is processed.
* @param   [in] stickyLockCmd  : Sticky lock command to apply : value of type @ref HAL_BSEC_OtpStickyLockTypeDef
*                                or combination (OR) of them.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered during process.
*          Or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_SetOtpStickyLock(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t stickyLockCmd)
{
  HAL_StatusTypeDef statusWait;
  uint32_t stickyLockMaxCmd = (BSEC_OTP_STICKY_LOCK_PROG_OTP_WORD |
                               BSEC_OTP_STICKY_LOCK_WRITE_SHADOW  |
                               BSEC_OTP_STICKY_LOCK_READ);

  /** Verify input parameters validity */
  assert_param(otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX);
  assert_param(stickyLockCmd <= stickyLockMaxCmd);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /* If otpWordIdx is in Upper area, while Hide Upper is currently set : this is an error */
  /* and this the case for all possible sticky lock actions                               */
  if((otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) &&
     (LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0))
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
    return(HAL_ERROR); /** Early exit */
  }

  /* If there is a Sticky write lock shadow operation in the list of sticky lock actions */
  /* then if the concerned word 'otpWordIdx' is not shadowed this is an error            */
  if(((stickyLockCmd & BSEC_OTP_STICKY_LOCK_WRITE_SHADOW) != 0) &&
     (LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx) == 0))
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_OTP_NOT_SHADOWED;
    return(HAL_ERROR); /** Early exit */
  }

  /** Now that error cases are evacuated : Handle one or several sticky lockings */
  /* at the same time depending on 'stickyLockCmd' value                         */
  LL_BSEC_Set_StickyLock(hbsec->Instance, otpWordIdx, stickyLockCmd);

  return(HAL_OK);
}

/**
* \brief   This function Gets sticky locks status (ie until next system Reset) for the
*          OTP word of index 'otpWordIdx'.
* @param   [in]     otpWordIdx         : OTP word index for which the sticky lock command is processed.
* @param   [in,out] pStickyLockStatus  : Pointer to get composite Sticky lock status : a combination
*                                       of values from type @ref HAL_BSEC_OtpStickyLockTypeDef for all
*                                       sticky locks currently set for this 'otpWordIdx'.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetOtpStickyLockStatus(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pStickyLockStatus)
{
  HAL_StatusTypeDef statusWait;
  uint32_t stickyLockStatus = BSEC_OTP_STICKY_LOCK_NOT_LOCKED;

  /** Verify input parameters validity */
  assert_param(pStickyLockStatus != (uint32_t *)NULL);
  assert_param(otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    * pStickyLockStatus = stickyLockStatus;
    return(HAL_ERROR); /* Early exit */
  }

  /* If otpWordIdx is in Upper area, while Hide Upper is currently set : this is an error */
  /* and this the case for all possible get sticky lock actions                           */
  if((otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) &&
     (LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0))
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
    return(HAL_ERROR); /** Early exit */
  }

  /* Now that all error cases have been evacuated, continue process */
  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_PROG_LOCKED) != 0)
  {
    stickyLockStatus |= BSEC_OTP_STICKY_LOCK_PROG_OTP_WORD;
  }

  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_WRITE_LOCKED) != 0)
  {
    stickyLockStatus |= BSEC_OTP_STICKY_LOCK_WRITE_SHADOW;
  }

  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_READ_LOCKED) != 0)
  {
    stickyLockStatus |= BSEC_OTP_STICKY_LOCK_READ;
  }

  * pStickyLockStatus = stickyLockStatus;

  return(HAL_OK);
}


/**
* \brief   This function should be used to read a fuse word 'otpWordIdx'.
*          It will detect if the 'otpWordIdx' is shadowed and if that is the case
*          read OTP Shadow HAL_BSEC_FVRx with x = 'otpWordIdx'. If the word is not
*          shadowed it will try an OTP Manual Reload operation.
*          Note that OTP Auto reload and/or Manual Reload errors are analyzed depending on each case.
* @param   [in]     otpWordIdx    : OTP word from which we want to read the value.
* @param   [in,out] pOtpShadowVal : pointer to get read fuse value, which is always via
*                                   the corresponding BSEC shadow register HAL_BSEC_FVRx (x = 'otpWordIdx').
* @param   [in] forceManualReload : value != 0 to force a Manual Reload operation (useful for 'otpWordIdx'
*               that are Shadowed in SoC for which we want to force Manual Reload : ie re-read from fuse).
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_OtpRead(
    BSEC_HandleTypeDef *hbsec,
    uint32_t otpWordIdx,
    uint32_t * pOtpShadowVal)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX);
  assert_param(pOtpShadowVal != (uint32_t *)NULL);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** 'otpWordIdx' is NOT Shadowed in SoC OR
    * 'otpWordIdx' is Shadowed in SoC BUT this is a
    * formal request to "force Manual Reload"
    * => read fuse value based on an OTP Reload operation
    *   analyzing errors linked OTP reload operation ONLY in that case.
    */
  return(HAL_BSEC_OtpManualReload(hbsec, otpWordIdx, pOtpShadowVal));
}



/**
* \brief   This function should be used to program (ie blow) a fuse word 'otpWordIdx'.
*          with value 'otpWordVal'.
* @param   [in]  otpWordIdx      : OTP word from which we want to read the value.
* @param   [in]  otpWordValReq   : Value of word requested to be programmed
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_OtpProgram(BSEC_HandleTypeDef *hbsec,
                               uint32_t                   otpWordIdx,
                               uint32_t                   otpWordValReq)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef statusWait;
  uint32_t otpShadowValBeforeProg;
  uint32_t otpDiffWord;
  uint32_t otpReloadedShadowVal;
  uint32_t bsecWrDataValue;    /*!<  value to write in 'BSEC_WDR' register */

  /** Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** 1- Is Global Sticky Write lock set in 'BSEC_LOCKR.GWLOCK' to lock write
   * to the bsec_corekp1 and bsec_core1 ?
   * This makes BSEC Shadow register write, OTP Reload operation and
   * OTP programming operation impossible (ie : without effect in other words a silent fail).
   */
  if(LL_BSEC_Is_Global_StickyWriteLocked(hbsec->Instance) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_GLOBAL_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /** 2- Is 'otpWordIdx' in OTP upper area ? */
  if(otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX)
  {
    /** Yes, Test 'BSEC_OTPSR.HIDEUP' current value : error if upper is currently hidden */
    if(LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0)
    {
      /** Yes : Upper area is hidden, so BSEC fuse programming will have no effect and should not
       * be attempted => raise an error and abort further processing
       */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
      return(HAL_ERROR); /* Early exit */

    } /* of if(LL_BSEC_OtpStatus_Is_HideUpper(BSEC) != 0) */

  } /* of if(otpWordIdx >= HAL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) */

  /** 3- Is 'otpWordIdx' Sticky Program Locked currently ? */
  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_PROG_LOCKED) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_STICKY_PROG_LOCKED;
    return(HAL_ERROR); /* Early exit */
  }

  /** Preset 'bsecWrDataValue' (value to be written in register 'BSEC_WDR' as word value to fuse)
   * with 'otpWordValReq' */
  bsecWrDataValue = otpWordValReq;

  /** 4a- Manage first Verify progOption if requested (this is a pre and post verification)
   */

  /** Pre Read 'otpWordIdx' by forcing a manual reload operation (for the case of non shadowed fuses) for two reasons :
   * -security : avoid to be hacked on contents of BSEC shadow false bit value written that we will
   * declare impossible to program falsely
   * -because in program verify case we want to manage the case where word otpWordIdx is Permanent Programming
   * locked.
   * Note : to know that : there is only one solution in STM32MP2 : Reload Manually the word by a Read
   * operation and check then bit 'BSEC_OTPSR.PPLF' bit that is updated (Perm Prog Lock flag)
   */
  
  /** If otpWordIdx not sticky program locked Force OTP Manual Reload operation : Fuse value 
   * before programming -> otpShadowValBeforeProg */
  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_READ_LOCKED) == 0)
  {
    status = HAL_BSEC_OtpManualReload(hbsec, otpWordIdx, &otpShadowValBeforeProg);
    if(status != HAL_OK)
    {
      /* Error code set in subfunction */
      return(HAL_ERROR); /* Early exit : no need to continue ... */

    } /* of if(status != HAL_OK) */
  }

  /* Check Permanent Programming lock flag and its validity */
  if((LL_BSEC_OtpStatus_Is_PermProgLocked(hbsec->Instance) != 0) &&
     (LL_BSEC_OtpStatus_Is_MismatchingPermProgLock_Error(hbsec->Instance) == 0))
  {
    /* Permanent Programming lock flag is set and is not suspect of error */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PROGRAMM_PERMPROGLOCKED_OTP;
    return(HAL_ERROR); /* Early exit : no need to continue further trying to program ... */
  }

  /* Manage also for the program verify case the fact that an OTP middle or upper word   */
  /* is a word "bulk" on which ECC is computed and that can be programmed only once ...  */
  /* It is only possible to program bulk word not virgin to its current value            */
  /* which is a non sense from application point of view. So this case can be considered */
  /* as an error                                                                         */
  if((otpWordIdx >= LL_BSEC_OTP_MIDDLE_FIRST_WORD_INDEX) && /* 'otpWordIdx' is a bulk word  */
     (otpShadowValBeforeProg != 0))                      /* && word is already programmed*/
  {
    /* Bulk word invalid reprogramming error */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PROGRAM_BULK_WORD_NOT_VIRGIN;
    return(HAL_ERROR); /* Early exit : no need to continue further trying to program ... */
  }

  /** If we are here, 'otpWordIdx' could be read via Manual Reload operation
   * value in 'otpShadowValBeforeProg' is considered valid and 'otpWordIdx' is not permanent
   * programming locked and if 'otpWordIdx' is a bulk word it has never been programmed.
   */

  /** 4b- Pre verify is target value to program 'otpWordValReq' is possible to achieve
   * from a technology point of view of fuses. A bit to 1' cannot be changed in a 0'.
   * otpDiffWord = otpShadowValBeforeProg XOR otpWordValReq
   * all bits set to 1b1' in 'otpDiffWord' are the bits that are requested to be changed
   * either from 0->1 (allowed) or 1->0 (forbidden by fuse technology)
   * Check for fuse technology forbidden transition 1->0 and block it if occurs...
   */
  otpDiffWord = (otpShadowValBeforeProg ^ otpWordValReq);

  /** Block fuse technology forbidden transition 1->0 */
  /* otpDiffWord & otpShadowValBeforeProg will return bit to 1' for invalid transitions */
  /* bit changing and transition 1 -> 0 impossible due to fuse technology               */
  if((otpDiffWord & otpShadowValBeforeProg) != 0)
  {
    /** Full word mask value is not possible to program in fuse : return error
     * Update Error description : not possible to write wanted value in fuse
     */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PROGRAM_BITWISE_WITH_ONE2ZERO;
    return(HAL_ERROR); /* Early exit */
}

  /** If we are still here, the requested word value is possible to be programmed in fuse
   * to save polyfuse life, as we know the bits to be programmed to 1b1' in fuse, program
   * only the difference word 'otpDiffWord'.
   */
  bsecWrDataValue = otpDiffWord;

  /** -5- Program register 'BSEC_WDR' with 'bsecWrDataValue' that is either the difference word if
   *  verify progOption was requested or the 'otpWordValReq' from function
   * parameter in "blind programming mode".
   */
  LL_BSEC_Set_WordValToFuse(hbsec->Instance, bsecWrDataValue);

  /** Programming Word Payload from HAL_BSEC_WDR
   * - manage eventual post Verify
   */

  /** -6- Program OTP Word itself without taking into    */
  /* account yet an eventual permanent prog lock request */
  LL_BSEC_Set_OtpCtrl(hbsec->Instance,
                      otpWordIdx,
                      LL_BSEC_OTPCR_PROG_BIT_IS_PROGRAM_REQ,
                      LL_BSEC_OTPCR_LOCK_BIT_IS_NOLOCK_REQ);

  /** -7- Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /* We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** -8- Analyze programming errors if any */
  if(LL_BSEC_OtpStatus_Is_ProgFail_Error(hbsec->Instance) != 0)
  {
    /* We had a programming fail error ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PROGFAIL;
    return(HAL_ERROR); /* Early exit */
  }

  /** -9- Reload shadow value and compare against requested word
   * value to be programmed (post verify) if otpWordIdx is not
   * sticky locked.
   */
  if (LL_BSEC_Is_Shadowed(hbsec->Instance, otpWordIdx))
  {
    if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_READ_LOCKED) == 0)
    {
      /* Force OTP Manual Reload operation */
      status = HAL_BSEC_OtpManualReload(hbsec, otpWordIdx, &otpReloadedShadowVal);
      if(status != HAL_OK)
      {
        return(status);
      }

      /* Verify programmed value Read back by manual reload versus expected */
      /* word value 'otpWordValReq'                                         */
      if(otpReloadedShadowVal != otpWordValReq)
      {
        /* Post program verify error detected */
        hbsec->ErrorCode |= HAL_BSEC_ERROR_VALUE_MISMATCH;
        return(HAL_ERROR); /* Early exit */
      }
    }
  }

  return(HAL_OK);
}


/**
* \brief   This function should be used to permant lock (ie blow) a fuse word 'otpWordIdx'.
* @param   [in]  otpWordIdx      : OTP word from which we want to read the value.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
*/
HAL_StatusTypeDef HAL_BSEC_SetOtpPermanentProgLock(BSEC_HandleTypeDef * hbsec,
                                                   uint32_t             otpWordIdx)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef statusWait;
  uint32_t otpReloadedShadowVal;

  /** Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is Global Sticky Write lock set in 'BSEC_LOCKR.GWLOCK' to lock write
   * to the bsec_corekp1 and bsec_core1 ?
   * This makes BSEC Shadow register write, OTP Reload operation and
   * OTP programming operation impossible (ie : without effect in other words a silent fail).
   */
  if(LL_BSEC_Is_Global_StickyWriteLocked(hbsec->Instance) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_GLOBAL_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is 'otpWordIdx' in OTP upper area ? */
  if(otpWordIdx >= LL_BSEC_OTP_UPPER_FIRST_WORD_INDEX)
  {
    /** Yes, Test 'BSEC_OTPSR.HIDEUP' current value : error if upper is currently hidden */
    if(LL_BSEC_OtpStatus_Is_HideUpper(hbsec->Instance) != 0)
    {
      /** Yes : Upper area is hidden, so BSEC fuse programming will have no effect and should not
       * be attempted => raise an error and abort further processing
       */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_HIDE_UP;
      return(HAL_ERROR); /* Early exit */

    } /* of if(LL_BSEC_OtpStatus_Is_HideUpper(BSEC) != 0) */

  } /* of if(otpWordIdx >= HAL_BSEC_OTP_UPPER_FIRST_WORD_INDEX) */

  /* Set HAL_BSEC_WDR = 0x0 : payload word is 0x0 for word */
  LL_BSEC_Set_WordValToFuse(hbsec->Instance, 0x0);

  /** Yes : Permanent programming lock request case PROG + PPLOCK bits set */
  LL_BSEC_Set_OtpCtrl(hbsec->Instance,
                      otpWordIdx,
                      LL_BSEC_OTPCR_PROG_BIT_IS_PROGRAM_REQ,
                      LL_BSEC_OTPCR_LOCK_BIT_IS_LOCK_REQ);

  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /* We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Analyze programming errors if any */
  if(LL_BSEC_OtpStatus_Is_ProgFail_Error(hbsec->Instance) != 0)
  {
    /* We had a programming fail error ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PROGFAIL;
    return(HAL_ERROR); /* Early exit */
  }

  /** Force OTP Manual Reload operation in order to refresh       */
  /* permanent programming lock information in 'BSEC_OTPSR.PPLMF' */
  status = HAL_BSEC_OtpManualReload(hbsec, otpWordIdx, &otpReloadedShadowVal);
  if(status != HAL_OK)
  {
    return(status); /* Early exit : no need to continue ... */

  } /* of if(status != HAL_OK) */

  /* Check Permanent Programming lock flag and its validity */
  if((LL_BSEC_OtpStatus_Is_PermProgLocked(hbsec->Instance) == 0) &&
     (LL_BSEC_OtpStatus_Is_MismatchingPermProgLock_Error(hbsec->Instance) == 0))
  {
    /* Permanent Programming lock flag is still not set and is not suspect of error */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_PERMPROGLOCK_FAIL;
    return(HAL_ERROR); /* Early exit */
  }

  /** Reload shadow if otpWordIdx is not sticky read locked */
  if(LL_BSEC_Is_StickyLocked(hbsec->Instance, otpWordIdx, LL_BSEC_STICKY_READ_LOCKED) == 0)
  {
    /* Force OTP Manual Reload operation */
    status = HAL_BSEC_OtpManualReload(hbsec, otpWordIdx, &otpReloadedShadowVal);
    if(status != HAL_OK)
    {
      return(status);
    }

    /** Important Note : the OTP shadow word value
     * is refreshed (reloaded) but is not returned
     * by the function itself.
     * So a call of
     * 'BSEC_OtpShadowRead(otpWordIdx, &otpShadowVal, isPostManualReload=1)'
     * have to be made by the caller of the current function HAL_BSEC_OtpProgram()
     */
  }

  return(HAL_OK);
}


/**
* @brief  Indicates if otpWordIdx is Permanently permanent programming locked.
*
* @param  BSECx : pointer on BSEC HW instance
* @param  [in]  otpWordIdx      : OTP word from which we want to read the value.
* @param  [in/out] pLockStatus  ; Pointer set to 0 when OTP is NOT permanent programmed
*                                         set to 1 when OTP is permanent programmed
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
*/
HAL_StatusTypeDef HAL_BSEC_GetOtpPermanentProgLockStatus(BSEC_HandleTypeDef * hbsec,
                                                         uint32_t             otpWordIdx, 
                                                         uint32_t           * pLockStatus)

{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef statusWait;
  uint32_t readValue;

  /** Verify input parameters validity */
  assert_param((otpWordIdx <= LL_BSEC_OTP_WORD_INDEX_MAX));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait == HAL_ERROR)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /* LL_BSEC_OtpStatus_Is_PermProgLocked return permanent lock status of last OTP word read */
  status =  HAL_BSEC_OtpRead(hbsec, otpWordIdx, &readValue);
  if(status != HAL_OK)
  {
    return HAL_ERROR;
  }
  /* Get permanent programming lock status */
  *pLockStatus = LL_BSEC_OtpStatus_Is_PermProgLocked(hbsec->Instance);

  return HAL_OK;
}


/**
* \brief   This function gets debug signals enabled/disabled status by reading BSEC register 'BSEC_DENR'.
* @param   [in/out] pDebugSignals  : pointer to get debug signals.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors were encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetDebugSignals(BSEC_HandleTypeDef *hbsec, uint32_t * pDebugSignals)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pDebugSignals != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  * pDebugSignals = LL_BSEC_Get_DenableSignals(hbsec->Instance);

  return(HAL_OK);
}

/**
* \brief   This function set debug signals in register 'BSEC_DENR'
*          This is possible if the HAL_BSEC_DENR register is not currently sticky write locked.
*          It is checked here.
* @param   [in] debugSignals  : debug signals to apply to HAL_BSEC_DENR' (payload part lower 16 bits).
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors were encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_SetDebugSignals(BSEC_HandleTypeDef *hbsec, uint32_t debugSignals)
{
  HAL_StatusTypeDef statusWait;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /** Is DENABLE sticky write locked in 'BSEC_LOCKR.DENLOCK' ? */
  if(LL_BSEC_Is_Denable_StickyWriteLocked(hbsec->Instance) != 0)
  {
    return(HAL_ERROR); /* Early exit */
  }

  /** HAL_BSEC_DENR is not sticky write locked : proceed to write HAL_BSEC_DENR */
  LL_BSEC_Set_Denable(hbsec->Instance, debugSignals);

  return(HAL_OK);
}


/**
* \brief   This function reads the BSEC Warm reset persistent Scratch word 'scratchWordNb'.
*
* @param   [in]     scratchWordNb : BSEC Warm reset persistent Scratch word index to read.
* @param   [in,out] pWordValue    : pointer to read Scratch word value
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetWarmResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t * pWordValue)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(pWordValue != (uint32_t *)NULL);
  assert_param(scratchWordNb < LL_BSEC_NB_WARM_RESET_SCRATCH_REG);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /** Read HAL_BSEC_SCRATCHRx with x = scratchWordNb */
  * pWordValue = LL_BSEC_Get_WarmResetPersistScratch(hbsec->Instance, scratchWordNb);

  return(HAL_OK);
}

/**
* \brief   This function Write the BSEC Warm reset persistent Scratch word 'scratchWordNb' with
*          value from second parameter 'value'.
*
* @param   [in]     scratchWordNb : scratch register word to write.
* @param   [in]     value        : word value to write to HAL_BSEC_SCRATCHx register
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_WriteWarmResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t value)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((scratchWordNb < LL_BSEC_NB_WARM_RESET_SCRATCH_REG));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /** Write HAL_BSEC_SCRATCHRx */
  LL_BSEC_Write_WarmResetPersistScratchWord(hbsec->Instance, scratchWordNb, value);

  return(HAL_OK);
}

/**
* \brief   This function reads the BSEC Hot reset persistent Scratch word 'scratchWordNb'.
*
* @param   [in]     scratchWordNb : BSEC Hot reset persistent Scratch word index to read.
* @param   [in,out] pWordValue    : pointer to read Scratch word value
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetHotResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t * pWordValue)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(pWordValue != (uint32_t *)NULL);
  assert_param(scratchWordNb < LL_BSEC_NB_HOT_RESET_SCRATCH_REG);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /** Read HAL_BSEC_WOSCRx with x = scratchWordNb */
  * pWordValue = LL_BSEC_Get_HotResetPersistScratch(hbsec->Instance, scratchWordNb);

  return(HAL_OK);
}

/**
* \brief   This function Write the BSEC Hot reset persistent Scratch word 'scratchWordNb' with
*          value from second parameter 'value'.
*
* @param   [in]     scratchWordNb : scratch register word to write.
* @param   [in]     value        : word value to write to HAL_BSEC_WOSCRx register
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_WriteHotResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t value)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param(scratchWordNb < LL_BSEC_NB_HOT_RESET_SCRATCH_REG);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /** Write HAL_BSEC_WOSCRx */
  LL_BSEC_Write_HotResetPersistScratchWord(hbsec->Instance, scratchWordNb, value);

  return(HAL_OK);
}

/**
* \brief   This function gets the warm reset counter : ie the number of warm resets
*          in BSEC ('rst_n') since the last scratch reset ('scratch_rst_n' = rst_por).
*
* @param   [in]     pWarmResetCount : pointer to get warm reset counter.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetWarmResetCounter(BSEC_HandleTypeDef *hbsec, uint32_t * pWarmResetCount)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pWarmResetCount != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  * pWarmResetCount = LL_BSEC_Get_WarmRstCounter(hbsec->Instance);

  return(HAL_OK);
}

/**
* \brief   This function gets the hot reset counter : ie the number of hot resets
*          in BSEC ('hot_rst_n') since the last warm ('rst_n') or scratch ('scratch_rst_n').
*
* @param   [in]     pHotResetCount : pointer to get hot reset counter.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_GetHotResetCounter(BSEC_HandleTypeDef *hbsec, uint32_t * pHotResetCount)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pHotResetCount != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  * pHotResetCount = LL_BSEC_Get_HotRstCounter(hbsec->Instance);

  return(HAL_OK);
}

/**
* \brief   This function gets information of SoC feature disabling from OTP_FEATURE_DISABLING word
*          : WORD9 (hard wired). This word is shadowed but is HW sticky write locked, read it via HAL_BSEC_SFR9.
*
* @param   [in,out] pFeatureDisabledWord : pointer to get feature disabling word information
*
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_Get_FeatureDisabled(BSEC_HandleTypeDef *hbsec, uint32_t * pFeatureDisabledWord)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pFeatureDisabledWord != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    return(HAL_ERROR); /* Early exit */
  }

  /* Word OTP_FEATURE_DISABLING is known to be shadowed */
  * pFeatureDisabledWord = LL_BSEC_Read_OtpShadow(hbsec->Instance, LL_BSEC_FEATURE_DISABLING_WORD_NB);

  return(HAL_OK);
}

/**
* \brief   This function returns information whether the feature in
*          first parameter 'feature' is feature disabled
  *         by OTP_FEATURE_DISABLING word : WORD9 (hard wired).
  *         This word is shadowed but is HW sticky write locked.
  *
  *         Example call : to know if dual CA35 is feature disabled call :
  *         status = HAL_BSEC_IsFeatureDisabled(HAL_BSEC_FEATURE_DISABLED_DUAL_CA35,
  *                                         &isFeatureDisabled);
  *         If isFeatureDisabled != 0 then the feature dual CA35 is effectively disabled
  *         otherwise dual CA35 feature is NOT disabled.
  *
* @param   [in]     feature            : feature to be tested use defines HAL_BSEC_FEATURE_DISABLED_XXX : one single feature at a time !
* @param   [in,out] pIsFeatureDisabled : pointer to get information whether the feature from parameter 'feature' is effectively
*                                        disabled : value != 0 returned when the feature is disabled.
*                                                   value = 0 when feature is NOT disabled.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_IsFeatureDisabled(BSEC_HandleTypeDef *hbsec, uint32_t feature, uint32_t * pIsFeatureDisabled)
{
  HAL_StatusTypeDef statusWait;
  HAL_StatusTypeDef status;
  uint32_t featureDisablingWord;

  /** Verify input parameters validity */
  assert_param(pIsFeatureDisabled != (uint32_t *)NULL);
  assert_param(feature < (1<<LL_BSEC_FEATURE_DISABLED_NB_BITS_USED));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

   status = HAL_BSEC_Get_FeatureDisabled(hbsec, &featureDisablingWord);
   if(status == HAL_OK)
   {
     * pIsFeatureDisabled = (featureDisablingWord & feature);
     return(HAL_OK);
   }

  return(HAL_ERROR);
}


#if 0
/**
* \brief   This function set debug signals enabled/disabled of Cortex-M
*          in register 'BSEC_DENR', to match given debug profile from parameter
*          'dbgProfile'.
* @param   [in] dbgProfile  : wanted debug profile to install for Cortex-M.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors were encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_SetCortexMDbgProfile(BSEC_HandleTypeDef *hbsec, BSEC_DebugProfileTypeDef dbgProfile)
{
    HAL_StatusTypeDef statusWait;
    uint32_t signals;

    /** Wait until previous BSEC operations are all over prior requesting new operation */
    statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
    if(statusWait != HAL_OK)
    {
      /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
      return(HAL_ERROR); /* Early exit */
    }

    /** Is HAL_BSEC_DENR sticky write locked in 'BSEC_LOCKR.DENLOCK' ? */
    /* If this is the case no need to go further ...               */
    if(HAL_BSEC_Is_Denable_StickyWriteLocked(hbsec) != 0)
    {
      hbsec->ErrorCode |= HAL_BSEC_ERROR_STICKY_WRITE_LOCK;
      return(HAL_ERROR); /* Early exit */
    }

    /* Get 'BSEC_DENR' current debug signals (all Cortex-M and Cortex-A) */
    HAL_BSEC_GetDebugSignals(&signals);

    /* Clear first all Cortex-M related signals */
    signals &= ~(HAL_BSEC_DBG_CM_ALL_SIGNALS);

    switch(dbgProfile)
    {
    case HAL_BSEC_DBGPF_DBG_NO :
      /*!< No Debug */
      /* Nothing more to do */
      break;
    case HAL_BSEC_DBGPF_DBG_NS_TRACEONLY :
      /*!< Debug Non Secure with Trace Only */
      signals |= HAL_BSEC_DBG_CM_NS_TRACEONLY;
      break;
    case HAL_BSEC_DBGPF_DBG_NS_FULL :
      /*!< Full Debug Non Secure */
      signals |= HAL_BSEC_DBG_CM_NS_FULLDBG;
      break;
    case HAL_BSEC_DBGPF_DBG_S_TRACEONLY :
      /*!< Debug Secure with Trace Only */
      signals |= HAL_BSEC_DBG_CM_S_TRACEONLY;
      break;
    case HAL_BSEC_DBGPF_DBG_S_FULL :
      signals |= HAL_BSEC_DBG_CM_S_FULLDBG;
      break;
    default :
      break;
    } /* of switch(dbgProfile) */

    /* Re-write 'BSEC_DENR' register with updated */
    /* signals affecting Cortex-M debug according wanted debug profile Cortex-M */
    HAL_BSEC_SetDebugSignals(signals);

    return(HAL_OK);
}

/**
* \brief   This function set debug signals enabled/disabled of Cortex-A
*          in register 'BSEC_DENR', to match given debug profile from parameter
*          'dbgProfile'.
* @param   [in] dbgProfile  : wanted debug profile to install for Cortex-A.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors were encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_SetCortexADbgProfile(BSEC_HandleTypeDef *hbsec, BSEC_DebugProfileTypeDef dbgProfile)
{
  HAL_StatusTypeDef statusWait;
  uint32_t signals;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Is HAL_BSEC_DENR sticky write locked in 'BSEC_LOCKR.DENLOCK' ? */
  /* If this is the case no need to go further ...               */
  if(HAL_BSEC_Is_Denable_StickyWriteLocked(hbsec) != 0)
  {
    hbsec->ErrorCode |= HAL_BSEC_ERROR_STICKY_WRITE_LOCK;
    return(HAL_ERROR); /* Early exit */
  }

  /* Get 'BSEC_DENR' current debug signals (all Cortex-M and Cortex-A) */
  HAL_BSEC_GetDebugSignals(&signals);

  /* Clear first all Cortex-A related signals */
  signals &= ~(HAL_BSEC_DBG_CA_ALL_SIGNALS);

  switch(dbgProfile)
  {
  case HAL_BSEC_DBGPF_DBG_NO :
    /*!< No Debug */
    /* Nothing more to do */
    break;
  case HAL_BSEC_DBGPF_DBG_NS_TRACEONLY :
    /*!< Debug Non Secure with Trace Only */
    signals |= HAL_BSEC_DBG_CA_NS_TRACEONLY;
    break;
  case HAL_BSEC_DBGPF_DBG_NS_FULL :
    /*!< Full Debug Non Secure */
    signals |= HAL_BSEC_DBG_CA_NS_FULLDBG;
    break;
  case HAL_BSEC_DBGPF_DBG_S_TRACEONLY :
    /*!< Debug Secure with Trace Only */
    signals |= HAL_BSEC_DBG_CA_S_TRACEONLY;
    break;
  case HAL_BSEC_DBGPF_DBG_S_FULL :
    signals |= HAL_BSEC_DBG_CA_S_FULLDBG;
    break;
  default :
    break;
  } /* of switch(dbgProfile) */

  /* Re-write 'BSEC_DENR' register with updated */
  /* signals affecting Cortex-A debug according wanted debug profile Cortex-A */
  HAL_BSEC_SetDebugSignals(signals);

  return(HAL_OK);
}

/**
* \brief   This function set all debug disabled for both Cortex-M & Cortex-A
*          in register 'BSEC_DENR'
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors were encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_SetAllDebugDisabled(BSEC_HandleTypeDef *hbsec)
{
  HAL_StatusTypeDef status;

  /* Set debug profile No Trace on Cortex-A */
  status = HAL_BSEC_SetCortexADbgProfile(BSEC_DBGPF_DBG_NO);
  if(status == HAL_OK)
  {
    /* Set debug profile No Trace on Cortex-M also */
    status = HAL_BSEC_SetCortexMDbgProfile(BSEC_DBGPF_DBG_NO);
  }

  return(status);
}

/**
* \brief   This function gets the functionality sticky locks status on functions in the following list :
*          - Global Sticky Write OTP Write and BSEC registers write lock
*          - Sticky lock write of BSEC register debug enable register 'BSEC_DENR'
*          - Lock of HWKEY so that it becomes unusable for SAES IP block
* @param   [in] pFunctionLockMaskRead : Pointer to get the Mask of functions locked
*               can be compared with values of @ref HAL_BSEC_FuncStickyLockTypeDef to know each function that is locked
*               currently.
*               Note : each bit for which the feature is currently locked have value 1b1', values to 1b0'
*               are the functions not locked.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_FunctionGetLock(BSEC_HandleTypeDef *hbsec, uint32_t * pFunctionLockMaskRead)
{
  uint32_t maxFuncLockFieldVal = (BSEC_FUNC_STICKY_GLOBAL_WRITE_LOCK  |
                                  HAL_BSEC_FUNC_STICKY_LOCK_DENABLE_WRITE |
                                  HAL_BSEC_FUNC_STICKY_LOCK_HWKEY);
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pFunctionLockMaskRead != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Read the mask from 'BSEC_LOCKR' */
  * pFunctionLockMaskRead = (HAL_BSEC_Get_FunctionStickyLock(hbsec) & maxFuncLockFieldVal);

  return(HAL_OK);
}

/**
* \brief   This function set the functionality sticky locks status on functions in the following list :
*          - Global Sticky Write lock
*          - Sticky lock write of BSEC register debug enable
* @param   [in] setFunctionLockMask : Mask set of functions to lock should be a value
*               created by ORring one or more of values from @ref HAL_BSEC_FuncStickyLockTypeDef.
*               The @ref HAL_BSEC_FuncStickyLockTypeDef are directly mapping the Hw register 'BSEC_LOCKR'.
*               Note : each bit for which the feature should be locked should have value 1b1', values to 1b0'
*               have no locking effect (and no clearing of a previous lock also !!).
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_FunctionSetLock(BSEC_HandleTypeDef *hbsec, uint32_t setFunctionLockMask)
{
  HAL_StatusTypeDef statusWait;
  uint32_t maxFuncLockFieldVal = (BSEC_FUNC_STICKY_GLOBAL_WRITE_LOCK  |
                                  HAL_BSEC_FUNC_STICKY_LOCK_DENABLE_WRITE |
                                  HAL_BSEC_FUNC_STICKY_LOCK_HWKEY);

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Write the mask in register 'BSEC_LOCKR' */
  HAL_BSEC_Set_FunctionStickyLock(hbsec, (setFunctionLockMask & maxFuncLockFieldVal));

  return (HAL_OK);
}

/**
* \brief   This function reads a CLTAP word (32 bits) written by JTAG/PC or tester side
*          STM32MP25xx SoC Receive direction, using HAL_BSEC_JTAGINR register.
* @param   [in,out] pJtagReadVal : pointer to word read from JTAG.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_JtagInRead(BSEC_HandleTypeDef *hbsec, uint32_t * pJtagReadVal)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pJtagReadVal != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /* Read HAL_BSEC_JTAGINR */
  * pJtagReadVal = HAL_BSEC_Get_JtagIn(hbsec);

  return(HAL_OK);
}

/**
* \brief   This function writes a JTAG word (32 bits) toward PC/JTAG :
*          STM32MP25xx SoC Transmit direction, using HAL_BSEC_JTAGOUTR register.
* @param   [in] jtagWriteVal  : word value to write to HAL_BSEC_JTAGOUTR.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_JtagOutWrite(BSEC_HandleTypeDef *hbsec, uint32_t jtagWriteVal)
{
  HAL_StatusTypeDef statusWait;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Write HAL_BSEC_JTAGOUTR */
  HAL_BSEC_Write_JtagOut(hbsec, jtagWriteVal);

  return(HAL_OK);
}

/**
* \brief   This function returns in first parameter 'isRomFullyMapped', whether or not
*          the ROM is currently fully mapped when * isRomFullyMapped != 0.
*
* @param   [in/out] pIsRomFullyMapped : pointer to read information on ROM fully mapped status.
*                                       Possible values read are :
*                                       value : HAL_BSEC_ROM_FULLY_MAPPED if all ROM is mapped : ie the ROM un-mappable part is mapped.
*                                       value : HAL_BSEC_ROM_PARTIAL_UNMAPPED0 when a part of ROM (the un-mappable part) is unmapped, then ROM is only
*                                               partially mapped.
*                                       value : HAL_BSEC_ROM_UNMAP_INVALID if ROM Unmap feature seems to have been hacked.
*
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_IsRomFullyMapped(BSEC_HandleTypeDef *hbsec, uint32_t * pIsRomFullyMapped)
{
  HAL_StatusTypeDef statusWait;

  /** Verify input parameters validity */
  assert_param((pIsRomFullyMapped != (uint32_t *)NULL));

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  * pIsRomFullyMapped = HAL_BSEC_Is_Rom_FullyMapped(hbsec);

  return(HAL_OK);
}

/**
* \brief   This function is used to post unmap from bootROM the un-mappable
*          part of ROM for security reasons to avoid attacks by hacker bootROM code
*          analysis.
*
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_RomPartialUnmap(BSEC_HandleTypeDef *hbsec)
{
  HAL_StatusTypeDef statusWait;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Request ROM un-mappable area unmap */
  HAL_BSEC_Rom_PartialUnmap(hbsec);

  /* TODO : Probable need to clean and invalidate I Cache to flush code is un-mappable area that could */
  /* have been pre-fetched while the area was mapped                                              */

  return(HAL_OK);
}

/**
* \brief   This function is used during the ECIES process on a sample in BSEC security
*          CLOSED state, with OTP_HW_WORD4[TK_RETRIES] != 0xFxyz (not last tk_retries
*          exhausted) to provision in EWS from tester an ECIES_CHIP_PRIVK_ENC (ECIES_CHIP_PRIVK
*          encrypted by TK value onchip by AES-ECB).
*          The ECIES process is a Hw process that lead to HW decryption AES-ECB with the TK
*          and fuse of ECIES_CHIP_PRIVK (by Hw process still) in OTP_CFG368..375 : "OTP_ECIES_CHIP_PRIVK0..7“.
*
* @param   [in] pEciesChipPrivKEnc : pointer on uint8_t of the ECIES_CHIP_PRIVK_ENC received from HSM-ST
*               via TESTER ST Key Provisioning structure field 'ecies_chip_privk_encrypted[]'.
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_ProvisionEciesChipPrivk(BSEC_HandleTypeDef *hbsec, uint8_t * pEciesChipPrivKEnc)
{
  HAL_StatusTypeDef statusWait;
  uint32_t wordIdx;
  uint32_t * pEciesChipPrivKEnc32;

  /** Verify input parameters validity */
  assert_param((pEciesChipPrivKEnc != (uint8_t *)NULL));

  pEciesChipPrivKEnc32 = (uint32_t *)pEciesChipPrivKEnc;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  /** Loop to write word by word the ECIES_CHIP_PRIVK_ENC in 'BSEC_ENCKEYRx' (x=0..7) registers */
  for(wordIdx = 0; wordIdx < LL_BSEC_NB_ENC_ECIES_WORDS; wordIdx++, pEciesChipPrivKEnc32++)
  {
    HAL_BSEC_Write_EncEcies_Word(hbsec, wordIdx, *pEciesChipPrivKEnc32);

    /** Wait until previous BSEC operations are all over prior requesting new operation */
    statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
    if(statusWait != HAL_OK)
    {
      /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
      return(HAL_ERROR); /* Early exit */
    }

  } /* of for(wordIdx = 0; wordIdx < LL_BSEC_NB_ENC_ECIES_WORDS; wordIdx++, pEciesChipPrivKEnc32++) */

  return(HAL_OK);
}

/**
* \brief   This function is used at the end of the EWS process to provision a HWKEY on a sample in BSEC security
*          CLOSED state.
*          The BSEC Wrapper Hw process 'HWKEY' in generates from RNG (and RNG bus) 8 random words, verify and fuse them in
*          OTP_CFG184..191 : "OTP_HWKEY0..7"
*
* \return  HAL_StatusTypeDef : Returned value is either :
*          HAL_OK if no error was encountered
*          or HAL_ERROR in case one or several errors where encountered.
******************************************************************************
*/
HAL_StatusTypeDef HAL_BSEC_ProvisionHwKey(BSEC_HandleTypeDef *hbsec)
{
  HAL_StatusTypeDef statusWait;
  uint32_t wordIdx;

  /** Wait until previous BSEC operations are all over prior requesting new operation */
  statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
  if(statusWait != HAL_OK)
  {
    /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
    hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
    return(HAL_ERROR); /* Early exit */
  }

  for(wordIdx = 0; wordIdx < LL_BSEC_NB_HWKEY_WORDS; wordIdx++)
  {
    HAL_BSEC_Write_HwKey_Word(hbsec, wordIdx);

    /* Wait until previous BSEC operations are all over prior requesting new operation */
    statusWait = BSEC_WaitBusyClearWithTimeout(hbsec,HAL_BSEC_TIMEOUT_VAL);
    if(statusWait != HAL_OK)
    {
      /** We had a timeout occurrence waiting for Busy bit to be cleared ... */
      hbsec->ErrorCode |= HAL_BSEC_ERROR_TIMEOUT;
      return(HAL_ERROR); /* Early exit */
    }

  } /* of for(wordIdx = 0; wordIdx < LL_BSEC_NB_HWKEY_WORDS; wordIdx++) */

  return(HAL_OK);
}
#endif

/**
  * @}
  */


/**
  * @}
  */


/**
  * @}
  */

/**
  * @}
  */

#endif /* defined(BSEC) && defined(HAL_BSEC_MODULE_ENABLED) */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

