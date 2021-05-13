/*  Get it innnnnn one button device

controls
  -rotary
    reg note -> change semitone for *next* note

    arp -> base note for arp

    weird ->
      raw freqency
      
      enc drives current freq value
      button makes pulses
      remember last few seconds of pulse?

      hit button
        begin playing at current frequency
        begin recording into buffer


  -toggle switches


  -display


Cancel button?

*/

#include <MozziGuts.h>
#include <EventDelay.h>
#include <Oscil.h> // oscillator template
#include <ADSR.h>

// effects stuff
#include <LowPassFilter.h>
#include <AudioDelay.h>
// #include <ReverbTank.h>
// #include <Phasor.h>

#define AUDIO_MODE STANDARD_PLUS
#include <mozzi_midi.h>
#include <tables/sin8192_int8.h> // sine table for oscillator
#include <tables/cos2048_int8.h> // sine table for oscillator

// da pix
#include <Adafruit_NeoPixel.h>
#define NUMPIXELS 7
#define PIXELPIN 13
#define PIXDELAY 4000
// #define NUMPIXFADESTEPS 160.0

#define COOL 0
#define SHOWMODE 1
#define PLAY 2
#define WEIRD 3
#define IDLE 4
#define LOADING 5
#define ROTATE 6

long pixel_timer = mozziMicros();
long display_idle_timer = mozziMicros();
long display_aux_timer = mozziMicros();
Adafruit_NeoPixel pixel(NUMPIXELS, PIXELPIN, NEO_GRBW + NEO_KHZ800);
// r,g,b,r,g,b... NUMPIX * 3
byte pixel_colors[21];
// byte from_pixel_colors[21];
// byte dest_pixel_colors[21];
// uint8_t pixel_lerp_steps[7];
byte display_mode;
byte pixel_counter = 1;
bool pixel_flag = true;

bool displayPlayNotes[4] = { false, false, false, false };
byte buffer_amount = 0;
byte brightness_factor = 1;


// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin0(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin1(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin2(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin3(SIN8192_DATA);

