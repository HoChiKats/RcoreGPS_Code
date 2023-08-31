#define UART_DEVICE_ID		XPAR_XUARTPS_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define UART_INT_IRQ_ID		XPAR_XUARTPS_1_INTR

#define GPS_ALL_FRAME	0
#define GPS_GGA_FRAME	1
#define GPS_RMC_FRAME	2
#define __TIME_CHECK__

typedef union
{
	u32 dat;
	struct
	{
		u32 bGGA:1;
		u32 bRMC:1;
		u32 bDataSend:1;
		u32 bErrorFind:1;
		u32 bGPS_Stop:1;
		u32 b2_GpsSel:3;

		u32 b8:1;
		u32 b9:1;
		u32 b10:1;
		u32 b11:1;
		u32 b12:1;
		u32 b13:1;
		u32 b14:1;
		u32 b15:1;

		u32 b16_23:8;
		u32 b24_31:8;
	};
}T_REG;
extern T_REG gu_GpsReg;

typedef struct
{
	u8	u8_Info[20];
	u8	u8_time[20];
	u8	u8_Latit[20];
	u8	u8_Direc[20];
	u8	u8_Longit[20];
	u8	u8_CardPoint[20];
	u8	u8_GPS_use[20];
	u8	u8_NumSatil[20];
	u8	u8_HDOP[20];
	u8	u8_Geo_High[20];
	u8	u8_Geo_meter[20];
	u8	u8_Elip_Geo[20];
	u8	u8_Elip_meter[20];
	u8	u8_Age[20];
}T_GGA;

typedef struct
{
	u8	u8_Info[20];
	u8	u8_time[20];
	u8	u8_Status[20];
	u8	u8_Latit[20];
	u8	u8_DirecN_S[20];
	u8	u8_Longit[20];
	u8	u8_DirecE_W[20];
	u8	u8_SpedOve[20];
	u8	u8_Degree[20];
	u8	u8_UTC[20];
	u8	u8_MagVari[20];
}T_RMC;

typedef struct
{
	u8	u8_Send_Data[500];
	u8	u8_Temp_Data[130];
	u8	u8_GPS_Data[130];
	u8	u8_GPS_ErrFrame[50];
	u8 	u8_CRC_Data[2];
	u8	u8_CRC_Temp;
	u32	u32_CRC_Lenght;
	T_GGA	GGA_Data;
	T_RMC	RMC_Data;
}T_GPS_INFO;

extern void GPS_Config(void * pvParameter);
extern void GPS_Data_Recv(void * pvParameter);
extern void GPS_DataParsing(void * pvParameter);
extern void GGA_Data_Disp(void * pvParameter);
extern void RMC_Data_Disp(void * pvParameter);
extern void GPS_Err_Chk(void* pvParameter);

extern void GPS_madeCRC(void);
void Uart_Config(XScuGic *IntcInstPtr, XUartPs *UartInstPtr, u16 DeviceId, u16 UartIntrId);
//extern void GPS_Err_ChkTimer(void);
void GGA_Data_Split(u8* GPS_Data, T_GGA *Gps_SplitData, u8 Data_L);
void RMC_Data_Split(u8* GPS_Data, T_RMC *Gps_SplitData, u8 Data_L);

u8 ASCII2Hex(u8 Ascii);
void SetupInterruptSystem(XScuGic *IntcInstancePtr, XUartPs *UartInstancePtr, u16 UartIntrId);
void Handler(void *CallBackRef, u32 Event, unsigned int EventData);

extern XUartPs UartPs;		/* Instance of the UART Device */

extern TaskHandle_t Task_Uart_Set, Task_GPS_Rece, Task_GPS_Parsing, Task_GGA_Send, Task_RMC_Send, Task_Err_Chk;
extern QueueHandle_t xQueue_GpsData, xQueue_GpsGGA, xQueue_GpsRMC;
extern EventGroupHandle_t xEvent_LedHandle;
extern u32 gu32_GpsErrTmr;
