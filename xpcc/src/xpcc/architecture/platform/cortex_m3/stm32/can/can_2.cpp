// coding: utf-8
// ----------------------------------------------------------------------------
/* Copyright (c) 2011, Roboterclub Aachen e.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Roboterclub Aachen e.V. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ROBOTERCLUB AACHEN E.V. ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ROBOTERCLUB AACHEN E.V. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// ----------------------------------------------------------------------------
/*
 * WARNING: This file is generated automatically, do not edit!
 * Please modify the corresponding *.in file instead and rebuild this file. 
 */
// ----------------------------------------------------------------------------

#include <xpcc/debug/error_report.hpp>
#include <xpcc/architecture/driver/atomic/queue.hpp>
#include <xpcc/utils.hpp>

#include "../gpio.hpp"
#include "../error_code.hpp"

#include "can_2.hpp"
#include <xpcc_config.hpp>


#if defined(STM32F10X_CL) || defined(STM32F4XX)


// ----------------------------------------------------------------------------
// CAN bit timing register (CAN_BTR)
#define CAN_BTR_SJW_POS		24
#define CAN_BTR_TS2_POS		20
#define CAN_BTR_TS1_POS		16


// ----------------------------------------------------------------------------
namespace
{
	GPIO__INPUT(CanRxB12, B, 12);
	GPIO__OUTPUT(CanTxB13, B, 13);
	
	GPIO__INPUT(CanRxB5, B, 5);
	GPIO__OUTPUT(CanTxB6, B, 6);
}

// ----------------------------------------------------------------------------
#if STM32_CAN2_TX_BUFFER_SIZE > 0
xpcc::atomic::Queue<xpcc::can::Message, STM32_CAN2_TX_BUFFER_SIZE> txQueue;
#endif
#if STM32_CAN2_RX_BUFFER_SIZE > 0
xpcc::atomic::Queue<xpcc::can::Message, STM32_CAN2_RX_BUFFER_SIZE> rxQueue;
#endif

// ----------------------------------------------------------------------------
// Configure the mailbox to send a CAN message.
// Low level function called by sendMessage and by Tx Interrupt.
static void
sendMailbox(const xpcc::can::Message& message, uint32_t mailboxId)
{
	CAN_TxMailBox_TypeDef* mailbox = &CAN2->sTxMailBox[mailboxId];
	
	if (message.isExtended()) {
		mailbox->TIR = message.identifier << 3 | CAN_TI0R_IDE;
	}
	else {
		mailbox->TIR = message.identifier << 21;
	}
	
	if (message.isRemoteTransmitRequest()) {
		mailbox->TIR |= CAN_TI0R_RTR;
	}
	
	// Set up the DLC
	mailbox->TDTR = message.getLength();
	
	// Set up the data field (copy the 8x8-bits into two 32-bit registers)
	const uint8_t * ATTRIBUTE_MAY_ALIAS data = message.data;
	mailbox->TDLR = reinterpret_cast<const uint32_t *>(data)[0];
	mailbox->TDHR = reinterpret_cast<const uint32_t *>(data)[1];
	
	// Request transmission
	mailbox->TIR |= CAN_TI0R_TXRQ;
}

// ----------------------------------------------------------------------------
// Low level function to receive a message from mailbox.
// Called by Rx Interrupt or by getMessage.
static void
readMailbox(xpcc::can::Message& message, uint32_t mailboxId)
{
	CAN_FIFOMailBox_TypeDef* mailbox = &CAN2->sFIFOMailBox[mailboxId];
	
	uint32_t rir = mailbox->RIR;
	if (rir & CAN_RI0R_IDE) {
		message.identifier = rir >> 3;
		message.setExtended();
	}
	else {
		message.identifier = rir >> 21;
		message.setExtended(false);
	}
	message.setRemoteTransmitRequest(rir & CAN_RI0R_RTR);
	
	message.length = mailbox->RDTR & CAN_TDT1R_DLC;
	
	uint8_t * ATTRIBUTE_MAY_ALIAS data = message.data;
	reinterpret_cast<uint32_t *>(data)[0] = mailbox->RDLR;
	reinterpret_cast<uint32_t *>(data)[1] = mailbox->RDHR;
}

