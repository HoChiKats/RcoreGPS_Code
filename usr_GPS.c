#include "usr_includes.h"

XUartPs UartPs	;		/* Instance of the UART Device */
XTime tEnd, tStart;
XTime period;
double D_TimeChk;

TaskHandle_t Task_Uart_Set = NULL, Task_GPS_Rece = NULL, Task_GPS_Parsing = NULL,
		Task_GGA_Send = NULL, Task_RMC_Send = NULL, Task_Err_Chk = NULL;
QueueHandle_t xQueue_GpsData, xQueue_GpsGGA, xQueue_GpsRMC;
EventGroupHandle_t xEvent_LedHandle;

T_REG gu_GpsReg;
T_GPS_INFO gstGpsInfo;

u16 u16_FrameNum = 0,u16_FrameErrNum = 0;
u32 TotalReceivedCount, TotalSentCount, ReceiveCount=0;
u32 TotalErrorCount[3];
u32 gu32_GpsErrTmr = 0;

double avg_TimeChk = 0;
double Time_save[100];
u8 Time_BufferIndex = 0;

void GPS_Config(void * pvParameter)
{
	Uart_Config(&InterruptController, &UartPs, UART_DEVICE_ID, UART_INT_IRQ_ID);
}

void Uart_Config(XScuGic *IntcInstPtr, XUartPs *UartInstPtr, u16 DeviceId, u16 UartIntrId)
{
	int Status;
	XUartPs_Config *Config;
	u32 IntrMask;

	Config = XUartPs_LookupConfig(DeviceId);
	if (NULL == Config) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}
	Status = XUartPs_CfgInitialize(UartInstPtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}
	SetupInterruptSystem(IntcInstPtr, UartInstPtr, UartIntrId);
	XUartPs_SetHandler(UartInstPtr, (XUartPs_Handler)Handler, UartInstPtr);
	IntrMask =
		XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING |
		XUARTPS_IXR_OVER | XUARTPS_IXR_TXEMPTY | XUARTPS_IXR_RXFULL |
		XUARTPS_IXR_RXOVR;
	if (UartInstPtr->Platform == XPLAT_ZYNQ_ULTRA_MP) {
		IntrMask |= XUARTPS_IXR_RBRK;
	}
	Status = XUartPs_SetBaudRate(UartInstPtr, 460800);	// Baur Rate Set, Default: 115200 .//
	if (Status != XST_SUCCESS) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}
	XUartPs_SetInterruptMask(UartInstPtr, IntrMask);
	XUartPs_SetRecvTimeout(UartInstPtr, 8);
	/*USER  SET*/
	gu32_GpsErrTmr  = 0;								// Error Check Timer .//
	xEventGroupSetBits(xEvent_LedHandle, BIT_0);
}
u32 Test_Val = 0, Test_Val_2 = 0;;

void GPS_Data_Recv(void * pvParameter)
{
	u8 Temp_Data = 0;
	XUartPs_Recv(&UartPs, &Temp_Data, 1);
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#ifdef __TIME_CHECK__
		XTime_GetTime(&tStart);
#endif
		if(gu_GpsReg.bGPS_Stop == FALSE)
			XUartPs_Recv(&UartPs, &Temp_Data, 1);

		if(Temp_Data == '$')
			ReceiveCount = 0;
		else;

		gstGpsInfo.u8_Temp_Data[ReceiveCount] = Temp_Data;
		ReceiveCount++;

		switch(Temp_Data)
		{
			case '*':
				gstGpsInfo.u32_CRC_Lenght = ReceiveCount-1;
				GPS_madeCRC();
				break;
			case '\r':
				if(gstGpsInfo.u8_Temp_Data[0] == '$')
				{
					gstGpsInfo.u8_CRC_Data[0] = (ASCII2Hex(gstGpsInfo.u8_Temp_Data[gstGpsInfo.u32_CRC_Lenght+1])<<4);
					gstGpsInfo.u8_CRC_Data[1] = ASCII2Hex(gstGpsInfo.u8_Temp_Data[gstGpsInfo.u32_CRC_Lenght+2]);
					u16_FrameNum++;
					xTaskNotifyGive(Task_GPS_Parsing);
				}
				else
				{
					gu_GpsReg.bErrorFind = TRUE;
					xTaskNotifyGive(Task_Err_Chk);
				}
				break;
			default:
				break;
		}
	}
}

