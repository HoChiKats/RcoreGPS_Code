#include "usr_includes.h"

volatile int TimerExpired;
u32 u32_ScaleValue = (0xFFFFFFFF - (100000*1));	// Init Timer 1 mSec .//
TaskHandle_t xAXI_Timer_Set = NULL;
XTmrCtr TimerCounterInst;

void AXI_Timer_Set(void* pvParameters)
{
	int Status;
	Status = TmrCtrIntrExample(&InterruptController,
					  &TimerCounterInst,
					  TMRCTR_DEVICE_ID,
					  TMRCTR_INTERRUPT_ID,
					  TIMER_CNTR_0);
		if (Status != XST_SUCCESS) {
			xil_printf("Tmrctr interrupt Example Failed\r\n");
		}
		else;
}

unsigned int TimerScale_Value(u32 u32_MS)	// Tranlate ms .//
{
	u32 ret;
	ret = (0xFFFFFFFF - (100000*u32_MS));
	return ret;
}

int TmrCtrIntrExample(XScuGic *IntcInstancePtr,
			XTmrCtr *TmrCtrInstancePtr,
			u16 DeviceId,
			u16 IntrId,
			u8 TmrCtrNumber)
{
	int Status;

	Status = XTmrCtr_Initialize(TmrCtrInstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XTmrCtr_SelfTest(TmrCtrInstancePtr, TmrCtrNumber);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = TmrCtrSetupIntrSystem(IntcInstancePtr,
					TmrCtrInstancePtr,
					DeviceId,
					IntrId,
					TmrCtrNumber);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XTmrCtr_SetHandler(TmrCtrInstancePtr, TimerCounterHandler,
					   TmrCtrInstancePtr);

	XTmrCtr_SetOptions(TmrCtrInstancePtr, TmrCtrNumber,
				XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);

	XTmrCtr_SetResetValue(TmrCtrInstancePtr, TmrCtrNumber, u32_ScaleValue);
	XTmrCtr_Start(TmrCtrInstancePtr, TmrCtrNumber);
	return XST_SUCCESS;
}


void TimerCounterHandler(void *CallBackRef, u8 TmrCtrNumber)
{
	XTmrCtr *InstancePtr = (XTmrCtr *)CallBackRef;
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;

	if (XTmrCtr_IsExpired(InstancePtr, TmrCtrNumber)) {
		gu32_GpsErrTmr++;
		if(gu32_GpsErrTmr > 5000)
		{
			gu_GpsReg.bErrorFind = TRUE;
			vTaskNotifyGiveFromISR(Task_Err_Chk, 0);
		}
		else;
	}
	else;

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


int TmrCtrSetupIntrSystem(XScuGic *IntcInstancePtr,
				 XTmrCtr *TmrCtrInstancePtr,
				 u16 DeviceId,
				 u16 IntrId,
				 u8 TmrCtrNumber)
{
	 int Status;
	XScuGic_Config *IntcConfig;
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId,
					0xA0, 0x3);

	Status = XScuGic_Connect(IntcInstancePtr, IntrId,
				 (Xil_ExceptionHandler)XTmrCtr_InterruptHandler,
				 TmrCtrInstancePtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(IntcInstancePtr, IntrId);
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
					(Xil_ExceptionHandler)
					INTC_HANDLER,
					IntcInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

void TmrCtrDisableIntr(XScuGic *IntcInstancePtr, u16 IntrId)
{
	XScuGic_Disable(IntcInstancePtr, IntrId);
	XScuGic_Disconnect(IntcInstancePtr, IntrId);
}

