/************************************************************************************//**
* \file         Demo/ARMCM3_STM32F2_Olimex_STM32P207_IAR/Prog/boot.c
* \brief        Demo program bootloader interface source file.
* \ingroup      Prog_ARMCM3_STM32F2_Olimex_STM32P207_IAR
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2018  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
* This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You have received a copy of the GNU General Public License along with OpenBLT. It
* should be located in ".\Doc\license.html". If not, contact Feaser to obtain a copy.
*
* \endinternal
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "header.h"                                    /* generic header               */


/****************************************************************************************
* Function prototypes
****************************************************************************************/
#if (BOOT_COM_UART_ENABLE > 0)
static void BootComUartInit(void);
static void BootComUartCheckActivationRequest(void);
#endif
#if (BOOT_COM_CAN_ENABLE > 0)
static void BootComCanInit(void);
static void BootComCanCheckActivationRequest(void);
#endif

/************************************************************************************//**
** \brief     Initializes the communication interface.
** \return    none.
**
****************************************************************************************/
void BootComInit(void)
{
#if (BOOT_COM_UART_ENABLE > 0)
  BootComUartInit();
#endif
#if (BOOT_COM_CAN_ENABLE > 0)
  BootComCanInit();
#endif
} /*** end of BootComInit ***/


/************************************************************************************//**
** \brief     Receives the CONNECT request from the host, which indicates that the
**            bootloader should be activated and, if so, activates it.
** \return    none.
**
****************************************************************************************/
void BootComCheckActivationRequest(void)
{
#if (BOOT_COM_UART_ENABLE > 0)
  BootComUartCheckActivationRequest();
#endif
#if (BOOT_COM_CAN_ENABLE > 0)
  BootComCanCheckActivationRequest();
#endif
} /*** end of BootComCheckActivationRequest ***/


/************************************************************************************//**
** \brief     Bootloader activation function.
** \return    none.
**
****************************************************************************************/
void BootActivate(void)
{
  /* perform software reset to activate the bootoader again */
  NVIC_SystemReset();
} /*** end of BootActivate ***/


#if (BOOT_COM_UART_ENABLE > 0)
/****************************************************************************************
*     U N I V E R S A L   A S Y N C H R O N O U S   R X   T X   I N T E R F A C E
****************************************************************************************/

/****************************************************************************************
* Macro definitions
****************************************************************************************/
/** \brief Timeout time for the reception of a CTO packet. The timer is started upon
 *         reception of the first packet byte.
 */
#define UART_CTO_RX_PACKET_TIMEOUT_MS (100u)


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief UART handle to be used in API calls. */
static UART_HandleTypeDef uartHandle;


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static unsigned char UartReceiveByte(unsigned char *data);


/************************************************************************************//**
** \brief     Initializes the UART communication interface.
** \return    none.
**
****************************************************************************************/
static void BootComUartInit(void)
{
  /* Configure UART peripheral. */
  uartHandle.Instance        = USART3;
  uartHandle.Init.BaudRate   = BOOT_COM_UART_BAUDRATE;
  uartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  uartHandle.Init.StopBits   = UART_STOPBITS_1;
  uartHandle.Init.Parity     = UART_PARITY_NONE;
  uartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  uartHandle.Init.Mode       = UART_MODE_TX_RX;
  /* Initialize the UART peripheral. */
  HAL_UART_Init(&uartHandle);
} /*** end of BootComUartInit ***/