void GPS_madeCRC(void)
{
	gstGpsInfo.u8_CRC_Temp = 0;

	for(int index = 1; index < gstGpsInfo.u32_CRC_Lenght; index++)
	{
		gstGpsInfo.u8_CRC_Temp ^= gstGpsInfo.u8_Temp_Data[index];
	}
}

void GPS_DataParsing(void * pvParameter)
{
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if(gstGpsInfo.u8_CRC_Temp == (gstGpsInfo.u8_CRC_Data[0]|gstGpsInfo.u8_CRC_Data[1]))
		{
			memcpy(gstGpsInfo.u8_GPS_Data, gstGpsInfo.u8_Temp_Data, gstGpsInfo.u32_CRC_Lenght);
			memset(gstGpsInfo.u8_Temp_Data, 0, sizeof(gstGpsInfo.u8_Temp_Data));

			if((gstGpsInfo.u8_GPS_Data[3] == 'G')&&(gstGpsInfo.u8_GPS_Data[4] == 'G')&&(gstGpsInfo.u8_GPS_Data[5] == 'A')&&
					((gu_GpsReg.b2_GpsSel == GPS_ALL_FRAME)||(gu_GpsReg.b2_GpsSel == GPS_GGA_FRAME)))
			{
				gu_GpsReg.bGGA = TRUE;
				gu_GpsReg.bRMC = FALSE;
			}
			else if((gstGpsInfo.u8_GPS_Data[3] == 'R')&&(gstGpsInfo.u8_GPS_Data[4] == 'M')&&(gstGpsInfo.u8_GPS_Data[5] == 'C')&&
					((gu_GpsReg.b2_GpsSel == GPS_ALL_FRAME)||(gu_GpsReg.b2_GpsSel == GPS_RMC_FRAME)))
			{
				gu_GpsReg.bGGA = FALSE;
				gu_GpsReg.bRMC = TRUE;
			}
			else
				gu_GpsReg.bGGA = gu_GpsReg.bRMC = FALSE;

			u32_Led_Sel = TRUE;
			xTaskNotifyGive(xGpio_Led);

			switch(0x00000003&gu_GpsReg.dat)
			{
				case 1:	// GPSGGA .//
					GGA_Data_Split(gstGpsInfo.u8_GPS_Data, &gstGpsInfo.GGA_Data, gstGpsInfo.u32_CRC_Lenght);
					xEventGroupSetBits(xEvent_LedHandle, BIT_1);
					break;
				case 2:	// GPSRMC .//
					RMC_Data_Split(gstGpsInfo.u8_GPS_Data, &gstGpsInfo.RMC_Data, gstGpsInfo.u32_CRC_Lenght);
					xEventGroupSetBits(xEvent_LedHandle, BIT_2);
					break;
				default:	// ERROR .//
					break;
			}
		}
		else
		{
			Test_Val++;
//			gstGpsInfo.u8_GPS_ErrFrame[Test_Val] = gstGpsInfo.u8_CRC_Data[0];
//			gstGpsInfo.u8_GPS_ErrFrame[Test_Val] = gstGpsInfo.u8_CRC_Data[1];
//			gstGpsInfo.u8_GPS_ErrFrame[Test_Val] = gstGpsInfo.u8_CRC_Temp;
		}
	}
}


