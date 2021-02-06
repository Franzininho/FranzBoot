// Standalone AVR ISP programmer
// August 2011 by Limor Fried / Ladyada / Adafruit
// Jan 2011 by Bill Westfield ("WestfW")
//
// Adaptado em 03/02/2021 para o Franzininho por Daniel "DQ" Quadros
//
// this sketch allows an Arduino to program a flash program
// into any AVR if you can fit the HEX file into program memory
// No computer is necessary. Two LEDs for status notification
// Press button to program a new chip.
// This is ideal for very fast mass-programming of chips!
//
// It is based on AVRISP
//
// using the following pins:
// 10: slave reset
// 11: MOSI
// 12: MISO
// 13: SCK


#include "optiLoader.h"
#include "SPI.h"

// Global Variables
int pmode=0;
byte pageBuffer[128];		       /* One page of flash */


void setup () {
  // Inicia os LEDs
  pinMode(LED_PROGMODE, OUTPUT);
  digitalWrite(LED_PROGMODE, LOW);
  pinMode(LED_ERR, OUTPUT);
  digitalWrite(LED_ERR, LOW);

  // Inicia serial
  Serial.begin(9600);
  Serial.println("\nGravador de Bootloader no Franzininho DIY!");
}

void loop (void) {
  pinMode(BUTTON, INPUT);     // button for next programming
  digitalWrite(BUTTON, HIGH); // pullup
  delay(10);
 
  Serial.println("\nDigite 'G' ou aperte o botao para gravar...");
  while (1) {
    int c = Serial.read();
    if  ((! digitalRead(BUTTON)) || (c == 'G') || (c == 'g'))
      break;  
  }
  while (!digitalRead(BUTTON)) {
    delay(10);
  }
  delay(100);
 
  target_poweron();			/* Turn on target power */

  uint16_t signature;
  image_t *targetimage;
        
  Serial.println("Conferindo identificacao.");
  if (! (signature = readSignature()))		// Figure out what kind of CPU
    error("confira as conexoes!");
  if (! (targetimage = findImage(signature)))	// look for an image
    error("nao eh um ATtiny85");

  Serial.println("Limpando a Flash.");
  eraseChip();

  if (! programFuses(targetimage->image_progfuses))	// get fuses ready to program
    error("falha ao gravar fuses");
  
  if (! verifyFuses(targetimage->image_progfuses, targetimage->fusemask) ) {
    error("falha ao verificar fuses");
  } 

  Serial.println("Gravando a Flash.");
  end_pmode();
  start_pmode();

  byte *hextext = targetimage->image_hexcode;  
  uint16_t pageaddr = 0;
  uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
  uint16_t chipsize = pgm_read_word(&targetimage->chipsize);
        
  //Serial.println(chipsize, DEC);
  while (pageaddr < chipsize) {
     byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer);
          
     boolean blankpage = true;
     for (uint8_t i=0; i<pagesize; i++) {
       if (pageBuffer[i] != 0xFF) blankpage = false;
     }          
     if (! blankpage) {
       if (! flashPage(pageBuffer, pageaddr, pagesize))	
      	 error("falha na programacao");
     }
     hextext = hextextpos;
     pageaddr += pagesize;
  }
  
  // Set fuses to 'final' state
  if (! programFuses(targetimage->image_normfuses))
    error("falha ao gravar os fuses");
    
  end_pmode();
  start_pmode();
  
  Serial.println("Verificando a gravacao...");
  if (! verifyImage(targetimage->image_hexcode) ) {
    error("falha ao verificar a gravacao");
  }

  if (! verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
    error("falha ao verificar os fuses");
  }
  target_poweroff();			/* turn power off */

  Serial.println("SUCESSO!\n");
  Serial.println("Pressione RESET para gravar outro chip"); 

  while (1);
}


void error(char *string) { 

  Serial.print("ERRO: ");
  Serial.println(string); 
  
  Serial.println("\nPressione RESET para nova tentativa"); 

  digitalWrite(LED_PROGMODE, LOW);
  digitalWrite(LED_ERR, HIGH);

  while (1) {
    delay(100);
  }
}

void start_pmode () {
  pinMode(13, INPUT); // restore to default

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128); 
  
  debug("...spi_init done");
  // following delays may not work on all targets...
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  delay(50);
  digitalWrite(RESET, LOW);
  delay(50);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  debug("...spi_transaction");
  spi_transaction(0xAC, 0x53, 0x00, 0x00);
  debug("...Done\n");
  pmode = 1;
}

void end_pmode () {
  SPCR = 0;				/* reset SPI */
  digitalWrite(MISO, 0);		/* Make sure pullups are off too */
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, 0);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, 0);
  pinMode(SCK, INPUT);
  digitalWrite(RESET, 0);
  pinMode(RESET, INPUT);
  SPI.end();      //  need to call SPI.end() before another call to SPI.begin()  pmode = 0;
}


/*
 * target_poweron
 * begin programming
 */
boolean target_poweron ()
{
  digitalWrite(LED_PROGMODE, HIGH);
  digitalWrite(RESET, LOW);  // reset it right away.
  pinMode(RESET, OUTPUT);
  delay(100);
  debug("Starting Program Mode");
  start_pmode();
  debug(" [OK]\n");
  return true;
}

boolean target_poweroff ()
{
  end_pmode();
  digitalWrite(LED_PROGMODE, LOW);
  return true;
}
