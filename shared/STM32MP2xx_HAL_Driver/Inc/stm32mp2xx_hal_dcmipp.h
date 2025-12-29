/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_dcmipp.h
  * @author  MCD Application Team
  * @brief   Header file of DCMIPP HAL module.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32MP2xx_HAL_DCMIPP_H
#define __STM32MP2xx_HAL_DCMIPP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32mp2xx_hal_def.h"

#if defined (DCMIPP)
/** @addtogroup __STM32MP2xx_HAL_Driver
  * @{
  */

/** @addtogroup DCMIPP DCMIPP
  * @brief DCMIPP HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/

/** @defgroup DCMIPP_Exported_Constants DCMIPP Exported Constants
  * @{
  */

/** @defgroup DCMIPP_Error_Code DCMIPP Error Code
  * @{
  */
#define HAL_DCMIPP_ERROR_NONE            (0x00000000U)    /*!< No error                                                */
#define HAL_DCMIPP_ERROR_AXI             (0x00000001U)    /*!< IPPLUG AXI Transfer error                               */
#define HAL_DCMIPP_ERROR_SYNC            (0x00000002U)    /*!< Synchronization error                                   */
#define HAL_DCMIPP_ERROR_TIMEOUT         (0x00000004U)    /*!< Timeout error                                           */
#define HAL_DCMIPP_ERROR_LIMIT_DUMP_PIPE (0x00000008U)    /*!< Limit error on dump pipe                                */
#define HAL_DCMIPP_ERROR_MODE            (0x00000010U)    /*!< Wrong DCMIPP mode (only DCMI or CSI modes supported)    */
#define HAL_DCMIPP_ERROR_STATE           (0x00000020U)    /*!< Wrong DCMIPP state                                      */
#define HAL_DCMIPP_ERROR_PIPE_STATE      (0x00000040U)    /*!< Wrong DCMIPP Pipe state                                 */
#define HAL_DCMIPP_ERROR_CAPTURE_MODE    (0x00000080U)    /*!< Wrong capture mode (only continuous/snapshot supported) */
#define HAL_DCMIPP_ERROR_PIPE_ID         (0x00000100U)    /*!< Wrong pipe ID                                           */
#define HAL_DCMIPP_ERROR_NULL_BUFFER     (0x00000200U)    /*!< NULL destination buffer                                 */
#define HAL_DCMIPP_ERROR_NULL_POINTER    (0x00000400U)    /*!< NULL pointer                                            */
#define HAL_DCMIPP_ERROR_OVR_DUMP_PIPE   (0x00000800U)    /*!< Overrun error                                           */
#define HAL_DCMIPP_ERROR_OVR_MAIN_PIPE   (0x00001000U)    /*!< Overrun error on Main Pipe                              */
#define HAL_DCMIPP_ERROR_OVR_AUX_PIPE    (0x00002000U)    /*!< Overrun error on Aux Pipe                               */

#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
#define  HAL_DCMIPP_ERROR_INVALID_CALLBACK ((uint32_t)0x00000080U)    /*!< Invalid Callback error  */
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
/**
  * @}
  */

/** @defgroup DCMIPP_hal_format  DCMIPP Format to be captured
* @{
*/
typedef enum {
  DCMIPP_FORMAT_BYTE           = 0x00U << DCMIPP_PRCR_FORMAT_Pos, /*!< BYTE    */
  DCMIPP_FORMAT_YUV422         = 0x1EU << DCMIPP_PRCR_FORMAT_Pos, /*!< YUV422  */
  DCMIPP_FORMAT_RGB565         = 0x22U << DCMIPP_PRCR_FORMAT_Pos, /*!< RGB565  */
  DCMIPP_FORMAT_RGB666         = 0x23U << DCMIPP_PRCR_FORMAT_Pos, /*!< RGB666  */
  DCMIPP_FORMAT_RGB888         = 0x24U << DCMIPP_PRCR_FORMAT_Pos, /*!< RGB888  */
  DCMIPP_FORMAT_RAW8           = 0x2AU << DCMIPP_PRCR_FORMAT_Pos, /*!< RAW8    */
  DCMIPP_FORMAT_RAW10          = 0x2BU << DCMIPP_PRCR_FORMAT_Pos, /*!< RAW10   */
  DCMIPP_FORMAT_RAW12          = 0x2CU << DCMIPP_PRCR_FORMAT_Pos, /*!< RAW12   */
  DCMIPP_FORMAT_RAW14          = 0x2DU << DCMIPP_PRCR_FORMAT_Pos, /*!< RAW14   */
  DCMIPP_FORMAT_MONOCHROME_8B  = 0x4AU << DCMIPP_PRCR_FORMAT_Pos, /*!< 8-bits  */
  DCMIPP_FORMAT_MONOCHROME_10B = 0x4BU << DCMIPP_PRCR_FORMAT_Pos, /*!< 10-bits */
  DCMIPP_FORMAT_MONOCHROME_12B = 0x4CU << DCMIPP_PRCR_FORMAT_Pos, /*!< 12-bits */
  DCMIPP_FORMAT_MONOCHROME_14B = 0x4DU << DCMIPP_PRCR_FORMAT_Pos, /*!< 14-bits */
} DCMIPP_InputFormatTypeDef;

/**
  * @}
  */

/** @defgroup DCMIPP_hal_interface  DCMIPP interface configuration
* @{
*/
typedef enum {
  DCMIPP_INTERFACE_8BITS  = 0x00U << DCMIPP_PRCR_EDM_Pos, /*!< 8bits on every pixel clock  */
  DCMIPP_INTERFACE_10BITS = 0x01U << DCMIPP_PRCR_EDM_Pos, /*!< 10bits on every pixel clock */
  DCMIPP_INTERFACE_12BITS = 0x02U << DCMIPP_PRCR_EDM_Pos, /*!< 12bits on every pixel clock */
  DCMIPP_INTERFACE_14BITS = 0x03U << DCMIPP_PRCR_EDM_Pos, /*!< 14bits on every pixel clock */
  DCMIPP_INTERFACE_16BITS = 0x04U << DCMIPP_PRCR_EDM_Pos, /*!< 16bits on every pixel clock */
  /* Not part of EDM register but used as extension of the interface configuration */
  DCMIPP_INTERFACE_CSI    = 0x05U << DCMIPP_PRCR_EDM_Pos, /*!< CSI interface:              */
} DCMIPP_InterfaceTypeDef;
/**
  * @}
  */

