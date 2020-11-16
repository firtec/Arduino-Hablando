/************************************************************************
**  Descripción  : Manejo del sintetizador de voz Epson S1V30120              
**                                  
**  Target       : Arduino Mega
**  ToolChain    : Arduino IDE 1.8.13
**  www.firtec.com.ar
*************************************************************************/
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>
#include <math.h>
#include "text_to_speech_img.h"


#define S1V30120_RST    49
#define S1V30120_RDY    10
#define S1V30120_CS     53
#define S1V30120_MUTE   A0
//------------------------------------------
// Defines parameters for S1V30120

// Comandos para el sintetizador en modo arranque
#define ISC_VERSION_REQ      0x0005
#define ISC_BOOT_LOAD_REQ   0x1000
#define ISC_BOOT_RUN_REQ    0x1002
#define ISC_TEST_REQ      0x0003

// Comandos para el sintetizador en modo normal

#define ISC_AUDIO_CONFIG_REQ  0x0008
#define ISC_AUDIO_VOLUME_REQ  0x000A
#define ISC_AUDIO_MUTE_REQ    0x000C

#define ISC_TTS_CONFIG_REQ    0x0012
// Sampleo en 11 kHz
#define ISC_TTS_SAMPLE_RATE   0x01
#define ISC_TTS_VOICE     0x00
#define ISC_TTS_EPSON_PARSE   0x01
#define ISC_TTS_LANGUAGE      0x04   /// Lenguaje español

// Trbajando a 200 palabras por minuto
#define ISC_TTS_SPEAK_RATE_LSB  0xC8
#define ISC_TTS_SPEAK_RATE_MSB  0x00
#define ISC_TTS_DATASOURCE    0x00
#define ISC_TTS_SPEAK_REQ     0x0014

// Configuración general
#define ISC_VERSION_RESP    0x0006
#define ISC_BOOT_LOAD_RESP    0x1001
#define ISC_BOOT_RUN_RESP     0x1003
#define ISC_TEST_RESP       0x0004

#define ISC_AUDIO_CONFIG_RESP   0x0009
#define ISC_AUDIO_VOLUME_RESP 0x000B
#define ISC_AUDIO_MUTE_RESP   0x000D
#define ISC_TTS_CONFIG_RESP   0x0013
#define ISC_TTS_SPEAK_RESP    0x0015
#define ISC_ERROR_IND       0x0000
#define ISC_MSG_BLOCKED_RESP  0x0007

#define ISC_TTS_FINISHED_IND  0x0021



// Parametros para el audio
// Página 42 del manual "S1V30120 Message Protocol Specification"

// MONO = 0x00, todos los otros valores son reservados
#define TTS_AUDIO_CONF_AS   0x00

// Ganancia de Audio = +18 db
#define TTS_AUDIO_CONF_AG   0x43

// Amplificador desactivado
#define TTS_AUDIO_CONF_AMP  0x00

// Sampleo en 11kHz
#define TTS_AUDIO_CONF_ASR  0x01

// Audio routing: application to DAC
#define TTS_AUDIO_CONF_AR   0x00

// Tono de audio debe ser 0
#define TTS_AUDIO_CONF_ATC  0x00

// Audio click source: internal, set a 0
#define TTS_AUDIO_CONF_ACS  0x00

// DAC fuciona solo mientras el sintetizador funciona
#define TTS_AUDIO_CONF_DC   0x00

//------- Funciones usadas en el ejmplo ---------------

void S1V30120_reset(void);
unsigned short S1V30120_get_version(void);
bool S1V30120_download(void);
bool S1V30120_load_chunk(unsigned short);
bool S1V30120_boot_run(void);
void show_response(bool);
bool S1V30120_registration(void);
bool S1V30120_parse_response(unsigned short, unsigned short, unsigned short);
void S1V30120_send_padding(unsigned short);
void S1V30120_send_message(volatile char, unsigned char);
bool S1V30120_configure_audio(void);
bool S1V30120_configure_tts(void);
bool S1V30120_speech(String , unsigned char);
bool S1V30120_set_volume(void);
bool S1V30120_load_chunk(unsigned short);

String mytext = "Hola soy una placa arduino y estoy+ hablando";

char rcvd_msg[20] = {0};

static volatile char send_msg[200] = {0};
static volatile unsigned short msg_len;
static volatile unsigned short txt_len;

unsigned short tmp;
long idx;
bool success;

static volatile unsigned short TTS_DATA_IDX;