/************************************************************************************//**
** \brief     Receives the CONNECT request from the host, which indicates that the
**            bootloader should be activated and, if so, activates it.
** \return    none.
**
****************************************************************************************/
static void BootComUartCheckActivationRequest(void)
{
  static unsigned char xcpCtoReqPacket[BOOT_COM_UART_RX_MAX_DATA+1];
  static unsigned char xcpCtoRxLength;
  static unsigned char xcpCtoRxInProgress = 0;
  static unsigned long xcpCtoRxStartTime = 0;

  /* start of cto packet received? */
  if (xcpCtoRxInProgress == 0)
  {
    /* store the message length when received */
    if (UartReceiveByte(&xcpCtoReqPacket[0]) == 1)
    {
      /* check that the length has a valid value. it should not be 0 */
      if ( (xcpCtoReqPacket[0] > 0) &&
           (xcpCtoReqPacket[0] <= BOOT_COM_UART_RX_MAX_DATA) )
      {
        /* store the start time */
        xcpCtoRxStartTime = TimerGet();
        /* indicate that a cto packet is being received */
        xcpCtoRxInProgress = 1;
        /* reset packet data count */
        xcpCtoRxLength = 0;
      }
    }
  }
  else
  {
    /* store the next packet byte */
    if (UartReceiveByte(&xcpCtoReqPacket[xcpCtoRxLength+1]) == 1)
    {
      /* increment the packet data count */
      xcpCtoRxLength++;

      /* check to see if the entire packet was received */
      if (xcpCtoRxLength == xcpCtoReqPacket[0])
      {
        /* done with cto packet reception */
        xcpCtoRxInProgress = 0;

        /* check if this was an XCP CONNECT command */
        if ((xcpCtoReqPacket[1] == 0xff) && (xcpCtoReqPacket[2] == 0x00))
        {
          /* connection request received so start the bootloader */
          BootActivate();
        }
      }
    }
    else
    {
      /* check packet reception timeout */
      if (TimerGet() > (xcpCtoRxStartTime + UART_CTO_RX_PACKET_TIMEOUT_MS))
      {
        /* cancel cto packet reception due to timeout. note that this automatically
         * discards the already received packet bytes, allowing the host to retry.
         */
        xcpCtoRxInProgress = 0;
      }
    }
  }
} /*** end of BootComUartCheckActivationRequest ***/


/************************************************************************************//**
** \brief     Receives a communication interface byte if one is present.
** \param     data Pointer to byte where the data is to be stored.
** \return    1 if a byte was received, 0 otherwise.
**
****************************************************************************************/
static unsigned char UartReceiveByte(unsigned char *data)
{
  HAL_StatusTypeDef result;

  /* receive a byte in a non-blocking manner */
  result = HAL_UART_Receive(&uartHandle, data, 1, 0);
  /* process the result */
  if (result == HAL_OK)
  {
    /* success */
    return 1;
  }
  /* error occurred */
  return 0;
} /*** end of UartReceiveByte ***/
#endif /* BOOT_COM_UART_ENABLE > 0 */


#if (BOOT_COM_CAN_ENABLE > 0)
/****************************************************************************************
*        C O N T R O L L E R   A R E A   N E T W O R K   I N T E R F A C E
****************************************************************************************/

/****************************************************************************************
* Type definitions
****************************************************************************************/
/** \brief Structure type for grouping CAN bus timing related information. */
typedef struct t_can_bus_timing
{
  unsigned char tseg1;                                /**< CAN time segment 1          */
  unsigned char tseg2;                                /**< CAN time segment 2          */
} tCanBusTiming;


/****************************************************************************************
* Local constant declarations
****************************************************************************************/
/** \brief CAN bittiming table for dynamically calculating the bittiming settings.
 *  \details According to the CAN protocol 1 bit-time can be made up of between 8..25
 *           time quanta (TQ). The total TQ in a bit is SYNC + TSEG1 + TSEG2 with SYNC
 *           always being 1. The sample point is (SYNC + TSEG1) / (SYNC + TSEG1 + SEG2) *
 *           100%. This array contains possible and valid time quanta configurations with
 *           a sample point between 68..78%.
 */
static const tCanBusTiming canTiming[] =
{                       /*  TQ | TSEG1 | TSEG2 | SP  */
                        /* ------------------------- */
    {  5, 2 },          /*   8 |   5   |   2   | 75% */
    {  6, 2 },          /*   9 |   6   |   2   | 78% */
    {  6, 3 },          /*  10 |   6   |   3   | 70% */
    {  7, 3 },          /*  11 |   7   |   3   | 73% */
    {  8, 3 },          /*  12 |   8   |   3   | 75% */
    {  9, 3 },          /*  13 |   9   |   3   | 77% */
    {  9, 4 },          /*  14 |   9   |   4   | 71% */
    { 10, 4 },          /*  15 |  10   |   4   | 73% */
    { 11, 4 },          /*  16 |  11   |   4   | 75% */
    { 12, 4 },          /*  17 |  12   |   4   | 76% */
    { 12, 5 },          /*  18 |  12   |   5   | 72% */
    { 13, 5 },          /*  19 |  13   |   5   | 74% */
    { 14, 5 },          /*  20 |  14   |   5   | 75% */
    { 15, 5 },          /*  21 |  15   |   5   | 76% */
    { 15, 6 },          /*  22 |  15   |   6   | 73% */
    { 16, 6 },          /*  23 |  16   |   6   | 74% */
    { 16, 7 },          /*  24 |  16   |   7   | 71% */
    { 16, 8 }           /*  25 |  16   |   8   | 68% */
};


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief CAN handle to be used in API calls. */
static CAN_HandleTypeDef canHandle;