/** @defgroup DCMIPP_hal_synchro DCMIPP interface configuration
* @{
*/
typedef enum {
  DCMIPP_HSPOLARITY_LOW      = 0x00U,                   /*!< Horizontal synchronization active Low  */
  DCMIPP_HSPOLARITY_HIGH     = DCMIPP_PRCR_HSPOL,       /*!< Horizontal synchronization active High */
} DCMIPP_HPolarityTypeDef;

typedef enum {
  DCMIPP_VSPOLARITY_LOW      = 0x00U,                    /*!< Vertical synchronization active Low    */
  DCMIPP_VSPOLARITY_HIGH     = DCMIPP_PRCR_VSPOL,        /*!< Vertical synchronization active High   */
} DCMIPP_VPolarityTypeDef;

typedef enum {
  DCMIPP_PCKPOLARITY_FALLING = 0x00U,                    /*!< Pixel clock active on Falling edge     */
  DCMIPP_PCKPOLARITY_RISING  = DCMIPP_PRCR_PCKPOL,       /*!< Pixel clock active on Rising edge      */
} DCMIPP_PClkPolarityTypeDef;

/**
  * @}
  */

/** @defgroup DCMIPP_hal_ESS DCMIPP Embedded Synchronization Selected
* @{
*/
#define DCMIPP_EMBEDDED_SYNCHRO    ((uint32_t)DCMIPP_PRCR_ESS)          /*!< Embedded Synchronization Selected */

/**
  * @}
  */
/** @defgroup DCMIPP_hal DCMIPP Multiline mode
* @{
*/
typedef enum {
  DCMIPP_LINEMULT_1_LINE  = 0x00U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 1   line  */
  DCMIPP_LINEMULT_2_LINE  = 0x01U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 2   lines  */
  DCMIPP_LINEMULT_4_LINE  = 0x02U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 4   lines  */
  DCMIPP_LINEMULT_8_LINE  = 0x03U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 8   lines  */
  DCMIPP_LINEMULT_16_LINE = 0x04U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 16  lines  */
  DCMIPP_LINEMULT_32_LINE = 0x05U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 32  lines  */
  DCMIPP_LINEMULT_64_LINE = 0x06U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 64  lines  */
  DCMIPP_LINEMULT_128_LINE= 0x07U << DCMIPP_P0PPCR_LINEMULT_Pos, /*!< Event after every 128 lines  */
} DCMIPP_MultiLineTypeDef;

/**
  * @}
  */
/** @defgroup DCMIPP_hal_P0D DCMIPP Pipe0 Decimation
* @{
*/
typedef enum {
  DCMIPP_P0_DEC_ALL_LINES    = 0x00U,                 /*!< All lines captured       */
  DCMIPP_P0_DEC_ODD_LINES    = (DCMIPP_P0PPCR_LSM | DCMIPP_P0PPCR_OELS),/*!< ODD lines only captured  */
  DCMIPP_P0_DEC_EVEN_LINES   = (DCMIPP_P0PPCR_LSM)            /*!< EVEN lines only captured */
} HAL_DCMIPP_P0_LinesDecimationModeTypeDef;

typedef enum {
  DCMIPP_P0_DEC_ALL_BYTES     = 0x00U,   /*!< All bytes captured       */
  DCMIPP_P0_DEC_EVEN_1_OUT_2  = 0x01U << DCMIPP_P0PPCR_BSM_Pos,           /*!< 1 data out of 2 starting from 1st data from line start */
  DCMIPP_P0_DEC_EVEN_1_OUT_4  = 0x02U << DCMIPP_P0PPCR_BSM_Pos,           /*!< 1 byte out of 4 starting from 1st data from line start (8 bit parallel mode only) */
  DCMIPP_P0_DEC_EVEN_2_OUT_4  = 0x03U << DCMIPP_P0PPCR_BSM_Pos,           /*!< 2 byte out of 4 starting from 1st data from line start (8 bit parallel mode only) */
  DCMIPP_P0_DEC_ODD_1_OUT_2   = DCMIPP_P0PPCR_OEBS | DCMIPP_P0_DEC_EVEN_1_OUT_2,            /*!< 1 data out of 2 starting from 2nd data from line start */
  DCMIPP_P0_DEC_ODD_1_OUT_4   = DCMIPP_P0PPCR_OEBS | DCMIPP_P0_DEC_EVEN_1_OUT_4,            /*!< 1 byte out of 4 starting from 2nd data from line start (8 bit parallel mode only) */
  DCMIPP_P0_DEC_ODD_2_OUT_4   = DCMIPP_P0PPCR_OEBS | DCMIPP_P0_DEC_EVEN_2_OUT_4,            /*!< 2 byte out of 4 starting from 2nd data from line start (8 bit parallel mode only) */
} HAL_DCMIPP_P0_BytesDecimationModeTypeDef;

/** @defgroup DCMIPP_hal_frame_rate  DCMIPP Frame rates
* @{
*/
typedef enum {
  DCMIPP_PIXEL_FRAME_RATE_ALL       = 0x00U << DCMIPP_P0FCTCR_FRATE_Pos, /*<! All frames captured     */
  DCMIPP_PIXEL_FRAME_RATE_1_OVER_2  = 0x01U << DCMIPP_P0FCTCR_FRATE_Pos, /*<! 1 frame over 2 captured */
  DCMIPP_PIXEL_FRAME_RATE_1_OVER_4  = 0x02U << DCMIPP_P0FCTCR_FRATE_Pos, /*<! 1 frame over 4 captured */
  DCMIPP_PIXEL_FRAME_RATE_1_OVER_8  = 0x03U << DCMIPP_P0FCTCR_FRATE_Pos, /*<! 1 frame over 8 captured */
} DCMIPP_PixelFrameRateTypeDef;

/**
  * @}
  */


/**
  * @}
  */