void setup() {
  // Seteo de Pines para el sitetizador
  pinMode(S1V30120_RST, OUTPUT);
  pinMode(S1V30120_RDY, INPUT);
  pinMode(S1V30120_CS, OUTPUT);
  pinMode(S1V30120_MUTE, OUTPUT);
  digitalWrite(S1V30120_MUTE,LOW);

  // Para hacer debug por la UART
  Serial.begin(9600);

  SPI.begin();  // Inicia el puerto SPI
    
  S1V30120_reset();
  tmp = S1V30120_get_version(); // Intenta encontrar el sintetizador S1V30120
  if (tmp == 0x0402)
  {
    Serial.println("S1V30120 encontrado. Cargando imagen de arranque!");
  }   
   
  success = S1V30120_download();
  Serial.print("Boot imagen cargada: ");
  show_response(success);
  success = S1V30120_boot_run();
  
  Serial.print("Boot imagen corriendo: ");
  show_response(success);
  delay(150); // Espera que se ejecute la imagen cargada
  Serial.print("Registrando: ");
  success = S1V30120_registration();
  show_response(success);
  // Mostrar la versión 
  S1V30120_get_version();
  success = S1V30120_configure_audio();
  Serial.print("Configurando audio: ");
  show_response(success);
  success = S1V30120_set_volume();
  Serial.print("Configurando volumen: ");
  show_response(success);

  success = S1V30120_configure_tts();
  Serial.print("Configure TTS: ");
  show_response(success);
}

void loop() {

    success = S1V30120_speech(mytext,1);
    delay(3000); // Esperar para repetir
}

// Esta función restablece el chip S1V30120 y carga el código de firmware
void S1V30120_reset(void)
{
  digitalWrite(S1V30120_CS,HIGH); 
  digitalWrite(S1V30120_RST,LOW);
  // Envia un byte ficticio, esto dejará alta la línea del reloj
  SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
  SPI.transfer(0x00);
  SPI.endTransaction();
  delay(5);
  digitalWrite(S1V30120_RST,HIGH);
  delay(150);
}

unsigned short S1V30120_get_version(void)
{
    unsigned short S1V30120_version = 0;
    unsigned short tmp_disp;
    // Envía ISC_VERSION_REQ = [0x00, 0x04, 0x00, 0x05];
    char msg_ver[] = {0x04, 0x00, 0x05, 0x00};
    S1V30120_send_message(msg_ver, 0x04);

    // Espera para leer la señal de respuesta
    while(digitalRead(S1V30120_RDY) == 0);

    // Recibe 20 bytes
    digitalWrite(S1V30120_CS,LOW);
    SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
    // Espera por el inicio del mensaje
    while(SPI.transfer(0x00) != 0xAA);
    for (int i = 0; i < 20; i++)
    {
      rcvd_msg[i]= SPI.transfer(0x00);
    }
    // Envía 16 bytes
    S1V30120_send_padding(16);
    SPI.endTransaction();
    digitalWrite(S1V30120_CS,HIGH);
    S1V30120_version = rcvd_msg[4] << 8 | rcvd_msg[5];
    Serial.print("HW version ");
    Serial.print(rcvd_msg[4],HEX);
    Serial.print(".");
    Serial.println(rcvd_msg[5],HEX);
    Serial.print("Firmware version ");
    Serial.print(rcvd_msg[6],HEX);
    Serial.print(".");
    Serial.print(rcvd_msg[7],HEX);
    Serial.print(".");
    Serial.println(rcvd_msg[16],HEX);
    Serial.print("Firmware features ");
    Serial.println(((rcvd_msg[11] << 24) | (rcvd_msg[10] << 16) | (rcvd_msg[9] << 8) | rcvd_msg[8]),HEX);
    Serial.print("Firmware extended features ");
    Serial.println(((rcvd_msg[15] << 24) | (rcvd_msg[14] << 16) | (rcvd_msg[13] << 8) | rcvd_msg[12]),HEX);
    return S1V30120_version;
}

