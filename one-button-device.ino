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
#include <mozzi_midi.h>
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

LowPassFilter lpf;

bool play_arp = false;

int current_note = -1;
// bool button_state = digitalRead(2);
int input_freq = 440;

unsigned int gains[4];
unsigned int gain = 0;

uint8_t mode = 1;
#define REGNOTEMODE 0
#define ARPMODE 1
#define WEIRDMODE 2

class Note {
  private:
    bool playing = false;
    bool available = true;

    float freq;
    int osc_index;
  public:
    Note(int, int);
    // for now just freq
    // void initialize(int, int);
    void note_on();
    void note_off(bool);

    bool is_playing();
    void set_playing(bool);
    bool is_available();
    void set_available(bool);

    float get_frequency();
    void set_frequency(float);
    void update_envelope();
    int osc_next();
    unsigned int env_next();
    int get_osc_index();
};

Note::Note(int init_osc_index, int init_freq){
  osc_index = init_osc_index;
  // int freq = init_freq;

  // start this osc off with our freq
  if(init_freq > 0){
    if(osc_index == 0){
      aSin0.setFreq(init_freq);
    } else if(osc_index == 1){
      aSin1.setFreq(init_freq);
    } else if(osc_index == 2){
      aSin2.setFreq(init_freq);
    } else if(osc_index == 3){
      aSin3.setFreq(init_freq);
    }
  }
}