/** @defgroup DCMIPP_hal_P0D DCMIPP Pipe1 Decimation (DEPRECATED)
* @{
*/
typedef enum {
  DCMIPP_P1_VDEC_ALL        = 0x00U << DCMIPP_P1DECR_VDEC_Pos,                 /*!< All lines captured  */
  DCMIPP_P1_VDEC_1_OUT_2    = 0x01U << DCMIPP_P1DECR_VDEC_Pos,
  DCMIPP_P1_VDEC_1_OUT_4    = 0x02U << DCMIPP_P1DECR_VDEC_Pos,
  DCMIPP_P1_VDEC_1_OUT_8    = 0x03U << DCMIPP_P1DECR_VDEC_Pos,
} HAL_DCMIPP_P1_VerticalDecimationModeTypeDef;

typedef enum {
  DCMIPP_P1_HDEC_ALL     = 0x00U << DCMIPP_P1DECR_HDEC_Pos,   /*!< All pixels captured       */
  DCMIPP_P1_HDEC_1_OUT_2 = 0x01U << DCMIPP_P1DECR_HDEC_Pos,
  DCMIPP_P1_HDEC_1_OUT_4 = 0x02U << DCMIPP_P1DECR_HDEC_Pos,
  DCMIPP_P1_HDEC_1_OUT_8 = 0x03U << DCMIPP_P1DECR_HDEC_Pos,
} HAL_DCMIPP_P1_HorizontalDecimationModeTypeDef;


/**
  * @}
  */
/** @defgroup DCMIPP_hal_P0D DCMIPP Pipex Decimation
* @{
*/
typedef enum {
  DCMIPP_VDEC_ALL        = 0x00U << DCMIPP_P1DECR_VDEC_Pos,                 /*!< All lines captured  */
  DCMIPP_VDEC_1_OUT_2    = 0x01U << DCMIPP_P1DECR_VDEC_Pos,
  DCMIPP_VDEC_1_OUT_4    = 0x02U << DCMIPP_P1DECR_VDEC_Pos,
  DCMIPP_VDEC_1_OUT_8    = 0x03U << DCMIPP_P1DECR_VDEC_Pos,
} HAL_DCMIPP_VerticalDecimationModeTypeDef;

typedef enum {
  DCMIPP_HDEC_ALL     = 0x00U << DCMIPP_P1DECR_HDEC_Pos,   /*!< All pixels captured       */
  DCMIPP_HDEC_1_OUT_2 = 0x01U << DCMIPP_P1DECR_HDEC_Pos,
  DCMIPP_HDEC_1_OUT_4 = 0x02U << DCMIPP_P1DECR_HDEC_Pos,
  DCMIPP_HDEC_1_OUT_8 = 0x03U << DCMIPP_P1DECR_HDEC_Pos,
} HAL_DCMIPP_HorizontalDecimationModeTypeDef;

typedef enum {
  DCMIPP_ISP_DECIMATION        = 0x00U,  /*!< Decimation bloc located within the ISP stage */
#ifndef STM32MP25XX_SI_CUT1_X
  DCMIPP_POSTPROC_DECIMATION   = 0x01U,  /*!< Decimation bloc located within the PostProc stage */
#endif
} HAL_DCMIPP_DecimationBlocTypeDef;

/**
  * @}
  */
  
/** @defgroup DCMIPP_hal_format  DCMIPP Pixel Packer Format
* @{
*/
typedef enum {
  DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1 = 0x00U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< RGB888 or YUV422 1-buffer                         */
  DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1        = 0x01U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< RGB565 1-buffer                                   */
  DCMIPP_PIXEL_PACKER_FORMAT_ARGB8888        = 0x02U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< ARGB888 (with A=0xff)                             */
  DCMIPP_PIXEL_PACKER_FORMAT_RGBA888         = 0x03U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< RGBA888 (with A=0xff)                             */
  DCMIPP_PIXEL_PACKER_FORMAT_MONO_Y8_G8_1    = 0x04U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< Monochrome Y8 or G8 1-buffer                      */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV444_1        = 0x05U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV444 1-buffer (32bpp, FourCC=AYUV, with A=0xff) */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV422_1        = 0x06U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV422 1-buffer (16bpp, FourCC=YUYV)              */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV422_YUYV_1   = 0x06U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV422 1-buffer (16bpp, FourCC=YUYV)              */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV422_2        = 0x07U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV422 2-buffer (16bpp, FourCC=none)              */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV420_2        = 0x08U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV420 2-buffer (12bpp, FourCC=NV21)              */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV420_3        = 0x09U << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV420 3-buffer (12bpp, FourCC=YV12)              */
  DCMIPP_PIXEL_PACKER_FORMAT_YUV422_UYVY_1   = 0x0aU << DCMIPP_P1PPCR_FORMAT_Pos, /*!< YUV422 1-buffer (16bpp, FourCC=UYVY)              */
} DCMIPP_PixelPackerFormatTypeDef;
/**
  * @}
  */
  
/** @defgroup DCMIPP_hal_stat_extraction  DCMIPP Statistics extraction
* @{
*/
typedef enum
{
  HAL_DCMIPP_STAT_MODE_AVERAGE = 0x00U << DCMIPP_P1ST1CR_MODE_Pos,
  HAL_DCMIPP_STAT_MODE_BINS =    0x01U << DCMIPP_P1ST1CR_MODE_Pos,
} HAL_DCMIPP_StatMode;

typedef enum
{
  HAL_DCMIPP_STAT_SOURCE_PRE_BLKLVL_R = 0x00U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_PRE_BLKLVL_G = 0x01U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_PRE_BLKLVL_B = 0x02U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_PRE_BLKLVL_L = 0x03U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_POST_DEMOS_R = 0x04U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_POST_DEMOS_G = 0x05U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_POST_DEMOS_B = 0x06U << DCMIPP_P1ST1CR_SRC_Pos,
  HAL_DCMIPP_STAT_SOURCE_POST_DEMOS_L = 0x07U << DCMIPP_P1ST1CR_SRC_Pos,
} HAL_DCMIPP_StatSource;

typedef enum
{
  HAL_DCMIPP_STAT_BINS_BINS_LOWER =  0x00U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_BINS_LOWMID = 0x01U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_BINS_UPMID =  0x02U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_BINS_UP =     0x03U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_AVER_ALL =     0x00U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_AVER_NOEXT16 = 0x01U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_AVER_NOEXT32 = 0x02U << DCMIPP_P1ST1CR_BINS_Pos,
  HAL_DCMIPP_STAT_BINS_AVER_NOEXT64 = 0x03U << DCMIPP_P1ST1CR_BINS_Pos,
} HAL_DCMIPP_StatBins;

