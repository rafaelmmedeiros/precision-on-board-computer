#ifndef LCD_init_h
#define LCD_init_h
#include "Arduino.h"
#include <SPI.h>

// NOTE (P-OBC): original demo used GPIO 11/13/12 for the ST7701S config
// SW-SPI. On ESP32 WROOM-32, GPIO 6-11 are strictly reserved for the internal
// SPI flash; driving them corrupts flash reads. Remapped to safe GPIOs.
// These pins are ONLY used once during ST7701S_Initial().
#define  SPI_CS     17
#define  SPI_CLK    14
#define  SPI_DI      4


#define  SPI_CS_RES   digitalWrite(SPI_CS, LOW)
#define  SPI_CS_SET   digitalWrite(SPI_CS, HIGH)
#define  SPI_CLK_RES   digitalWrite(SPI_CLK, LOW)
#define  SPI_CLK_SET   digitalWrite(SPI_CLK, HIGH)
#define  SPI_DI_RES   digitalWrite(SPI_DI, LOW)
#define  SPI_DI_SET   digitalWrite(SPI_DI, HIGH)



void HW_SPI_Send(unsigned char i)
{ 
  SPI.transfer( i);
}




//RGB+9b_SPI(rise)
 void SW_SPI_Send(unsigned char i)
{  
   unsigned char n;
   
   for(n=0; n<8; n++)			
   {       
			SPI_CLK_RES;
			if(i&0x80)SPI_DI_SET ;
                        else SPI_DI_RES ;

			SPI_CLK_SET;

			i<<=1;
	  
   }
}

void SPI_WriteComm(unsigned char i)
{
    SPI_CS_RES;
    SPI_DI_RES;//DI=0 COMMAND
 
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);
    SW_SPI_Send(i);	
    SPI_CS_SET;	
}



void SPI_WriteData(unsigned char i)
{ 
    SPI_CS_RES;
    SPI_DI_SET;//DI=1 DATA
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);   
    SW_SPI_Send(i);
    SPI_CS_SET;  		
} 