/** \brief Message buffer for receiving CAN messages. */
static CanRxMsgTypeDef canRxMessage;


/************************************************************************************//**
** \brief     Search algorithm to match the desired baudrate to a possible bus
**            timing configuration.
** \param     baud The desired baudrate in kbps. Valid values are 10..1000.
** \param     prescaler Pointer to where the value for the prescaler will be stored.
** \param     tseg1 Pointer to where the value for TSEG2 will be stored.
** \param     tseg2 Pointer to where the value for TSEG2 will be stored.
** \return    1 if the CAN bustiming register values were found, 0 otherwise.
**
****************************************************************************************/
static unsigned char CanGetSpeedConfig(unsigned short baud, unsigned short *prescaler,
                                       unsigned char *tseg1, unsigned char *tseg2)
{
  unsigned char cnt;

  /* loop through all possible time quanta configurations to find a match */
  for (cnt=0; cnt < sizeof(canTiming)/sizeof(canTiming[0]); cnt++)
  {
    if (((BOOT_CPU_SYSTEM_SPEED_KHZ/4) % (baud*(canTiming[cnt].tseg1+canTiming[cnt].tseg2+1))) == 0)
    {
      /* compute the prescaler that goes with this TQ configuration */
      *prescaler = (BOOT_CPU_SYSTEM_SPEED_KHZ/4)/(baud*(canTiming[cnt].tseg1+canTiming[cnt].tseg2+1));

      /* make sure the prescaler is valid */
      if ( (*prescaler > 0) && (*prescaler <= 1024) )
      {
        /* store the bustiming configuration */
        *tseg1 = canTiming[cnt].tseg1;
        *tseg2 = canTiming[cnt].tseg2;
        /* found a good bus timing configuration */
        return 1;
      }
    }
  }
  /* could not find a good bus timing configuration */
  return 0;
} /*** end of CanGetSpeedConfig ***/