typedef struct
{
  HAL_DCMIPP_StatMode mode;
  HAL_DCMIPP_StatSource src;
  HAL_DCMIPP_StatBins bins;
} HAL_DCMIPP_StatConfigTypeDef;

typedef struct
{
  uint32_t shift;
  uint32_t multiply;
} HAL_DCMIPP_ComponentExposureTypeDef;

typedef struct
{
  HAL_DCMIPP_ComponentExposureTypeDef R;
  HAL_DCMIPP_ComponentExposureTypeDef G;
  HAL_DCMIPP_ComponentExposureTypeDef B;
} HAL_DCMIPP_ExposureControlTypeDef;

typedef struct
{
  uint8_t LUM_0;
  uint8_t LUM_32;
  uint8_t LUM_64;
  uint8_t LUM_96;
  uint8_t LUM_128;
  uint8_t LUM_160;
  uint8_t LUM_192;
  uint8_t LUM_224;
  uint8_t LUM_256;
} HAL_DCMIPP_ContrastControlTypeDef;

/** @defgroup DCMIPP_hal_format  DCMIPP Format generated by the Test Pattern Generator
* @{
*/
enum {
  DCMIPP_TPG_FORMAT_YUV422         = 0x1EU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< YUV422  */
  DCMIPP_TPG_FORMAT_RGB565         = 0x22U << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RGB565  */
  DCMIPP_TPG_FORMAT_RGB888         = 0x24U << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RGB888  */
  DCMIPP_TPG_FORMAT_RAW8           = 0x2AU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RAW8    */
  DCMIPP_TPG_FORMAT_RAW10          = 0x2BU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RAW10   */
  DCMIPP_TPG_FORMAT_RAW12          = 0x2CU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RAW12   */
  DCMIPP_TPG_FORMAT_RAW14          = 0x2DU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< RAW14   */
  DCMIPP_TPG_FORMAT_MONOCHROME_8B  = 0x4AU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< 8-bits  */
  DCMIPP_TPG_FORMAT_MONOCHROME_10B = 0x4BU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< 10-bits */
  DCMIPP_TPG_FORMAT_MONOCHROME_12B = 0x4CU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< 12-bits */
  DCMIPP_TPG_FORMAT_MONOCHROME_14B = 0x4DU << DCMIPP_CMTPGCR2_FORMAT_Pos, /*!< 14-bits */
};

/** @defgroup DCMIPP_TPG_RT  DCMIPP Raw Bayer format
* @{
*/
enum {
  DCMIPP_TPG_RT_RGGB         = 0x0U << DCMIPP_CMTPGCR2_RT_Pos, /*!< RGGB  */
  DCMIPP_TPG_RT_GRBG         = 0x1U << DCMIPP_CMTPGCR2_RT_Pos, /*!< GRBG  */
  DCMIPP_TPG_RT_GBRG         = 0x2U << DCMIPP_CMTPGCR2_RT_Pos, /*!< GBRG  */
  DCMIPP_TPG_RT_BGGR         = 0x3U << DCMIPP_CMTPGCR2_RT_Pos, /*!< BGGR  */
};

/** @defgroup DCMIPP_TPG_YT  DCMIPP YUV Type format
* @{
*/
enum {
  DCMIPP_TPG_YT_BT601_FULL         = 0x0U << DCMIPP_CMTPGCR2_YT_Pos, /*!< BT.601 Full range  */
  DCMIPP_TPG_YT_BT709_FULL         = 0x1U << DCMIPP_CMTPGCR2_YT_Pos, /*!< BT.709 Full range  */
  DCMIPP_TPG_YT_BT601_REDUCED      = 0x2U << DCMIPP_CMTPGCR2_YT_Pos, /*!< BT.601 Reduced range  */
  DCMIPP_TPG_YT_BT709_REDUCED      = 0x3U << DCMIPP_CMTPGCR2_YT_Pos, /*!< BT.709 Reduced range  */
};

#define DCMIPP_TPG_SQUARE    (0 << DCMIPP_CMTPGCR2_PATTERN_Pos)
#define DCMIPP_TPG_COLORBAR  (1 << DCMIPP_CMTPGCR2_PATTERN_Pos)
#define DCMIPP_TPG_COLOR     (0 << DCMIPP_CMTPGCR2_GSEN_Pos)
#define DCMIPP_TPG_GREYSCALE (1 << DCMIPP_CMTPGCR2_GSEN_Pos)
#define DCMIPP_TPG_LIFELINE  (1 << DCMIPP_CMTPGCR2_LFLEN_Pos)

/**
  * @}
  */

/** @defgroup DCMIPP_hal_parallel_if_swap  Swap R/U and B/V
* @{
*/
#define DCMIPP_PARALLEL_IF_SWAPRB  ((1 << DCMIPP_CMCR_SWAPRB_Pos) & DCMIPP_CMCR_SWAPRB_Msk) /*<! Swap R/U and B/V */

/**
  * @}
  */

/** @defgroup DCMIPP_hal_parallel_if_swap_cycle  swap data from cycle 0 vs cycle 1, for pixels received on 2 cycles.
* @{
*/
#define DCMIPP_PARALLEL_IF_SWAPCYCLES  ((1 << DCMIPP_PRCR_SWAPCYCLES_Pos) & DCMIPP_PRCR_SWAPCYCLES_Msk) /*<! swap data from cycle 0 vs cycle 1 */

/**
  * @}
  */
  
/** @defgroup DCMIPP_hal_parallel_if_swap_bits  swap lsb vs msb within each received component
* @{
*/
#define DCMIPP_PARALLEL_IF_SWAPBITS  ((1 << DCMIPP_PRCR_SWAPBITS_Pos) & DCMIPP_PRCR_SWAPBITS_Msk) /*<! swap lsb vs msb within each received component */

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/

/** @defgroup DCMIPP_Exported_Macros DCMIPP Exported Macros
  * @{
  */

/**
  * @}
  */

/* Exported types ------------------------------------------------------------*/

