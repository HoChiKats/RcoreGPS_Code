#include "usr_includes.h"
XScuGic InterruptController;	/* Instance of the Interrupt Controller */
int main(void)
{
	xQueue_GpsData = xQueueCreate(1, sizeof(T_GPS_INFO));
	xQueue_GpsGGA = xQueueCreate(1, sizeof(T_GPS_INFO));
	xQueue_GpsRMC = xQueueCreate(1, sizeof(T_GPS_INFO));

	xEvent_LedHandle = xEventGroupCreate();

	xTaskCreate(AXI_Timer_Set, "AXI_TIMER_SET",2048,NULL, 4, &xAXI_Timer_Set);

	xTaskCreate(GPS_Config, "GPS_CONFIG", 2048, NULL, 2, &Task_Uart_Set);
	xTaskCreate(GPS_Data_Recv, "GPS_RECEV", 2048, NULL, 2, &Task_GPS_Rece);
	xTaskCreate(GPS_DataParsing, "GPS_PARSING", 2048, NULL, 2, &Task_GPS_Parsing);
	xTaskCreate(GGA_Data_Disp, "GGA_SEND", 2048, NULL, 2, &Task_GGA_Send);
	xTaskCreate(RMC_Data_Disp, "RMC_SEND", 2048, NULL, 2, &Task_RMC_Send);
	xTaskCreate(GPS_Err_Chk, "GPS_ERR_CHK", 2048, NULL, 4, &Task_Err_Chk);


	xTaskCreate(Axi_INT_Set, "GPIO_SET",2048, NULL, 2, &xAXI_Set);
	xTaskCreate(AXI_INT_Ctrl, "GPIO_CTRL",2048, NULL, 2, &xAXI_Ctrl);
	xTaskCreate(Gpio_LedCtrl, "GPIO_LED",2048, NULL, 2, &xGpio_Led);

	vTaskStartScheduler();
	return 0;
}
