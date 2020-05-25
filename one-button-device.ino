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

#include <mozzi_midi.h>
#include <tables/sin8192_int8.h> // sine table for oscillator
#include <tables/cos8192_int8.h> // sine table for oscillator

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin0(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin1(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin2(SIN8192_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin3(SIN8192_DATA);

// davibrato :D
Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aVibrato(COS8192_DATA);

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


// // is 'cells'
uint8_t flange_samps = 512;
AudioDelay <256> aDel;



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
uint8_t play_continuing = STOPPED;

bool buffer_empty = true;
bool waiting_to_play_buffer = false;

char buffer[BUFFER_LENGTH];
uint8_t buffcount = 0;
uint8_t buffstepcount = 0;
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

int current_note = -1;

uint8_t chord_schema = 0;
int input_freq = 440;

#define REGNOTEMODE 0
#define ARPMODE 1
#define WEIRDMODE 2
#define CHORDMODE 3
#define SWEEPMODE 4
#define REPEATMODE 5
#define CHORDSCHEMAMODE 6

uint8_t mode = REGNOTEMODE;

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
    int osc_index;
  public:
    Note(int, int);
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

uint8_t arp_note_index = 0;
// this is for timing out arp notes
EventDelay arp_delay = EventDelay(300);

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
// time to hold down, 1200ms
uint8_t rb_delay = 300000;
long rb_timer = mozziMicros();

// rotary
int rotary_position = 12;
int last_a;
bool clockwise = false;

long sweep_timer = mozziMicros();

void play_note(int new_note, unsigned int delay_time){
  int available_slot = 0;
  for(int i=0; i<4; i++){
    // Serial.println("is_palying was");
    // Serial.println(notes[i]->is_playing());
    if(notes[i]->is_available() == true){
      Serial.print(i);
      Serial.println(" was available to play...");
      available_slot = i;
      break;
    }
  }
  Serial.println("NOTE IS");
  Serial.println(new_note);
  float freq = note_to_freq(new_note);
  // Serial.println("FREQ IS");
  // Serial.println(freq);
  notes[available_slot]->set_frequency(freq);

  // Serial.println("Maybe it fucking worked");
  // Serial.println(notes[available_slot]->get_frequency());
  
  if(mode == REGNOTEMODE){
    // this is the which notes[i] slot were playing from in regular mode
    current_note = available_slot;  
  }
  
    
  Serial.println("Running ::note_on for ");
  Serial.println(available_slot);
  notes[available_slot]->note_on();
  notes[available_slot]->set_available(false);

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

    // offset note by current rotary value!
    next_note += rotary_position;

    arp_note_index += 1;
    play_note(next_note, 800);
    arp_delay.start(300);
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
    if(readin != button_state){
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
        if(mode > 6){
          mode = 0;
        }

        setup_mode(mode);
        Serial.print("I incremented MODE to ");
        Serial.println(mode);
      }
    }
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

      // only do osmething if the new button state is HIGH (uh)
      if(button_state && current_note<0) {

        // HERE MEANS I"M GOING TO start PLAYing SOMTHING
        if(mode == REGNOTEMODE){
          // input_freq = random(130, 4186);

          // next note is C3 offset by current rotary value!
          // Serial.print("Trying to play");
          // Serial.println(48 + rotary_position);
          // Serial.print("rotary_position");
          // Serial.println(rotary_position);
          // play_note(48 + rotary_position, 0);
          play_note(72 + rotary_position, 0);
        } else if(mode == ARPMODE){
          play_continuing = PLAYING;
        } else if(mode == WEIRDMODE){
          // do some ol other shit
          start_play_weird();
        } else if(mode == CHORDMODE){

          // play a chord
          Serial.print("love to play chord ");
          Serial.println(rotary_position);
          play_seq_chord(rotary_position);
        } else if(mode == CHORDSCHEMAMODE){

          Serial.print("love to play schema chord ");
          Serial.println(rotary_position);

          // in this case, were thinking of input as a note were modifying
          play_schema_chord(60+rotary_position);
        } else if(mode == SWEEPMODE){
          start_play_sweep();
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
          for(uint8_t i=0; i<4; i++){
            // start noteoffs because button was released
            notes[i]->note_off();
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

// chord based on starting note and selected chord shape
void play_schema_chord(uint8_t starting_note){
    for(uint8_t i=0; i<4; i++) {
    notes[i]->note_off();
    notes[i]->set_available(true);
  }
  uint8_t note1 = 0;
  uint8_t note2 = 0;
  uint8_t note3 = 0;
  uint8_t note4 = 0;

  Serial.print("Chord schema is ");
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

  play_note(note1, 1000);

  if(note2 > 0){
    play_note(note2, 1000);
  }

  if(note3 > 0){
    play_note(note3, 1000);
  }

  if(note4 > 0){
    play_note(note4, 1000);
  }
}

// chords in a seequence bsed on offset
void play_seq_chord(int note_offset){
  // open up all of the notes so we dont get a straggling one from prev chord that fucks up the signal
  for(uint8_t i=0; i<4; i++) {
    notes[i]->note_off();
    notes[i]->set_available(true);
  }
  uint8_t note1;
  uint8_t note2;
  uint8_t note3;

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
  int octave_offset = ( note_offset/9 ) * 12;

  Serial.print("Octave Offset... ");
  Serial.println(octave_offset);

  play_note(note1 + octave_offset, 800);
  play_note(note2 + octave_offset, 800);
  play_note(note3 + octave_offset, 800);
}

void start_play_weird(){
  envelope0.noteOn(true);
  play_continuing = PLAYING;
}

void stop_play_weird(){
  envelope0.noteOff();
  play_continuing = TAILING;
}

void start_play_sweep(){
  Serial.println("I start play Sweep"); 
  envelope0.noteOn(true);
  play_continuing = PLAYING;
}

void stop_play_sweep(){
  Serial.println("I Stop play Sweep");
  envelope0.noteOff();
  play_continuing = TAILING;
  // set up env timer for the one Note we're using (0)
  Serial.println(getReleaseTime( short_env_enabled() ));
  note_delays[0]->start( 1000 );
}
// effects helpers
bool flanger_enabled(){
  // YOOO
  return false;
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
  for(uint8_t i=0; i<4; i++){
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
  if(flanger_enabled()){
    // do delay
    signal = signal + aDel.next((int8_t)signal, 512);
  }

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
  pinMode(TOGGLE_0_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_1_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_2_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_3_PIN, INPUT_PULLUP);

  pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);

  arp_delay.start(300);

  lpf.setResonance(200);
  lpf.setCutoffFreq(6000);


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

  setup_mode(SWEEPMODE);
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

void setup_mode(uint8_t newmode){
  if(newmode != mode){
    
    mode = newmode;
    // turn this off so we dont carry over from other mode
    play_continuing = STOPPED;

    if(mode == REGNOTEMODE || mode == CHORDMODE || mode == CHORDSCHEMAMODE){
      
      envelope0.update();
      envelope1.update();
      envelope2.update();
      envelope3.update();
    } else if(mode == ARPMODE){

    } else if(mode == WEIRDMODE){
      set_weird_freq();

      // init buffer
      for(uint8_t i=0; i<BUFFER_LENGTH; i++){
        buffer[i] = 0;
      }
      buffer_empty = true;
    } else if(mode == SWEEPMODE){

      envelope0.update();
      set_sweep_freq();
    }
  }
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
    Serial.print("Rotary postition is ");
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
  for(int i=0; i<4; i++){

    if(mode == REGNOTEMODE){
      
      if(notes[i]->is_playing() && current_note != i){

        // do regular note off if button not held for this note
        notes[i]->note_off();
        note_delays[i]->start(800);
        // notes[i]->set_available(false);
      } else if(note_delays[i]->ready()){

        notes[i]->set_available(true);
      }

    } else if(mode == CHORDMODE){

    } else if(mode == ARPMODE) {
      
      if(note_delays[i]->ready()){
        notes[i]->note_off();
        notes[i]->set_available(true);
      }

    } else if(mode == WEIRDMODE){
      // set above
    } else if(mode == SWEEPMODE){
      // wait 2ms to change sweep freq
      if((mozziMicros() - sweep_timer) > 200){
        set_sweep_freq();
        sweep_timer = mozziMicros();
      }
    }

    // shut off after we waited for the release
    if(play_continuing == TAILING && note_delays[0]->ready()){
      Serial.println("I stoped play con");
      play_continuing = STOPPED;
    }
  }
}

int county = 0;

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
    }
    
    sig = envelope0.next() * sig;
  } else if(mode == SWEEPMODE){

    // if we're still holding the button, or TAILING
    if(play_continuing != STOPPED){
      sig = (int) envelope0.next() * aSin0.next();
      notes[0]->update_envelope();
    }
  } else {
    // REGNOTEMODE, ARPMODE

    for(int i=0; i<4; i++){
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
      if(mode == REGNOTEMODE || mode == ARPMODE){
        sig += ( notes[i]->env_next() * lpf.next( next_sample ) );
      } else if(mode == CHORDMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i+1) * 0.89)  );
      } else if(mode == CHORDSCHEMAMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * lpf.next( next_sample ) * (1/(i^2) * 0.89)  );
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