/** @defgroup DCMIPP_Exported_Types DCMIPP Exported Types
  * @{
  */

/**
 * @brief  HAL DCMIPP modes (DCMI or CSI): modes are exclusive
 */
typedef enum
{
  HAL_DCMIPP_MODE_DCMI             = 0x00U,  /*!< DCMIPP DCMI mode */
  HAL_DCMIPP_MODE_CSI              = 0x01U,  /*!< DCMIPP CSI mode  */
} HAL_DCMIPP_ModeTypeDef;

/**
* @brief  HAL DCMIPP Pipe naming
*/
typedef enum
{
  HAL_DCMIPP_DUMP_PIPE             = 0x00U,  /*!< DCMIPP Dump pipe                 */
  HAL_DCMIPP_MAIN_PIPE             = 0x01U,  /*!< DCMIPP Main pipe                 */
  HAL_DCMIPP_ANCILLARY_PIPE        = 0x02U,  /*!< DCMIPP Ancillary pipe            */
  HAL_DCMIPP_NUM_OF_PIPES          = 0x03U   /*!< DCMIPP number of pipes available */
} HAL_DCMIPP_PipeTypeDef;

/**
   * @brief  HAL DCMIPP Capture mode
   */
typedef enum
{
  HAL_DCMIPP_MODE_SNAPSHOT              = 0x00U,  /*!< DCMIPP snapshot mode             */
  HAL_DCMIPP_MODE_CONTINUOUS            = 0x01U,  /*!< DCMIPP continuous mode (preview) */
} HAL_DCMIPP_CaptureModeTypeDef;

/**
  * @brief  HAL DCMIPP State structures definition
  */
typedef enum
{
  HAL_DCMIPP_STATE_RESET             = 0x00U,  /*!< DCMIPP not yet initialized or disabled  */
  HAL_DCMIPP_STATE_READY             = 0x01U,  /*!< DCMIPP initialized and ready for use    */
  HAL_DCMIPP_STATE_BUSY              = 0x02U,  /*!< DCMIPP internal processing is ongoing   */
  HAL_DCMIPP_STATE_TIMEOUT           = 0x03U,  /*!< DCMIPP timeout state                    */
  HAL_DCMIPP_STATE_ERROR             = 0x04U,  /*!< DCMIPP error state                      */
  HAL_DCMIPP_STATE_SUSPENDED         = 0x05U   /*!< DCMIPP suspend state                    */
} HAL_DCMIPP_StateTypeDef;

/**
  * @brief  HAL DCMIPP Pipe State structures definition
  */

typedef enum
{
  HAL_DCMIPP_PIPE_STATE_RESET             = 0x00U,  /*!< DCMIPP not yet initialized or disabled     */
  HAL_DCMIPP_PIPE_STATE_READY             = 0x01U,  /*!< DCMIPP initialized and ready for use       */
  HAL_DCMIPP_PIPE_STATE_BUSY              = 0x02U,  /*!< DCMIPP internal processing is ongoing      */
  HAL_DCMIPP_PIPE_STATE_STOP              = 0x03U,  /*!< DCMIPP pipe stopped or stop requested      */
  HAL_DCMIPP_PIPE_STATE_SUSPEND           = 0x04U,  /*!< DCMIPP pipe suspended or suspend requested */
} HAL_DCMIPP_PipeStateTypeDef;

/**
  * @brief  HAL DCMIPP Pipe Lock structures definition
  */

typedef struct
{
  HAL_LockTypeDef Lock;  /*!< DCMIPP pipe's locking object */
} HAL_DCMIPP_PipeLockTypeDef;

/**
  * @brief  HAL DCMIPP Frame Counter Reset Condition definition
  */

typedef enum {
  HAL_DCMIPP_FC_RESET_NONE            = 0x00U,  /* No automatic reset */
  HAL_DCMIPP_FC_RESET_PIPE_CHG        = 0x01U,  /* Reset on Pipe change */
  HAL_DCMIPP_FC_RESET_PIPE_OR_VC_CHG  = 0x02U,  /* Reset on Pipe or Virtual Channel change */
  HAL_DCMIPP_FC_RESET_VC_CHG          = 0x03U   /* Reset on Virtual Channel change */
} HAL_DCMIPP_FCResetTypedef;

/**********************************************************************************************/
/*                                  Pixel Packer definitions                                  */
/**********************************************************************************************/

/**
  * @brief  HAL DCMIPP Pixel Packer structures definition
  */
typedef struct
{
  uint32_t *pDestinationMemory0; /*!< Pointer to Destination Memory address 0 (All Pipes)      */
  uint32_t *pDestinationMemory1; /*!< Pointer to Destination Memory address 1 (Main Pipe only) */
  uint32_t *pDestinationMemory2; /*!< Pointer to Destination Memory address 2 (Main Pipe only) */
  uint32_t Pitch;              /*!< Number of bytes between the address of 2 consecutive lines */
  DCMIPP_PixelPackerFormatTypeDef Format;        /*!< Format used by the pixel packer                            */
  DCMIPP_MultiLineTypeDef MultiLine;		 /*!< MultiLine interrupt configuration        */
  uint32_t SwapRB;             /*!< Indicate that R/B (in RGB) or U/V (in YUV) should be swapped */
  uint32_t HeaderEN;           /*!< Indicate that the CSI2 header should be kept within data captured by Dump pipe */
  uint32_t PAD;                /*!< Indicate that data should be padded to the MSB instead of LSB */
} DCMIPP_PixelPackerTypeDef;

/**********************************************************************************************/
/*                                  Pipe definitions                                          */
/**********************************************************************************************/

typedef enum {
  HAL_DCMIPP_PIPE_SRC_INDEPENDANT     = 0x00U,
  HAL_DCMIPP_PIPE_SRC_SHARED          = 0x01U,
} HAL_DCMIPP_Pipe_Source;

/**
  * @brief  HAL DCMIPP  Pipe configuration structure definition
  */