/************************************************************************************//**
** \brief     Initializes the CAN communication interface.
** \return    none.
**
****************************************************************************************/
static void BootComCanInit(void)
{
  unsigned short prescaler = 0;
  unsigned char tseg1 = 0, tseg2 = 0;
  CAN_FilterConfTypeDef filterConfig;
  unsigned long rxMsgId = BOOT_COM_CAN_RX_MSG_ID;
  unsigned long rxFilterId, rxFilterMask;

  /* obtain bittiming configuration information. */
  CanGetSpeedConfig(BOOT_COM_CAN_BAUDRATE/1000, &prescaler, &tseg1, &tseg2);

  /* set the CAN controller configuration. */
  canHandle.Instance = CAN1;
  canHandle.pTxMsg = NULL;
  canHandle.pRxMsg = &canRxMessage;
  canHandle.Init.TTCM = DISABLE;
  canHandle.Init.ABOM = DISABLE;
  canHandle.Init.AWUM = DISABLE;
  canHandle.Init.NART = DISABLE;
  canHandle.Init.RFLM = DISABLE;
  canHandle.Init.TXFP = DISABLE;
  canHandle.Init.Mode = CAN_MODE_NORMAL;
  canHandle.Init.SJW = CAN_SJW_1TQ;
  canHandle.Init.BS1 = ((unsigned long)tseg1 - 1) << CAN_BTR_TS1_Pos;
  canHandle.Init.BS2 = ((unsigned long)tseg2 - 1) << CAN_BTR_TS2_Pos;
  canHandle.Init.Prescaler = prescaler;
  /* initialize the CAN controller. this only fails if the CAN controller hardware is
   * faulty. no need to evaluate the return value as there is nothing we can do about
   * a faulty CAN controller.
   */
  (void)HAL_CAN_Init(&canHandle);
  /* determine the reception filter mask and id values such that it only leaves one
   * CAN identifier through (BOOT_COM_CAN_RX_MSG_ID).
   */
  if ((rxMsgId & 0x80000000) == 0)
  {
    rxFilterId = rxMsgId << CAN_RI0R_STID_Pos;
    rxFilterMask = (CAN_RI0R_STID_Msk) | CAN_RI0R_IDE;
  }
  else
  {
    /* negate the ID-type bit */
    rxMsgId &= ~0x80000000;
    rxFilterId = (rxMsgId << CAN_RI0R_EXID_Pos) | CAN_RI0R_IDE;
    rxFilterMask = (CAN_RI0R_EXID_Msk) | CAN_RI0R_IDE;
  }
  /* configure the reception filter. note that the implementation of this function
   * always returns HAL_OK, so no need to evaluate the return value.
   */
  if (canHandle.Instance == CAN1)
  {
    /* filter 0 is the first filter assigned to the bxCAN master (CAN1) */
    filterConfig.FilterNumber = 0;
  }
  else
  {
    /* filter 14 is the first filter assigned to the bxCAN slave (CAN2) */
    filterConfig.FilterNumber = 14;
  }
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  filterConfig.FilterIdHigh = (rxFilterId >> 16) & 0x0000FFFFu;
  filterConfig.FilterIdLow = rxFilterId & 0x0000FFFFu;
  filterConfig.FilterMaskIdHigh = (rxFilterMask >> 16) & 0x0000FFFFu;
  filterConfig.FilterMaskIdLow = rxFilterMask & 0x0000FFFFu;
  filterConfig.FilterFIFOAssignment = 0;
  filterConfig.FilterActivation = ENABLE;
  /* select the start slave bank number (for CAN1). this configuration assigns filter
   * banks 0..13 to CAN1 and 14..27 to CAN2.
   */
  filterConfig.BankNumber = 14;
  (void)HAL_CAN_ConfigFilter(&canHandle, &filterConfig);
} /*** end of BootComCanInit ***/


/************************************************************************************//**
** \brief     Receives the CONNECT request from the host, which indicates that the
**            bootloader should be activated and, if so, activates it.
** \return    none.
**
****************************************************************************************/
static void BootComCanCheckActivationRequest(void)
{
  unsigned long rxMsgId = BOOT_COM_CAN_RX_MSG_ID;
  unsigned char packetIdMatches = 0;

  /* poll for received CAN messages that await processing. */
  if (HAL_CAN_Receive(&canHandle, CAN_FIFO0, 0) == HAL_OK)
  {
    /* check if this message has the configured CAN packet identifier. */
    if ((rxMsgId & 0x80000000) == 0)
    {
      /* was an 11-bit CAN message received that matches? */
      if ( (canHandle.pRxMsg->StdId == rxMsgId) &&
           (canHandle.pRxMsg->IDE == CAN_ID_STD) )
      {
        /* set flag that a packet with a matching CAN identifier was received. */
        packetIdMatches = 1;
      }
    }
    else
    {
      /* negate the ID-type bit */
      rxMsgId &= ~0x80000000;
      /* was an 29-bit CAN message received that matches? */
      if ( (canHandle.pRxMsg->ExtId == rxMsgId) &&
           (canHandle.pRxMsg->IDE == CAN_ID_EXT) )
      {
        /* set flag that a packet with a matching CAN identifier was received. */
        packetIdMatches = 1;
      }
    }

    /* only continue if a packet with a matching CAN identifier was received. */
    if (packetIdMatches == 1)
    {
      /* check if this was an XCP CONNECT command */
      if ((canHandle.pRxMsg->Data[0] == 0xff) && (canHandle.pRxMsg->Data[1] == 0x00))
      {
        /* connection request received so start the bootloader */
        BootActivate();
      }
    }
  }
} /*** end of BootComCanCheckActivationRequest ***/
#endif /* BOOT_COM_CAN_ENABLE > 0 */


/*********************************** end of boot.c *************************************/