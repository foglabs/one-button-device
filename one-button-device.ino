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
#include <tables/sin8192_int8.h> // sine table for oscillator

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin0(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin1(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin2(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin3(SIN8192_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 256 // Hz, powers of 2 are most reliable
ADSR <AUDIO_RATE, AUDIO_RATE> envelope0;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope1;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope2;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope3;

int held_down_note = -1;
// bool button_state = digitalRead(2);
int input_freq = 440;

long note_timers[4];
unsigned int gains[4];
unsigned int gain = 0;

int max_osc = 0;
int min_osc = 0;


class Note {
  private:
    bool playing = false;

    int freq;
    int osc_index;
  public:
    Note(int, int);
    // for now just freq
    // void initialize(int, int);
    void note_on();
    void note_off(bool);

    bool is_playing();
    void set_is_playing(bool);

    int get_frequency();
    void set_frequency(int);
    void update_envelope();
    int osc_next();
    unsigned int env_next();
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

// only in updastecontrol! 
void Note::set_is_playing(bool timer_was_up){
  // if(osc_index == 0){
  //   playing = envelope0.playing();
  // } else if(osc_index == 1){
  //   playing = envelope1.playing();
  // } else if(osc_index == 2){
  //   playing = envelope2.playing();
  // } else if(osc_index == 3){
  //   playing = envelope3.playing();
  // } else {
  //   playing = false;
  // }
  playing = timer_was_up;

}

bool Note::is_playing(){
  return playing;
}


int Note::get_osc_index(){
  return osc_index;
}

void Note::note_off(bool kill=false){
  if(playing){
    // begin tail off
    if(osc_index == 0){
      // Serial.println("I did my notofos");
      envelope0.noteOff();
    } else if(osc_index == 1){

      envelope1.noteOff();
    } else if(osc_index == 2){

      envelope2.noteOff();
    } else if(osc_index == 3){

      envelope3.noteOff();
    }
  }
  
  // // deflag dat
  // if(kill){
    playing = false;
  // }
}

void Note::note_on(){
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
  if(osc_index == 0){
    return aSin0.setFreq(new_freq);
  } else if(osc_index == 1){
    return aSin1.setFreq(new_freq);
  } else if(osc_index == 2){
    return aSin2.setFreq(new_freq);
  } else if(osc_index == 3){
    return aSin3.setFreq(new_freq);
  }
}

void Note::update_envelope(){
  if(osc_index == 0){
    envelope0.update();
  } else if(osc_index == 1){
    envelope1.update();
  } else if(osc_index == 2){
    envelope2.update();
  } else if(osc_index == 3){
    envelope3.update();
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

unsigned int Note::env_next(){
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

// int sigs[4] = {0,0,0,0};

void play_note(int freq){
  int available_slot = 0;
  for(int i=0; i<4; i++){
    Serial.println("is_palying was");
    Serial.println(notes[i]->is_playing());
    if(notes[i]->is_playing() == false){
      available_slot = i;
      break;
    }
  }

  notes[available_slot]->set_frequency(freq);

  // Serial.println("Maybe it fucking worked");
  // Serial.println(notes[available_slot]->get_frequency());
  held_down_note = available_slot;

  // set timer for new note
  note_timers[available_slot] = mozziMicros();

    
  Serial.println("Running ::note_on for ");
  Serial.println(available_slot);
  notes[available_slot]->note_on();

  // total time of all envelope parts
  note_delays[available_slot]->start(40000);
}

void setup(){

  Serial.begin(9600);
  startMozzi(CONTROL_RATE); // :)
  note0 = Note(0, 0);
  note1 = Note(1, 0);
  note2 = Note(2, 0);
  note3 = Note(3, 0);
  notes[0] = &note0;
  notes[1] = &note1;
  notes[2] = &note2;
  notes[3] = &note3;

  EventDelay event_delay0 = EventDelay(3000);
  EventDelay event_delay1 = EventDelay(3000);
  EventDelay event_delay2 = EventDelay(3000);
  EventDelay event_delay3 = EventDelay(3000);
  note_delays[0] = &event_delay0;
  note_delays[1] = &event_delay1;
  note_delays[2] = &event_delay2;
  note_delays[3] = &event_delay3;
    
  unsigned int a_t = 800;
  byte a_level = 255;
  unsigned int d_t = 800;
  byte d_level = 255;
  unsigned int s_t = 4000;
  byte s_level = 0;
  unsigned int r_t = 800;
  byte r_level = 0;

  // aSin0.setFreq(freq);
  envelope0.setADLevels(a_level,d_level);
  // milliseconds
  envelope0.setTimes(a_t,d_t,s_t,r_t);
  envelope0.update();

  // aSin1.setFreq(freq);
  envelope1.setADLevels(a_level,d_level);
  // milliseconds
  envelope1.setTimes(a_t,d_t,s_t,r_t);
  envelope1.update();

  // aSin2.setFreq(freq);
  envelope2.setADLevels(a_level,d_level);
  // milliseconds
  envelope2.setTimes(a_t,d_t,s_t,r_t);
  envelope2.update();

  // aSin3.setFreq(freq);
  envelope3.setADLevels(a_level,d_level);
  // milliseconds
  envelope3.setTimes(a_t,d_t,s_t,r_t);
  envelope3.update();
}


bool button_state = false;
bool last_button_state = false;
long last_debounce_time = mozziMicros();
// 10ms
uint8_t debounce_delay = 10000;

void updateControl() {
  // read the state of the switch into a local variable:
  bool reading = digitalRead(2);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != last_button_state) {
    // reset the debouncing timer
    last_debounce_time = mozziMicros();
    Serial.println("Raeding was note same");
  }

  if ((mozziMicros() - last_debounce_time) > debounce_delay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != button_state) {
      button_state = reading;

      // only toggle the LED if the new button state is HIGH (uh)
      if (button_state && held_down_note<0) {
        // ledState = !ledState;
        Serial.println("Shit bitch I played a note");
        play_note(input_freq);
        Serial.println(held_down_note);

      } else if(!button_state) {
        Serial.println("No more note held");
        // no more note held
        // regular note held is set in play_note
        held_down_note = -1;
      }
    }
  }

  // digitalWrite(ledPin, ledState);

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  last_button_state = reading;

  for(int i=0; i<4; i++){
    // Serial.println(notes[i]->is_playing());

    // have to set this here because envelope runs at control rate!
    bool is_playing = notes[i]->is_playing() && !note_delays[i]->ready();
    notes[i]->set_is_playing(is_playing);
    // Serial.println("Note 0");
    // Serial.println(notes[0]->is_playing());
    // Serial.println("Note 1");
    // Serial.println(notes[1]->is_playing());
    // Serial.println("Note 2");
    // Serial.println(notes[2]->is_playing());
    // Serial.println("Note 3");
    // Serial.println(notes[3]->is_playing());
    
    
    if(notes[i]->is_playing() && held_down_note != i){
      Serial.println("Note goin off now");
      Serial.println(i);
      // do regular note off if button not held for this note
      notes[i]->note_off();
    }
    // } else if(note_delays[i]->ready()){
    //   Serial.println("I'm done");
      
    //   // kill that bitch, timer is up
    //   notes[i]->note_off(true);
    // }

    // update these at control pace, keep .nexting in updateAudio
    
  }
}

int updateAudio(){
  int sig = 0;

  // count up how man oscs are playing
  int active_sigs = 0;

  // get vals from all playing notes
  for(int i=0; i<4; i++){
    notes[i]->update_envelope();

    if(notes[i]->is_playing()){
    // found note, play dat
      active_sigs += 1;
    }

    sig += (notes[i]->env_next() * notes[i]->osc_next());
  }
  
  // average sigs if multiple
  if(active_sigs>1){
    sig = sig/active_sigs;
    // Serial.println("I divided by");
    // Serial.println(active_sigs);
  }
  // simplest
  // int sig = 0;
  // for(uint8_t i=0;i<4;i++){
  //   sig += (notes[i]->osc_next() * gains[i]);
  // }
  // return (int) sig>>8;

  // Serial.println(notes[0]->env_next());
  // return (int) (notes[0]->osc_next() * (notes[0]->env_next()/256))>>8;

  // yay!
  // return (int) (envelope0.next() * aSin0.next())>>8;
  
  return (int) sig>>8;
}

void loop(){
  audioHook(); // required here
}