typedef struct
{
  DCMIPP_PixelFrameRateTypeDef FrameRate;        /*!< Frame capture rate                                                            */
  uint32_t VcDtMode;                             /*!< Flow mode selection                                                           */
  uint32_t VcDtIdA;                              /*!< Flow selection ID A                                                           */
  uint32_t VcDtIdB;                              /*!< Flow selection ID B                                                           */
  uint32_t VcId;                                 /*!< Virtual Channel                                                               */
  HAL_DCMIPP_Pipe_Source PipeSrc;                /*!< Pipe source (Main pipe or standalone)                                         */
  DCMIPP_PixelPackerTypeDef PixelPacker;         /*!< Pixel Packer                                                                  */
  uint32_t Limit;                                /*!< Maximum number of bytes to be transmitted in a frame (only for the dump pipe) */
} DCMIPP_ConfPipeTypeDef;

/**********************************************************************************************/
/*                                  Pixel/Image processing                                    */
/**********************************************************************************************/

/**
  * @brief  HAL DCMIPP Embedded Synchronization codes (CCIR656)
  */
typedef struct
{
  uint32_t VerticalDivFactor;
  uint32_t HorizontalDivFactor;
  uint32_t VerticalRatio;
  uint32_t HorizontalRatio;
  uint32_t VerticalSize;
  uint32_t HorizontalSize;
} DCMIPP_DownsizeTypeDef;

/**
  * @brief  HAL DCMIPP Gamma Conversion
  */
  
/**
   * @brief  HAL DCMIPP RGB shift value
   */
typedef enum
{
  HAL_DCMIPP_MULTIPY_BY_1 = 0x00U,  /*!< DCMIPP multiply R/G/B by 1 */
  HAL_DCMIPP_MULTIPY_BY_2 = 0x01U,  /*!< DCMIPP multiply R/G/B by 2 */
  HAL_DCMIPP_MULTIPY_BY_4 = 0x02U,  /*!< DCMIPP multiply R/G/B by 4 */
  HAL_DCMIPP_MULTIPY_BY_8 = 0x03U,  /*!< DCMIPP multiply R/G/B by 8 */
} HAL_DCMIPP_RGBShiftedValue;

/**
   * @brief  RGB component slope/offset
   */
typedef enum
{
  HAL_DCMIPP_R = 0x00U, /* !< DCMIPP R component */
  HAL_DCMIPP_G = 0x01U, /* !< DCMIPP G component */
  HAL_DCMIPP_B = 0x02U, /* !< DCMIPP B component */
  HAL_DCMIPP_NUM_OF_COMPONENTS = 0x03U,
} HAL_DCMIPP_RGB_TypeDef;

typedef struct
{
  uint32_t Slope;  /* <! Slope for a component (from 0 to 63)   */
  uint32_t Offset; /* <! Offset for a component (from 0 to 127) */
} DCMIPP_GammaCorrectionTypeDef;

typedef struct
{
  DCMIPP_GammaCorrectionTypeDef Segment0;  /* <! Segment #0 */
  DCMIPP_GammaCorrectionTypeDef Segment1;  /* <! Segment #1 */
  DCMIPP_GammaCorrectionTypeDef Segment2;  /* <! Segment #2 */
} DCMIPP_GammaCorrectionSegmentTypeDef;

/**
   * @brief  Gamma Conversion parameters
   */

typedef struct
{
  HAL_DCMIPP_RGBShiftedValue ShiftR;                                                  /* !< Shift R component                    */
  HAL_DCMIPP_RGBShiftedValue ShiftG;                                                  /* !< Shift G component                    */
  HAL_DCMIPP_RGBShiftedValue ShiftB;                                                  /* !< Shift B component                    */
  DCMIPP_GammaCorrectionSegmentTypeDef GammaCorrection[HAL_DCMIPP_NUM_OF_COMPONENTS]; /* <! Gamma Correction for each components */
} DCMIPP_GammaConversionTypeDef;

/**
   * @brief  RawBayer to RGB parameters
   */
   
typedef enum
{
  DCMIPP_RAWBAYER_ALGO_NONE = 0x00U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_3 = 0x01U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_4 = 0x02U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_6 = 0x03U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_8 = 0x04U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_12 = 0x05U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_16 = 0x06U,
  DCMIPP_RAWBAYER_ALGO_STRENGTH_24 = 0x07U,
} DCMIPP_RawBayerAlgoStrengthTypeDef;

typedef enum
{
  DCMIPP_RAWBAYER_RGGB = 0x00U,
  DCMIPP_RAWBAYER_GRBG = 0x01U,
  DCMIPP_RAWBAYER_GBRG = 0x02U,
  DCMIPP_RAWBAYER_BGGR = 0x03U,
} DCMIPP_RawBayerTypeTypeDef;
   
typedef struct
{
  DCMIPP_RawBayerTypeTypeDef Type;          /* <! Raw Bayer type                            */
  DCMIPP_RawBayerAlgoStrengthTypeDef Peak;  /* <! Strength of the peak detection            */
  DCMIPP_RawBayerAlgoStrengthTypeDef LineV; /* <! Strength of the vertical line detection   */
  DCMIPP_RawBayerAlgoStrengthTypeDef LineH; /* <! Strength of the horizontal line detection */
  DCMIPP_RawBayerAlgoStrengthTypeDef Edge;  /* <! Strength of the edge detection            */
} DCMIPP_RawBayer2RGBTypeDef;

/**
   * @brief  Color Conversion parameters
   */
typedef struct
{
  uint32_t Clamp;  /* <! Clamp the output samples depending on type        */
  uint32_t Type;   /* <! Output samples type used while clamp is activated */
  int16_t RR;     /* <! Coefficient Row 1 Column 1 of the matrix          */
  int16_t RG;     /* <! Coefficient Row 1 Column 2 of the matrix          */
  int16_t RB;     /* <! Coefficient Row 1 Column 3 of the matrix          */
  int16_t RA;     /* <! Coefficient Row 1 of the added Column             */
  int16_t GR;     /* <! Coefficient Row 2 Column 1 of the matrix          */
  int16_t GG;     /* <! Coefficient Row 2 Column 2 of the matrix          */
  int16_t GB;     /* <! Coefficient Row 2 Column 3 of the matrix          */
  int16_t GA;     /* <! Coefficient Row 2 of the added Column             */
  int16_t BR;     /* <! Coefficient Row 3 Column 1 of the matrix          */
  int16_t BG;     /* <! Coefficient Row 3 Column 2 of the matrix          */
  int16_t BB;     /* <! Coefficient Row 3 Column 3 of the matrix          */
  int16_t BA;     /* <! Coefficient Row 3 of the added Column             */
} DCMIPP_ColorConversionTypeDef;

