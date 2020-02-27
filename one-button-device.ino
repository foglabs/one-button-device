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
#include <EventDelay.h>
#include <Oscil.h> // oscillator template
#include <ADSR.h>
#include <LowPassFilter.h>
#include <tables/sin2048_int8.h> // sine table for oscillator

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin0(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin3(SIN2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 128 // Hz, powers of 2 are most reliable
ADSR <CONTROL_RATE, CONTROL_RATE> envelope0;
ADSR <CONTROL_RATE, CONTROL_RATE> envelope1;
ADSR <CONTROL_RATE, CONTROL_RATE> envelope2;
ADSR <CONTROL_RATE, CONTROL_RATE> envelope3;

long button_down_timer = mozziMicros();

int held_down_note = -1;
bool button_state = digitalRead(2);
int input_freq = 440;

long note_timers[4];
unsigned int gains[4];
unsigned int gain = 0;
// Oscil<2048, 16384> note_oscs[4];
// ADSR<64, 64> note_envs[4];

// placeholder var for new notes

class Note {
  private:
    bool playing = false;

    int freq;
    int osc_index;
    
    // unsigned int a_t = 4;
    // byte a_level = 255;
    // unsigned int d_t = 15;
    // byte d_level = 64;
    // unsigned int s_t = 3000;
    // // byte s_level = 255;
    // unsigned int r_t = 300;
    // byte r_level = 255;

  public:
    Note(int, int);
    // for now just freq
    // void initialize(int, int);
    void note_on();
    void note_off();

    bool is_playing();

    int get_frequency();
    void set_frequency(int);
    void setup_envelope();
    int osc_next();
    int env_next();
    int get_osc_index();
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

}

bool Note::is_playing(){
  // if(osc_index == 0){
  //   return envelope0.playing();
  // } else if(osc_index == 1){
  //   return envelope1.playing();
  // } else if(osc_index == 2){
  //   return envelope2.playing();
  // } else if(osc_index == 3){
  //   return envelope3.playing();
  // } else {
  //   return false;
  // }
  return playing;
}

int Note::get_osc_index(){
  return osc_index;
}

void Note::note_off(){
  // begin tail off
  int oi = get_osc_index();
  if(oi == 0){
    // Serial.println("I did my notofos");
    envelope0.noteOff();
  } else if(oi == 1){

    envelope1.noteOff();
  } else if(oi == 2){

    envelope2.noteOff();
  } else if(oi == 3){

    envelope3.noteOff();
  }

  // deflag dat
  playing = false;
}

void Note::note_on(){
  // begin tail off
  // note_envs[oi].noteOff();

  playing = true;
  int oi = get_osc_index();
  if(oi == 0){
    // Serial.println("note on");
    envelope0.noteOn(true);
  } else if(oi == 1){

    envelope1.noteOn(true);
  } else if(oi == 2){

    envelope2.noteOn(true);
  } else if(oi == 3){

    envelope3.noteOn(true);
  }
}

int Note::get_frequency(){
  return freq;
}

void Note::set_frequency(int new_freq){
  freq = new_freq;
  int oi = get_osc_index();
  if(oi == 0){
    return aSin0.setFreq(new_freq);
  } else if(oi == 1){
    return aSin1.setFreq(new_freq);
  } else if(oi == 2){
    return aSin2.setFreq(new_freq);
  } else if(oi == 3){
    return aSin3.setFreq(new_freq);
  }
}

void Note::setup_envelope(){
  int oi = get_osc_index();
  if(oi == 0){
    envelope0.noteOff();
  } else if(oi == 1){
    envelope1.noteOff();
  } else if(oi == 2){
    envelope2.noteOff();
  } else if(oi == 3){
    envelope3.noteOff();
  }
}

int Note::osc_next(){

  // modify that shit based on current control vals
  if(osc_index == 0){
    return (int)aSin0.next();
  } else if(osc_index == 1){
    return (int)aSin1.next();
  } else if(osc_index == 2){
    return (int)aSin2.next();
  } else if(osc_index == 3){
    return (int)aSin3.next();
  }
  // return note_oscs[osc_index].next();
}

int Note::env_next(){

  // modify that shit based on current control vals
  if(osc_index == 0){
    // Serial.println("enve 0");
    // Serial.println(envelope0.next());
    return (unsigned int)envelope0.next();
  } else if(osc_index == 1){
    // Serial.println("enve 1");
    // Serial.println(envelope1.next());
    return (unsigned int)envelope1.next();
  } else if(osc_index == 2){
    // Serial.println("enve 2");
    // Serial.println(envelope2.next());
    return (unsigned int)envelope2.next();
  } else if(osc_index == 3){
    // Serial.println("enve 3");
    // Serial.println(envelope3.next());
    return (unsigned int)envelope3.next();
  }
}

Note *notes[4];
EventDelay *note_delays[4];

// Note new_note = Note(0, 0);
Note note0 = Note(0, 0);
Note note1 = Note(1, 0);
Note note2 = Note(2, 0);
Note note3 = Note(3, 0);

int sigs[4] = {0,0,0,0};

void play_note(int freq){
  int available_slot = 0;
  for(int i=0; i<4; i++){
    if(notes[i]->get_frequency() == 0){
      available_slot = i;
      break;
    }
  }

  notes[available_slot]->set_frequency(freq);

  // Serial.println("Maybe it fucking worked");
  // Serial.println(notes[available_slot]->get_frequency());

  // notes[available_slot].set_frequency(freq);
  held_down_note = available_slot;

  // set timer for new note
  note_timers[available_slot] = mozziMicros();

  // reinit envelope (noteoff)
  notes[available_slot]->setup_envelope();
  notes[available_slot]->note_on();

  // total time of all envelope parts
  note_delays[available_slot]->start(371);
}

void setup(){
  // Serial.begin(9600);
  startMozzi(CONTROL_RATE); // :)
  note0 = Note(0, 0);
  note1 = Note(1, 0);
  note2 = Note(2, 0);
  note3 = Note(3, 0);
  notes[0] = &note0;
  notes[1] = &note1;
  notes[2] = &note2;
  notes[3] = &note3;

  EventDelay event_delay0 = EventDelay(371);
  EventDelay event_delay1 = EventDelay(371);
  EventDelay event_delay2 = EventDelay(371);
  EventDelay event_delay3 = EventDelay(371);
  note_delays[0] = &event_delay0;
  note_delays[1] = &event_delay1;
  note_delays[2] = &event_delay2;
  note_delays[3] = &event_delay3;



    
  unsigned int a_t = 8;
  byte a_level = 255;
  unsigned int d_t = 22;
  byte d_level = 128;
  unsigned int s_t = 1;
  // byte s_level = 255;
  unsigned int r_t = 340;

  // aSin0.setFreq(freq);
  envelope0.setADLevels(a_level,d_level);
  // milliseconds
  envelope0.setTimes(a_t,d_t,s_t,r_t);
  envelope0.update();

  // aSin1.setFreq(freq);
  envelope1.setADLevels(a_level,d_level);
  // milliseconds
  envelope1.setTimes(a_t*10,d_t,s_t,r_t);
  envelope1.update();

  // aSin2.setFreq(freq);
  envelope2.setADLevels(a_level,d_level);
  // milliseconds
  envelope2.setTimes(a_t*10,d_t,s_t,r_t);
  envelope2.update();

  // aSin3.setFreq(freq);
  envelope3.setADLevels(a_level,d_level);
  // milliseconds
  envelope3.setTimes(a_t*10,d_t,s_t,r_t);
  envelope3.update();
}


void updateControl(){

  // button_state = digitalRead(2);

  // put changing controls in here
  // if(button_state && mozziMicros() > now + 10000){
  //   freq += 1;
  //   if(freq > 1000){
  //     freq = 0;
  //   }
  //   aSin.setFreq(freq);
  //   now = mozziMicros();
  // }

  // button down and not playing note and held for 10ms
  if(button_state && (held_down_note<0) && mozziMicros() > button_down_timer + 10000){
    // init playing
    // Serial.println("Button state true");
    play_note(input_freq);
    button_down_timer = mozziMicros();
  } else if(!button_state && held_down_note>=0) {
    // I stopped holding
    // Serial.println("time for note off ");
    // Serial.println(held_down_note);
    // notes[held_down_note]->note_off();
    // held_down_note = -1;
    
    notes[held_down_note]->note_off();
    held_down_note = -1;
  }  

  bool note_playing;
  for(int i=0; i<4; i++){
    // is note timer active and has been 600ms
    // if(held_down_note != i && note_timers[i] > 0 && mozziMicros() > note_timers[i] + 1000000 ){
    //   // Serial.println("finishined note timer for");
    //   // Serial.println(i);
    //   note_timers[i] = 0;
    //   notes[i]->set_frequency(0);
    //   // notes[i]->note_off();
    //   gains[i] = 0;
    // } else if(note_timers[i] > 0) {
    //   // note still rockin
    // }

    note_playing = notes[i]->is_playing();


    unsigned int this_gain = notes[i]->env_next();
      
    // is the note still held and is it time to go
    if(!note_playing || note_delays[i]->ready()){
      // Serial.println("I'm done");
      notes[i]->note_off();
      // held_down_note = -1;
      
    } else {
      // Serial.println("I'm playing");
      gains[i] = this_gain;
    }
  }
}

int updateAudio(){
  int sig = 0;
  // unsigned int gain = 0;
  // int active_sigs = 0;
  bool a_note_is_playing = false;
  // get vals from all playing notes
  for(int i=0; i<4; i++){

    // 0 frequency means note is dead :(
    if(notes[i]->is_playing()){
       a_note_is_playing = true;
      // found note, play dat
      // sig += notes[i]->osc_next();
      sigs[i] = notes[i]->osc_next();
      // active_sigs += 1;

      if(sigs[i]){
        sig += (gains[i] * sigs[i]);
      }
      // sig += sigs[i];

    }
  }
  
  // gain = 1;

  // return (int) (gain * sig)>>8;

  if(!a_note_is_playing){
    sig = 0;
  }
  return (int) sig>>8;
  // char dasig = low_pass.next((gain*sig));
  // return (int) low_pass.next(sig)>>8;
  // return (int) dasig;
}

void loop(){
  button_state = digitalRead(2);
  audioHook(); // required here
}