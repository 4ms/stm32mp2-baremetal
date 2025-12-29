/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_bsec.h
  * @author  MCD Application Team
  * @brief   Header file of BSEC HAL module.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32MP2xx_HAL_BSEC_H
#define STM32MP2xx_HAL_BSEC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32mp2xx_hal_def.h"
#include "stm32mp2xx_ll_bsec.h"

/** @addtogroup STM32MP2xx_HAL_Driver
  * @{
  */

/** @addtogroup CRC
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/** @defgroup CRC_Exported_Types CRC Exported Types
  * @{
  */

/**
  * @brief  CRC HAL State Structure definition
  */
typedef enum
{
  HAL_BSEC_STATE_RESET     = 0x00U,  /*!< CRC not yet initialized or disabled */
  HAL_BSEC_STATE_READY     = 0x01U,  /*!< CRC initialized and ready for use   */
  HAL_BSEC_STATE_BUSY      = 0x02U,  /*!< CRC internal process is ongoing     */
  HAL_BSEC_STATE_TIMEOUT   = 0x03U,  /*!< CRC timeout state                   */
  HAL_BSEC_STATE_ERROR     = 0x04U   /*!< CRC error state                     */
} HAL_BSEC_StateTypeDef;




/** @defgroup BSEC_handle_Structure_definition BSEC handle Structure definition
  * @brief  BSEC handle Structure definition
  * @{
  */

typedef struct __BSEC_HandleTypeDef
{
  BSEC_TypeDef                   *Instance;              /*!< Register base address */
  __IO HAL_BSEC_StateTypeDef     State;                  /*!< BSEC state */
  __IO uint32_t                 ErrorCode;              /*!< BSEC Error code */
} BSEC_HandleTypeDef;



// TO_DO : compute timeout in 'ck_hsi' from gentimer and estimated time for a Read and a Write operation @ 48 MHz clock
#define HAL_BSEC_TIMEOUT_VAL                       1000000

 /** \brief : For OtpProgram function : list of Programming options
  */
#define BSEC_PROG_OPT_BLIND_MODE               (0 << 0) /* Programming in Blind mode */
#define BSEC_PROG_OPT_RELOAD_SHADOW            (1 << 0) /* Programming in Blind mode with shadow reload at end of programming */
#define BSEC_PROG_OPT_VERIFY                   (1 << 1) /* Programming + pre and post verify : requires read access */

 /** \brief : For OtpProgram function : list of Programming Lock options
  */
#define BSEC_PROGLOCK_OPT_NO_LOCK              (0 << 0) /* No programming lock */
#define BSEC_PROGLOCK_OPT_LOCK                 (1 << 0) /* programming lock */
#define BSEC_PROGLOCK_OPT_VERIFY               (1 << 1) /* programming lock verify */

/** @defgroup BSEC3_Error_Code Error Code
  * @{
  */
  
/* otp_r : reload OTP     */
/* otp_w : prog OTP       */
/* sha_r : read shadow    */
/* sha_w : write shadow   */
/* all   : all operation  */
  
#define HAL_BSEC_ERROR_NONE                           (0x00000000U)    /*!< No error                                                                                                           */
#define HAL_BSEC_ERROR_TIMEOUT                        (0x00000001U)    /*!< Timeout error                                                                              all                     */
																																									