// ----------------------------------------------------------------------------
/* Transmit Interrupt
 *
 * Generated when Transmit Mailbox 0..2 becomes empty.
 */
extern "C" void
CAN2_TX_IRQHandler()
{
#if STM32_CAN2_TX_BUFFER_SIZE > 0
	uint32_t mailbox;
	uint32_t tsr = CAN2->TSR;
	
	if (tsr & CAN_TSR_RQCP2) {
		mailbox = 2;
		CAN2->TSR = CAN_TSR_RQCP2;
	}
	else if (tsr & CAN_TSR_RQCP1) {
		mailbox = 1;
		CAN2->TSR = CAN_TSR_RQCP1;
	}
	else {
		mailbox = 0;
		CAN2->TSR = CAN_TSR_RQCP0;
	}
	
	if (!txQueue.isEmpty())
	{
		sendMailbox(txQueue.get(), mailbox);
		txQueue.pop();
	}
#endif
}

// ----------------------------------------------------------------------------
/* FIFO0 Interrupt
 *
 * Generated on a new received message, FIFO0 full condition and Overrun
 * Condition.
 */
extern "C" void
CAN2_RX0_IRQHandler()
{
	if (CAN2->RF0R & CAN_RF0R_FOVR0) {
		xpcc::ErrorReport::report(xpcc::stm32::CAN2_FIFO0_OVERFLOW);
		
		// release overrun flag & access the next message
		CAN2->RF0R = CAN_RF0R_FOVR0 | CAN_RF0R_RFOM0;
	}
	
#if STM32_CAN2_RX_BUFFER_SIZE > 0
	xpcc::can::Message message;
	readMailbox(message, 0);
	
	// Release FIFO (access the next message)
	CAN2->RF0R = CAN_RF0R_RFOM0;
	
	if (!rxQueue.push(message)) {
		xpcc::ErrorReport::report(xpcc::stm32::CAN2_FIFO0_OVERFLOW);
	}
#endif
}

// ----------------------------------------------------------------------------
/* FIFO1 Interrupt
 *
 * See FIFO0 Interrupt
 */
extern "C" void
CAN2_RX1_IRQHandler()
{
	if (CAN2->RF1R & CAN_RF1R_FOVR1) {
		xpcc::ErrorReport::report(xpcc::stm32::CAN2_FIFO1_OVERFLOW);
		
		// release overrun flag & access the next message
		CAN2->RF1R = CAN_RF1R_FOVR1 | CAN_RF1R_RFOM1;
	}
	
#if STM32_CAN2_RX_BUFFER_SIZE > 0
	xpcc::can::Message message;
	readMailbox(message, 1);
	
	// Release FIFO (access the next message)
	CAN2->RF1R = CAN_RF1R_RFOM1;
	
	if (!rxQueue.push(message)) {
		xpcc::ErrorReport::report(xpcc::stm32::CAN2_FIFO1_OVERFLOW);
	}
#endif
}

// ----------------------------------------------------------------------------
void
xpcc::stm32::Can2::configurePins(Mapping mapping)
{
	// Enable clock
	RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
	
	// Initialize IO pins
#if defined(STM32F2XX) || defined(STM32F4XX)
	if (mapping == REMAP_PB5_PB6) {
		CanRxB5::setAlternateFunction(AF_CAN2, xpcc::stm32::PULLUP);
		CanTxB6::setAlternateFunction(AF_CAN2, xpcc::stm32::PUSH_PULL);
	}
	else {
		CanRxB12::setAlternateFunction(AF_CAN2, xpcc::stm32::PULLUP);
		CanTxB13::setAlternateFunction(AF_CAN2, xpcc::stm32::PUSH_PULL);
	}
#elif defined(STM32F3XX)
	
#elif defined(STM32F10X)
	AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_CAN2_REMAP) | mapping;
	if (mapping == REMAP_PB5_PB6) {
		CanRxB5::setInput(xpcc::stm32::PULLUP);
		CanTxB6::setAlternateFunction(xpcc::stm32::PUSH_PULL);
	}
	else {
		CanRxB12::setInput(xpcc::stm32::PULLUP);
		CanTxB13::setAlternateFunction(xpcc::stm32::PUSH_PULL);
	}
