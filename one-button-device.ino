/*  Get it innnnnn one button device

controls
  -rotary
    reg note -> change semitone for *next* note

    arp -> base note for arp

    weird ->
      raw freqency

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
LowPassFilter lpf;


// is 'cells', some bs-ass measure from mozzi
uint8_t flange_samps = 512;
AudioDelay <256> aDel;

// Phasor <AUDIO_RATE> aPhasor1;
// ReverbTank reverb;

bool play_arp = false;

int current_note = -1;
// bool button_state = digitalRead(2);
int input_freq = 440;

#define REGNOTEMODE 0
#define ARPMODE 1
#define WEIRDMODE 2
#define SWEEPMODE 3
uint8_t mode = REGNOTEMODE;

#define TOGGLE_0_PIN 3
#define TOGGLE_1_PIN 4
#define TOGGLE_2_PIN 5
#define TOGGLE_3_PIN 6

#define ROTARY_A_PIN 11
#define ROTARY_B_PIN 12

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
    void set_playing(bool);
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

// rotary
int rotary_position = 12;
int last_a;
bool clockwise = false;


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

    // offset note by current rotary value!
    next_note += rotary_position;

    arp_note_index += 1;
    play_note(next_note, 800);
    arp_delay.start(300);
  }
}

void get_note_button(){
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
          // input_freq = random(130, 4186);

          // next note is C3 offset by current rotary value!
          // Serial.print("Trying to play");
          // Serial.println(48 + rotary_position);
          Serial.print("rotary_position");
          Serial.println(rotary_position);
          // play_note(48 + rotary_position, 0);
          play_note(72 + rotary_position, 0);
        } else if(mode == ARPMODE){
          play_arp = true;
        } else if(mode == WEIRDMODE){
          // do some ol other shit
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

// effects helpers
bool flanger_enabled(){
  return toggles[0];
}

bool compressor_enabled(){
  return toggles[1];
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

    toggles[i] = digitalRead(i+3);
    // toggles[i] = true;
  }
}



#define THRESHOLD -20.0
#define WIDTH 6.0
#define LOWER -23.0
#define UPPER -17.0
#define RATIO 3.0
#define SLOPE -2.0
// #define SOFTSLOPEFACTOR -1.0/6.0;

int render_phaser(uint8_t note_index){

}

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

void setup(){
  pinMode(TOGGLE_0_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_1_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_2_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_3_PIN, INPUT_PULLUP);

  arp_delay.start(1000);

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
  // increment that bad boy

  // rotary_position++;
  // Serial.println(rotary_position);
  // get_rotary();

  // why the fuck does this only work out here?! inside void get_rotary() function, ++ behaves differently
  bool aVal = digitalRead(ROTARY_A_PIN);
  if(aVal != last_a){
    // Means the knob is rotating// if the knob is rotating, we need to determine direction// We do that by reading pin B.
    if(digitalRead(ROTARY_B_PIN) != aVal){
      // Means pin A Changed first -We're RotatingClockwise.
      rotary_position++;
      // clockwise = true;
      Serial.println("Rotary ++");
      Serial.print("Rotary is now");
  Serial.println(rotary_position);  
    } else {
      // Otherwise B changedfirst and we're moving CCW
      // clockwise = false;
      rotary_position--;
      Serial.println("Rotary --");
        Serial.print("Rotary is now");
        Serial.println(rotary_position);
    }
    // Serial.print ("Rotated: ");
    // if(clockwise){
    //   Serial.println ("clockwise");
    // } else {
    //   Serial.println("counterclockwise");
    // }
    // Serial.print("Encoder Position: ");
    // Serial.println(rotary_position);
  }
  last_a = aVal;





  // check if note buttons down/ready for action -> if so, play note!
  get_note_button();
  // just set em into toggles[]
  set_effects();


  if(play_arp){
    Serial.println("Im Playin arp");
    play_arp_notes();
  }

  // handle events for oscillatorsz
  for(int i=0; i<4; i++){
    // Serial.println(notes[i]->is_playing());

    if(mode == REGNOTEMODE){
      
      if(notes[i]->is_playing() && current_note != i){
        // Serial.println("Note goin off now");
        // Serial.println(i);
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

    } else if(mode == WEIRDMODE){

    } else if(mode == SWEEPMODE){
      
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

    int next_sample;
    if(true || vibrato_enabled()){
      // intensity value * phase offset value
      Q15n16 vibrato = (Q15n16) 160 * aVibrato.next();
      next_sample = notes[i]->osc_phmod_next(vibrato);
    } else {
      next_sample = notes[i]->osc_next();
    }

    // gain * filter(oscnext)
    sig += ( notes[i]->env_next() * lpf.next( next_sample ) );
  }

  // if(true || effects_active){

  // dry will just be passed along if not enabled
  sig = render_effects(sig);
  // }

  return (int) sig>>8;
}

void loop(){
  audioHook(); // required here
}