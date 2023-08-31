/****************************************************/
/*Don't Touch Define */
#define TMRCTR_DEVICE_ID	XPAR_TMRCTR_0_DEVICE_ID
#define TMRCTR_INTERRUPT_ID	XPAR_FABRIC_TMRCTR_0_VEC_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define TIMER_CNTR_0	 0
#define INTC_HANDLER	XScuGic_InterruptHandler
/****************************************************/
/* User Define */

/* FUNCTION */
// Task Function .//
extern void AXI_Timer_Set(void* pvParameters);
// Normal Function .//
int TmrCtrIntrExample(XScuGic *IntcInstancePtr, XTmrCtr *InstancePtr,
			u16 DeviceId, u16 IntrId, u8 TmrCtrNumber);
int TmrCtrSetupIntrSystem(XScuGic *IntcInstancePtr, XTmrCtr *InstancePtr,
				u16 DeviceId, u16 IntrId, u8 TmrCtrNumber);
void TmrCtrDisableIntr(XScuGic *IntcInstancePtr, u16 IntrId);
void AXI_Timer_ReSetting(void);
unsigned int TimerScale_Value(u32 u32_MS);
// Handler ISR .//
void TimerCounterHandler(void *CallBackRef, u8 TmrCtrNumber);

/* VARIABLE */
// FreeRTOS .//
extern TaskHandle_t xAXI_Timer_Set;