bool S1V30120_load_chunk(unsigned short chunk_len)
{
  // Cargar una gran cantidad de datos
  char len_msb = ((chunk_len + 4) & 0xFF00) >> 8;
  char len_lsb = (chunk_len + 4) & 0xFF;
  digitalWrite(S1V30120_CS,LOW);
  SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
  SPI.transfer(0xAA);  // Inicia el comando
  SPI.transfer(len_lsb);  // La longitud del mensaje es 2048 bytes = 0x0800
  SPI.transfer(len_msb);  // Primero LSB
  SPI.transfer(0x00);  // Enviar SC_BOOT_LOAD_REQ (0x1000)
  SPI.transfer(0x10);
  for (int chunk_idx = 0; chunk_idx < chunk_len; chunk_idx++)
  {
    //SPI.transfer(TTS_INIT_DATA[TTS_DATA_IDX]);
    SPI.transfer(pgm_read_byte_near(TTS_INIT_DATA + TTS_DATA_IDX));
    TTS_DATA_IDX++;
  }
  SPI.endTransaction();
  digitalWrite(S1V30120_CS,HIGH);
  return S1V30120_parse_response(ISC_BOOT_LOAD_RESP, 0x0001, 16);
}

bool S1V30120_download(void)
{
   // TTS_INIT_DATA es del tipo byte
   unsigned short len = sizeof (TTS_INIT_DATA);
   unsigned short fullchunks;
   unsigned short remaining;
   bool chunk_result;
   long data_index = 0;
   Serial.print("TTS_INIT_DATA length is ");
   Serial.println(len);
   // Estamos cargando fragmentos de datos
   // Cada fragmento, incluido el encabezado, debe tener un máximo de 2048 bytes
   // como el encabezado tiene 4 bytes, esto deja 2044 bytes para cargar cada vez
   // Calcular el número de fragmentos
   fullchunks = len / 2044;
   remaining = len - fullchunks * 2044;
   Serial.print("Full chunks to load: ");
   Serial.println(fullchunks);
   Serial.print("Remaining bytes: ");
   Serial.println(remaining);
   for (int num_chunks = 0; num_chunks < fullchunks; num_chunks++)
   {
     chunk_result = S1V30120_load_chunk (2044);
     if (chunk_result)
     {
       Serial.println("Perfecto!!");
     }
     else
     {
       Serial.print("ERROR!! ");
       Serial.println(num_chunks);
       return 0;
     }
   }
   chunk_result = S1V30120_load_chunk (remaining);
   if (chunk_result)
   {
     Serial.println("Perfecto!!");
   }
   else
   {
     Serial.print("ERROR!! ");
     return 0;
   }
// Si todo ha sdo OK retorna 1
return 1;
}

bool S1V30120_boot_run(void)
{
    char boot_run_msg[] = {0x04, 0x00, 0x02, 0x10};
    S1V30120_send_message(boot_run_msg, 0x04);
    return S1V30120_parse_response(ISC_BOOT_RUN_RESP, 0x0001, 8);
}

void show_response(bool response)
{
  if(response)
    Serial.println("OK!");
  else
  {
    Serial.println("ERROR, sistema detenido!");
    while(1);
  }
}

