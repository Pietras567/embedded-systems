/*************************************************************************************
 * 
 * @Authors: Kacper Bialek,
 *           Franciszek Pawlak,
 *           Piotr Janiszek,
 *
 ******************************************************************************/

#include "general.h"
#include <lpc2xxx.h>
#include <printf_P.h>
#include <printf_init.h>
#include <consol.h>
#include <config.h>
#include "irq/irq_handler.h"
#include "timer.h"
#include "VIC.h"
#include "lcd.h"
#include "additional.h"
#include "i2c.h"
#include "eeprom.h"
#include "adc.h"
#include "add_irq.h"

#include "Common_Def.h"
#include <stdio.h>


/*****************************************************************************
 * Global variables
 ****************************************************************************/
volatile tU32 ms;

/************************************************************************
 * @Description: uruchomienie obsługi przerwań
 * @Parameter:
 *    [in] period    : okres generatora przerwań
 *    [in] duty_cycle: wypełnienie w %
 * @Returns: Nic
 * @Side effects:
 *    przeprogramowany timer #1
 *************************************************************************/
static void init_irq (tU32 period, tU8 duty_cycle)
{
	//Zainicjuj VIC dla przerwań od Timera #1
    VICIntSelect &= ~TIMER_1_IRQ;           //Przerwanie od Timera #1 przypisane do IRQ (nie do FIQ)
    VICVectAddr5  = (tU32)IRQ_Test;         //adres procedury przerwania
    VICVectCntl5  = VIC_ENABLE_SLOT | TIMER_1_IRQ_NO;
    VICIntEnable  = TIMER_1_IRQ;            // Przypisanie i odblokowanie slotu w VIC od Timera #1

    T1TCR = TIMER_RESET;                    //Zatrzymaj i zresetuj
    T1PR  = 0;                              //Preskaler nieużywany
    T1MR0 = ((tU64)period)*((tU64)PERIPHERAL_CLOCK)/1000;
    T1MR1 = (tU64)T1MR0 * duty_cycle / 100; //Wypełnienie
    T1IR  = TIMER_ALL_INT;                  //Resetowanie flag przerwań
    T1MCR = MR0_I | MR1_I | MR0_R;          //Generuj okresowe przerwania dla MR0 i dodatkowo dla MR1
    T1TCR = TIMER_RUN;                      //Uruchom timer
}

/************************************************************************
 * @Description: Zapisanie nowego najwyzszego wyniku gry
 * @Parameter:
 *    [in] nowy_rekord    : rekord z ostatniej gry
 *    [in] aktualny_rekord: najwyzszy dotychczasowy rekord zapisany w pamieci
 * @Returns: Nic
 * @Side effects: Brak
 *************************************************************************/
void zapisz_wynik(int nowy_rekord, int aktualny_rekord)
{
    if (nowy_rekord > aktualny_rekord)
    {
        char wynik[6];
        sprintf(wynik, "%d", nowy_rekord);

        tS8 kod_bledu;
        kod_bledu = eepromWrite(0x0000,(tU8*) wynik, 6);
        if (kod_bledu == I2C_CODE_OK) {
            lcdGotoxy(5, 10);
            lcdPuts("Zapisano wynik");
            mdelay(1500);
        } else {
            lcdGotoxy(5, 10);
            lcdPuts("Blad zapisu");
            mdelay(1500);
        }
    }
}

/************************************************************************
 * @Description: Wczytanie wyniku gry z pamieci
 * @Parameter: Brak
 * @Returns: wynik, liczba calkowita okreslajaca wynik gracza
 * @Side effects: Brak
 *************************************************************************/
int wczytaj_wynik()
{
    tU8 testBuf[6];
    eepromPageRead(0x0000, testBuf, 6);
    int wynik;

    sscanf(testBuf, "%d", &wynik);

    return wynik;
}

/************************************************************************
 * @Description: Logika gry. Gra trwa 10 rund.
 * W kazdej rundzie uzytkownik ma za zadanie nacisnac przycisk odpowiadajacy
 * zapalonej diodzie w ograniczonym czasie.
 * @Parameter:
 *     [in] trudnosc    : Okresla czas, w ktorym uzytkownik musi nacisnac przycisk aby nie przegrac
 * @Returns: Nic
 * @Side effects: Brak
 *************************************************************************/