#else
#error "Please be more specific about the processor. "
#endif
}

// ----------------------------------------------------------------------------
// Re-implemented here to save some code space. As all arguments in the calls
// below are constant the compiler is able to calculate everything at
// compile time.
static ALWAYS_INLINE void
nvicEnableInterrupt(IRQn_Type IRQn)
{
	NVIC->ISER[((uint32_t)(IRQn) >> 5)] = (1UL << ((uint32_t)(IRQn) & 0x1F));
}

// ----------------------------------------------------------------------------
bool
xpcc::stm32::Can2::initialize(can::Bitrate bitrate,
		uint32_t interruptPriority, bool overwriteOnOverrun)
{
	// enable clock
	RCC->APB1ENR  |=  RCC_APB1ENR_CAN2EN;
	
	// reset controller
	RCC->APB1RSTR |=  RCC_APB1RSTR_CAN2RST;
	RCC->APB1RSTR &= ~RCC_APB1RSTR_CAN2RST;

	// CAN Master Reset
	// FMP bits and CAN_MCR register are initialized to the reset values
	CAN2->MCR |= CAN_MCR_RESET;
	while (CAN2->MCR & CAN_MCR_RESET)
		;

	// Exit from sleep mode
	CAN2->MCR &= (~(uint32_t)CAN_MCR_SLEEP);

	// Bus off is left automatically by the hardware after 128 occurrences
	// of 11 recessive bits, TX Order depends on the order of request and
	// not on the CAN priority.
	if (overwriteOnOverrun) {
		CAN2->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;
	}
	else {
		// No overwrite at RX FIFO: Once a receive FIFO is full the next
		// incoming message will be discarded
		CAN2->MCR |= CAN_MCR_ABOM | CAN_MCR_RFLM | CAN_MCR_TXFP;
	}

	// Request initialization
	CAN2->MCR |= CAN_MCR_INRQ;
	while ((CAN2->MSR & CAN_MSR_INAK) == 0) {
		// Wait until the initialization mode is entered.
		// The CAN hardware waits until the current CAN activity (transmission
		// or reception) is completed before entering the initialization mode.
	}
	
	// Enable Interrupts:
	// FIFO1 Overrun, FIFO0 Overrun
	CAN2->IER = CAN_IER_FOVIE1 | CAN_IER_FOVIE0;
	
	// Set vector priority
	NVIC_SetPriority(CAN2_RX0_IRQn, interruptPriority);
	NVIC_SetPriority(CAN2_RX1_IRQn, interruptPriority);
	
	// Register Interrupts at the NVIC
	nvicEnableInterrupt(CAN2_RX0_IRQn);
	nvicEnableInterrupt(CAN2_RX1_IRQn);
	
#if STM32_CAN2_TX_BUFFER_SIZE > 0
	CAN2->IER |= CAN_IER_TMEIE;
	nvicEnableInterrupt(CAN2_TX_IRQn);
	NVIC_SetPriority(CAN2_TX_IRQn, interruptPriority);
#endif
	
#if STM32_CAN2_RX_BUFFER_SIZE > 0
	CAN2->IER |= CAN_IER_FMPIE1 | CAN_IER_FMPIE0;
#endif
	
	/* Example for CAN bit timing:
	 *   CLK on APB1 = 36 MHz
	 *   BaudRate = 125 kBPs = 1 / NominalBitTime
	 *   NominalBitTime = 8uS = tq + tBS1 + tBS2
	 * with:
	 *   tBS1 = tq * (TS1[3:0] + 1) = 12 * tq
	 *   tBS2 = tq * (TS2[2:0] + 1) = 5 * tq
	 *   tq = (BRP[9:0] + 1) * tPCLK
	 * where tq refers to the Time quantum
	 *   tPCLK = time period of the APB clock = 1 / 36 MHz
	 * 
	 * STM32F1xx   tPCLK = 1 / 36 MHz
	 * STM32F20x   tPCLK = 1 / 30 MHz
	 * STM32F40x   tPCLK = 1 / 42 MHz 
	 */
	uint16_t prescaler;
	switch (bitrate)
	{
		case can::BITRATE_10_KBPS:	prescaler = 200; break;
		case can::BITRATE_20_KBPS:	prescaler = 100; break;
		case can::BITRATE_50_KBPS:	prescaler =  40; break;
		case can::BITRATE_100_KBPS:	prescaler =  20; break;
		case can::BITRATE_125_KBPS:	prescaler =  16; break;
		case can::BITRATE_250_KBPS:	prescaler =   8; break;
		case can::BITRATE_500_KBPS:	prescaler =   4; break;
		case can::BITRATE_1_MBPS:	prescaler =   2; break;
		default: prescaler = 16; break;		// 125 kbps
	}
	
#if defined(STM32F10X) || defined(STM32F3XX)
	XPCC__STATIC_ASSERT(STM32_APB1_FREQUENCY == 36000000UL,
			"Unsupported frequency for APB1 (only 36 MHz)");
	CAN2->BTR =
			 ((1 - 1) << CAN_BTR_SJW_POS) |		// SJW (1 to 4 possible)
			 ((5 - 1) << CAN_BTR_TS2_POS) |		// BS2 Samplepoint
			((12 - 1) << CAN_BTR_TS1_POS) |		// BS1 Samplepoint
			(prescaler - 1);
#elif defined(STM32F2XX)
	XPCC__STATIC_ASSERT(STM32_APB1_FREQUENCY == 30000000UL,
			"Unsupported frequency for APB1 (only 30 MHz)");
	CAN2->BTR =
			 ((1 - 1) << CAN_BTR_SJW_POS) |		// SJW (1 to 4 possible)
			 ((4 - 1) << CAN_BTR_TS2_POS) |		// BS2 Samplepoint
			((10 - 1) << CAN_BTR_TS1_POS) |		// BS1 Samplepoint
			(prescaler - 1);
#elif defined(STM32F4XX)
	//XPCC__STATIC_ASSERT(STM32_APB1_FREQUENCY == 42000000UL,
	//		"Unsupported frequency for APB1 (only 42 MHz)");
	CAN2->BTR =
			 ((1 - 1) << CAN_BTR_SJW_POS) |		// SJW (1 to 4 possible)
			 ((6 - 1) << CAN_BTR_TS2_POS) |		// BS2 Samplepoint
			((14 - 1) << CAN_BTR_TS1_POS) |		// BS1 Samplepoint
			(prescaler - 1);
#else
#	error "Unknown CPU Type. Please define STM32F10X, STM32F2XX, STM32F3XX or STM32F4XX"
#endif
	
	// Request leave initialization
	CAN2->MCR &= ~(uint32_t)CAN_MCR_INRQ;
	while ((CAN2->MSR & CAN_MSR_INAK) == CAN_MSR_INAK) {
		// wait for the normal mode
	}

	return true;
}