bool S1V30120_registration(void)
{
  SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
  char reg_code[] = {0x0C, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  S1V30120_send_message(reg_code, 0x0C);
  return S1V30120_parse_response(ISC_TEST_RESP, 0x0000, 16);
}


// Analizador de mensajes
// Esta función recibe como parámetro el código de respuesta esperado y el resultado
// Y devuelve 1 si se recibe el resultado esperado, 0 en caso contrario
// Como observación, la mayoría de los mensajes tienen una longitud de 6 bytes
// (2 bytes de longitud + 2 bytes de código de respuesta + 2 bytes de respuesta)
bool S1V30120_parse_response(unsigned short expected_message, unsigned short expected_result, unsigned short padding_bytes)
{
    unsigned short rcvd_tmp;
    while(digitalRead(S1V30120_RDY) == 0);
    digitalWrite(S1V30120_CS,LOW);
    SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
    // wait for message start
    while(SPI.transfer(0x00) != 0xAA);
    for (int i = 0; i < 6; i++)
    {
      rcvd_msg[i]= SPI.transfer(0x00);
    }
    S1V30120_send_padding(padding_bytes);
    SPI.endTransaction();
    digitalWrite(S1V30120_CS,HIGH);
    rcvd_tmp = rcvd_msg[3] << 8 | rcvd_msg[2];
    if (rcvd_tmp == expected_message) // ISC_BOOT_RUN_RESP?
    {
       // We check the response
       rcvd_tmp = rcvd_msg[5] << 8 | rcvd_msg[4];
       if (rcvd_tmp == expected_result) // OK, return 1
         return 1;
       else
         return 0;
    }
    else
    return 0;
}


void S1V30120_send_padding(unsigned short num_padding_bytes)
{
  for (int i = 0; i < num_padding_bytes; i++)
  {
     SPI.transfer(0x00);
  }
}

void S1V30120_send_message(volatile char message[], unsigned char message_length)
{
  while(digitalRead(S1V30120_RDY) == 1);  // blocking
  digitalWrite(S1V30120_CS,LOW);
  SPI.beginTransaction(SPISettings(750000, MSBFIRST, SPI_MODE3));
  SPI.transfer(0xAA);  // Inicia el mensaje comando
  for (int i = 0; i < message_length; i++)
  {
    SPI.transfer(message[i]);
  }
  SPI.endTransaction();
}
//#########################################################################

bool S1V30120_configure_audio(void)
{
  msg_len = 0x0C;
  send_msg[0] = msg_len & 0xFF;          // LSB of msg len
  send_msg[1] = (msg_len & 0xFF00) >> 8; // MSB of msg len
  send_msg[2] = ISC_AUDIO_CONFIG_REQ & 0xFF;
  send_msg[3] = (ISC_AUDIO_CONFIG_REQ & 0xFF00) >> 8;
  send_msg[4] = TTS_AUDIO_CONF_AS;
  send_msg[5] = TTS_AUDIO_CONF_AG;
  send_msg[6] = TTS_AUDIO_CONF_AMP;
  send_msg[7] = TTS_AUDIO_CONF_ASR;
  send_msg[8] = TTS_AUDIO_CONF_AR;
  send_msg[9] = TTS_AUDIO_CONF_ATC;
  send_msg[10] = TTS_AUDIO_CONF_ACS;
  send_msg[11] = TTS_AUDIO_CONF_DC;
  S1V30120_send_message(send_msg, msg_len);
  return S1V30120_parse_response(ISC_AUDIO_CONFIG_RESP, 0x0000, 16);
}
//###########################################################################
// set gain to 0 db
bool S1V30120_set_volume(void)
{
  char setvol_code[]={0x06, 0x00, 0x0A, 0x00, 0x00, 0x00};
  S1V30120_send_message(setvol_code, 0x06);
  return S1V30120_parse_response(ISC_AUDIO_VOLUME_RESP, 0x0000, 16);
}

bool S1V30120_configure_tts(void)
{
  msg_len = 0x0C;
  send_msg[0] = msg_len & 0xFF;          // LSB of msg len
  send_msg[1] = (msg_len & 0xFF00) >> 8; // MSB of msg len
  send_msg[2] = ISC_TTS_CONFIG_REQ & 0xFF;
  send_msg[3] = (ISC_TTS_CONFIG_REQ & 0xFF00) >> 8;
  send_msg[4] = ISC_TTS_SAMPLE_RATE;
  send_msg[5] = ISC_TTS_VOICE;
  send_msg[6] = ISC_TTS_EPSON_PARSE;
  send_msg[7] = ISC_TTS_LANGUAGE;
  send_msg[8] = ISC_TTS_SPEAK_RATE_LSB;
  send_msg[9] = ISC_TTS_SPEAK_RATE_MSB;
  send_msg[10] = ISC_TTS_DATASOURCE;
  send_msg[11] = 0x00;
  S1V30120_send_message(send_msg, msg_len);
  return S1V30120_parse_response(ISC_TTS_CONFIG_RESP, 0x0000, 16);
}

bool S1V30120_speech(String text_to_speech, unsigned char flush_enable)
{
  bool response;
  txt_len = text_to_speech.length();
  msg_len = txt_len + 6;
  send_msg[0] = msg_len & 0xFF;          // LSB of msg len
  send_msg[1] = (msg_len & 0xFF00) >> 8; // MSB of msg len
  send_msg[2] = ISC_TTS_SPEAK_REQ & 0xFF;
  send_msg[3] = (ISC_TTS_SPEAK_REQ & 0xFF00) >> 8;
  send_msg[4] = flush_enable; // flush control
  for (int i = 0; i < txt_len; i++)
  {
     send_msg[i+5] = text_to_speech[i];
  }
  send_msg[msg_len-1] = '\0'; // null character
  S1V30120_send_message(send_msg, msg_len);
  response = S1V30120_parse_response(ISC_TTS_SPEAK_RESP, 0x0000, 16);
  while (!S1V30120_parse_response(ISC_TTS_FINISHED_IND, 0x0000, 16)); 
  return response;
}