// only in updastecontrol! 
void Note::set_playing(bool timer_was_up){
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

bool Note::is_available(){
  return available;
}

void Note::set_available(bool is_it_available){
  available = is_it_available;
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
  
  playing = false;
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

float Note::get_frequency(){
  return freq;
}

void Note::set_frequency(float new_freq){
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

// this is for timing out arp notes
Note *notes[4];
EventDelay *note_delays[4];

Note note0 = Note(0, 0);
Note note1 = Note(1, 0);
Note note2 = Note(2, 0);
Note note3 = Note(3, 0);

uint8_t arp_note_index = 0;
EventDelay arp_delay = EventDelay(300);

long rando;

// button stuff
bool button_state = false;
bool last_button_state = false;
long last_debounce_time = mozziMicros();
// 10ms
uint8_t debounce_delay = 10000;

void play_note(int new_note, unsigned int delay_time){
  int available_slot = 0;
  for(int i=0; i<4; i++){
    Serial.println("is_palying was");
    Serial.println(notes[i]->is_playing());
    if(notes[i]->is_available() == true){
      available_slot = i;
      break;
    }
  }
  Serial.println("NOTE IS");
  Serial.println(new_note);
  float freq = note_to_freq(new_note);
  Serial.println("FREQ IS");
  Serial.println(freq);
  notes[available_slot]->set_frequency(freq);

  // Serial.println("Maybe it fucking worked");
  // Serial.println(notes[available_slot]->get_frequency());
  current_note = available_slot;
    
  Serial.println("Running ::note_on for ");
  Serial.println(available_slot);
  notes[available_slot]->note_on();

  if(delay_time > 0){
    // for arp, set a timer to know when to note off
    note_delays[available_slot]->start(delay_time);
  }
}

float note_to_freq(int midi_note){
  // internet trash
  // return (pow (2.0, ((float)(noter-69)/12.0))) * 440.0;
  // return (pow (2.0, ((float)(noter-69.0)/12.0))) * 440.0;
  return 440.0 * ( pow( 2.0, ((midi_note-69.0)/12.0) ) );
}

int next_arp_note(){
  switch (arp_note_index) {
    case 0: return 70;
        break;
    case 1: return 72;
        break;
    case 2: return 74;
        break;
    case 3: return 75;
        break;
    case 4: return 77;
        break;
    case 5: return 79;
        break;
    case 6: return 81;
        break;
    case 7: return 82;
        break;
    case 8: return 84;
        break;
    case 9: return 86;
        break;
    case 10: return 87;
        break;
    case 11: arp_note_index = 0;
        return 70;
        break;
    default: return 70;
  }
}

void play_arp_notes(){
  if(arp_delay.ready()){
    int next_note = next_arp_note();
    arp_note_index += 1;
    play_note(next_note, 800);
    arp_delay.start(300);
  }
}

void handle_note_button(){
  // read the state of the switch into a local variable:
  bool reading = digitalRead(2);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != last_button_state) {
    // reset the debouncing timer
    last_debounce_time = mozziMicros();
    // Serial.println("Raeding was note same");
  }

  if ((mozziMicros() - last_debounce_time) > debounce_delay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != button_state) {
      button_state = reading;

      // only toggle the LED if the new button state is HIGH (uh)
      if (button_state && current_note<0) {

        // HERE MEANS I"M GOING TO start PLAYing SOMTHING
        if(mode == REGNOTEMODE){
          input_freq = random(130, 4186);
          play_note(input_freq, 0);
        } else if(mode == ARPMODE){
          play_arp = true;
        }
        
      } else if(!button_state) {
        // Serial.println("No more note held");
        // no more note held
        // regular note held is set in play_note
        current_note = -1;
        play_arp = false;
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  last_button_state = reading;
}


void setup(){
  arp_delay.start(1000);

  lpf.setResonance(200);
  lpf.setCutoffFreq(6000);
  randomSeed(analogRead(0));

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


  // these are for letting notes play out their envs!
  EventDelay event_delay0 = EventDelay(3000);
  EventDelay event_delay1 = EventDelay(3000);
  EventDelay event_delay2 = EventDelay(3000);
  EventDelay event_delay3 = EventDelay(3000);
  note_delays[0] = &event_delay0;
  note_delays[1] = &event_delay1;
  note_delays[2] = &event_delay2;
  note_delays[3] = &event_delay3;


  unsigned int a_t = 120;
  byte a_level = 255;
  unsigned int d_t = 300;
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

void updateControl() {
  // check if note buttons down/ready for action -> if so, play note!
  handle_note_button();

  if(play_arp){
    play_arp_notes();
  }

  for(int i=0; i<4; i++){
    // Serial.println(notes[i]->is_playing());

    // have to set this here because envelope runs at control rate!
    // bool is_playing = notes[i]->is_playing() && !note_delays[i]->ready();
    // Serial.println("Note 0");
    // Serial.println(notes[0]->is_playing());
    // Serial.println("Note 1");
    // Serial.println(notes[1]->is_playing());
    // Serial.println("Note 2");
    // Serial.println(notes[2]->is_playing());
    // Serial.println("Note 3");
    // Serial.println(notes[3]->is_playing());
    
    if(mode == REGNOTEMODE){
      if(notes[i]->is_playing() && current_note != i){
        Serial.println("Note goin off now");
        Serial.println(i);
        // do regular note off if button not held for this note
        notes[i]->note_off();
        note_delays[i]->start(800);
        notes[i]->set_available(false);
      } else if(note_delays[i]->ready()){
        notes[i]->set_available(true);
      }  
    } else if(mode == ARPMODE) {
      if(note_delays[i]->ready()){
        notes[i]->note_off();
        notes[i]->set_available(true);
      } 

    }
    
  }
}

int updateAudio(){
  int sig = 0;

  // count up how man oscs are playing
  int active_sigs = 0;

  // get vals from all playing notes
  for(int i=0; i<4; i++){
    notes[i]->update_envelope();

    // if(!notes[i]->is_available()){
    // // found note, play dat
    //   active_sigs += 1;
    // }

    sig += ( notes[i]->env_next() * lpf.next(notes[i]->osc_next()) );
  }

  // simplest
  // int sig = 0;
  // for(uint8_t i=0;i<4;i++){
  //   sig += (notes[i]->osc_next() * gains[i]);
  // }
  // return (int) sig>>8;
  return (int) sig>>8;
}

void loop(){
  audioHook(); // required here
}