/**
   * @brief  Black Level parameters
   */
typedef struct
{
  uint8_t BLCR;  /* <! Black value register to red component */
  uint8_t BLCG;  /* <! Black value register to green component */
  uint8_t BLCB;  /* <! Black value register to blue component */
} DCMIPP_BlackLevelTypeDef;

/**
  * @brief  HAL DCMIPP Test Pattern Generator configuration structure definition
  */
typedef struct
{
  uint32_t Width;
  uint32_t Height;
  uint16_t VBL;
  uint32_t Mode;
} DCMIPP_TPGConfigTypeDef;

/**********************************************************************************************/
/*                                  DCMIPP HAL Control / Init definitions                     */
/**********************************************************************************************/

/**
  * @brief  HAL DCMIPP Embedded Synchronization codes (CCIR656)
  */
typedef struct
{
  uint8_t FEC;
  uint8_t LEC;
  uint8_t LSC;
  uint8_t FSC;
} DCMIPP_EmbeddedSyncCodesTypeDef;

/**
  * @brief  HAL DCMIPP Embedded Synchronization Unmask (CCIR656)
  */
typedef struct
{
  uint8_t FEU;
  uint8_t LEU;
  uint8_t LSU;
  uint8_t FSU;
} DCMIPP_EmbeddedSyncUnmaskTypeDef;

/**
  * @brief  HAL DCMIPP Control structures definition
  */
typedef struct
{
  DCMIPP_InputFormatTypeDef Format;                     /*!< YUV, RGB, RAW , monochrome          */
  DCMIPP_VPolarityTypeDef VPolarity;                    /*!< Vertical synchronization polarity   */
  DCMIPP_HPolarityTypeDef HPolarity;                    /*!< Horizontal synchronization polarity */
  DCMIPP_PClkPolarityTypeDef PCKPolarity;               /*!< Pixel Clock polarity                */
  DCMIPP_InterfaceTypeDef ExtendedDataMode;             /*!< Extended Data Mode                  */
  uint32_t EmbeddedSynchro;                             /*!< Embedded Synchronization select     */
  DCMIPP_EmbeddedSyncCodesTypeDef EmbeddedSynchroCodes; /*!< Embedded Synchro codes values       */
  uint32_t SwapRB;                                      /*!< Swap R/U and B/V                    */
  uint32_t SwapCycles;                                  /*!< Swap Data from cycle 0 vs cycle 1   */
  uint32_t SwapBits;                                    /*!< Swap lsb vs msb                     */
} DCMIPP_InitDCMIControlTypeDef;



/**
  * @brief  HAL DCMIPP initialisation  structures definition
  */
typedef struct
{
  DCMIPP_InitDCMIControlTypeDef DCMIControl;      /*!< DCMIPP Main configuration */
                                                  /*<! TODO: Add CSI config      */
} DCMIPP_InitTypeDef;

/**
  * @brief  HAL DCMIPP handle structures definition
  */
typedef struct __DCMIPP_HandleTypeDef
{
  DCMIPP_TypeDef                    *Instance;                            /*!< DCMIPP registers base address */
  DCMIPP_InitTypeDef                Init;                                 /*!< DCMIPP parameters             */
  HAL_DCMIPP_PipeLockTypeDef        *p_Lock[HAL_DCMIPP_NUM_OF_PIPES];     /*!< DCMIPP Lock Object            */
  __IO HAL_DCMIPP_StateTypeDef      State;                                /*!< DCMIPP state                  */
  HAL_DCMIPP_ModeTypeDef            Mode;                                 /*!< DCMIPP mode                   */
  __IO HAL_DCMIPP_PipeStateTypeDef  a_PipeState[HAL_DCMIPP_NUM_OF_PIPES]; /*!< DCMIPP Pipes state            */
  __IO uint32_t                     ErrorCode;                            /*!< DCMIPP Error code             */
} DCMIPP_HandleTypeDef;


/* Exported functions --------------------------------------------------------*/
/** @addtogroup DCMIPP_Exported_Functions DCMI Exported Functions
  * @{
  */

/** @addtogroup DCMI_Exported_Functions_Group1 Initialization and Configuration functions
 * @{
 */
