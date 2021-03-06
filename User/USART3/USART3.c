#include "USART3.h"

/****************************USART3  内存分配******************************/
uint8_t USART3_SendData[USART3_SendSizeMax];                               //串口数据发送缓冲区
uint8_t USART3_ReceiveData[USART3_ReceiveSizeMax];                         //串口数据接收缓冲区
uint16_t USART3_SendFront ,USART3_SendRear ,USART3_SendNextStart;          //串口发送缓冲区数据管理
uint16_t USART3_ReceiveFront ,USART3_ReceiveRear ;                         //串口接收缓冲区数据管理
uint8_t USART3_SendBusyFlag  ;                                             //串口DMA发送数据忙碌标志位 ，1：正在发送数据 ，0：DMA不处于数据发送 ，此时可以触发发送下一次数据


/****************************USART3  内部函数******************************/ 
static void USART3_GPIO_Config(void);                                      //配置串口USART3数据传送引脚I/O口                 
static void USART3_Mode_Config(uint32_t Baudrate);                         //配置串口USART3工作模式 
static void USART3_NVIC_Config(void);                                      //中断优先级设置
static void USART3_DMA_Config(void);                                       //USART3 DMA 配置
static void USART3_DataSendTrigger(void);                                  //根据DMA是否正在发送，和 内存区是否还有数据触发下一次数据的发送
static void USART3_MemoryInit(void);                                       //初始化串口缓冲区数据管理



/****************************USART3  用户函数******************************/ 
void USART3_Init(uint32_t Baudrate);                                       //USART3串口配置初始化 ,Baudrate :波特率
ErrorStatus USART3_WriteDMASendMulData(uint8_t *Data , uint16_t Num);      //串口发送数据     
ErrorStatus USART3_ReadDMAReceiveMulData(uint8_t *Data , uint16_t Num);    //串口读取数据

void USART3_Debug(void);                                                  //调试使用 ，串口接收什么数据就给串口回数目数据（DMA方式）





/***************************硬件初始化函数*******************************/


/*************************************************************************
  * @brief  USART3串口配置全过程
  * @param  Baudrate ： 波特率
  * @retval 无
  * @notice 先配置串口工作模式再配置I/O ，因为配置I/O口再配置串口时，
  *         在配置I/O与配置串口之间时I/O引脚会输出一个默认的电平：低电平，因此造成了串口多出一个数据
**************************************************************************/ 
void USART3_Init(uint32_t Baudrate)
{
	/***************时钟配置*********************/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1   ,ENABLE);                  //Enable DMA clock 
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB  ,ENABLE);	                //配置串口GPIO引脚时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3 ,ENABLE);                  //配置串口USART3的时钟
	
	/***************内存配置*********************/
	USART3_MemoryInit();                                                    //内存缓冲区初始化
	
	/***************硬件配置*********************/
  USART3_NVIC_Config();                                                   //配置USART3中断优先级 
	USART3_Mode_Config(Baudrate);                                           //配置串口USART3工作模 ,波特率设置
	USART3_DMA_Config();                                                    //配置串口USART3 DMA 模式
  USART3_GPIO_Config();                                                   //配置串口USART3数据传送引脚I/O口
}



/*************************************************************************                
  * @brief  配置串口USART3数据传送引脚I/O口
  * @param  无
  * @retval 无 
  * @notice TXD推挽输出 ，RXD悬空输入 
*************************************************************************/
static void USART3_GPIO_Config(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		
		//串口3对应引脚复用映射
		GPIO_PinAFConfig(GPIOB,GPIO_PinSource10 ,GPIO_AF_USART3);              //GPIOB11复用为USART3
		GPIO_PinAFConfig(GPIOB,GPIO_PinSource11,GPIO_AF_USART3);               //GPIOB10复用为USART3
	
	  /* Configure USART3 */
	 	GPIO_InitStructure.GPIO_Pin    = GPIO_Pin_10 | GPIO_Pin_11;	          //RXD :PB11 ;  //TXD :PB10
		GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_AF;                        //复用方式
		GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;                       //推挽输出 
		GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_2MHz;                     //设置引脚速率为2MHz 
		GPIO_InitStructure.GPIO_PuPd   =  GPIO_PuPd_UP;                       //上拉
		GPIO_Init(GPIOB, &GPIO_InitStructure);                                //调用库函数，初始化
}




