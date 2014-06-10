/*
 * firmware_ups.c
 * - User Programming Space -
 * Firmware for SeeedStudio Mesh Bee(Zigbee) module
 *
 * Copyright (c) NXP B.V. 2012.
 * Spread by SeeedStudio
 * Author     : Oliver Wang
 * Create Time: 2014/4
 * Change Log :
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "firmware_aups.h"
#include "suli.h"
#include "ups_arduino_sketch.h"
#include "firmware_hal.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifndef TRACE_UPS
#define TRACE_UPS FALSE
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
extern bool searchAtStarter(uint8 *buffer, int len);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
/* If node works on Master Mode,create two aups_ringbuf[UART,AirPort] */
struct ringbuffer rb_uart_aups;
struct ringbuffer rb_air_aups;

uint8 aups_uart_mempool[AUPS_UART_RB_LEN] = {0};
uint8 aups_air_mempool[AUPS_AIR_RB_LEN] = {0};


/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE uint32 _loopInterval = 0;


/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/


/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: UPS_vInitRingbuffer
 *
 * DESCRIPTION:
 * init ringbuffer of user programming space
 *
 * PARAMETERS: Name         RW  Usage
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void UPS_vInitRingbuffer()
{
    /* aups ringbuffer is required in Master mode */
    init_ringbuffer(&rb_uart_aups, aups_uart_mempool, AUPS_UART_RB_LEN);
    init_ringbuffer(&rb_air_aups, aups_air_mempool, AUPS_AIR_RB_LEN);
}
/****************************************************************************
 *
 * NAME: ups_init
 *
 * DESCRIPTION:
 * init user programming space
 *
 * PARAMETERS: Name         RW  Usage
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void ups_init(void)
{
	/* Init ringbuffer */
	UPS_vInitRingbuffer();

	/* init suli */
    suli_init();

    /* init arduino sketch with arduino-style setup function */
    arduino_setup();

    /* Activate Arduino-ful MCU */
    OS_eStartSWTimer(Arduino_LoopTimer, APP_TIME_MS(500), NULL);
}


/****************************************************************************
 *
 * NAME: setNodeState
 *
 * DESCRIPTION:
 * set the state of node
 *
 * PARAMETERS: Name         RW  Usage
 *             state        W   state of node
 *             0:DATA_MODE
 *             1:AT_MODE
 *             2:MCU_MODE
 * RETURNS:
 * void
 *
 ****************************************************************************/
void setNodeState(uint32 state)
{
    g_sDevice.eMode = state;
    PDM_vSaveRecord(&g_sDevicePDDesc);
}

/****************************************************************************
 *
 * NAME: vDelayMsec
 *
 * DESCRIPTION:
 * Delay n ms
 *
 * PARAMETERS: Name         RW  Usage
 *             u32Period    R   ms
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void vDelayMsec(uint32 u32Period)
{
    uint32 i, k;
    const uint32 u32MsecCount = 1800;
    volatile uint32 j;                  //declare as volatile so compiler doesn't optimize increment away

    for (k = 0; k < u32Period; k++)
        for (i = 0; i < u32MsecCount; i++) j++;
}

/****************************************************************************
 *
 * NAME: Arduino_Loop
 *
 * DESCRIPTION:
 * task for arduino loop
 *
 ****************************************************************************/
OS_TASK(Arduino_Loop)
{
	/*
	  Mutex, only in MCU mode,this loop will be called
      or data in ringbuffer may become mess
    */
	if(E_MODE_MCU == g_sDevice.eMode)
	{
		/* Back-Ground to search AT delimiter */
		uint8 tmp[AUPS_UART_RB_LEN];
		uint32 avlb_cnt = suli_uart_readable(NULL, NULL);
		uint32 min_cnt = MIN(AUPS_UART_RB_LEN, avlb_cnt);

		/* Read,not pop,make sure we don't pollute user data in AUPS ringbuffer */
		vHAL_UartRead(tmp, min_cnt);
		if (searchAtStarter(tmp, min_cnt))
		{
			/* Set AT mode */
			setNodeState(E_MODE_AT);
			suli_uart_printf(NULL, NULL, "Enter AT Mode.\r\n");

			/* Clear ringbuffer of AUPS */
			OS_eEnterCriticalSection(mutexRxRb);
			clear_ringbuffer(&rb_uart_aups);
			OS_eExitCriticalSection(mutexRxRb);
		}
		else
		{
		    arduino_loop();
		}

        /*
         * If a sleep event has already been scheduled in arduino_loop,
         * don't set a new arduino_loop
        */
		if(true == bGetSleepStatus())
			return;

        /* re-activate Arduino_Loop */
		if(g_sDevice.config.upsXtalPeriod > 0)
		{
			OS_eStartSWTimer(Arduino_LoopTimer, APP_TIME_MS(g_sDevice.config.upsXtalPeriod), NULL);
		}
		else
		{
			OS_eActivateTask(Arduino_Loop);  //this task is the lowest priority
		}
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
