/*******************************************************/
/* Don't Touch Define */
// Interrupt define .//
#define INTR_CHANNEL		1
#define INTC_GPIO_INTERRUPT_ID	XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR
#define INTC_DEVICE_ID	XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTERRUPT_CONTROL_VALUE 0x7
#define INTR_DELAY	0x00FFFFFF
#define INTC_DEVICE_ID	XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTC_HANDLER	XScuGic_InterruptHandler
//Gpio define .//
#define GPIO_DEVICE_ID_INTR		XPAR_GPIO_1_DEVICE_ID
#define GPIO_EXAMPLE_DEVICE_ID_OUT XPAR_GPIO_0_DEVICE_ID
#define GPIO_EXAMPLE_DEVICE_ID_IN  XPAR_GPIO_1_DEVICE_ID
/*******************************************************/

/* User Define */
#define GPIO_ALL_LEDS		0xFFFF
#define GPIO_ALL_BUTTONS	0xFFFF
#define LED 	0x01   /* Assumes bit 0 of GPIO is connected to an LED  */
#define ALL_LED 0xff
#define LED_CHANNEL 1

/* FUNCTION */
// Task Function .//
extern void Axi_INT_Set(void* pvParameters);
extern void AXI_INT_Ctrl(void* pvParameters);
extern void Gpio_LedCtrl(void* pvParameters);
extern void Gpio_LedCtrl_AXI(void* pvParameters);
// Normal Function .//
int GpioSetupIntrSystem(XScuGic *IntcInstancePtr, XGpio *InstancePtr, u16 DeviceId, u16 IntrId, u16 IntrMask);
void GpioDisableIntr(XScuGic *IntcInstancePtr, XGpio *InstancePtr, u16 IntrId, u16 IntrMask);

// Handler ISR .//
void GpioHandler(void *CallBackRef);

/* VARIABLE*/
// FreeRTOS .//
extern TaskHandle_t xAXI_Set, xAXI_Ctrl, xGpio_Led; // xGpio_Led_Axi;
extern u32 u32_Led_Sel;