void GGA_Data_Disp(void * pvParameter)
{
	while(1)
	{
		xEventGroupWaitBits(xEvent_LedHandle, BIT_0 | BIT_1, TRUE, TRUE, portMAX_DELAY);

		u8 u8_temp = 0;
		float F_temp = 0;
		u16 IndexNum = 0, data_len = 0;
		char * C_PTemp = NULL;

		memset(gstGpsInfo.u8_Send_Data, 0, sizeof(gstGpsInfo.u8_Send_Data));
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u16_FrameNum);
		IndexNum += data_len;

		// GPS INFO .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"**GGA**");
		IndexNum += data_len;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u16_FrameErrNum);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// TIME .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"TIME: ");
		IndexNum += data_len;

		u8_temp = (ASCII2Hex(gstGpsInfo.GGA_Data.u8_time[0])*10 + ASCII2Hex(gstGpsInfo.GGA_Data.u8_time[1]))+9;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u8_temp);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'h';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_time[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_time[3];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'm';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_time[4];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_time[5];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 's';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// LATI .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"LATI:");
		IndexNum += data_len;

		F_temp = strtof((char*)(gstGpsInfo.GGA_Data.u8_Latit+2), &C_PTemp);
		if(F_temp >= 60)
		{
			if(gstGpsInfo.GGA_Data.u8_Latit[1] >= 0x39)
			{
				gstGpsInfo.GGA_Data.u8_Latit[1] = 0x30;
				gstGpsInfo.GGA_Data.u8_Latit[0] += 0x31;
			}
			else
				gstGpsInfo.GGA_Data.u8_Latit[1] += 0x31;
			F_temp = ((F_temp/60) - 1)*1000;
		}
		else
			F_temp = (F_temp/60)*1000;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Latit[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Latit[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '.';
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",(u8)F_temp);
		IndexNum += data_len;

		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// DIRECT .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"DIRECT:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Direc[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// LONGIT .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"LONGIT:");
		IndexNum += data_len;

		F_temp = strtof((char*)(gstGpsInfo.GGA_Data.u8_Longit+3), &C_PTemp);
		if(F_temp >= 60)
		{
			if(gstGpsInfo.GGA_Data.u8_Longit[2] >= 0x39)
			{
				gstGpsInfo.GGA_Data.u8_Longit[2] = 0x30;
				gstGpsInfo.GGA_Data.u8_Longit[1] += 0x31;
			}
			else
				gstGpsInfo.GGA_Data.u8_Longit[2] += 0x31;
			F_temp = ((F_temp/60) - 1)*1000;
		}
		else
			F_temp = (F_temp/60)*1000;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Longit[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Longit[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Longit[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '.';
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",(u8)F_temp);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_CardPoint[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"CARDPO:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_CardPoint[0];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"\r\n");
		IndexNum += data_len;

		// u8_GPS_use[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"GPS:");
		IndexNum += data_len;
		switch(gstGpsInfo.GGA_Data.u8_GPS_use[0])
		{
			case '0':
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"Invalid");
				IndexNum += data_len;
				break;
			case '1':
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"Valid");
				IndexNum += data_len;
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100," SPS");
				IndexNum += data_len;
				break;
			case '2':
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"Valid");
				IndexNum += data_len;
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100," DGPS");
				IndexNum += data_len;
				break;
			case '3':
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"Valid");
				IndexNum += data_len;
				data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100," PPS");
				IndexNum += data_len;
			default:
				break;
		}
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_NumSatil[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"NUM");
		IndexNum += data_len;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100," SATEL:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_NumSatil[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_NumSatil[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_HDOP[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"HDOP:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_HDOP[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_HDOP[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_HDOP[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Geo_High[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"MSL:");
		IndexNum += data_len;
		for(int index = 0; index< 20; index++)
		{
			if(gstGpsInfo.GGA_Data.u8_Geo_High[index] == ',')
				break;
			else
				gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Geo_High[index];
		}
		// u8_Geo_meter[20];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Geo_meter[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Elip_Geo[20];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"GEO-");
		IndexNum += data_len;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"MSL:");
		IndexNum += data_len;
		for(int index = 0; index< 20; index++)
		{
			if(gstGpsInfo.GGA_Data.u8_Elip_Geo[index] == ',')
				break;
			else
				gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Elip_Geo[index];
		}
		// u8_Elip_meter[20];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.GGA_Data.u8_Elip_meter[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Age[20];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		XUartPs_Send(&UartPs, (gstGpsInfo.u8_Send_Data), IndexNum);
	}
}