/*************************************************************************
  * @brief  配置串口USART3工作模式
  * @param  Baudrate ： 波特率 
  * @retval 无 
  * @notice 无
*************************************************************************/
static void USART3_Mode_Config(uint32_t Baudrate)
{
	USART_InitTypeDef USART_InitStructure;
  
	/* USART3 mode config */	
	USART_InitStructure.USART_BaudRate   = Baudrate ;                       //串口波特率：USART3_BAUDRATE
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;             //帧数据长度：8位
	USART_InitStructure.USART_StopBits   = USART_StopBits_1;                //停止位位数：1位
	USART_InitStructure.USART_Parity     = USART_Parity_No ;                //奇偶校验 ：无校验
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流
	USART_InitStructure.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;   //接收，发送使能
	USART_Init(USART3, &USART_InitStructure);                               //配置
	USART_ITConfig(USART3, USART_IT_IDLE , ENABLE );                        //接收总线空闲中断使能
	USART_ITConfig(USART3, USART_IT_TXE  , DISABLE);                        //发送缓冲区空中断关闭
	USART_ITConfig(USART3, USART_IT_TC   , DISABLE);                        //发送完成中断关闭
	USART_Cmd(USART3, ENABLE);                                              //串口USART3使能 
}




/*************************************************************************
  * @brief  配置串口USART3中断优先级
  * @param  无
  * @retval 无 
  * @notice 优先级 在 #include "USART3.h" 中用 define 宏定义
*************************************************************************/
static void USART3_NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure; 
	
	/* USART3_RX :USART3 RX ILDE configuration */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;	 	                    // 指定 USART3 接收空闲中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = USART3_IRQPreemptionPrio ;//抢占式优先级设置
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = USART3_IRQSubPrio;      //次优先级设置
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                         //中断使能
	NVIC_Init(&NVIC_InitStructure);                                         //配置寄存器
	
	/* USART3_RX :DMA1 configuration */
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream1_IRQn;	 	              // 指定 USART3 接收DMA完成中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = DMA1_Stream1_IRQPreemptionPrio  ;//抢占式优先级设置
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = DMA1_Stream1_IRQSubPrio ;               //次优先级设置
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                         //中断使能
	NVIC_Init(&NVIC_InitStructure);                                         //配置寄存器

	/* USART3_TX :DMA1  configuration */
  NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;                 // 指定串口USART3 dma发送完成中断
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = DMA1_Stream4_IRQPreemptionPrio ;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = DMA1_Stream4_IRQSubPrio;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                         //中断使能
  NVIC_Init(&NVIC_InitStructure);                                         //配置寄存器
	

} 



/*************************************************************************
  * @brief  配置串口USART3的DMA模式 
  * @param  无
  * @retval 无 
  * @notice 不使能发送DMA,在数据需要发送时使能DMA
*************************************************************************/
static void USART3_DMA_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	  
  /* USART3_TX :DMA1 Stream4 channel7 configuration */
  DMA_InitStructure.DMA_Channel            = DMA_Channel_7 ;              //通道 
  DMA_InitStructure.DMA_DIR                = DMA_DIR_MemoryToPeripheral ; //方向：从内存到外设
  DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t)USART3_SendData;	  //DMA源地址 ：内存地址
  DMA_InitStructure.DMA_BufferSize         = USART3_SendSizeMax;          //DMA传输的数据量
  DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable ;       //直接传送 ，不使能FIFO
  DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_1QuarterFull;  //FIFO 满闸门
  DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single ;     //单次
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;     //存储器数据长度:8位
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;        //内存地址加1
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;				  	  //单次传输
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DR ;      //DMA目的地址   : 串口1的数据发送地址
  DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;  //单次
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; //外设数据长度:8位
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;   //外设地址固定
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;           //DMA优先级
  DMA_Init(DMA1_Stream4,&DMA_InitStructure);                              //配置
	
	DMA_ITConfig(DMA1_Stream4,DMA_IT_TC,ENABLE);                            //DMA1 Stream4 数据传输完成中断使能
	USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);                          //USART3 发送DMA使能                
	DMA_Cmd(DMA1_Stream4, DISABLE);                                         //不使能DMA,在数据需要发送时使能DMA



	/* USART3_RX :DMA1 Stream1 channel4 configuration */
  DMA_InitStructure.DMA_Channel         = DMA_Channel_4 ;                //通道 
  DMA_InitStructure.DMA_DIR             = DMA_DIR_PeripheralToMemory ;   //方向：从外设到内存
  DMA_InitStructure.DMA_Mode            = DMA_Mode_Circular;					   //循环传输
	DMA_InitStructure.DMA_Memory0BaseAddr =(uint32_t)USART3_ReceiveData;   //DMA目的地址 ：内存地址
  DMA_Init(DMA1_Stream1,&DMA_InitStructure);                             //配置
 
	DMA_ITConfig(DMA1_Stream1,DMA_IT_TC, ENABLE);                          //DMA1 Stream1 数据传输完成中断打开
	USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE);                         //使能 USART3 数据接收DMA使能  
	DMA_Cmd(DMA1_Stream1, ENABLE);                                         //使能接收DMA,等待数据的到来

}