// ----------------------------------------------------------------------------
void
xpcc::stm32::Can2::setMode(can::Mode mode)
{
	// Request initialization
	CAN2->MCR |= CAN_MCR_INRQ;
	while ((CAN2->MSR & CAN_MSR_INAK) == 0) {
		// Wait until the initialization mode is entered.
		// The CAN hardware waits until the current CAN activity (transmission
		// or reception) is completed before entering the initialization mode.
	}

	uint32_t btr = CAN2->BTR & ~(CAN_BTR_SILM | CAN_BTR_LBKM);
	switch (mode)
	{
	case can::NORMAL:
		break;
	case can::LISTEN_ONLY:
		btr |= CAN_BTR_SILM;
		break;
	case can::LOOPBACK:
		btr |= CAN_BTR_LBKM;
		break;
	}
	CAN2->BTR = btr;

	// Leave initialization mode
	CAN2->MCR &= ~CAN_MCR_INRQ;
}

// ----------------------------------------------------------------------------
bool
xpcc::stm32::Can2::isMessageAvailable()
{
#if STM32_CAN2_RX_BUFFER_SIZE > 0
	return !rxQueue.isEmpty();
#else
	// Check if there are any messages pending in the receive registers
	return ((CAN2->RF0R & CAN_RF0R_FMP0) > 0 || (CAN2->RF1R & CAN_RF1R_FMP1) > 0);
#endif
}