void RMC_Data_Disp(void * pvParameter)
{
	while(1)
	{
		xEventGroupWaitBits(xEvent_LedHandle, BIT_0 | BIT_2, TRUE, TRUE, portMAX_DELAY);

		u8 u8_temp = 0;
		u16 IndexNum = 0, data_len = 0;
		float F_temp = 0;
		char * C_PTemp = NULL;

		memset(gstGpsInfo.u8_Send_Data, 0, sizeof(gstGpsInfo.u8_Send_Data));
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u16_FrameNum);
		IndexNum += data_len;
		// GPS INFO .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"**RMC**");
		IndexNum += data_len;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u16_FrameErrNum);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// TIME .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"TIME:");
		IndexNum += data_len;

		u8_temp = (ASCII2Hex(gstGpsInfo.RMC_Data.u8_time[0])*10 + ASCII2Hex(gstGpsInfo.RMC_Data.u8_time[1]))+9;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",u8_temp);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'h';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_time[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_time[3];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'm';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_time[4];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_time[5];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 's';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Status .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"STATUS:");
		IndexNum += data_len;
		if(gstGpsInfo.RMC_Data.u8_Status[0] == 'A')
			data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"ACTIVE");
		else
			data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"VOID");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Latit .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"LATI:");
		IndexNum += data_len;

		F_temp = strtof((char*)(gstGpsInfo.RMC_Data.u8_Latit+2), &C_PTemp);
		if(F_temp >= 60)
		{
			if(gstGpsInfo.RMC_Data.u8_Latit[1] >= 0x39)
			{
				gstGpsInfo.RMC_Data.u8_Latit[1] = 0x30;
				gstGpsInfo.RMC_Data.u8_Latit[0] += 0x31;
			}
			else
				gstGpsInfo.RMC_Data.u8_Latit[1] += 0x31;
			F_temp = ((F_temp/60) - 1)*1000;
		}
		else
			F_temp = (F_temp/60)*1000;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Latit[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Latit[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '.';
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",(u8)F_temp);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// DIRECT N or S.//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"DIR_NS:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_DirecN_S[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// LONGIT .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"LONGIT:");
		IndexNum += data_len;

		F_temp = strtof((char*)(gstGpsInfo.RMC_Data.u8_Longit+3), &C_PTemp);
		if(F_temp >= 60)
		{
			if(gstGpsInfo.RMC_Data.u8_Longit[2] >= 0x39)
			{
				gstGpsInfo.RMC_Data.u8_Longit[2] = 0x30;
				gstGpsInfo.RMC_Data.u8_Longit[1] += 0x31;
			}
			else
				gstGpsInfo.RMC_Data.u8_Longit[2] += 0x31;
			F_temp = ((F_temp/60) - 1)*1000;
		}
		else
			F_temp = (F_temp/60)*1000;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Longit[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Longit[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Longit[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '.';
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"%d",(u8)F_temp);
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// DIRECT E or W.//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"DIR_EW:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_DirecE_W[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';


		// u8_SpedOve .//
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"SPEED ");
		IndexNum += data_len;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"OVER G:");
		IndexNum += data_len;

		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[3];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[4];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[5];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_SpedOve[6];
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"Knot");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_Degree;
		data_len = snprintf((char*)&gstGpsInfo.u8_Send_Data[IndexNum],100,"DEGREE:");
		IndexNum += data_len;
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Degree[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Degree[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Degree[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Degree[3];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_Degree[4];

		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// u8_UTC;
		gstGpsInfo.u8_Send_Data[IndexNum++] = '2';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '0';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[4];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[5];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'Y';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[2];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[3];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'M';
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[0];
		gstGpsInfo.u8_Send_Data[IndexNum++] = gstGpsInfo.RMC_Data.u8_UTC[1];
		gstGpsInfo.u8_Send_Data[IndexNum++] = 'D';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		// END .//
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\r';
		gstGpsInfo.u8_Send_Data[IndexNum++] = '\n';

		XUartPs_Send(&UartPs, (gstGpsInfo.u8_Send_Data), IndexNum);
	}
}


void GGA_Data_Split(u8* GPS_Data, T_GGA *Gps_SplitData, u8 Data_L)
{
	int num = 0;
	int Data_index = 0;

	for(int index = 0; index < Data_L; index ++)
	{
		switch(Data_index)
		{
			case 0:
				Gps_SplitData->u8_Info[num] = GPS_Data[index];
				num++;
				break;
			case 1:
				Gps_SplitData->u8_time[num] = GPS_Data[index];
				num++;
				break;
			case 2:
				Gps_SplitData->u8_Latit[num] = GPS_Data[index];
				num++;
				break;
			case 3:
				Gps_SplitData->u8_Direc[num] = GPS_Data[index];
				num++;
				break;
			case 4:
				Gps_SplitData->u8_Longit[num] = GPS_Data[index];
				num++;
				break;
			case 5:
				Gps_SplitData->u8_CardPoint[num] = GPS_Data[index];
				num++;
				break;
			case 6:
				Gps_SplitData->u8_GPS_use[num] = GPS_Data[index];
				num++;
				break;
			case 7:
				Gps_SplitData->u8_NumSatil[num] = GPS_Data[index];
				num++;
				break;
			case 8:
				Gps_SplitData->u8_HDOP[num] = GPS_Data[index];
				num++;
				break;
			case 9:
				Gps_SplitData->u8_Geo_High[num] = GPS_Data[index];
				num++;
				break;
			case 10:
				Gps_SplitData->u8_Geo_meter[num] = GPS_Data[index];
				num++;
				break;
			case 11:
				Gps_SplitData->u8_Elip_Geo[num] = GPS_Data[index];
				num++;
				break;
			case 12:
				Gps_SplitData->u8_Elip_meter[num] = GPS_Data[index];
				num++;
				break;
			case 13:
				Gps_SplitData->u8_Age[num] = GPS_Data[index];
				num++;
				break;
			default:
				break;
		}


		if(GPS_Data[index] == ',')
		{
			Data_index++;
			num = 0;
		}
	}
}

void RMC_Data_Split(u8* GPS_Data, T_RMC *Gps_SplitData, u8 Data_L)
{
	int num = 0;
	int Data_index = 0;

	for(int index = 0; index < Data_L; index ++)
	{
		switch(Data_index)
		{
			case 0:
				Gps_SplitData->u8_Info[num] = GPS_Data[index];
				num++;
				break;
			case 1:
				Gps_SplitData->u8_time[num] = GPS_Data[index];
				num++;
				break;
			case 2:
				Gps_SplitData->u8_Status[num] = GPS_Data[index];
				num++;
				break;
			case 3:
				Gps_SplitData->u8_Latit[num] = GPS_Data[index];
				num++;
				break;
			case 4:
				Gps_SplitData->u8_DirecN_S[num] = GPS_Data[index];
				num++;
				break;
			case 5:
				Gps_SplitData->u8_Longit[num] = GPS_Data[index];
				num++;
				break;
			case 6:
				Gps_SplitData->u8_DirecE_W[num] = GPS_Data[index];
				num++;
				break;
			case 7:
				Gps_SplitData->u8_SpedOve[num] = GPS_Data[index];
				num++;
				break;
			case 8:
				Gps_SplitData->u8_Degree[num] = GPS_Data[index];
				num++;
				break;
			case 9:
				Gps_SplitData->u8_UTC[num] = GPS_Data[index];
				num++;
				break;
			case 10:
				Gps_SplitData->u8_MagVari[num] = GPS_Data[index];
				num++;
				break;
			default:
				break;
		}


		if(GPS_Data[index] == ',')
		{
			Data_index++;
			num = 0;
		}
	}
}

u8 ASCII2Hex(u8 Ascii)
{
	u8 re_hex;
	if(Ascii != 0)
	{
		if(Ascii <= 0x39)
		{
			re_hex = Ascii - 0x30;
		}
		else
		{
			re_hex = Ascii - 0x37;
		}
	}else;

	return re_hex;
}


void GPS_Err_Chk(void* pvParameter)
{
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if(gu_GpsReg.bErrorFind == TRUE)
		{
			ReceiveCount = 0;
			gu32_GpsErrTmr = 0;
			memset(TotalErrorCount, 0 , sizeof(TotalErrorCount));
			memset(gstGpsInfo.u8_Send_Data, 0, sizeof(gstGpsInfo.u8_Send_Data));
			memset(gstGpsInfo.u8_GPS_Data, 0, sizeof(gstGpsInfo.u8_GPS_Data));
			u16_FrameErrNum++;
			xEventGroupSetBits(xEvent_LedHandle, BIT_0);
			gu_GpsReg.bErrorFind = FALSE;
			xTaskNotifyGive(Task_GPS_Rece);
		}
	}
}



void Handler(void *CallBackRef, u32 Event, unsigned int EventData)
{
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;
	/* All of the data has been sent */
	if (Event == XUARTPS_EVENT_SENT_DATA) {
		TotalSentCount = EventData;
		xEventGroupSetBitsFromISR(xEvent_LedHandle, BIT_0, &xHigherPriorityTaskWoken);
		gu32_GpsErrTmr = 0;
#ifdef __TIME_CHECK__
		XTime_GetTime(&tEnd);
		period = tEnd - tStart;
		D_TimeChk = (double)period / (double)COUNTS_PER_SECOND;
		if((Time_BufferIndex >= 100)&&(Time_BufferIndex <101))
		{
			Time_BufferIndex++;
			avg_TimeChk = avg_TimeChk / 100;
		}
		else if(Time_BufferIndex < 100)
		{
			Time_save[Time_BufferIndex] = D_TimeChk;
			if(Time_BufferIndex > 0)
				avg_TimeChk += Time_save[Time_BufferIndex];
			Time_BufferIndex++;
		}
#endif
		u32_Led_Sel = FALSE;
		vTaskNotifyGiveFromISR(xGpio_Led,0);
	}

	/* All of the data has been received */
	if (Event == XUARTPS_EVENT_RECV_DATA) {
		TotalReceivedCount = EventData;
		vTaskNotifyGiveFromISR(Task_GPS_Rece, 0);
	}

	/*
	 * Data was received, but not the expected number of bytes, a
	 * timeout just indicates the data stopped for 8 character times
	 */
	if (Event == XUARTPS_EVENT_RECV_TOUT) {
		TotalReceivedCount = EventData;
	}

	/*
	 * Data was received with an error, keep the data but determine
	 * what kind of errors occurred
	 */
	if (Event == XUARTPS_EVENT_RECV_ERROR) {
		TotalReceivedCount = EventData;
		TotalErrorCount[0]++;
	}

	/*
	 * Data was received with an parity or frame or break error, keep the data
	 * but determine what kind of errors occurred. Specific to Zynq Ultrascale+
	 * MP.
	 */
	if (Event == XUARTPS_EVENT_PARE_FRAME_BRKE) {
		TotalReceivedCount = EventData;
		TotalErrorCount[1]++;
	}

	/*
	 * Data was received with an overrun error, keep the data but determine
	 * what kind of errors occurred. Specific to Zynq Ultrascale+ MP.
	 */
	if (Event == XUARTPS_EVENT_RECV_ORERR) {
		TotalReceivedCount = EventData;
		TotalErrorCount[2]++;
	}

	if((TotalErrorCount[0] > 200)||(TotalErrorCount[1] > 200)||(TotalErrorCount[2] > 200))
	{
		gu_GpsReg.bErrorFind = TRUE;
		vTaskNotifyGiveFromISR(Task_Err_Chk, 0);
	}


	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SetupInterruptSystem(XScuGic *IntcInstancePtr,
				XUartPs *UartInstancePtr,
				u16 UartIntrId)
{
	int Status;

	XScuGic_Config *IntcConfig; /* Config for interrupt controller */
	/* Initialize the interrupt controller driver */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}

	/*
	 * Connect the interrupt controller interrupt handler to the
	 * hardware interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				(Xil_ExceptionHandler) XScuGic_InterruptHandler,
				IntcInstancePtr);

	/*
	 * Connect a device driver handler that will be called when an
	 * interrupt for the device occurs, the device driver handler
	 * performs the specific interrupt processing for the device
	 */
	Status = XScuGic_Connect(IntcInstancePtr, UartIntrId,
				  (Xil_ExceptionHandler) XUartPs_InterruptHandler,
				  (void *) UartInstancePtr);
	if (Status != XST_SUCCESS) {
		xil_printf("UART Interrupt Example Test Failed\r\n");
	}

	/* Enable the interrupt for the device */
	XScuGic_Enable(IntcInstancePtr, UartIntrId);


	/* Enable interrupts */
	 Xil_ExceptionEnable();

}