/***************************中断服务函数*********************************/


/*************************************************************************
  * @brief  USART3 中断服务函数(用于DMA数据接收时，串口空闲检测)
  * @param  无
  * @retval 无
  * @notice 这里用的是总线空闲中断  ， 一个字节时间的空闲时间产生中断 ；
  *         清除总线空闲中断标志位  只能通过先读USART_SR ,再读USART_DR软件序列来清零
*************************************************************************/
void USART3_IRQHandler(void) 
{
	/******空闲中断 ，记录现在数据接收到的位置*****/
	if(USART_GetITStatus(USART3,USART_IT_IDLE)!= RESET)                     //判断是否为总线空闲中断 ，表示接收到一次数据
	{	
		USART_ReceiveData(USART3);                                            //清除总线空闲中断标志位 ( 只能通过先读USART_SR ,再读USART_DR软件序列来清零) 
		USART3_ReceiveFront = USART3_ReceiveSizeMax - DMA_GetCurrDataCounter(DMA1_Stream1);  //保存现在接收到数据地址
	}
}






/*************************************************************************
  * @brief  DMA1_Channel4中断服务函数(用于DMA数据发送 ，发送完数据时产生中断)
  * @param  无
  * @retval 无
  * @notice USART3_SendRear 在这次数据开始发送时给下次发送数据的地址
            USART3_SendNextStart在这次数据发送完成将USART3_SendRear里的下次数据发送地址给保存
*************************************************************************/
void DMA1_Stream4_IRQHandler(void)
{
	/******一次数据发送完 ，触发下次数据的发送****/
	if(DMA_GetITStatus(DMA1_Stream4 , DMA_IT_TCIF4)!= RESET)                //判断是否为发送完成中断
  { 
    DMA_ClearITPendingBit(DMA1_Stream4 , DMA_IT_TCIF4);                   //清除DMA中断标志位 
		DMA_Cmd(DMA1_Stream4,DISABLE);                                        //一帧数据发送完成 ，关闭DMA发送的数据传送
	  USART3_SendNextStart = USART3_SendRear ;                              //正在传送的一帧数据发送完成，将下一次数据发送的地址 给 USART3_SendNextStart
	  USART3_SendBusyFlag  = 0 ;                                            //USART3 DMA 现在不处于忙碌状态 ，可以发送下一帧数据
	  USART3_DataSendTrigger() ;                                            //检测是否还有需要发送的数据，有则开启数据再次发送 
	}
}




/*************************************************************************
  * @brief  DMA1_Channe1 中断服务函数(用于DMA数据接收 ，在重复模式下，清除中断位后将再次开始接受数据
  * @param  无
  * @retval 无
  * @notice 主要的目的为 ：清除中断标志位 ，确保接收到的下一个字节将保存在USART3_ReceiveData[0]里
*************************************************************************/
void DMA1_Stream1_IRQHandler(void)
{
		/******清中断标志，下次数据从头接收****/
	if(DMA_GetITStatus(DMA1_Stream1 , DMA_IT_TCIF1)!= RESET)                //判断是否为接收DMA是否完成
  { 
    DMA_ClearITPendingBit(DMA1_Stream1 , DMA_IT_TCIF1);                   //清除DMA中断标志位 ， 在重复模式下 ，下一个字节将再次从USART3_ReceiveData[0]开始
	}
}