void game(int trudnosc){
    tBool przegrana = FALSE;
    int wynik = 0;
    int runda = 0;
    int czas = 0;
    if (trudnosc == 0){
		czas = 3000;
	} else if (trudnosc == 1){
		czas = 2000;
	} else if (trudnosc == 2){
		czas = 1000;
	}

    while (!przegrana && runda < 10) {
        mdelay(1000);

		T0TCR = 0x02;
		T0PR = (PERIPHERAL_CLOCK / 1000) - 1;
		T0MR0 = 4000;
		T0MCR = 0x03;
		T0TCR = 0x01;

        if (losowosc % 4 == 0) {

            IOCLR1 = DIODA_16;

            while (T0TC < czas) {

                if ((IOPIN1 & PRZYCISK_20) == 0)
                {
                    IOSET1 = DIODA_16;
                    runda++;
                    break;
                }
            }
            if (T0TC >= czas) {
                przegrana = TRUE;
                IOSET1 = DIODA_16;
            }
            wynik += (4000 - czas) - T0TC;
			losowosc = T0TC; // kontynuujemy losowe wybieranie di�d

        } else if (losowosc % 4 == 1) {

            IOCLR1 = DIODA_17;

			while (T0TC < czas) {

                if ((IOPIN1 & PRZYCISK_21) == 0) {
                    IOSET1 = DIODA_17;
                    runda++;
                    break;
                }
            }
            if (T0TC >= czas) {
                przegrana = TRUE;
                IOSET1 = DIODA_17;
            }
            wynik += (4000 - czas) - T0TC;
			losowosc = T0TC;

        } else if (losowosc % 4 == 2) {

            IOCLR1 = DIODA_18;

			while (T0TC < czas) {

                if ((IOPIN1 & PRZYCISK_22) == 0) {
                    IOSET1 = DIODA_18;
                    runda++;
                    break;
                }
            }
            if (T0TC >= czas) {
                przegrana = TRUE;
                IOSET1 = DIODA_18;
            }
            wynik += (4000 - czas) - T0TC;
			losowosc = T0TC;

        } else if (losowosc % 4 == 3) {

            IOCLR1 = DIODA_19;

			while (T0TC < czas) {

                if ((IOPIN1 & PRZYCISK_23) == 0) {
                    IOSET1 = DIODA_19;
                    runda++;
                    break;
                }
            }
            if (T0TC >= czas) {
                przegrana = TRUE;
                IOSET1 = DIODA_19;
            }
            wynik += (4000 - czas) - T0TC;
			losowosc = T0TC;
        }

		T0TCR = 0x02;
    }
    if(przegrana){
    	lcdClrscr();
		lcdGotoxy(2, 10);
		lcdPuts("Uplynal czas\n PRZEGRALES");
    } else {
    	lcdClrscr();
		lcdGotoxy(35, 65);
		char pkt_char[7];
		sprintf(pkt_char, "%d", wynik);
		lcdPuts(pkt_char);
		zapisz_wynik(wynik, wczytaj_wynik());
    }
}

/************************************************************************
 * @Description: Funkcja glowna programu
 * @Parameter: Brak
 * @Returns: Poprawnosc wykonania programu
 * @Side effects: Brak
 *************************************************************************/
int main(void)
{
	//Inicjowanie
    printf_init();
    i2cInit();
    lcdInit();
    initAdc();
    init_irq(500, 20);
	lcdColor(0xff, 0x00);
//  uruchomienie GPIO na nodach P1.16 do P1.25.
	PINSEL2 &= ~(1 << 3);
//  Kierunek out na nodzach P1.16 - P1.19
	IODIR1 |= DIODA_16;  //(1 << 16);
	IODIR1 |= DIODA_17;
	IODIR1 |= DIODA_18;
	IODIR1 |= DIODA_19;
	IOSET1 = DIODA_16;
	IOSET1 = DIODA_17;
	IOSET1 = DIODA_18;
	IOSET1 = DIODA_19;

	int rekord = wczytaj_wynik();
	char rek_char[8];
	sprintf(rek_char, "%d", rekord);

	lcdClrscr();
	lcdGotoxy(20, 5);
	lcdPuts("Gra Refleks");
	lcdGotoxy(2, 25);
	lcdPuts("Poziom trudnosci");
	lcdGotoxy(2, 50);
	lcdPuts("Najwyzszy wynik");
	lcdGotoxy(40, 64);
	lcdPuts(rek_char);
	lcdGotoxy(2, 76);
	lcdPuts("Nacisnij dowolny");
	lcdGotoxy(2, 89);
	lcdPuts("przycisk by");
	lcdGotoxy(2, 102);
	lcdPuts("rozpoczac gre");

    int poziom = 0;
    int poz = 0;
    while(TRUE){
    	if ((IOPIN1 & PRZYCISK_20) == 0 || (IOPIN1 & PRZYCISK_21) == 0 || (IOPIN1 & PRZYCISK_22) == 0 || (IOPIN1 & PRZYCISK_23) == 0)
    	{
    		break;
    	}
    	poz = getAnalogueInput(AIN1);
    	lcdGotoxy(2, 38);
    	poziom = poz / 400;
		if (poziom == 0){
			lcdPuts("Latwy ");
		} else if (poziom == 1){
			lcdPuts("Sredni");
		} else if (poziom == 2){
			lcdPuts("Trudny");
		}


    }

    mdelay(1000);

	game(poziom);

}


/************************************************************************
 * @Description: The timer tick entry function that is called once every timer tick
 *    interrupt in the RTOS. Observe that any processing in this
 *    function must be kept as short as possible since this function
 *    execute in interrupt context.
 * @Parameter: [in] elapsedTime - The number of elapsed milliseconds since last call.
 * @Returns: Nic
 * @Side effects: Brak
 *************************************************************************/
void
appTick(tU32 elapsedTime)
{
  ms += elapsedTime;
}

