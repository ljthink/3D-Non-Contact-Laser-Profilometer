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

#include <xpcc/architecture/platform/avr/atxmega.hpp>

#include <xpcc/architecture/driver/atomic/queue.hpp>
#include <xpcc/architecture/driver/atomic/lock.hpp>

#include <xpcc/architecture/driver/gpio.hpp>
#include <xpcc/architecture/platform/avr/atxmega/gpio.hpp>

#include "uart_c0.hpp"
#include "xpcc_config.hpp"

#ifdef USARTC0_RXC_vect

#if UARTC0_RTS_PIN >= 0
// RTS_PIN = -1 deactivates flow control 

namespace
{
	static xpcc::atomic::Queue<uint8_t, UARTC0_RX_BUFFER_SIZE> rxBuffer;
	static xpcc::atomic::Queue<uint8_t, UARTC0_TX_BUFFER_SIZE> txBuffer;
	
	GPIO__INPUT (RXD, C, 2);
	GPIO__OUTPUT(TXD, C, 3);
	
	GPIO__INPUT (RTS, UARTC0_RTS_PORT, UARTC0_RTS_PIN);
	GPIO__OUTPUT(CTS, UARTC0_CTS_PORT, UARTC0_CTS_PIN);

	uint8_t error;
}

// ----------------------------------------------------------------------------
#if UARTC0_RTS_PIN != -1
ISR(USARTC0_RXC_vect)
{		
	// first save the errors
	error |= USARTC0_STATUS & (USART_FERR_bm | USART_BUFOVF_bm | USART_PERR_bm);
	
	// then read the buffer
	uint8_t data = USARTC0_DATA;
	
	if (!rxBuffer.push(data)) {
		// TODO: Error handling if internal buffer is full.		
	}
	 
	// If the internal buffer is now nearly full set CTS to high to inhibit 
	// remote from sending new data. As the remote may not stop instantenously
	// it is important to stop before the buffer is completely full. 
	if (rxBuffer.isNearlyFull()) {
		CTS::set();
	}
}

// ----------------------------------------------------------------------------
ISR(USARTC0_DRE_vect)
{
	if (txBuffer.isEmpty())
	{
		// transmission finished, disable DRE interrupt
		USARTC0_CTRLA = USART_RXCINTLVL_MED_gc;
	}
	else {
		// check if RTS is high which indicates that the remote device is ready
		// to accept data
		if (RTS::read())
		{
			// RTS of remote device high: do not send new data. 
			// Disable DRE interrupt
			// TODO: Setup interrupt for rising edge of RTS to resume sending. 
			USARTC0_CTRLA = USART_RXCINTLVL_MED_gc;
		}
		else
		{
			// RTS of remote device low: ready to receive new data.
			// get one byte from buffer and write it to the UART buffer
			// which starts the transmission
			USARTC0_DATA = txBuffer.get();
			txBuffer.pop();
		}
	}
}
#endif


// ----------------------------------------------------------------------------
void
xpcc::atxmega::BufferedUartFlowC0::initialize()
{
	TXD::set();
	TXD::setOutput();
	
	RXD::setInput();
	
	// RTS and CTS pins, ready for reception in few cycles.
	CTS::setOutput(xpcc::gpio::LOW);
	RTS::setInput(xpcc::atxmega::PULLUP);
	
	// interrupts should be disabled during initialisation
	atomic::Lock lock;
		
	// enable receive complete interrupt
	USARTC0.CTRLA = USART_RXCINTLVL_MED_gc;
	
	// setting the frame size to 8 bit
	USARTC0.CTRLC = USART_CHSIZE_8BIT_gc;
	
	// enable both, receiver and transmitter
	USARTC0.CTRLB |= USART_RXEN_bm | USART_TXEN_bm;
}

// ----------------------------------------------------------------------------
void
xpcc::atxmega::BufferedUartFlowC0::write(uint8_t c)
{
	while ( !txBuffer.push(c) ) {
		// wait for a free slot in the buffer
	}
	
	// Either a byte was enqueued to the buffer or it was not possible because
	// the buffer was full. In both cases there is something to send now. 
	
	// disable interrupts
	atomic::Lock lock;
	
	// enable DRE interrupt
	USARTC0_CTRLA = USART_RXCINTLVL_MED_gc | USART_DREINTLVL_MED_gc;
}

// ----------------------------------------------------------------------------
void
xpcc::atxmega::BufferedUartFlowC0::write(const uint8_t *s, uint8_t n)
{
	while (n-- != 0) {
		BufferedUartFlowC0::write(*s++);
	}
}

// ----------------------------------------------------------------------------
bool
xpcc::atxmega::BufferedUartFlowC0::read(uint8_t& c)
{
	if (rxBuffer.isNearlyEmpty()) {
		// When the buffer is nearly empty allow remote device to send again.
		// This will create a continous flow of data.  
		CTS::reset();
	}
	
	// Small hack: When the AVR stopped transmission due to a high RTS signal try to resume
	// transmission now when RTS is low again and there is something to send. 
	// TODO: can be removed if RTS interrupt is included. 
	if (!RTS::read() && !txBuffer.isEmpty()) {
		// enable DRE interrupt to resume transmission
		USARTC0_CTRLA = USART_RXCINTLVL_MED_gc | USART_DREINTLVL_MED_gc;
	}
	
	if (rxBuffer.isEmpty()) {
		// no data in buffer anymore
		CTS::reset();
		return false;
	}
	else {
		c = rxBuffer.get();
		rxBuffer.pop();
		return true;
	}
}

// ----------------------------------------------------------------------------
uint8_t
xpcc::atxmega::BufferedUartFlowC0::read(uint8_t *buffer, uint8_t n)
{
	uint8_t i;
	for (i = 0; i < n; ++i)
	{
		if (rxBuffer.isEmpty()) {
			CTS::reset();
			return i;
		}
		else {
			*buffer++ = rxBuffer.get();
			rxBuffer.pop();
		
			if (rxBuffer.isNearlyEmpty()) {
				// When the buffer is nearly empty allow remote device to send again.
				// This will create a continous flow of data.  
				CTS::reset();
			}
		}
	}
	
	return i;
}

// ----------------------------------------------------------------------------
uint8_t
xpcc::atxmega::BufferedUartFlowC0::readErrorFlags()
{
	return error;
}

void
xpcc::atxmega::BufferedUartFlowC0::resetErrorFlags()
{
	error = 0;
}

// ----------------------------------------------------------------------------
uint8_t
xpcc::atxmega::BufferedUartFlowC0::flushReceiveBuffer()
{
	uint8_t i = 0;
	while(!rxBuffer.isEmpty()) {
		rxBuffer.pop();
		++i;
	}
	
	uint8_t c;
	while (USARTC0_STATUS & USART_RXCIF_bm) {
		c = USARTC0_DATA;
	}
	
	return i;
}

//uint8_t
//xpcc::atxmega::BufferedUartFlowC0::flushTransmitBuffer()
//{
//	uint8_t i(0);
//	while(!txBuffer.isEmpty()) {
//		txBuffer.pop();
//		++i;
//	}
//
//	return i;
//}


#endif	
#endif	// USARTC0_RXC_vect