// davibrato :D
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aVibrato(COS2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 256 // Hz, powers of 2 are most reliable
ADSR <AUDIO_RATE, AUDIO_RATE> envelope0;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope1;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope2;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope3;

// Effects objects
bool toggles[4] = {false,false,false,false};
// toggle this based on inputs
bool effects_active = false;

// to know if we should reset enveleopes
bool last_env_toggle = false;

LowPassFilter lpf;

// for flanger
// AudioDelay <256> aDel;

// Weirdmode stuff
// 4 seconds?
#define BUFFER_LENGTH 128
#define BUFFSTEP_LENGTH 128

// flag for ARPMODE, WEIRDMODE, SWEEPMODE to keep playing outside of individual Note code
// WEIRDMODE to say if were going into buffer machinery
// SWEEPMODE signal or no
// ARPMODE for if we're actually playing notes in arpmode
// bool play_continuing = false;
#define STOPPED 0
#define PLAYING 1
#define TAILING 2
byte play_continuing = STOPPED;

bool buffer_empty = true;
bool waiting_to_play_buffer = false;

char buffer[BUFFER_LENGTH];
byte buffcount = 0;
byte buffstepcount = 0;
EventDelay buffdelay = EventDelay(2000);
uint8_t bufferreplay = 0;
float slope = 0;

// chordschemamode stuff
#define MAJOR 0
#define MINOR 1
#define MAJORSEVENTH 2
#define MINORSEVENTH 3
#define MINORTHIRDS 4
#define MAJORTHIRDS 5
#define STEPFIFTH 6

int8_t current_note = 0;

// this is actually disabled right now
byte chord_schema = 0;

#define REGNOTEMODE 0
#define ARPMODE 1
#define WEIRDMODE 2
#define CHORDMODE 3
#define SWEEPMODE 4
#define REPEATMODE 5
#define CHORDSCHEMAMODE 6
#define BEAUTYMODE 7
#define PIANOMODE 8

// set via setup_mode in setup()
byte mode = REGNOTEMODE;

#define TOGGLE_0_PIN 3
#define TOGGLE_1_PIN 4
#define TOGGLE_2_PIN 5
#define TOGGLE_3_PIN 6

#define ROTARY_A_PIN 11
#define ROTARY_B_PIN 12
#define ROTARY_BUTTON_PIN 8

// this class wraps oscillators, when they're intended to be used for notes
// need to make sure resetting them works, causes weird mode will probably bypass this
class Note {
  private:
    bool playing = false;
    bool available = true;

    float freq;
    byte osc_index;
  public:
    Note(byte, int);
    // for now just freq
    // void initialize(int, int);
    void note_on();
    void note_off(bool);

    bool is_playing();
    bool is_available();
    void set_available(bool);

    float get_frequency();
    void set_frequency(float);
    void update_envelope();
    int osc_next();
    int osc_phmod_next(Q15n16);
    unsigned int env_next();
    byte get_osc_index();
};

Note::Note(byte init_osc_index, int init_freq){
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

bool Note::is_playing(){
  return playing;
}

bool Note::is_available(){
  return available;
}

void Note::set_available(bool is_it_available){
  available = is_it_available;
}

byte Note::get_osc_index(){
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

  byte oi = get_osc_index();
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

int Note::osc_phmod_next(Q15n16 phasemod_val){
  // modify that shit based on current control vals
  if(osc_index == 0){
    return (int)aSin0.phMod(phasemod_val);
  } else if(osc_index == 1){
    return (int)aSin1.phMod(phasemod_val);
  } else if(osc_index == 2){
    return (int)aSin2.phMod(phasemod_val);
  } else if(osc_index == 3){
    return (int)aSin3.phMod(phasemod_val);
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

Note *notes[4];
EventDelay *note_delays[4];

Note note0 = Note(0, 0);
Note note1 = Note(1, 0);
Note note2 = Note(2, 0);
Note note3 = Note(3, 0);

byte arp_note_index = 0;
// this is for timing out arp notes
EventDelay arp_delay = EventDelay(300);

uint8_t repeat_notes[4] = {0,0,0,0};


// for each chord
// EventDelay chord_delay = EventDelay(1300);




// long rando;

// button stuff
bool button_state = false;
bool last_button_state = false;
long last_debounce_time = mozziMicros();
// 10ms
uint8_t debounce_delay = 10000;

// rotary button
bool rbstate = false;
bool last_rbstate = false;
long last_rbdebounce_time = mozziMicros();
// 10ms
uint8_t rbdebounce_delay = 10000;
// time to hold down, 800ms
uint8_t rb_delay = 800000;
long rb_timer = mozziMicros();

// rotary
byte rotary_position = 12;
bool last_a;
bool clockwise = false;

long sweep_timer = mozziMicros();

byte available_note_slot(){
  for(byte i=0; i<4; i++){
    // Serial.println("is_palying was");
    // Serial.println(notes[i]->is_playing());

    if(mode == REPEATMODE && i>=2){
      // save the top 2 Notes for repeating
      break;
    }

    if(notes[i]->is_available() == true){
      Serial.print(i);
      Serial.println(F(" was available to play..."));
      return i;
      break;
    }
  }

  return 0;
}

void play_note(int new_note, unsigned int delay_time, uint8_t available_slot){
  // int available_slot = 0;
  // for(int i=0; i<4; i++){
  //   // Serial.println("is_palying was");
  //   // Serial.println(notes[i]->is_playing());

  //   if(mode == REPEATMODE && i>=2){
  //     // save the top 2 Notes for repeating
  //     break;
  //   }

  //   if(notes[i]->is_available() == true){
  //     Serial.print(i);
  //     Serial.println(" was available to play...");
  //     available_slot = i;
  //     break;
  //   }
  // }
  Serial.println(F("NOTE IS"));
  Serial.println(new_note);
  float freq = note_to_freq(new_note);
  // Serial.println("FREQ IS");
  // Serial.println(freq);
  notes[available_slot]->set_frequency(freq);

  // Serial.println("Maybe it fucking worked");
  // Serial.println(notes[available_slot]->get_frequency());
  
  if(mode == REGNOTEMODE || mode == REPEATMODE){
    // this is the which notes[i] slot were playing from in regular mode

    // not used in beauty or piano mode cause we already use all 4
    current_note = available_slot;  
  }
  
    
  Serial.println(F("Running ::note_on for "));
  Serial.println(available_slot);
  notes[available_slot]->note_on();
  notes[available_slot]->set_available(false);

  if(delay_time > 0){
    // for arp, set a timer to know when to note off
    note_delays[available_slot]->start(delay_time);
  }

  if(display_mode != PLAY){
    // if not play display, switch
    setDisplayMode(PLAY);
  }

  // show a pip for this note goddamnit!
  displayPlayNote(available_slot);
  Serial.print(F("Start display for "));
  Serial.println(available_slot);
  // keep resetting disp timer os play stay (rather than go back to cool)
  display_idle_timer = mozziMicros();
}

float note_to_freq(int midi_note){
  // internet trash
  // return (pow (2.0, ((float)(noter-69)/12.0))) * 440.0;
  // return (pow (2.0, ((float)(noter-69.0)/12.0))) * 440.0;
  return 440.0 * ( pow( 2.0, ((midi_note-69.0)/12.0) ) );
}

byte next_arp_note(){
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
  // ARPMODE
  if(arp_delay.ready()){
    byte next_note = next_arp_note();

    // offset note by current rotary value!
    next_note += rotary_position;

    arp_note_index += 1;

    int r_time = getReleaseTime( short_env_enabled() );
    byte available_slot = available_note_slot();
    play_note(next_note, r_time, available_slot);
    uint16_t arp_time;

    if(short_env_enabled()){
      arp_time = 300;
    } else {
      arp_time = 1200;
    }
    
    arp_delay.start(arp_time);
  }
}

void set_weird_freq(){
  float weird_freq = 440.0 + (rotary_position * 10);
  aSin0.setFreq(weird_freq);
}

void set_sweep_freq(){

  // exponentially increase with rot pos
  float sweep_freq = 523.25 + (pow(rotary_position, 2));
  aSin0.setFreq(sweep_freq);
}

void handle_rotary_button(){

  // goes low when pressed
  bool readin = !digitalRead(ROTARY_BUTTON_PIN);

  if(readin != last_rbstate){
    last_rbdebounce_time = mozziMicros();
  }

  long now = mozziMicros();
  if((now - last_rbdebounce_time) > rbdebounce_delay){
    if(readin != rbstate){

      Serial.println(F("Got Redin"));

      rbstate = readin;

      // this is wher da magik happens


      if(rbstate && (now - rb_timer) > rb_delay){
        // Serial.println("I incremented chordschema");
        // chord_schema++;
        // rb_timer = now;
        // if(chord_schema > 6){
        //   chord_schema = 0;
        // }

        mode++;
        rb_timer = now;
        if(mode > 8){
          mode = 0;
        }

        setup_mode(mode);
        Serial.print(F("I incremented MODE to "));
        Serial.println(mode);
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  last_rbstate = readin;
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

      // only do osmething if the new button state is HIGH (uh)
      if(button_state && current_note<0) {

        byte available_slot = available_note_slot();

        // HERE MEANS I"M GOING TO start PLAYing SOMTHING
        if(mode == REGNOTEMODE){
          // next note is C3 offset by current rotary value!
          // Serial.print("Trying to play");
          // Serial.println(48 + rotary_position);
          // Serial.print("rotary_position");
          // Serial.println(rotary_position);
          // play_note(48 + rotary_position, 0);
          play_note(72 + rotary_position, 0, available_slot);
        } else if(mode == ARPMODE){
          play_continuing = PLAYING;
        } else if(mode == WEIRDMODE){
          // do some ol other shit
          start_play_weird();
          
          // show buffer fill on pixel now tghat were playing
          setDisplayMode(WEIRD);

        } else if(mode == CHORDMODE){

          // play a chord
          Serial.print(F("love to play chord "));
          Serial.println(rotary_position);
          play_seq_chord(rotary_position);
        } else if(mode == CHORDSCHEMAMODE){

          Serial.print(F("love to play schema chord "));
          Serial.println(rotary_position);

          // in this case, were thinking of input as a note were modifying
          play_schema_chord(60+rotary_position);
        } else if(mode == SWEEPMODE){

          start_play_sweep();
          if(display_mode != ROTATE){
            setDisplayMode(ROTATE);
          }
        } else if(mode == REPEATMODE){

          Serial.println(F("Playing Repeat"));

          uint8_t note = 72 + rotary_position;
          play_note(note, 0, available_slot);
          setRepeatNote(note);

        } else if(mode == BEAUTYMODE){
          // next note is C3 offset by current rotary value!

          // play overtones fool
          Serial.println(F("trying hard to play beauty..."));
          play_note(72 + rotary_position, 2000, 0);
          play_note(84 + rotary_position, 100, 1);
          play_note(91 + rotary_position, 2000, 2);
          play_note(96 + rotary_position, 3000, 3);
        } else if(mode == PIANOMODE){
          // next note is C3 offset by current rotary value!

          // play overtones fool
          Serial.println(F("trying hard to play piano..."));
          play_note(72 + rotary_position, 1800, 0);
          play_note(84 + rotary_position, 800, 1);
          play_note(91 + rotary_position, 400, 2);
          play_note(96 + rotary_position, 600, 3);
        } else {
          // something
        }
        
      } else if(!button_state) {
        // Serial.println("No more note held");
        // no more note held
        // regular note held is set in play_note
        // chord root held is set in play_seq_chord
        current_note = -1;

        play_continuing = TAILING;

        // stop playing weird
        if(mode == WEIRDMODE){
          // do some ol other shit
          stop_play_weird();
          // start waiting to playback buffer
          buffdelay.start(600);
        } else if(mode == CHORDMODE || mode == CHORDSCHEMAMODE){
          for(byte i=0; i<4; i++){
            // start noteoffs because button was released
            notes[i]->note_off();
            displayPlayNotes[i] = false;
          }
        } else if(mode == SWEEPMODE){

          stop_play_sweep();
        }

      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  last_button_state = reading;
}

void setRepeatNote(byte note){

    byte available_slot = 2;
    for(byte i=2; i<4; i++){

    if(notes[i]->is_available() == true){
      Serial.print(i);
      Serial.println(F(" was available to set repeat..."));
      available_slot = i;
      break;
    }
  }

  // wait 1 sec ... then play elsewheref  
  note_delays[available_slot]->start(1000);

  for(byte i=0;i<4;i++){
    if(repeat_notes[i] == 0){
      // save which note to repeat at the first available spot
      repeat_notes[i] = note;
      break;
    }
  }
}

byte getRepeatNote(){
  for(byte i=0; i<4;i++){
    if(repeat_notes[i] > 0){
      byte note = repeat_notes[i];
      repeat_notes[i] = 0;
      return note;
    }
  }

  return 0;
}

// chord based on starting note and selected chord shape
void play_schema_chord(byte starting_note){
  for(byte i=0; i<4; i++) {
    notes[i]->note_off();
    notes[i]->set_available(true);
    displayPlayNotes[i] = false;
  }
  byte note1 = 0;
  byte note2 = 0;
  byte note3 = 0;
  byte note4 = 0;

  Serial.print(F("Chord schema is "));
  Serial.println(chord_schema); 

  if(chord_schema == MAJOR){
    note1 = starting_note;
    note2 = starting_note+4;
    note3 = starting_note+7;
  } else if(chord_schema == MINOR){
    note1 = starting_note;
    note2 = starting_note+3;
    note3 = starting_note+7;
  } else if(chord_schema == MAJORSEVENTH){
    note1 = starting_note;
    note2 = starting_note+3;
    note3 = starting_note+7;
    note4 = starting_note+11;
  } else if(chord_schema == MINORSEVENTH){
    note1 = starting_note;
    note2 = starting_note+3;
    note3 = starting_note+7;
    note4 = starting_note+10;
  } else if(chord_schema == MINORTHIRDS){
    note1 = starting_note;
    note2 = starting_note+3;
    note3 = starting_note+3;
  } else if(chord_schema == MAJORTHIRDS){
    note1 = starting_note;
    note2 = starting_note+4;
    note3 = starting_note+8;
  } else if(chord_schema == STEPFIFTH){
    note1 = starting_note;
    note2 = starting_note+2;
    note3 = starting_note+7;
  }

// just set which slot to what we want
  play_note(note1, 1000, 0);

  if(note2 > 0){
    play_note(note2, 1000, 1);
  }

  if(note3 > 0){
    play_note(note3, 1000, 2);
  }

  if(note4 > 0){
    play_note(note4, 1000, 3);
  }
}

// chords in a seequence bsed on offset
void play_seq_chord(byte note_offset){
  // open up all of the notes so we dont get a straggling one from prev chord that fucks up the signal
  for(byte i=0; i<4; i++) {
    notes[i]->note_off();
    notes[i]->set_available(true);

    displayPlayNotes[i] = false;
  }
  byte note1;
  byte note2;
  byte note3;

  switch (abs(note_offset % 9)) {
    case 0: note1 = 72;
            note2 = 76;
            note3 = 79;
        break;
    case 1: note1 = 74;
            note2 = 78;
            note3 = 81;
        break;
    case 2: note1 = 76;
            note2 = 79;
            note3 = 83;
        break;
    case 3: note1 = 77;
            note2 = 81;
            note3 = 84;
        break;
    case 4: note1 = 78;
            note2 = 82;
            note3 = 85;
        break;
    case 5: note1 = 79;
            note2 = 83;
            note3 = 86;
        break;
    case 6: note1 = 81;
            note2 = 83;
            note3 = 88;
        break;
    case 7: note1 = 81;
            note2 = 84;
            note3 = 88;
    case 8: note1 = 81;
            note2 = 85;
            note3 = 88;            
        break;
        default: return;
  }

  // track this so we know what the root note is
  current_note = note_offset;

  // /72 is our base note C3
  //  how many of our 9-step 'octaves' did we get, add a real octave for each additional one
  byte octave_offset = ( note_offset/9 ) * 12;

  Serial.print(F("Octave Offset... "));
  Serial.println(octave_offset);

  int r_time = getReleaseTime( short_env_enabled() );
  
  byte available_slot = available_note_slot();
  play_note(note1 + octave_offset, r_time, available_slot);
  
  available_slot = available_note_slot();
  play_note(note2 + octave_offset, r_time, available_slot);
  
  available_slot = available_note_slot();
  play_note(note3 + octave_offset, r_time, available_slot);
}

void start_play_weird(){
  // start playing all oscs
  envelope0.noteOn(true);
  play_continuing = PLAYING;
}

void stop_play_weird(){
  // stop playing - we using 1 env for all oscs
  envelope0.noteOff();
  play_continuing = TAILING;
  for(byte i=0; i<BUFFER_LENGTH; i++){
    buffer[i] = 0;
  }
}

void start_play_sweep(){
  Serial.println(F("I start play Sweep")); 
  envelope0.noteOn(true);
  play_continuing = PLAYING;
}

void stop_play_sweep(){
  Serial.println(F("I Stop play Sweep"));
  envelope0.noteOff();
  play_continuing = TAILING;
  // set up env timer for the one Note we're using (0)
  Serial.println(getReleaseTime( short_env_enabled() ));
  note_delays[0]->start( 1000 );
}
// effects helpers
bool flanger_enabled(){
  // YOOO
  return toggles[0];
}

bool compressor_enabled(){
  return toggles[1];
}

bool short_env_enabled(){
  // YOOO
  return toggles[0];
  return toggles[2];
}

// too big for memory :/
// bool reverb_enabled(){
//   return toggles[2];
// }

// bool waveshaper_enabled(){
//   return toggles[1] == 1;
// }

bool vibrato_enabled(){
  // defined near osc_next
  return toggles[3];
}

void set_effects() {
  for(byte i=0; i<4; i++){
    // bool newtoggle = digitalRead(i);
    // if(newtoggle != toggles[i]){
    //   Serial.print(i);
    //   Serial.print(" Changed to ");
    //   Serial.println(newtoggle);
    // }
    
    // comes in backwards... (and noexistent ones are true)
    toggles[i] = !digitalRead(i+3);;
  }
}

#define THRESHOLD -20.0
#define WIDTH 6.0
#define LOWER -23.0
#define UPPER -17.0
#define RATIO 3.0
#define SLOPE -2.0
// #define SOFTSLOPEFACTOR -1.0/6.0;

int render_compressor(int signal){

  // keep this here!
  // take away the sign
  // float amplitude = abs(signal/256);

  // convert 0-1 amp to db;
  float sig_db = 20 * log10(abs(signal/256));

  // float threshold = -20.0;
  // // knee?
  // float width = 6.0;

  // float lower = threshold - (width * 0.5);
  // float upper = threshold + (width * 0.5);
  // float ratio = 3.0;
  // float slope = 1.0f - ratio;

  if (sig_db <= LOWER){
    return 0;
  } else if (sig_db <= UPPER){
    // original
    // float soft_slope = SLOPE * ((sig_db - LOWER) / WIDTH) * 0.5;

    // return (int) ((1.0/6.0) * (sig_db-LOWER)) * (LOWER - sig_db);
    return (int) (sig_db + 23.0)/17.0;
  } else {
    return (int) SLOPE * (THRESHOLD - sig_db);
  }
}

int render_reverb(int signal){
  // return signal + reverb.next(signal);
  return signal;
}

int render_effects(int signal) {
  // its not enabled
  // if(flanger_enabled()){
  //   // do delay
  //   signal = signal + aDel.next((int8_t)signal, 512);
  // }

  // if(waveshaper_enabled()){
  //   // float max = 20;
  //   // signal = (int) (signal > max) ? max : (signal < -max) ? -max : signal;

  //   // softclip from Q
  //   Serial.println("Wave");
  //   signal = (int) 270 * signal - 90 * signal * signal * signal;
  // }

  if(compressor_enabled()){
    signal = signal + render_compressor(signal);
  }
  
  // if(reverb_enabled()){
  //   signal = signal + render_reverb(signal);
  // }

  return (int) signal;
}

float get_slope(float y2, float x2, int y1, int x1){
  return (y2-y1/x2-x1);
}

void setup(){
  // start somewhere random in the 'random seq'
  randomSeed(analogRead(0));

  // start up the neopix
  pixel.begin();

  // initialize colors
  for(byte i=0; i<21; i++){
    pixel_colors[i] = 0;
    // from_pixel_colors[i] = 0;
    // dest_pixel_colors[i] = 0;
  }

  pinMode(TOGGLE_0_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_1_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_2_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_3_PIN, INPUT_PULLUP);

  pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);

  arp_delay.start(300);

  // lpf currently applied to EVERYTHING, maybe dump if corrected passive filter sounds better
  lpf.setResonance(20);
  lpf.setCutoffFreq(14000);


  // float phase_freq = 55.f;
  // aPhasor1.setFreq(phase_freq);
  
  // byte vib_intensity = 255;
  aVibrato.setFreq(3.f);

  
  // -128 to 127
  // for udpatecontrol
  // aDel.setFeedbackLevel(-111);
  // this is how used in updateaudio
  // return asig/8 + aDel.next(asig, deltime); // mix some straight signal with the delayed signal

  // seed dat
  // randomSeed(analogRead(0));

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

  setup_envelopes(false);
  setup_mode(PIANOMODE);
}

void setup_envelopes(bool short_env){

  unsigned int a_t = 0;
  unsigned int d_t = 0;
  unsigned int s_t = 0;
  unsigned int r_t = 0;
  byte a_level = 0;
  byte d_level = 0;
  byte s_level = 0;
  byte r_level = 0;

  // if(short_env){
  
  //   a_t = 10;
  //   a_level = 255;
  //   d_t = 10;
  //   d_level = 255;
  //   s_t = 10000;
  //   s_level = 0;
  //   r_t = 100;
  //   r_level = 0;
  // } else {

  //   a_t = 120;
  //   a_level = 255;
  //   d_t = 300;
  //   d_level = 255;
  //   s_t = 4000;
  //   s_level = 0;
  //   r_t = 1600;
  //   r_level = 0;
  // }
  
  a_t = msToMozziTime( getAttackTime(short_env) );
  a_level = getAttackLevel(short_env);
  d_t =msToMozziTime(  getDecayTime(short_env) );
  d_level = getDecayLevel(short_env);
  s_t = msToMozziTime( getSustainTime(short_env) );
  s_level = getSustainLevel(short_env);
  r_t = msToMozziTime( getReleaseTime(short_env) );
  r_level = getReleaseLevel(short_env);

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

uint16_t msToMozziTime(uint16_t ms){
  return (uint16_t) ms / 4;
}

uint16_t getAttackTime(bool short_env){
  if(short_env){
    return 40;
    
  } else {
    return 480;
  }
}
uint16_t getAttackLevel(bool short_env){
  if(short_env){
    return 255;
  } else {

    return 255;
  }

}
uint16_t getDecayTime(bool short_env){
  if(short_env){
    return 10;
  } else {

    return 300;
  }

}
uint16_t getDecayLevel(bool short_env){
  if(short_env){
    return 255;
  } else {
    return 255;
  }

}
uint16_t getSustainTime(bool short_env){
  if(short_env){
    return 200000;
  } else {
    return 400000;
  }

}
uint16_t getSustainLevel(bool short_env){
  if(short_env){
    return 255;
  } else {
    return 255;
  }

}
uint16_t getReleaseTime(bool short_env){
  if(short_env){
    return 100;
  } else {
    return 800;
  }

}
uint16_t getReleaseLevel(bool short_env){
  if(short_env){
    return 0;
  } else {
    return 0;
  }
}

void setup_mode(byte newmode){
  
  mode = newmode;
  // turn this off so we dont carry over from other mode
  play_continuing = STOPPED;

  if(mode == REGNOTEMODE || mode == CHORDMODE || mode == CHORDSCHEMAMODE || mode == BEAUTYMODE|| mode == PIANOMODE ){
    
    envelope0.update();
    envelope1.update();
    envelope2.update();
    envelope3.update();
  } else if(mode == ARPMODE){

  } else if(mode == WEIRDMODE){
    set_weird_freq();

    // init buffer, its 128 so byte okay
    for(byte i=0; i<BUFFER_LENGTH; i++){
      buffer[i] = 0;
    }
    buffer_empty = true;
  } else if(mode == SWEEPMODE){

    envelope0.update();
    set_sweep_freq();
  }

  // qa dat display
  setDisplayMode(SHOWMODE);
}

void setDisplayMode(byte dmode){
  bool zeroOutDisplay = false;
  if(dmode == SHOWMODE){
    Serial.println(F("I started showmode"));
  } else if(dmode == PLAY){
    Serial.println(F("I started playmode"));
    // set colors to 0 to get rid of cool blue
    zeroOutDisplay = true;
  } else if(dmode == COOL){
    Serial.println(F("I started coolmode"));
    zeroOutDisplay = true;
  } else if (dmode == WEIRD){
    Serial.println(F("I started weridmode"));
  } else if(dmode == IDLE){
    pixel_flag = true;
    // set random colors for init on IDLE
    Serial.println(F("I started IDLEmode"));
    // zeroOutDisplay = true;
    for(byte i=0; i<21; i++){
      pixel_colors[i] = rand() % 10;
    }
  } else if(dmode == LOADING){
    Serial.println(F("I started loadingmode"));
    zeroOutDisplay = true;
  }
  if(zeroOutDisplay){
    for(byte i=0; i<21; i++){
      pixel_colors[i] = 0;
    }
  }

  display_idle_timer = mozziMicros();

  // reset all this reusable shit  
  pixel_counter = 0;
  display_aux_timer = mozziMicros();
  pixel_flag = false;

  display_mode = dmode;
}

void updateControl() {

  // why the fuck does this only work out here?! inside void get_rotary() function, ++ behaves differently
  bool aVal = digitalRead(ROTARY_A_PIN);
  if(aVal != last_a){
    // Means the knob is rotating// if the knob is rotating, we need to determine direction// We do that by reading pin B.
    if(digitalRead(ROTARY_B_PIN) != aVal){
      // Means pin A Changed first -We're RotatingClockwise.
      rotary_position++;
      // clockwise = true;
      // Serial.println("Rotary ++");
      // Serial.print("Rotary is now");
  // Serial.println(rotary_position);  
    } else {
      // Otherwise B changedfirst and we're moving CCW
      // clockwise = false;
      rotary_position--;
      // Serial.println("Rotary --");
        // Serial.print("Rotary is now");
        // Serial.println(rotary_position);
    }
    Serial.print(F("Rotary postition is "));
    Serial.println(rotary_position);

    // only set when changing freq
    if(mode == WEIRDMODE){
      set_weird_freq();
    }
  }
  last_a = aVal;

  handle_rotary_button();
  // check if note buttons down/ready for action -> if so, play note!
  handle_note_button();
  // just set em into toggles[]
  set_effects();

  bool this_env_toggle = short_env_enabled();
  if(last_env_toggle != this_env_toggle){
    setup_envelopes(short_env_enabled());
  }
  last_env_toggle = this_env_toggle;

  if(mode == ARPMODE && play_continuing){
    // keep playin that arp
    play_arp_notes();
  }

  // handle events for oscillatorsz
  for(byte i=0; i<4; i++){

    if(mode == REGNOTEMODE){
      
      if(notes[i]->is_playing() && current_note != i){

        // do regular note off if button not held for this note
        notes[i]->note_off();
        note_delays[i]->start(800);
        
        // get rid of that pip
        // Serial.print("Stop pip! ");
        // Serial.println(i);
        displayPlayNotes[i] = false;

      } else if(note_delays[i]->ready()){

        notes[i]->set_available(true);
      }

    } else if(mode == CHORDMODE){
      // bazwah?!
    } else if(mode == ARPMODE) {
      
      if(note_delays[i]->ready()){
        notes[i]->note_off();
        notes[i]->set_available(true);
        displayPlayNotes[i] = false;
      }

    } else if(mode == WEIRDMODE){
      // set above
    } else if(mode == SWEEPMODE){
      // wait 2ms to change sweep freq
      //  oops 200 is 2 microseconds
      if((mozziMicros() - sweep_timer) > 200000){
        set_sweep_freq();
        sweep_timer = mozziMicros();
      }
    } else if(mode == REPEATMODE){

      if(i < 2){

        if(notes[i]->is_playing() && current_note != i){

          // do regular note off if button not held for this note
          notes[i]->note_off();
          displayPlayNotes[i] = false;
          note_delays[i]->start(800);
          // notes[i]->set_available(false);
        } else if(note_delays[i]->ready()){

          notes[i]->set_available(true);
        }
      } else {

        // 2,3 are for repeatnotes

        // first note delay to be ready should correspond to first available repeatnote
        if(  note_delays[i]->ready() ){

          // grabs and delets first found repeatnote
          byte note = getRepeatNote();
          if(note > 0){

            // yes, werre repeating, start playing a repeat note, this will start another timer for the note we play
            Serial.print(F("I payed a repeat dummy "));
            Serial.println(note);
            play_note(note, 600, i);
          } else {

            // note was still playing, so now start note off for repeat note
            notes[i]->note_off();
            note_delays[i]->start(800);
            notes[i]->set_available(true);
            displayPlayNotes[i] = false;
          }
        }
      }
    } else if(mode == BEAUTYMODE || mode == PIANOMODE){
      if(!button_state){
        // if note0 playing but button not held anymore
        notes[i]->note_off();
        displayPlayNotes[i] = false;
      }
    }

    // shut off after we waited for the release
    if(play_continuing == TAILING && note_delays[0]->ready()){
      Serial.println(F("I stoped play con"));
      play_continuing = STOPPED;
    }
  }


  // set pixel now that we've changed state
  writeDisplay();
}

void handleDisplay(){
  if(display_mode == COOL){
    // showRandom();
    showCool();
  } else if(display_mode == SHOWMODE) {
    // display what mode were in
    showMode(mode);
  } else if(display_mode == PLAY){
    // show notes and level
    showPlay();
  } else if(display_mode == WEIRD){
    // show buffer filling up
    // then show buffer draining out
    showWeird();
  } else if(display_mode == IDLE){
    showIdle();
  } else if(display_mode == LOADING){
    showLoading();
  } else if(display_mode == ROTATE){
    showRotate();
  }

  if(!button_state && display_mode != IDLE && mozziMicros() - display_idle_timer >= 3000000){
    // go back after 2s if button not held
    setDisplayMode(IDLE);
    Serial.println(F("Set IDLE again."));
    Serial.println(IDLE);
  }
}

void writeDisplay(){  
// set neopixel
  long now = mozziMicros();
  if(now - pixel_timer >= PIXDELAY){
    handleDisplay();

    pixel.clear();
    byte pixcolorindex = 0;
    for(byte i=0; i<NUMPIXELS; i++) {
      // uint8_t r = rand() % 6;
      // uint8_t g = rand() % 6;
      // uint8_t b = rand() % 6;
      pixcolorindex = i * 3;
      // Serial.print("Setting pix to ");
      // Serial.print(pixel_colors[pixcolorindex]);
      // Serial.print(" ");
      // Serial.print(pixel_colors[pixcolorindex+1]);
      // Serial.print(" ");
      // Serial.println(pixel_colors[pixcolorindex+2]);
      pixel.setPixelColor( i, pixel.Color(pixel_colors[pixcolorindex], pixel_colors[pixcolorindex+1], pixel_colors[pixcolorindex+2]) );

      pixel.show();
    }
    pixel_timer = now;
  }
}

bool inRange(byte low, byte high, byte value){
  return low <= value && value <= high;
}

bool colorCloseEnough(byte color, byte destColor){
  return inRange(destColor, destColor, color);
}

void showWeird(){
  if(buffer_amount >= 0){

    // x3 so its just one loop to 255 each rgb
    byte numlights = floor(buffer_amount/16);
    Serial.print(F("Numlights is "));
    Serial.println(numlights);
    for(byte i=0; i<21; i++){
        
      if(i<numlights){
        pixel_colors[i] = 255;
      } else {
        pixel_colors[i] = 0;
      }
    }

  }
}

void showPlay(){
  // well hi
  byte pixcolorindex;
  bool someonePlayin = false;
  for(byte i=0; i<4; i++){
    pixcolorindex = i * 3;
    if(displayPlayNotes[i]){
      someonePlayin = true;
    } else {
      // if it aint playin, shut it off
      pixel_colors[pixcolorindex] = 0;
      pixel_colors[pixcolorindex+1] = 0;
      pixel_colors[pixcolorindex+2] = 0;
    }
  }


  byte decrement;
  for(byte i=12; i<21; i++){
    if(!someonePlayin){
      
      // fade the 'higher' meter pixels faster so it fades down
      decrement = (21 - i) * 8;

      if(pixel_colors[i] > 0){
        // fade down
        // Serial.print("Fade down stupid! ");
        // Serial.println(decrement);

        if(decrement < pixel_colors[i]){
          // DONT go 'below' 0, you not gonna like it
          pixel_colors[i] = pixel_colors[i] - decrement;
        } else {
          pixel_colors[i] = 0;
        }
      }      
    }
  }
}

void displayPlayNote(byte note_index){
  // well hi
  byte pixcolorindex = note_index * 3;
  displayPlayNotes[note_index] = true;

  // note is playin but not already lit

  // set rand color on corresponding jewel
  Serial.print(F("Setting pix at "));
  Serial.println(pixcolorindex);

  pixel_colors[pixcolorindex] = rand() % 64;
  pixel_colors[pixcolorindex+1] = rand() % 32;
  pixel_colors[pixcolorindex+2] = rand() % 128;

  // turn white
  // for(byte i=12; i<21; i++){
  //   // if its notes playin, turn on the meter lgiths
  //   pixel_colors[i] = 255;
  // }
  for(byte i=4; i<NUMPIXELS; i++){
    // if its notes playin, turn on the meter lgiths
    pixel_colors[i*3] = 180 - mode * 16;
    pixel_colors[i*3+1] = mode * 16;
    pixel_colors[i*3+2] = 255 - mode / 7;
  }
}

void showRotate(){
  // for(byte i=0; i<NUMPIXELS; i++){
  //   pixel_colors[i*3] = i * 17;
  //   pixel_colors[i*3+1] = i * 17;
  //   pixel_colors[i*3+2] = i * 17;
  // }
  // return;
  long now = mozziMicros();
  if(now - display_aux_timer >= 160000){
    display_aux_timer = now;

    for(byte i=1; i<NUMPIXELS; i++){
      if(pixel_counter == i || pixel_counter == i+1 || pixel_counter == 1 && i==6 ){

        // == 6 to cover 6 and  0 case
        pixel_colors[i*3] = 0;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 255;
      } else {
        pixel_colors[i*3] = 0;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 0;
      }
    }  

    pixel_counter += 1;
    if(pixel_counter > 6){
      pixel_counter = 1;
    }
  }
}

void showLoading(){
  // byte num = rand() % 3;
  // for(byte i=0; i<num; i++){
  //   if(pixel_flag){
  //     pixel_colors[pixel_counter*3+i] += 1;
  //   } else {
  //     pixel_colors[pixel_counter*3+i] -= 5;
  //   }
  // }
  long now = mozziMicros();
  if(now - display_aux_timer >= 1000){
    display_aux_timer = now;

    byte second_pix = pixel_counter + 2;
    byte third_pix = pixel_counter + 4;
    if(second_pix > 6){
      second_pix = 0;
    }
    if(third_pix > 6){
      third_pix = 0;
    }

    if(pixel_flag){
      pixel_colors[pixel_counter*3+2] += 5;
      
      pixel_colors[second_pix*3] += 1;
      pixel_colors[second_pix*3+2] += 1;

      pixel_colors[third_pix*3] += 17;
      pixel_colors[third_pix*3+1] += 17;
    } else {
      pixel_colors[pixel_counter*3+2] -= 5;
      
      pixel_colors[second_pix*3] -= 5;
      pixel_colors[second_pix*3+2] -= 5;

      pixel_colors[third_pix*3] -= 3;
      pixel_colors[third_pix*3+1] -= 3;
    }

    if(pixel_colors[pixel_counter*3+2] == 255){
      pixel_flag = false;
    }

    if(!pixel_flag && pixel_colors[pixel_counter*3+2] == 0){
      pixel_counter += 1;
    }

    if(pixel_counter == NUMPIXELS){
      pixel_counter = 0;
    }
  }
}

void showIdle(){
  long now = mozziMicros();
  if(now - display_aux_timer >= 40000){
    display_aux_timer = now;
    
    byte choice = rand() % 21;
    if(pixel_colors[choice] < 10){
      pixel_colors[choice] += 1;
    } else {
      pixel_colors[choice] = 0;
    }
  }
}

void showCool(){
  // check if we arrived at dest colors
  // for(byte i=0; i<21; i++){
  //   if(pixel_colors[i] == dest_pixel_colors[i]){
  //     dest_pixel_colors[i] = rand() % 10;
  //     pixel_lerp_steps[i] = 0;
  //   } else {
  //     pixel_colors[i] = lerp(pixel_colors[i], dest_pixel_colors[i], pixel_lerp_steps[i]);
  //     pixel_lerp_steps[i] += 1;
  //   }
  // }

  // byte pixcolorindex;
  // for(byte i=0; i<NUMPIXELS; i++){
  //   // loop through each pixel
  //   pixcolorindex = i * 3;
  //   byte compoindex;

  //   if( colorCloseEnough(pixel_colors[pixcolorindex], dest_pixel_colors[pixcolorindex]) && colorCloseEnough(pixel_colors[pixcolorindex+1], dest_pixel_colors[pixcolorindex+1]) && colorCloseEnough(pixel_colors[pixcolorindex+2], dest_pixel_colors[pixcolorindex+2]) ){

  //     // Serial.print("I set new color for ");
  //     // Serial.println(pixcolorindex);

  //     // if we reached the rgb desgination for this pixel
  //     // preserve dest color as from color so we can fade propaly
  //     from_pixel_colors[pixcolorindex] = dest_pixel_colors[pixcolorindex];
  //     dest_pixel_colors[pixcolorindex] = 0;
  //     // dest_pixel_colors[pixcolorindex] = rand() % 100;
  //     pixel_lerp_steps[pixcolorindex] = 0;

  //     from_pixel_colors[pixcolorindex+1] = dest_pixel_colors[pixcolorindex+1];
  //     dest_pixel_colors[pixcolorindex+1] = 0;
  //     // dest_pixel_colors[pixcolorindex+1] = rand() % 25;
  //     pixel_lerp_steps[pixcolorindex+1] = 0;

  //     from_pixel_colors[pixcolorindex+2] = dest_pixel_colors[pixcolorindex+2];
  //     dest_pixel_colors[pixcolorindex+2] = rand() % 150;
  //     pixel_lerp_steps[pixcolorindex+2] = 0;

  //     // Serial.print("The new destination is.. ");
  //     // Serial.print(dest_pixel_colors[pixcolorindex]);
  //     // Serial.print(" ");
  //     // Serial.print(dest_pixel_colors[pixcolorindex+1]);
  //     // Serial.print(" ");
  //     // Serial.print(dest_pixel_colors[pixcolorindex+2]);
  //   } else {
  //     for(byte x=0; x<3; x++){
  //       // loop through r,g,b for each pixel
  //       compoindex = pixcolorindex+x;
  //       // track u as byte, turn into float here -> only 7 so same u steps for each whole rgb
  //       pixel_colors[compoindex] = lerp(pixel_colors[compoindex], dest_pixel_colors[compoindex], pixel_lerp_steps[ pixcolorindex ] );
  //     }

  //     pixel_lerp_steps[ pixcolorindex ] += 1;
  //   }

  // }
}

byte lerp(byte a, byte b, uint8_t u){
  float uval = u/255.0;
  // return (endValue - startValue) * stepNumber / lastStepNumber + startValue;
  return (byte) (b-a) * uval + a;
}

// void showRandom(){
//   for(byte i=0; i<21; i++){
//     // set our pix to random colors
//     pixel_colors[i] = rand() % 10;
//   }
// }

void showMode(byte mode){
  byte r = 255;
  byte g = 0;
  byte b = 255;
  byte numToLight = mode;

  if(mode >= NUMPIXELS){
    // for big mode (7 or more aka more than 0..6), different color
    r = 0;
    g = 255;
    b = 0;
    numToLight = mode - NUMPIXELS;
  }

  byte pixcolorindex;
  for(byte i=0; i<NUMPIXELS; i++){
    pixcolorindex = i * 3;
    if(i <= numToLight){
      // light up each pixel up to mode number
      pixel_colors[pixcolorindex] = r;
      pixel_colors[pixcolorindex+1] = g;
      pixel_colors[pixcolorindex+2] = b;
    } else {
      // dark it out
      pixel_colors[pixcolorindex] = 0;
      pixel_colors[pixcolorindex+1] = 0;
      pixel_colors[pixcolorindex+2] = 0;
    }
  }
}

int updateAudio(){
  int sig = 0;
  // -if button down, play and fill buffer
  // -if button was down, now up, start wait
  // -if wait complete, play buffer

  // get vals from all playing notes
  if(mode == WEIRDMODE){
    
    // keep on playin
    if(play_continuing != STOPPED){
      // play while I'm holdin that button
      sig = aSin0.next();


      // fill buffer
      // take a sample every 128 steps... eww!
      if(buffstepcount == 0){

        // Serial.print("SetBUFFERE ");
        // Serial.println((int)buffer[buffcount]);
        buffcount++;

        // there is officially *something* in the buffer
        buffer_empty = false;

        // only TAKE a sample once every 63 or whatever
        buffer[buffcount] = (char) sig/2.0;
        // fill up lights as we fill buffer
        if(buffer_amount < BUFFER_LENGTH){
          buffer_amount += 2;
          if(buffer_amount>BUFFER_LENGTH){
            buffer_amount=BUFFER_LENGTH;
          }
        }

      }

      buffstepcount++;
      if(buffstepcount == BUFFSTEP_LENGTH){
        buffstepcount = 0;
      }
  
      // restart filling buffer if still holding button
      if(buffcount == BUFFER_LENGTH){
        buffcount = 0;
        buffstepcount = 0;
      }
    }

    // i am not holding the button, there is buffer, and we waited for buffdelay
    if(play_continuing == STOPPED && !buffer_empty && buffdelay.ready()) {

      // Serial.print("I went to play my buffertown at buffcount ");
      // Serial.println(buffcount);
      if(buffcount < BUFFER_LENGTH){
        // play back from buffer
        // Serial.print("PLAYINGBCKS ");
        // Serial.println(buffer[buffcount]);

        int this_sig = (int) buffer[buffcount] * 2.0;
        // y = mx + b
        // sig = (int) (slope * buffstepcount + this_sig);
        // attenuate siga little
        sig = (int) (slope * buffstepcount + this_sig) * 0.36;

        // only switch to the next sample once we've arrived there
        if(buffstepcount == 0){
          buffcount++;

          // y2 is next amp, y1 is this amp
          // xs are where we are within this bufferstep
          int next_sig = (int) buffer[buffcount + 1] * 2.0;

          // set next slope
          slope = get_slope(next_sig, this_sig, 63, 0);
        }

        buffstepcount++;
        if(buffstepcount == BUFFSTEP_LENGTH){
          buffstepcount = 0;
        }

        // Serial.print("Played from buffer ");
        // Serial.println(sig);
      } else {
        // Serial.println("Finished Playing Buffer");
        // stop all that downloadin
        // we are actually done with the buffer
        buffcount = 0;
        buffstepcount = 0;

        bufferreplay++;
        if(bufferreplay == 2){
          // stop repeating buffer playback after 5
          bufferreplay = 0;
          buffer_empty = true;
        }
      }

      if(buffer_amount > 0 && buffer_amount > 4){
        // we finsihed using  a buffer sample, drain out (for lights), only on last repeat
        Serial.print(F("Sink buffer amt "));
        Serial.println(buffer_amount);
        buffer_amount -= 4;
      }
    }
    
    sig = envelope0.next() * sig;
  } else if(mode == SWEEPMODE){

    // if we're still holding the button, or TAILING
    if(play_continuing != STOPPED){
      sig = (int) envelope0.next() * aSin0.next();
      notes[0]->update_envelope();
    }
  } else {
    // REGNOTEMODE, ARPMODE, BEAUTYMODE...

    for(byte i=0; i<4; i++){
      notes[i]->update_envelope();

      int next_sample;
      if(vibrato_enabled()){
        // intensity value * phase offset value
        // Serial.println("Vibrato active...");
        Q15n16 vibrato = (Q15n16) 160 * aVibrato.next();
        next_sample = notes[i]->osc_phmod_next(vibrato);
      } else {
        next_sample = notes[i]->osc_next();
      }

      // gain * filter(oscnext)
      // this was arpmode... mby put back?
      if(mode == REGNOTEMODE ||  mode == REPEATMODE){
        sig += ( notes[i]->env_next() * lpf.next( next_sample ) );
        // sig += ( notes[i]->env_next() * next_sample );

      } else if(mode == ARPMODE){
        // huh?! not sure if this makes sense (which Note does arp note play on)
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i+1) * 0.89)  );
      } else if(mode == CHORDMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i+1) * 0.89)  );
      } else if(mode == CHORDSCHEMAMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i^2) * 0.89)  );
      } else if(mode == BEAUTYMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i+1) * 0.48)  );
      } else if(mode == PIANOMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i+1) * 0.48)  );
      }
    }
  }

  // dry will just be passed along if not enabled
  sig = render_effects(sig);
  return (int) sig>>8;
}

void loop(){
  audioHook(); // required here
}