#define HAL_BSEC_ERROR_GLOBAL_WRITE_LOCK              (0x00000002U)    /*!< Sticky global write lock bit set. OTP program/reload op. and shadow reg. write forbidden   otp_r otp_p       sha_w */
#define HAL_BSEC_ERROR_HIDE_UP                        (0x00000004U)    /*!< OTP upper can not be access                                                                otp_r otp_p             */
#define HAL_BSEC_ERROR_STICKY_READ_LOCK               (0x00000008U)    /*!< Sticky Read Lock bit is set. Reload operation on this OTP is forbidden                     otp_r                   */
#define HAL_BSEC_ERROR_OTP_NOT_VALID                  (0x00000010U)    /*!< OTP reloaded and OTPVLD bit is not set                                                     ? (only after BSEC reset??) */
#define HAL_BSEC_ERROR_RELOAD_DISTURB                 (0x00000020U)    /*!< Disturb error bit set for an OTP (auto/manual) reload operation                            otp_r                   */
#define HAL_BSEC_ERROR_ECC_SEC                        (0x00000040U)    /*!< ECC single error detected (and corrected)                                                  otp_r                   */
#define HAL_BSEC_ERROR_ECC_DED                        (0x00000080U)    /*!< ECC double error detected (and can not be corrected)                                       otp_r                   */
#define HAL_BSEC_ERROR_ADDR_MISMATCH                  (0x00000100U)    /*!< Address mismatch error                                                                     otp_r                   */
#define HAL_BSEC_ERROR_STICKY_PROG_LOCKED             (0x00000200U)    /*!< Attempt to programm an OTP whose the sticky prog locked bit is set                               otp_p             */
#define HAL_BSEC_ERROR_PROGRAMM_PERMPROGLOCKED_OTP    (0x00000400U)    /*!< Attempt to programm an OTP which is perm prog. locked                                            otp_p             */
#define HAL_BSEC_ERROR_MISMATCHINGPERMPROGLOCK        (0x00000800U)    /*!< mismatching perm prog lock                                                                 otp_r                   */
#define HAL_BSEC_ERROR_PROGRAM_BULK_WORD_NOT_VIRGIN   (0x00001000U)    /*!< Attempt to programm a bulk word OTP which is not virgin                                          otp_p             */
#define HAL_BSEC_ERROR_PROGRAM_BITWISE_WITH_ONE2ZERO  (0x00002000U)    /*!< Attempt to programm a bitwise OTP with 1 -> 0 transition                                         otp_p             */
#define HAL_BSEC_ERROR_PROGFAIL                       (0x00004000U)    /*!< Program fail                                                                                     otp_p             */
#define HAL_BSEC_ERROR_VALUE_MISMATCH                 (0x00008000U)    /*!< Programmed value and reloaded value are mismatching                                        otp_r otp_p             */
#define HAL_BSEC_ERROR_PERMPROGLOCK_FAIL              (0x00010000U)    /*!< Perm Prog lock has been asked and is not set                                                     otp_p             */
#define HAL_BSEC_ERROR_OTP_NOT_SHADOWED               (0x00020000U)    /*!< Try to read or write a shadowed register of a non shadowed OTP                                         sha_r sha_w */
#define HAL_BSEC_ERROR_STICKY_WRITE_LOCK              (0x00040000U)    /*!< Sticky Write Lock bit is set. Shadow register write operation is forbidden                                   sha_w */

#if (USE_HAL_BSEC_REGISTER_CALLBACKS == 1)
#define HAL_BSEC_ERROR_INVALID_CALLBACK               (0x10000000U)    /*!< Invalid Callback error                                              */
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */

/**
* \enum BSEC_ChipSecurityTypeDef
* \brief  BSEC Chip security definitions
*/
typedef enum
{
  BSEC_INVALID_STATE          = 0x00,   /*!< Invalid Security state                                                           */
                                        /*!< Can be a Tamper attempt to be managed as a major security error                  */

  BSEC_UNSECURED_CLOSE_STATE  = 0x01,   /*!< All debug is opened from reset, upper shadows (secrets) not loaded in shadows     */
                                        /*!< Ebeam TK is inaccessible and in reset                                             */
                                        /*!< upper fuse (secrets) in reset and inaccessible                                    */
                                        /*!< ROM is partially un-mapped. BootROM can be bypassed                               */

  BSEC_SECURED_CLOSE_STATE    = 0x02,   /*!< All debug is closed from reset, upper shadows (secrets) are loaded in shadows     */
                                        /*!< for the one shadowed. Upper secrets may be accessed by TZ secure SW if not locked */
                                        /*!< ROM is fully mapped. BootROM cannot be bypassed                                   */
} BSEC_ChipSecurityTypeDef;