// ----------------------------------------------------------------------------
bool
xpcc::stm32::Can2::getMessage(can::Message& message)
{
#if STM32_CAN2_RX_BUFFER_SIZE > 0
	if (rxQueue.isEmpty())
	{
		// no message in the receive buffer
		return false;
	}
	else {
		memcpy(&message, &rxQueue.get(), sizeof(message));
		rxQueue.pop();
		return true;
	}
#else
	if ((CAN2->RF0R & CAN_RF0R_FMP0) > 0)
	{
		readMailbox(message, 0);
		
		// Release FIFO (access the next message)
		CAN2->RF0R = CAN_RF0R_RFOM0;
		return true;
	}
	else if ((CAN2->RF1R & CAN_RF1R_FMP1) > 0)
	{
		readMailbox(message, 1);
		
		// Release FIFO (access the next message)
		CAN2->RF1R = CAN_RF1R_RFOM1;
		return true;
	}
	return false;
#endif
}

// ----------------------------------------------------------------------------
bool
xpcc::stm32::Can2::isReadyToSend()
{
#if STM32_CAN2_TX_BUFFER_SIZE > 0
	return !txQueue.isFull();
#else
	return ((CAN2->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME1)) != 0);
#endif
}

// ----------------------------------------------------------------------------
bool
xpcc::stm32::Can2::sendMessage(const can::Message& message)
{
	// This function is not reentrant. If one of the mailboxes is empty it
	// means that the software buffer is empty too. Therefore the mailbox
	// will stay empty and won't be taken by an interrupt.
	if ((CAN2->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME1)) == 0)
	{
		// All mailboxes used at the moment
#if STM32_CAN2_TX_BUFFER_SIZE > 0
		if (!txQueue.push(message)) {
			xpcc::ErrorReport::report(xpcc::stm32::CAN2_TX_OVERFLOW);
			return false;
		}
		return true;
#else
		return false;
#endif
	}
	else {
		// Get number of the first free mailbox
		uint32_t mailbox = (CAN2->TSR & CAN_TSR_CODE) >> 24;
		
		sendMailbox(message, mailbox);
		return true;
	}
}

// ----------------------------------------------------------------------------
xpcc::can::BusState
xpcc::stm32::Can2::getBusState()
{
	if (CAN2->ESR & CAN_ESR_BOFF) {
		return can::BUS_OFF;
	}
	else if (CAN2->ESR & CAN_ESR_BOFF) {
		return can::ERROR_PASSIVE;
	}
	else if (CAN2->ESR & CAN_ESR_EWGF) {
		return can::ERROR_WARNING;
	}
	else {
		return can::CONNECTED;
	}
}

// ----------------------------------------------------------------------------
void
xpcc::stm32::Can2::enableStatusChangeInterrupt(
		uint32_t interruptEnable,
		uint32_t interruptPriority)
{
	NVIC_SetPriority(CAN2_SCE_IRQn, interruptPriority);
	nvicEnableInterrupt(CAN2_SCE_IRQn);
	
	CAN2->IER = interruptEnable | (CAN2->IER & 0x000000ff);
}


#endif