/***************************对缓冲内存的操作*******************************/

/*************************************************************************
  * @brief  USART3缓冲内存初始化
  * @param  无
  * @retval 无
  * @notice USART3_SendBusyFlag开始必须为 0 ，表示开始能够发送数据
*************************************************************************/
static void USART3_MemoryInit( void )
{
	USART3_SendBusyFlag = 0 ;                                               // 一开始串口空闲 ，可以发送数据
	USART3_SendFront    = 0 ;                                               // 发送数据缓冲的数据头地址初始化
	USART3_SendRear     = 0 ;                                               // 发送数据缓冲的数据尾地址初始化
	USART3_SendNextStart= 0 ;                                               // 下一帧数据发送的开始地址初始化
	USART3_ReceiveFront = 0 ;                                               // 接收数据缓冲的数据头地址初始化
	USART3_ReceiveRear  = 0 ;                                               // 接收数据缓冲的数据尾地址初始化
}



/*************************************************************************
  * @brief  串口数据发送触发 ，根据DMA是否正在发送，和 内存区是否还有数据配置下一次数据的发送
  * @param  无
  * @retval 无
  * @notice 在中断和主函数都有使用
*************************************************************************/
static void USART3_DataSendTrigger( void )
{
	uint16_t SendNum ;                                                      //需要发送的数据量 
	uint16_t SendNextRear;                                                  //在一次数据传输没有超过dma一次最大能够传输的字节数时的下一帧数据的开始位置
	
	/******判断是否需要启动DMA的数据发送 **********/
	if(USART3_SendBusyFlag  != 0 ) return ;                                 //串口上次数据还未发送完成 ，正处于忙碌状态 ，
	if( USART3_SendFront == USART3_SendRear ) return ;                      //缓冲区数据为空 ，没有数据需要发送
	
	/***在需要发送的情况下，计算需要发送的数据量***/
	if(USART3_SendFront > USART3_SendRear)                                  //根据数据头和数据尾的位置确定数据发送的量
	{
		SendNum      =  USART3_SendFront -  USART3_SendRear ;                 //帧头比帧尾大的一次性发送完成 	
		SendNextRear =  USART3_SendFront ;                                    //下一次数据的发送从USART3_SendFront 开始
	}
	else                                                                    //帧头比帧尾小的分两次数据发送 ，先发送  USART3_SendRear 到  USART3_SendSizeMax 的数据
	{                                                                       //再发送从0开始到USART3_SendFront 的数据
		SendNum      = USART3_SendSizeMax -  USART3_SendRear ;                //计算第一次需要发送的数据量
	  SendNextRear = 0 ;                                                    //下一次数据的发送从 0 开始
	}
	

	/**限制一次DMA最大传输的数据量，便于内存释放**/
	if( SendNum > USART3_DMASendOneSizeMax )	                              //比较现在需要传输的字节数与最大字节数的大小                           
	{
		SendNum = USART3_DMASendOneSizeMax ;                                  //超出最大的一次DMA数据传输 ，使用最大一次数据传输
		USART3_SendRear = USART3_SendRear + USART3_DMASendOneSizeMax ;        //下一次数据的发送从USART3_SendRear + USART3_DMASendOneSizeMax  开始
	}
	else
	{
		USART3_SendRear = SendNextRear ;                                      //下一次数据的发送从SendNextRear开始
	} 

	
	/******置位发送忙碌标志位 ，配置数据发送******/
	USART3_SendBusyFlag  = 1 ;                                              //忙碌状态
	DMA_Cmd(DMA1_Stream4,DISABLE);                                          //DMA_CNDTR需要DMA_CCRx中的EN=0时才能写入 
  DMA1_Stream4->M0AR = (uint32_t)&USART3_SendData[USART3_SendNextStart];  //设置数据发送的起始地址
  DMA1_Stream4->NDTR = SendNum;                                           //设置数据发送的数据量 ，单位 ：字节
	DMA_Cmd(DMA1_Stream4,ENABLE);                                           //使能DMA数据发送

}