void ST7701S_Initial(void)
{  
     pinMode(SPI_CS,   OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    pinMode(SPI_CLK,   OUTPUT);
    digitalWrite(SPI_CLK, LOW);
    pinMode(SPI_DI,   OUTPUT);
    digitalWrite(SPI_DI, LOW);  
  

	//ST7701S+AUO4.58
	SPI_WriteComm (0xFF);     
	SPI_WriteData (0x77); 
	SPI_WriteData (0x01);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x13);   
	
	SPI_WriteComm (0xEF);     
	SPI_WriteData (0x08);   
	
	SPI_WriteComm (0xFF);     
	SPI_WriteData (0x77);   
	SPI_WriteData (0x01);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x10);   
	
	SPI_WriteComm (0xC0);     
	SPI_WriteData (0x77);   
	SPI_WriteData (0x00);   
	
	SPI_WriteComm (0xC1);     
	SPI_WriteData (0x09);   
	SPI_WriteData (0x08);   
	
	SPI_WriteComm (0xC2);//inv     
	SPI_WriteData (0x37);   
	SPI_WriteData (0x02);  
	
	SPI_WriteComm (0xC3); //????    
	SPI_WriteData (0x80);
	SPI_WriteData (0x05);
	SPI_WriteData (0x0d);	 
	
	SPI_WriteComm (0xCC);     
	SPI_WriteData (0x10);   
	
	SPI_WriteComm (0xB0);     
	SPI_WriteData (0x40);   
	SPI_WriteData (0x14);   
	SPI_WriteData (0x59);   
	SPI_WriteData (0x10);   
	SPI_WriteData (0x12);   
	SPI_WriteData (0x08);   
	SPI_WriteData (0x03);   
	SPI_WriteData (0x09);   
	SPI_WriteData (0x05);   
	SPI_WriteData (0x1E);   
	SPI_WriteData (0x05);   
	SPI_WriteData (0x14);   
	SPI_WriteData (0x10);   
	SPI_WriteData (0x68);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x15);   
	
	SPI_WriteComm (0xB1);     
	SPI_WriteData (0x40);   
	SPI_WriteData (0x08);   
	SPI_WriteData (0x53);   
	SPI_WriteData (0x09);   
	SPI_WriteData (0x11);   
	SPI_WriteData (0x09);   
	SPI_WriteData (0x02);   
	SPI_WriteData (0x07);   
	SPI_WriteData (0x09);   
	SPI_WriteData (0x1A);   
	SPI_WriteData (0x04);   
	SPI_WriteData (0x12);   
	SPI_WriteData (0x12);   
	SPI_WriteData (0x64);   
	SPI_WriteData (0x29);   
	SPI_WriteData (0x29);   
	
	SPI_WriteComm (0xFF);     
	SPI_WriteData (0x77);   
	SPI_WriteData (0x01);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x11);   
	
	SPI_WriteComm (0xB0);     
	SPI_WriteData (0x6D);  //6D 
	
	SPI_WriteComm (0xB1);   //vcom  
	SPI_WriteData (0x1D);   
	
	SPI_WriteComm (0xB2);     
	SPI_WriteData (0x87);   
	
	SPI_WriteComm (0xB3);     
	SPI_WriteData (0x80);   
	
	SPI_WriteComm (0xB5);     
	SPI_WriteData (0x49);   
	
	SPI_WriteComm (0xB7);     
	SPI_WriteData (0x85);   
	
	SPI_WriteComm (0xB8);     
	SPI_WriteData (0x20);   
	
	SPI_WriteComm (0xC1);     
	SPI_WriteData (0x78);   
	
	SPI_WriteComm (0xC2);     
	SPI_WriteData (0x78);   
	
	SPI_WriteComm (0xD0);     
	SPI_WriteData (0x88);   
	
	SPI_WriteComm (0xE0);     
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x02);   
	
	SPI_WriteComm (0xE1);     
	SPI_WriteData (0x02);   
	SPI_WriteData (0x8C);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x03);   
	SPI_WriteData (0x8C);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	
	SPI_WriteComm (0xE2);     
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0xC9);   
	SPI_WriteData (0x3C);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0xCA);   
	SPI_WriteData (0x3C);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	
	SPI_WriteComm (0xE3);     
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	
	SPI_WriteComm (0xE4);     
	SPI_WriteData (0x44);   
	SPI_WriteData (0x44);   
	
	SPI_WriteComm (0xE5);     
	SPI_WriteData (0x05);   
	SPI_WriteData (0xCD);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x01);   
	SPI_WriteData (0xC9);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x07);   
	SPI_WriteData (0xCF);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x03);   
	SPI_WriteData (0xCB);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	
	SPI_WriteComm (0xE6);     
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x33);   
	SPI_WriteData (0x33);   
	
	SPI_WriteComm (0xE7);     
	SPI_WriteData (0x44);   
	SPI_WriteData (0x44);   
	
	SPI_WriteComm (0xE8);     
	SPI_WriteData (0x06);   
	SPI_WriteData (0xCE);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x02);   
	SPI_WriteData (0xCA);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x08);   
	SPI_WriteData (0xD0);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x04);   
	SPI_WriteData (0xCC);   
	SPI_WriteData (0x82);   
	SPI_WriteData (0x82);   
	
	SPI_WriteComm (0xEB);     
	SPI_WriteData (0x08);   
	SPI_WriteData (0x01);   
	SPI_WriteData (0xE4);   
	SPI_WriteData (0xE4);   
	SPI_WriteData (0x88);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x40);   
	
	SPI_WriteComm (0xEC);     
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	
	SPI_WriteComm (0xED);     
	SPI_WriteData (0xFF);   
	SPI_WriteData (0xF0);   
	SPI_WriteData (0x07);   
	SPI_WriteData (0x65);   
	SPI_WriteData (0x4F);   
	SPI_WriteData (0xFC);   
	SPI_WriteData (0xC2);   
	SPI_WriteData (0x2F);   
	SPI_WriteData (0xF2);   
	SPI_WriteData (0x2C);   
	SPI_WriteData (0xCF);   
	SPI_WriteData (0xF4);   
	SPI_WriteData (0x56);   
	SPI_WriteData (0x70);   
	SPI_WriteData (0x0F);   
	SPI_WriteData (0xFF);   
	
	SPI_WriteComm (0xEF);     
	SPI_WriteData (0x10);   
	SPI_WriteData (0x0D);   
	SPI_WriteData (0x04);   
	SPI_WriteData (0x08);   
	SPI_WriteData (0x3F);   
	SPI_WriteData (0x1F);   
	
	SPI_WriteComm (0xFF);     
	SPI_WriteData (0x77);   
	SPI_WriteData (0x01);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	SPI_WriteData (0x00);   
	
	SPI_WriteComm (0x11);     
	delay(120);                
	
	SPI_WriteComm (0x35);     
	SPI_WriteData (0x00);   
	
	SPI_WriteComm (0x3A);     
	SPI_WriteData (0x66);   
	
	SPI_WriteComm (0x29); 



}

#endif