/**
 * \enum BSEC_OtpStickyLockTypeDef
 * \brief  BSEC sticky lock by OTP word capabilities definitions
 * The enumerate is made so that all commands can be combined to have several sticky lock operation on the same OTP index
 * or shadow index done at the same time or reported at the same time (status).
 * - sticky lock OTP word programming operation (until next system Reset)
 * - sticky lock OTP word shadow register write (until next system Reset)
 * - sticky lock OTP word read (until next system Reset)
 */
typedef enum
{
  /*!< Sticky lock programming of a given OTP word  */
  /* no programming allowed until next System Reset */
  /*!< No Sticky lock of any kind */
  BSEC_OTP_STICKY_LOCK_NOT_LOCKED      = LL_BSEC_STICKY_NOT_LOCKED,

  /*!< Sticky lock programming of a given OTP word */
  BSEC_OTP_STICKY_LOCK_PROG_OTP_WORD   = LL_BSEC_STICKY_PROG_LOCKED,

  /*!< Sticky lock write of a given OTP shadow register */
  BSEC_OTP_STICKY_LOCK_WRITE_SHADOW    = LL_BSEC_STICKY_WRITE_LOCKED,

  /*!< Sticky lock read of a given OTP word                                         */
  /* hence : update of shadow by read : ie OTP Reload manual operation is forbidden */
  BSEC_OTP_STICKY_LOCK_READ            = LL_BSEC_STICKY_READ_LOCKED

} BSEC_OtpStickyLockTypeDef;

/**
  * \enum BSEC_AnalyzeErrorTypeDef
  * \brief  Types of BSEC errors to analyze from BSEC_OTPSR register.
  */
typedef enum
{
  /*!< Analyze automatic reload errors  */
  HAL_BSEC_ANALYZE_AUTO_RELOAD_ERRORS = 1,

  /*!< Analyze manual reload errors  */
  HAL_BSEC_ANALYZE_MANUAL_RELOAD_ERRORS

} HAL_BSEC_AnalyzeErrorTypeDef;

/**
  * \enum BSEC_ProgramOptionTypeDef
  * \brief  BSEC programming options for function BSEC_OtpProgram parameter
  * 'progOptions' Possible programming options available to user.
  */
 typedef enum
 {
   BSEC_PROGRAM_OPTION_BLIND_MODE                    =  BSEC_PROG_OPT_BLIND_MODE,                   /*!< programming in fully Blind mode */
   BSEC_PROGRAM_OPTION_BLIND_MODE_WITH_RELOAD_SHADOW = (BSEC_PROGRAM_OPTION_BLIND_MODE | BSEC_PROG_OPT_RELOAD_SHADOW), /*!< programming in blind mode then reload shadow at end */
   BSEC_PROGRAM_OPTION_VERIFY                        =  BSEC_PROG_OPT_VERIFY,                       /*!< programming and pre + post verify */

 } BSEC_ProgramOptionTypeDef;

 /**
   * \enum BSEC_ProgramLockOptionTypeDef
   * \brief  BSEC programming lock options for function BSEC_OtpProgram parameter
   * 'progOptions' Possible programming options available to user.
   */
typedef enum
{
  BSEC_PROGLOCK_NOLOCK                = BSEC_PROGLOCK_OPT_NO_LOCK, /*!< No programming lock */
  BSEC_PROGLOCK_LOCK_BLIND_MODE       = BSEC_PROGLOCK_OPT_LOCK,    /*!< programming lock in blind mode */
  BSEC_PROGLOCK_LOCK_VERIFY           = (BSEC_PROGLOCK_LOCK_BLIND_MODE | BSEC_PROGLOCK_OPT_VERIFY), /*!< programming lock and verify */

} BSEC_ProgramLockOptionTypeDef;

 /**
   * \enum BSEC_DebugProfileTypeDef
   * \brief  Definition of possible Debug Profiles valid for both debug sub-systems
   *         Cortex-M and Cortex-A
   */
