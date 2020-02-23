/*  Example playing a sinewave at a set frequency,
    using Mozzi sonification library.

    Demonstrates the use of Oscil to play a wavetable.

    Circuit: Audio output on digital pin 9 on a Uno or similar, or
    DAC/A14 on Teensy 3.1, or
    check the README or http://sensorium.github.com/Mozzi/

    Mozzi documentation/API
    https://sensorium.github.io/Mozzi/doc/html/index.html

    Mozzi help/discussion/announcements:
    https://groups.google.com/forum/#!forum/mozzi-users

    Tim Barrass 2012, CC by-nc-sa.
*/

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <ADSR.h>
#include <tables/sin2048_int8.h> // sine table for oscillator

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin0(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin3(SIN2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // Hz, powers of 2 are most reliable
ADSR <CONTROL_RATE, CONTROL_RATE> envelope0;
// ADSR <CONTROL_RATE, CONTROL_RATE> envelope1;
// ADSR <CONTROL_RATE, CONTROL_RATE> envelope2;
// ADSR <CONTROL_RATE, CONTROL_RATE> envelope3;

long button_timer = mozziMicros();

int held_down_note = NULL;
bool button_state = digitalRead(2);
int input_freq = 440;

long note_timers[4];
// Oscil<2048, 16384> note_oscs[4];
// ADSR<64, 64> note_envs[4];


class Note {
  private:
    int freq;
    int osc_index;
    int a_t = 10;
    int a_level = 100;
    int d_t = 100;
    int d_level = 100;
    int s_t = 100;
    int s_level = 100;
    int r_t = 10;
    int r_level = 100;

  public:
    Note(int, int);
    // for now just freq
    // void initialize(int, int);
    void note_on();
    void note_off();

    int frequency();
    void set_frequency(int);
    int osc_next();
};

Note::Note(int init_osc_index, int init_freq){
  osc_index = init_osc_index;
  int freq = init_freq;

  // start this osc off with our freq
  if(freq > 0){
    if(osc_index == 0){
      aSin0.setFreq(freq);
    } else if(osc_index == 1){
      aSin1.setFreq(freq);
    } else if(osc_index == 2){
      aSin2.setFreq(freq);
    } else if(osc_index == 3){
      aSin3.setFreq(freq);
    }
  }

  envelope0.setADLevels(a_level,d_level);
  // milliseconds
  envelope0.setTimes(a_t,d_t,s_t,r_t);
  envelope0.update();
}

void Note::note_off(){
  // begin tail off
  // note_envs[osc_index].noteOff();
  envelope0.noteOff();
}

void Note::note_on(){
  // begin tail off
  // note_envs[osc_index].noteOff();
  envelope0.noteOn();
}

int Note::frequency(){
  return freq;
}

void Note::set_frequency(int new_freq){
  freq = new_freq;  
}

int Note::osc_next(){

  // modify that shit based on current control vals
  if(osc_index == 0){
    return aSin0.next();
  } else if(osc_index == 1){
    return aSin1.next();
  } else if(osc_index == 2){
    return aSin2.next();
  } else if(osc_index == 3){
    return aSin3.next();
  }
  // return note_oscs[osc_index].next();
}

Note note0 = Note(0, 0);
Note note1 = Note(1, 0);
Note note2 = Note(2, 0);
Note note3 = Note(3, 0);

Note notes[4] = {note0, note1, note2, note3};

void play_note(int freq){
  int available_slot = 0;
  for(int i=0; i<4; i++){
    if(notes[i].frequency() == 0){
      available_slot = i;
      break;
    }
  }

  // if no slot, override one
  notes[available_slot] = Note(available_slot, freq);
  // notes[available_slot].set_frequency(freq);
  held_down_note = available_slot;
  // set timer for new note
  note_timers[available_slot] = mozziMicros();
  notes[available_slot].note_on();
}

void setup(){
  Serial.begin(9600);
  startMozzi(CONTROL_RATE); // :)
  // aSin.setFreq(input_freq); // set the frequency

  // pass those refs baby!
  // note_oscs[0] = aSin0;
  // note_oscs[1] = aSin1;
  // note_oscs[2] = aSin2;
  // note_oscs[3] = aSin3;

  // note_envs[0] = envelope0;
  // note_envs[1] = envelope1;
  // note_envs[2] = envelope2;
  // note_envs[3] = envelope3;
}


void updateControl(){
  // button_state = digitalRead(2);
  // Serial.print(button_state);

  // put changing controls in here
  // if(button_state && mozziMicros() > now + 10000){
  //   freq += 1;
  //   if(freq > 1000){
  //     freq = 0;
  //   }
  //   aSin.setFreq(freq);
  //   now = mozziMicros();
  // }

  if(button_state && !held_down_note && mozziMicros() > button_timer + 100000 ) {
    Serial.println("started button hold");
    play_note(input_freq);
    button_timer = mozziMicros();
  } else if(!button_state && held_down_note) {
    // I stopped holding
    Serial.println("time for note off ");
    Serial.println(held_down_note);
    notes[held_down_note].note_off();
    held_down_note = NULL;
  }

  for(int i=0; i<4; i++){
    // is note timer active and has been 3s
    if(note_timers[i] > 0 && mozziMicros() > note_timers[i] + 600000 ){
      Serial.println("finishined note timer for");
      Serial.println(i);

      note_timers[i] = 0;
      notes[i].set_frequency(0);
    }
  }

}


int updateAudio(){
  // bool button_state = digitalRead(2);
  // if(button_state){
  //   int sig = aSin.next();
  //   return sig; // return an int signal centred around 0
  // }

  button_state = digitalRead(2);
  int sig = 0;

  // get vals from all playing notes
  for(int i=0; i<4; i++){
    if(notes[i].frequency() > 0){
      // found note, play dat
      // Serial.println("OSC NXX");
      // Serial.println(notes[i].osc_next());
      sig += notes[i].osc_next();
    }
  }

  // Serial.println("Sig is this ");
  // Serial.println(sig);
  return sig;
}

void loop(){
  audioHook(); // required here
}