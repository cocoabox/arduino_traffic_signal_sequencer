#include <avr/pgmspace.h>

//#define VERBOSE

/*
    2016-03-14 : added potentiometer option for adjusting time (min=programmed time .. max=programmed time * 5

*/


#define MSG(a, b) { Serial.print(a); Serial.print(b); Serial.println(""); }

// pins 
#define CH1 2
#define CH2 3
#define CH3 4
#define CH4 5

#define POTENTIOMETER A0
#define PUSH_BUTTON A1

typedef struct {
    uint8_t adjustable;
    uint16_t time_ori; // # of seconds original
    uint32_t time;	 // actual millisecond
    uint16_t channels; // 1111 = all on, 4444 = all flash2, etc
} PHASE;

// <NAME (MAX 10 CHARS);yTTTabcd
// y   = y = speed adjustable 
//       n = not speed adjustable 
//       Y = speed adjustable, time reduced by push button
//       N = not speed adjustable, time not affected by push button
// TTT = time in seconds
// abcd = Channels 1,2,3,4 respectively. 
// '1' = on
// '2' = off
// '3' = flash1
// '4' = flash2
const char prog_rag_uk[] PROGMEM = "RAG UK;y0201221,n0021121,y0202211,n0032121\n";
const char prog_rag_us[] PROGMEM = "RAG US(JP);y0201221,n0021221,y0202211,n0032121\n";
const char prog_ped_uk[] PROGMEM = "PED UK(JP);y0011222,y0122212,y0062222,n0031222,y0201222,n0031222\n";
const char prog_ped_jp[] PROGMEM = "PED UK(JP);y0011222,y0122212,y0062232,n0031222,y0201222,n0031222\n";
//const char prog_ped_us[] PROGMEM = "PED US;y0102212,y0103222,y0201222\n";
const char prog_ped_us[] PROGMEM = "PED US;y0011222,y0122212,y0063222,n0031222,y0201222,n0031222\n";
const char prog_ped_us_potentiometer[] PROGMEM = "PED US(POT);n0011222,n0152212,y0103222,n0141222\n";
const char prog_flash_amber[] PROGMEM = "FLASH Amb;n0052322\n";
const char prog_flash_red[] PROGMEM = "FLASH Red;n0053222\n";
const char prog_flash_all[] PROGMEM = "FLASH 1+2;n0053333\n";
const char prog_ped_uk_puffin[] PROGMEM = "PED UK2(JP);y0011222,y0122212,y0062212,n0031222,y0201222,n0031222\n";
const char prog_raga1_uk[] PROGMEM = "RAGA1 UK;y0101222,y0101221,n0021121,y0202212,n0032122\n";
const char prog_raga2_uk[] PROGMEM = "RAGA2 UK;y0201222,n0021122,y0102212,y0102211,n0032122\n";
const char prog_raga3_uk[] PROGMEM = "RAGA2 UK;y0101222,y0101221,n0021121,y0202212,n0032122,y0201222,n0021122,y0102212,y0102211,n0032122\n";
const char prog_sample_rag[] PROGMEM="TEST;n0011222,n0012212,n0012122\n";

#define HAS_POTENTIOMETER 0
#define HAS_PUSH_BUTTON 0
#define PROG_NUM 3

// high/low-trigger 
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

const char* progs[] = {
	prog_rag_uk,    // 0
	prog_rag_us,    // 1
	prog_ped_uk,    // 2
	prog_ped_jp,    // 3
	prog_ped_us,    // 4
	prog_flash_amber,    // 5
	prog_flash_red,       // 6
        prog_ped_us_potentiometer, //7
        prog_flash_all,    //8
        prog_ped_uk_puffin, // 9        
        prog_raga1_uk,       //10
        prog_raga2_uk,       //11
        prog_raga3_uk,       //12
        prog_sample_rag      //13
};

/*** global variables ***/
#define PROG_NAME_LEN 20

unsigned char  current_prog_name[PROG_NAME_LEN + 1] = { 0 };
PHASE current_prog[10] = { 0 };
uint8_t current_prog_length = 0;

uint32_t flasher1_next = 0;
uint32_t flasher2_next = 0;
uint8_t flasher1_state = 0;
uint8_t flasher2_state = 0;

#define FLASH1 350
#define FLASH2 180

#if defined(HAS_POTENTIOMETER) && (HAS_POTENTIOMETER)
// 0..1024
int potentiometer_readout = 511;  
#endif

uint8_t step = 0;
uint8_t max_step = 0;
uint32_t next_time = 0;

void read_pchar(const char *src, uint8_t len, unsigned char* dst) {
    for (uint8_t i = 0; i < len; ++i) {
        *(dst + i) = (unsigned char)pgm_read_byte(src + i); 
    }
}

uint8_t read_pchar_until(const char *src, unsigned char until_what, unsigned char* dst) {
    uint8_t i = 0;
    while (1) {
        unsigned char c = (unsigned char)pgm_read_byte(src + i); 
        if (! c || c == until_what) { break; }
        *(dst + i) = c;
        i++;
    }
    return i+1;
}