typedef enum
{
    BSEC_DBGPF_DBG_NO,            /*!< No Debug possible                */
    BSEC_DBGPF_DBG_NS_TRACEONLY,  /*!< Debug Non Secure with Trace Only */
    BSEC_DBGPF_DBG_NS_FULL,       /*!< Full Debug Non Secure            */
    BSEC_DBGPF_DBG_S_TRACEONLY,   /*!< Debug Secure with Trace Only     */
    BSEC_DBGPF_DBG_S_FULL         /*!< Full Debug Secure                */

} BSEC_DebugProfileTypeDef;

 /**
   * \enum BSEC_FuncStickyLockTypeDef
   * \brief  BSEC functional sticky lock capabilities definitions
   * - Global Sticky Write lock for 'bsec_corekp1' and 'bsec_core1' that prevent operation such as BSEC shadow write,
   *   OTP Reload, OTP programming (ie fusing) or even any BSEC register write operation.
   * - Debug Enable Write : for debug (CoreSight) access control (global, for non invasive or invasive acces in
   *   either non secure or secure privileged modes) : allows to disable or re-enable a debug access.
   * - HWKEY lock so that it becomes unusable by SAES block as potential key.
   *   The enumeration values are matching the mapping of fields of register BSEC_LOCKR and can be
   *   ORed to create a mask of functionalities to sticky lock.
   */
 typedef enum
 {
   /*!< Global Sticky Write lock on all BSEC registers */
   BSEC_FUNC_STICKY_GLOBAL_WRITE_LOCK   = BSEC_LOCKR_GWLOCK,

   /*!< Sticky lock write of BSEC register debug enable */
   BSEC_FUNC_STICKY_LOCK_DENABLE_WRITE  = BSEC_LOCKR_DENLOCK,

   /*!< Sticky lock HWKEY (will be unusable by SAES block as potential key) */
   BSEC_FUNC_STICKY_LOCK_HWKEY          = BSEC_LOCKR_HKLOCK

 } BSEC_FuncStickyLockTypeDef;

/*
 * Global feature disable list. It corresponds to the OTP9 register value 
 */
typedef enum
{
  BSEC_FEATURE_DISABLED_DUAL_CA35                 = (1<<0),
  BSEC_FEATURE_DISABLED_CM0_PLUS                  = (1<<1),
  BSEC_FEATURE_DISABLED_GPU                       = (1<<2),
  BSEC_FEATURE_DISABLED_DSI                       = (1<<3),
  BSEC_FEATURE_DISABLED_CSI                       = (1<<4),
  BSEC_FEATURE_DISABLED_DUAL_ETH_GMAC             = (1<<5),
  BSEC_FEATURE_DISABLED_FDCAN                     = (1<<6),
  BSEC_FEATURE_DISABLED_USB3                      = (1<<7),
  BSEC_FEATURE_DISABLED_PCIE                      = (1<<8),
  BSEC_FEATURE_DISABLED_CRYPTO_CRYP_SAES_PKA_FULL = (1<<9),
  BSEC_FEATURE_DISABLED_DDRCRYP                   = (1<<10),
  BSEC_FEATURE_DISABLED_OTFDEC                    = (1<<11),
  BSEC_FEATURE_DISABLED_LVDS                      = (1<<12),
  BSEC_FEATURE_DISABLED_RNG                       = (1<<13),
  BSEC_FEATURE_DISABLED_RESERVED_FUTURE1          = (1<<14), /* reserved for future use 1 */
  BSEC_FEATURE_DISABLED_RESERVED_FUTURE2          = (1<<15), /* reserved for future use 2 */
  BSEC_FEATURE_DISABLED_GPU_NN_EXTENSION          = (1<<16),
  BSEC_FEATURE_DISABLED_VDEC_VENC                 = (1<<17),
  BSEC_FEATURE_DISABLED_CA35_AARCH64_BOOT         = (1<<18),
  BSEC_FEATURE_DISABLED_ETHSW                     = (1<<19) /* TSN switch */
} BSEC_Feature_disabled;

/* Exported functions --------------------------------------------------------*/
/** @defgroup BSEC_Exported_Functions BSEC Exported Functions
  * @{
  */

