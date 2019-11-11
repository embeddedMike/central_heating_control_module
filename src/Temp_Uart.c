/*
 * Temp_Uart.c
 *
 *  Created on: 1 maj 2016
 *      Author: Micha�
 */

#include "include/Temp_Uart.h"


void set_baud(uint32_t baud) // ustawienie baudrate
{
	USART1->CR1 &= ~USART_CR1_UE;
	//USART1->BRR = (FREQUENCY + baud / 2) / baud;
	USART1->BRR = 8000000/baud; // tu zmiana
	USART1->CR1 |= USART_CR1_UE;
}

void ow_init(void){

	// PA9 - TX, PA10 - RX, Open Drain

	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

	//GPIOA->CRH |= (GPIOA->CRH & (~(GPIO_CRH_MODE9 | GPIO_CRH_MODE10))) | (GPIO_CRH_MODE9_1 | GPIO_CRH_MODE10_1);
    //GPIOA->CRH |= (GPIOA->CRH & (~(GPIO_CRH_CNF9 | GPIO_CRH_CNF10))) | (GPIO_CRH_CNF9 | GPIO_CRH_CNF10); // funkcja alternatywna

    gpio_pin_cfg(USART_GPIO, USART_TX, GPIO_CRx_MODE_CNF_ALT_OD_2M_value);
    gpio_pin_cfg(USART_GPIO, USART_RX, GPIO_CRx_MODE_CNF_IN_FLOATING_value);

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE;
    //USART1->BRR = (FREQUENCY + 9600 / 2) / 9600;
    USART1->BRR = 8000000/9600;
    USART1->CR1 |= USART_CR1_UE;

}

uint8_t uart_txrx(uint8_t data)
{
	while(!(USART1->SR & USART_SR_TXE));
	USART1->DR = data; // wpisanie warto�ci do rejestru


	while(!(USART1->SR & USART_SR_RXNE)); // TU COS NIE GRA jest co� na 800 str. manuala

	return USART1->DR;
}

uint8_t ow_reset_pulse(void) // tutaj u�yty set_baud bo jest RESET PULSE
{
	uint8_t temp;
	set_baud(9600); // dlaczego zmiana baudrate? RESET i PRESENCE PULSE s� sygna�ami o stosunkowo d�ugim czasie
	// trwania dlatego ustawiany jest w wolniejszy tryb 9600 bod�w(dzi�ki temu wyd�u�a si� czas pr�bkowania magistrali o 1000 us)

	temp = uart_txrx(0xF0); // wpisanie 0xF0 powoduje wygenerowanie RESET PULSE czyli sekwencj� inicjalizacyjn�
	if(temp == 0xF0) return DS_ERROR;
	// o odebraniu sygna�u PRESENCE PULSE �wiadczy warto�c rejestru r�na od 0xF0
	// gdy warto�c rejsetru r�wna 0xF0 �wiadczy to o braku urz�dze� na magistrali
	else return DS_OK;
}

void ow_write_1b(uint8_t data) // ta funkcja umo�liwia nadawanie bt�w o warto�ci 0 lub 1
{
	// tutaj ju� zainicjalizowana magistrala wi�c mo�na przej�c w tryb 115200 bod�w
	if((data & 0x01)==1)//nam�odszy bit jest tym kt�ry wysy�amy, bo tak sobie przyj��em
	{
		set_baud(115200);
		uart_txrx(0xFF); // generowany impuls ujemny o czasie trwania ok 8,6us
		// oxFF to jeden bit
	}
	else
	{
		set_baud(115200);
		uart_txrx(0x00); // generowany impuls usjemny o czsie trwania ok 86 us
	}
}

uint8_t ow_read_1b(void) //odbiera jeden bit z magistrali
{
	set_baud(115200);
	uint8_t temp = uart_txrx(0xFF); // wpisanie do rejestru nadajnika 0xFF wi��e si� z wygenerowaniem
	// kr�tkiego impulsu ujemnego(bitu startu). Jest to pocz�tek procesu odczytu bitu z magistrali
	// przypisuj� do zmiennej temp bo odczytuj� temperatur�.

	if(temp == 0xFF)// u francuza w ksi��ce jest tak napisane. Ma tak byc
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


// "WYSOKOPOZIOMOWE FUNKCJE:"

void ow_write(uint8_t data) // wysy�am bajt na magistral�
{
	uint8_t i;
	for(i=0;i<8;++i) // u�ywam p�tli i niskopoziomowej funkcji do przes�ania 1 bitu i w ten spos�b przys�am bajt
	{
		ow_write_1b(data); // tutaj u�yta funkcja do przes�ania 1 bitu
		data= data>>1; //  przesuni�cie bitowe o jeden
	}
}

uint8_t ow_read(void) // odczyt bajtu z magistrali
{
	uint8_t i, temp, dat=0;
	for(i=0;i<8;++i)
	{
		temp = ow_read_1b();
		dat |= (0x01 & temp)<<i; // do ogarni�cia ta linijka
	}
	return dat; // to jak result u francuza
}

uint8_t ds18b20_convert(void)
{
	uint8_t error;
	error = ow_reset_pulse(); // tutaj zostaje przypisane to co zwr�ci funkcja
	if(error==DS_OK) // je�eli zostanie wykryte urz�dznie na magistrali to warunek spe�niony
	{
		ow_write(DS_SKIP_ROM); //  ma byc
		ow_write(DS_CONVERT_T); // pojedy�cza konwersja
		return DS_OK;
	}
	else return DS_ERROR;
}

uint8_t ds18b20_read_temperature(uint8_t *znak, uint8_t *temperatur, uint8_t *reszta)
{
	uint8_t ok, crc_temp=0, th, tl, temp_reszta, temperatura;//crc_temp, temp,
	uint32_t  t=0;
	//odczyt temp z ds
	ok = ow_reset_pulse();
	if(ok==DS_ERROR) return DS_ERROR;
	ow_write(DS_SKIP_ROM); // wczytuj� komend�
	ow_write(DS_READ_SCRATCHPAD);
	tl = ow_read(); // tl
	th = ow_read(); // th


	//konwersja temp opisane na mikrokontrolery.blogspot
    //http://mikrokontrolery.blogspot.com/2011/04/temperatura-wyswietlacz-konwersja.html

	t = (((uint32_t)(th))<<8) + tl; // th to MSB tl to LSB ,  dlatego te� rzutowanie
	if(t>0x8FFF) // tabelka z digital output 0x8fff to -0,5 stopnia, wi�ksze warto�ci to dalej temp ujemna
	{// z kolei mniejsze warto�ci to temperatura dodatnia
		//temperatura ujemna
		*znak = DS_ZNAK_MINUS; // wykorzystywane w mainie w wyra�eniu warunkowym
		t = 0xFFFF-t+1; // +1 bo minus i ca�o�c zanegowana
		temp_reszta = (uint8_t)(0x000F & t); // na pierwszych czterech bitach cz�c u�amkowa
		// dlatego jest maska na tych bitach bo tylko t� cz�c chce miec
		temperatura = (uint8_t)(t>>4);
		*temperatur = temperatura;
		*reszta = temp_reszta;
	}
	else
	{
		//temperatura dodatnia
		*znak = DS_ZNAK_PLUS;
		temp_reszta = (uint8_t)(0x000F & t);
		temperatura = (uint8_t)(t>>4);
		*temperatur = temperatura;
		*reszta = temp_reszta;
	}
	if(crc_temp == 0) return DS_OK;
	return DS_CRC_ERROR;
}