/* Initialization and de-initialization functions *****************************/
HAL_StatusTypeDef HAL_DCMIPP_Init(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_DeInit(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_MspInit(DCMIPP_HandleTypeDef *pDcmipp);
void HAL_DCMIPP_MspDeInit(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_ConfigPipe(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_ConfPipeTypeDef *pConfPipe, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Config(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe,
                                                 HAL_DCMIPP_FCResetTypedef Reset);
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Reset(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_FrameCounter_Read(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCounter);

/**
  * @}
  */

/** @addtogroup DCMIPP_Exported_Functions_Group2 IO operation functions
 * @{
 */
HAL_StatusTypeDef HAL_DCMIPP_Start(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_CaptureModeTypeDef CaptureMode);
HAL_StatusTypeDef HAL_DCMIPP_Stop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_Get_Pipe0_Transfered_size(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCounter);
HAL_StatusTypeDef HAL_DCMIPP_ConfigSyncUnmask(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_EmbeddedSyncUnmaskTypeDef *SyncUnmask);


/**
  * @}
  */

/** @addtogroup DCMIPP_Exported_Functions_Group3 callback functions
* @{
*/
void HAL_DCMIPP_FrameEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_VsyncEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_LineEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_LimitEventDumpPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_FrameEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_VsyncEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_LineEventMainPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_FrameEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_LineEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_VsyncEventAncillaryPipeCallback(DCMIPP_HandleTypeDef *pHdcmipp);
void HAL_DCMIPP_SyncErrorEventCallback(DCMIPP_HandleTypeDef *pHdcmipp);

/* Callbacks Register/UnRegister functions  ***********************************/
#if (USE_HAL_DCMIPP_REGISTER_CALLBACKS == 1)
#endif /* USE_HAL_DCMIPP_REGISTER_CALLBACKS */
/**
  * @}
  */

/** @addtogroup DCMIPP_Exported_Functions_Group4 Peripheral State functions
* @{
*/
/* Peripheral State functions *************************************************/
HAL_DCMIPP_StateTypeDef HAL_DCMIPP_GetState(DCMIPP_HandleTypeDef *pHdcmipp);
uint32_t HAL_DCMIPP_GetError(DCMIPP_HandleTypeDef *pHdcmipp);
/**
  * @}
  */

/** @addtogroup DCMIPP_Exported_Functions_Group5 IRQ handler function
* @{
*/
/* IRQ handler function *************************************************/
void HAL_DCMIPP_IRQHandler(DCMIPP_HandleTypeDef *pHdcmipp);
/**
  * @}
  */

/** @addtogroup DCMIPP_Exported_Functions_Group5 Image processing functions
* @{
*/
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_ConfigCrop(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize, uint32_t In);
HAL_StatusTypeDef HAL_DCMIPP_ConfigCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize);
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_DisableCrop(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_DisableCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_Pipe0_EnableCrop(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_EnableCrop(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);

HAL_StatusTypeDef HAL_DCMIPP_Pipe0_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_P0_LinesDecimationModeTypeDef LinesMode, HAL_DCMIPP_P0_BytesDecimationModeTypeDef BytesMode);
HAL_StatusTypeDef HAL_DCMIPP_ConfigIPPlug(DCMIPP_HandleTypeDef *pHdcmipp, int MemoryPage, int Client,
                                          int Traffic, int Outstanding, int DpregStart, int DpregEnd,
                                          int Wlru);
HAL_StatusTypeDef HAL_DCMIPP_Pipe1_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_P1_VerticalDecimationModeTypeDef vdec, HAL_DCMIPP_P1_HorizontalDecimationModeTypeDef hdec);
HAL_StatusTypeDef HAL_DCMIPP_ConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_DecimationBlocTypeDef Bloc, HAL_DCMIPP_VerticalDecimationModeTypeDef vdec, HAL_DCMIPP_HorizontalDecimationModeTypeDef hdec);
HAL_StatusTypeDef HAL_DCMIPP_Pipe1_GetConfigDecimation(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_P1_VerticalDecimationModeTypeDef *pvdec, HAL_DCMIPP_P1_HorizontalDecimationModeTypeDef *phdec, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_ConfigDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_DownsizeTypeDef *pDownsizeConfig);
HAL_StatusTypeDef HAL_DCMIPP_DisableDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_EnableDownsize(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);

HAL_StatusTypeDef HAL_DCMIPP_EnableGammaConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_DisableGammaConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);

HAL_StatusTypeDef HAL_DCMIPP_ConfigRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_RawBayer2RGBTypeDef *pRawBayer2RGBConfig);
HAL_StatusTypeDef HAL_DCMIPP_DisableRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_EnableRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigRawBayer2RGB(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_RawBayer2RGBTypeDef *pRawBayer2RGBConfig, uint8_t *pEnable);

HAL_StatusTypeDef HAL_DCMIPP_EnableStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t NbHeadLines, uint32_t NbValidLines);
HAL_StatusTypeDef HAL_DCMIPP_DisableStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pNbHeadLines, uint32_t *pNbValidLines, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_DisableBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp);
HAL_StatusTypeDef HAL_DCMIPP_EnableBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t Strengh);
HAL_StatusTypeDef HAL_DCMIPP_GetBadPixelCount(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pCount);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigBadPixelRemoval(DCMIPP_HandleTypeDef *pHdcmipp, uint32_t *pStrength, uint8_t *pEnable);


HAL_StatusTypeDef HAL_DCMIPP_ConfigColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_ColorConversionTypeDef *pColorConversionConfig);
HAL_StatusTypeDef HAL_DCMIPP_DisableColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_EnableColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigColorConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_ColorConversionTypeDef *pColorConversionConfig, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_ConfigYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_ColorConversionTypeDef *pColorConversionConfig);
HAL_StatusTypeDef HAL_DCMIPP_DisableYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_EnableYUVConversion(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);

HAL_StatusTypeDef HAL_DCMIPP_ConfigBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_BlackLevelTypeDef *pBlackLevelConfig);
HAL_StatusTypeDef HAL_DCMIPP_DisableBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_EnableBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigBlackLevelCalibration(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, DCMIPP_BlackLevelTypeDef *pBlackLevelConfig, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_ConfigStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId, HAL_DCMIPP_StatConfigTypeDef *pStatExtractionConfig);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId, HAL_DCMIPP_StatConfigTypeDef *pStatExtractionConfig, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_ConfigStatExtractionArea(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t X0, uint32_t Y0, uint32_t XSize, uint32_t YSize);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigStatExtractionArea(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t *pX0, uint32_t *pY0, uint32_t *pXSize, uint32_t *pYSize, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_EnableStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId);
HAL_StatusTypeDef HAL_DCMIPP_DisableStatExtraction(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId);
HAL_StatusTypeDef HAL_DCMIPP_GetStatAccuValue(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, uint32_t moduleId, uint32_t *pAccu);

HAL_StatusTypeDef HAL_DCMIPP_ConfigExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ExposureControlTypeDef *pExposureControl);
HAL_StatusTypeDef HAL_DCMIPP_EnableExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_DisableExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigExposureControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ExposureControlTypeDef *pExposureControl, uint8_t *pEnable);

HAL_StatusTypeDef HAL_DCMIPP_ConfigContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ContrastControlTypeDef *pContrastControl);
HAL_StatusTypeDef HAL_DCMIPP_EnableContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_DisableContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe);
HAL_StatusTypeDef HAL_DCMIPP_GetConfigContrastControl(DCMIPP_HandleTypeDef *pHdcmipp, HAL_DCMIPP_PipeTypeDef Pipe, HAL_DCMIPP_ContrastControlTypeDef *pContrastControl, uint8_t *pEnable);
HAL_StatusTypeDef HAL_DCMIPP_EnableTestPatternGenerator(DCMIPP_HandleTypeDef *pHdcmipp, DCMIPP_TPGConfigTypeDef *Config);
HAL_StatusTypeDef HAL_DCMIPP_DisableTestPatternGenerator(DCMIPP_HandleTypeDef *pHdcmipp);
/**
  * @}
  */

#endif /* DCMIPP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32MP2xx_HAL_DCMIPP_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