/*************************************************************************
  * @brief  串口数据发送 ，根据串口是否忙碌再触发DMA数据的发送
  * @param  Data  ： 需发送数据量首地址
					  Num   ： 需发送的数据量
  * @retval 无
  * @notice if( Num >= IdleNum ) return ERROR ;处有等于号，留出一个字节不用 ，
  *         用于区分数据满 和 数据空
*************************************************************************/
ErrorStatus USART3_WriteDMASendMulData(uint8_t *Data , uint16_t Num)
{
	uint16_t  i ,IdleNum ;                                                  //缓冲区空闲的内存数                                               
	
	/******计算可用内存 ，判断是否还够用 ***********/
	if(USART3_SendFront >= USART3_SendNextStart )                           //根据情况计算还有多少内存够用 ，注意等于号“ = ” 
		IdleNum = USART3_SendSizeMax-USART3_SendFront + USART3_SendNextStart ;                   
	else
		IdleNum = USART3_SendNextStart - USART3_SendFront ;
	if( Num >= IdleNum ) return ERROR ;                                     //缓冲区内存不够 ，返回写入错误，流出一个字节不用，注意等于号“=”
	
	/*****在内存够用的情况将数据写入缓冲内存里******/
	for( i = 0 ; i < Num ; i++ )                                            //写入内存计数
	{
		USART3_SendData[USART3_SendFront] = *Data++ ;                         //将数据写入缓冲内存区
		if(USART3_SendFront == USART3_SendSizeMax - 1)                        //是否写到内存区尾部
			USART3_SendFront  = 0 ;                                             //写到内存区最后一个字节 ，下一个字节从0从新开始
		else
			USART3_SendFront++ ;                                                //正常情况计算下一个字节的保存位置
	}
	
	/******************触发数据的发送***************/
	USART3_DataSendTrigger();                                               //触发数据的发送 
	return SUCCESS;                                                         //数据传输成功 
}






/*************************************************************************
  * @brief  串口读取一帧数据 
  * @param  Data  ：读取数据保存位置 
  *         Num   ：读取到这帧数据的长度
  * @retval 无
  * @notice USART3_ReceiveFinishFlag 表示 一帧数据读取完成 ，没有待处理的数据 
*************************************************************************/
ErrorStatus USART3_ReadDMAReceiveMulData(uint8_t *Data , uint16_t Num)
{
	uint16_t  i , num ;                                                     //接收缓冲区的有效字节数
	
	/******判断缓冲区是否有Num个数据******/
	if( USART3_ReceiveFront >= USART3_ReceiveRear)                          //根据情况计算接收到的字节数
		num = USART3_ReceiveFront - USART3_ReceiveRear ;                      
	else                                                                    
		num = USART3_ReceiveSizeMax - USART3_ReceiveRear + USART3_ReceiveFront ; 
	if( num < Num ) return ERROR ;                                          //数据不够 ，返回读取失败 ：ERROR
	
	/******在数据够的情况下读取数据******/
	for( i = 0 ; i < Num ; i++ )                                            //复制数据计数
	{
			*Data++ = USART3_ReceiveData[USART3_ReceiveRear];                   //从接收缓冲内存读取数据保存到Data中
	    if(USART3_ReceiveRear == USART3_ReceiveSizeMax - 1 )                //是否读取到内存区尾部
				USART3_ReceiveRear = 0 ;                                          //读取到内存区最后一个字节 ，下一个字节从0从新开始
			else                          
				USART3_ReceiveRear++ ;                                            //正常情况下计算读取的下一个字节位置
	}
	return SUCCESS;                                                         //数据读取成功
}







///************************************************************************
//  * @brief  调试使用 ，串口接收什么数据就给串口回什么数据（DMA方式）
//  * @param  无
//  * @retval 无
//************************************************************************/
void USART3_Debug(void)
{
	uint8_t USART3_Dat[16];
	uint8_t USART3_Num = 16;

	if(USART3_ReadDMAReceiveMulData(USART3_Dat , USART3_Num ) == SUCCESS)            //读取接收的一帧数据
		USART3_WriteDMASendMulData(USART3_Dat , USART3_Num ) ;                         //发送接受到的一帧数据
}


/*********************************************END OF FILE**********************/
