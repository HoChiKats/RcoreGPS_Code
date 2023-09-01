#include "usr_includes.h"



/*****************************************/
/*********Git Branch Test*****************/
/*****************************************/
/*****************************************/

/*****************************************/
/*********Git Taf Test********************/
/*****************************************/
/*****************************************/

TaskHandle_t xAXI_Set = NULL, xAXI_Ctrl = NULL, xGpio_Led = NULL;// xGpio_Led_Axi = NULL;

u32 IntrFlag, u32_Input_data;
u32 u32_Led_Sel;
u32 u32_input_Sel; /* Interrupt Handler Flag */
u16 GlobalIntrMask;
XGpio Gpio_Int; /* The Instance of the GPIO Driver */
XGpio Gpio_Out; /* The Instance of the GPIO Driver */
XGpio Gpio_In; /* The Instance of the GPIO Driver */


void Axi_INT_Set(void* pvParameters)
{
    int Status;

	/* Initialize the GPIO driver. If an error occurs then exit */
	Status = XGpio_Initialize(&Gpio_Int, GPIO_DEVICE_ID_INTR);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
	}

	Status = GpioSetupIntrSystem(&InterruptController, &Gpio_Int, GPIO_DEVICE_ID_INTR,
					INTC_GPIO_INTERRUPT_ID, INTR_CHANNEL);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
	}


	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&Gpio_Out, GPIO_EXAMPLE_DEVICE_ID_OUT);
	Status = XGpio_Initialize(&Gpio_In, GPIO_EXAMPLE_DEVICE_ID_IN);

	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
	}

	/* Set the direction for all signals as inputs except the LED output */
	XGpio_SetDataDirection(&Gpio_Out, LED_CHANNEL, ~ALL_LED);
	XGpio_SetDataDirection(&Gpio_In, LED_CHANNEL, ALL_LED);

	IntrFlag = 0;
}

void AXI_INT_Ctrl(void* pvParameters)
{
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		if(IntrFlag)
		{
			u32_Input_data = XGpio_DiscreteRead(&Gpio_In, LED_CHANNEL);
			u32_input_Sel = u32_Input_data&0x0F;

			switch(u32_input_Sel)
			{
				case 0x01:	// Send TTC TImer .//
					gu_GpsReg.bGPS_Stop ^= TRUE;
					xTaskNotifyGive(Task_GPS_Rece);
					break;
				case 0x04:	// Send AXI Timer .//
					gu_GpsReg.b2_GpsSel++;
					if(gu_GpsReg.b2_GpsSel > 2)
						gu_GpsReg.b2_GpsSel = 0;
					break;
				default:
					break;
			}
			IntrFlag = 0;
		}
	}
}

void Gpio_LedCtrl(void* pvParameters)
{
	u32 LEDCtrl = 0;
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		LEDCtrl = 0x0003&gu_GpsReg.dat;
		if(u32_Led_Sel == TRUE)
			XGpio_DiscreteWrite(&Gpio_Out, LED_CHANNEL, LEDCtrl);
		else
			XGpio_DiscreteClear(&Gpio_Out, LED_CHANNEL, LEDCtrl);
	}
}

int GpioSetupIntrSystem(XScuGic *IntcInstancePtr, XGpio *InstancePtr,
			u16 DeviceId, u16 IntrId, u16 IntrMask)
{
	int Result;

	GlobalIntrMask = IntrMask;

	XScuGic_Config *IntcConfig;

	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Result = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Result != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId,
					0xA0, 0x1);

	Result = XScuGic_Connect(IntcInstancePtr, IntrId,
				 (Xil_ExceptionHandler)GpioHandler, InstancePtr);
	if (Result != XST_SUCCESS) {
		return Result;
	}

	XScuGic_Enable(IntcInstancePtr, IntrId);

	XGpio_InterruptEnable(InstancePtr, IntrMask);
	XGpio_InterruptGlobalEnable(InstancePtr);

	Xil_ExceptionInit();

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 (Xil_ExceptionHandler)INTC_HANDLER, IntcInstancePtr);

	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

void GpioHandler(void *CallbackRef)
{
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	IntrFlag = 1;

	vTaskNotifyGiveFromISR(xAXI_Ctrl,0);

	XGpio_InterruptClear(GpioPtr, GlobalIntrMask);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void GpioDisableIntr(XScuGic *IntcInstancePtr, XGpio *InstancePtr,
			u16 IntrId, u16 IntrMask)
{
	XGpio_InterruptDisable(InstancePtr, IntrMask);

	XScuGic_Disable(IntcInstancePtr, IntrId);
	XScuGic_Disconnect(IntcInstancePtr, IntrId);
	return;
}