/* Initialization and de-initialization functions  ****************************/
/** @defgroup CRC_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
HAL_StatusTypeDef HAL_BSEC_Init(BSEC_HandleTypeDef *hbsec);
HAL_StatusTypeDef HAL_BSEC_DeInit(BSEC_HandleTypeDef *hbsec);
void              HAL_BSEC_MspInit(BSEC_HandleTypeDef *hbsec);
void              HAL_BSEC_MspDeInit(BSEC_HandleTypeDef *hbsec);
/**
  * @}
  */

/* Peripheral Control functions ***********************************************/
HAL_StatusTypeDef HAL_BSEC_Is_Shadowed(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pIsShadowed);
HAL_StatusTypeDef HAL_BSEC_Analyze_OtpManualReloadErrors(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx);
HAL_StatusTypeDef HAL_BSEC_Analyze_AutomaticReloadErrors(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx);
HAL_StatusTypeDef HAL_BSEC_OtpManualReload(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pOtpShadowVal);
HAL_StatusTypeDef HAL_BSEC_GetSecurityStatus(BSEC_HandleTypeDef *hbsec, BSEC_ChipSecurityTypeDef * pSecStatus);
HAL_StatusTypeDef HAL_BSEC_OtpShadowRead(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pOtpShadowVal);
HAL_StatusTypeDef HAL_BSEC_OtpShadowWrite(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t otpWordVal);
HAL_StatusTypeDef HAL_BSEC_SetOtpStickyLock(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t stickyLockCmd);
HAL_StatusTypeDef HAL_BSEC_GetOtpStickyLockStatus(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pStickyLockStatus);
HAL_StatusTypeDef HAL_BSEC_OtpRead(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t * pOtpShadowVal);
HAL_StatusTypeDef HAL_BSEC_OtpProgram(BSEC_HandleTypeDef *hbsec, uint32_t otpWordIdx, uint32_t otpWordValReq);
HAL_StatusTypeDef HAL_BSEC_SetOtpPermanentProgLock(BSEC_HandleTypeDef * hBsec, uint32_t otpWordIdx);
HAL_StatusTypeDef HAL_BSEC_GetOtpPermanentProgLockStatus(BSEC_HandleTypeDef * hBsec, uint32_t otpWordIdx, uint32_t * pLockStatus);
HAL_StatusTypeDef HAL_BSEC_GetDebugSignals(BSEC_HandleTypeDef *hbsec, uint32_t * pDebugSignals);
HAL_StatusTypeDef HAL_BSEC_SetDebugSignals(BSEC_HandleTypeDef *hbsec, uint32_t debugSignals);
HAL_StatusTypeDef HAL_BSEC_GetWarmResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t * pWordValue);
HAL_StatusTypeDef HAL_BSEC_WriteWarmResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t value);
HAL_StatusTypeDef HAL_BSEC_GetHotResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t * pWordValue);
HAL_StatusTypeDef HAL_BSEC_WriteHotResetPersistScratchWord(BSEC_HandleTypeDef *hbsec, uint32_t scratchWordNb, uint32_t value);
HAL_StatusTypeDef HAL_BSEC_GetWarmResetCounter(BSEC_HandleTypeDef *hbsec, uint32_t * pWarmResetCount);
HAL_StatusTypeDef HAL_BSEC_GetHotResetCounter(BSEC_HandleTypeDef *hbsec, uint32_t * pHotResetCount);
HAL_StatusTypeDef HAL_BSEC_Get_FeatureDisabled(BSEC_HandleTypeDef *hbsec, uint32_t * pFeatureDisabledWord);
HAL_StatusTypeDef HAL_BSEC_IsFeatureDisabled(BSEC_HandleTypeDef *hbsec, uint32_t feature, uint32_t * pIsFeatureDisabled);


/* Peripheral State and Error functions ***************************************/
/** @defgroup CRC_Exported_Functions_Group3 Peripheral State functions
  * @{
  */
HAL_BSEC_StateTypeDef HAL_BSEC_GetState(BSEC_HandleTypeDef *hbsec);
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

#ifdef __cplusplus
}
#endif

#endif /* STM32MP2xx_HAL_BSEC_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