void read_program(uint8_t prog_number) {
#ifdef VERBOSE
    MSG("Read program: ", prog_number);
#endif
    const char* prog = progs[prog_number];
    
    // read program name
    memset(current_prog_name, 0, sizeof(current_prog_name));
    uint8_t i = read_pchar_until(prog, ';', current_prog_name);

#ifdef VERBOSE
    MSG("Program name: ", (char*)current_prog_name);
#endif

    uint8_t prog_idx = 0; 
    while (1) {
        unsigned char current_step[8] = { 0 };
        read_pchar(prog+i, 8, current_step);
        i += 8;

        // update current_prog
        current_prog[prog_idx].adjustable = current_step[0] == 'y';

        // read time (in sec) 3 bytes, e.g. "010" --> 10 sec
        unsigned char time_in_sec[4] = { 0 };
        strncpy((char*)time_in_sec, (char*)current_step+1, 3);
        current_prog[prog_idx].time_ori = atoi((char*)time_in_sec);
        current_prog[prog_idx].time = current_prog[prog_idx].time_ori * 1000;
#ifdef VERBOSE
        MSG("Time_ori: ", current_prog[prog_idx].time_ori);
#endif

        // read channel state 4 bytes, e.g. "1122" --> 1122 (ch1:on, ch2:on, ch3:off, ch4:off)
        char channels[5] = { 0 };
        strncpy((char*)channels, (char*)current_step+1+3, 4);
        current_prog[prog_idx].channels = atoi((char*)channels);
#ifdef VERBOSE
        MSG("Channels: ", current_prog[prog_idx].channels);
#endif
        unsigned char end_marker = 0;
        read_pchar(prog+i, 1, &end_marker); ++i;
#ifdef VERBOSE
        MSG("End marker: ", end_marker);
#endif

        if (! end_marker || end_marker == '\n') { break; }

        prog_idx++;
    }

    max_step = prog_idx;
#ifdef VERBOSE
    MSG("Done ! Max step is : ", max_step);
#endif
}

uint8_t get_nth_digit(uint8_t nth_digit, uint16_t in) {
   return (int)floor(in / pow(10, nth_digit)) % 10;  
}

uint8_t get_channel_on_off_state(uint8_t channel_idx) {
    uint8_t numerical_state = get_nth_digit(
            3 - channel_idx , current_prog[step].channels);
    
    switch (numerical_state) {
        case 1:
            // ON
            return RELAY_ON; 
        case 3:
            // FLASHER 1
            return flasher1_state ? RELAY_ON : RELAY_OFF;
        case 4:
            // FLASHER 2
            return flasher2_state ? RELAY_ON : RELAY_OFF;
        case 2:
        default:
            // OFF
            return RELAY_OFF;
    }
}

void setup() {
        Serial.begin(9600);

  	uint32_t now = millis();
  	flasher1_next = now + FLASH1;
  	flasher2_next = now + FLASH2;

        pinMode(CH1, OUTPUT);
        pinMode(CH2, OUTPUT);
        pinMode(CH3, OUTPUT);
        pinMode(CH4, OUTPUT);

        read_program(PROG_NUM); 
  	
        step = 0;
 	next_time = 0;
}

uint32_t get_time_delta(int step_num) {
  uint32_t out;
#if defined(HAS_POTENTIOMETER) && (HAS_POTENTIOMETER > 0)
    // todo: adjust time
    out = current_prog[step_num].time * 
      (
      current_prog[step_num].adjustable 
      ? (4 + (potentiometer_readout * (-3)/1023))
      : 1 
      ); 
    #ifdef VERBOSE
      Serial.print("Readout="); Serial.println(potentiometer_readout);
      Serial.print("Modified time (ms)="); Serial.println(out);
      Serial.print("Original time (ms)="); Serial.println(current_prog[step_num].time);
    #endif
#else
    out = current_prog[step_num].time;
#endif  
  return out;
}

void loop() {
  uint32_t now = millis();
  uint32_t delta;
  uint8_t flashers_updated = 0;
  uint8_t step_updated = 0;

#if defined(HAS_POTENTIOMETER) && HAS_POTENTIOMETER
  potentiometer_readout = analogRead(POTENTIOMETER);
  // Serial.print("Read: "); Serial.println(potentiometer_readout);
#endif

  // update flashers
  if (now >= flasher1_next) {
    flasher1_state = flasher1_state ? 0 : 1;
    flasher1_next = now + FLASH1;
    flashers_updated = 1;
  }
  if (now >= flasher2_next) {
    flasher2_state = flasher2_state ? 0 : 1;
    flasher2_next = now + FLASH2;
    flashers_updated = 1;
  }

  // update prog step
  if (! next_time) {
    step = 0;
    next_time = now + get_time_delta(step);
    step_updated = 1;
#ifdef VERBOSE
    MSG("[loop] Going to step 0; next time: ", next_time);
#endif
  }
  else if (now >= next_time) {
    step = ++step > max_step ? 0 : step;
    next_time = now + get_time_delta(step);
    
    step_updated = 1;
#ifdef VERBOSE
    MSG("[loop] Step : ", step);
    MSG("[loop] Next time: ", next_time);
#endif
  }
  
  // repaint if needed
  if (flashers_updated || step_updated) {
        digitalWrite(CH1, get_channel_on_off_state(0));
        digitalWrite(CH2, get_channel_on_off_state(1));
        digitalWrite(CH3, get_channel_on_off_state(2));
        digitalWrite(CH4, get_channel_on_off_state(3));
  }
  

}
