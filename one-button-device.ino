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

// #include <EEPROM.h>
// #define SETTINGS_ADDRESS 0

// 0 tempo
// 1 detune
// 2 keylock bool
// 3 harmonicMode
// 4 key
// 5 mode
// 6 chordschema


// 3 delay not stored too big 
// 2 volume not stored too float
// 1 filter not stored too not stored currently

// defaults
// byte userSettings[] = { 88, 196, 0, 0, 0, 0, 0, 0 };

#include <MozziGuts.h>
#include <EventDelay.h>
#include <Oscil.h> // oscillator template
#include <ADSR.h>

// effects stuff
#include <LowPassFilter.h>
// #include <AudioDelay.h>
// #include <ReverbTank.h>
// #include <Phasor.h>

#define AUDIO_MODE STANDARD_PLUS
#include <mozzi_midi.h>
#include <tables/sin8192_int8.h> // sine table for oscillator
// #include <tables/cos2048_int8.h> // sine table for oscillator

// da pix
#include <Adafruit_NeoPixel.h>
#define NUMPIXELS 7
#define PIXELPIN 13
#define PIXDELAY 32
// #define NUMPIXFADESTEPS 160.0

// its modes luv
#define REGNOTEMODE 0
#define ARPMODE 1
#define CHORDSCHEMAMODE 2
#define CHORDMODE 3
#define BEAUTYMODE 4
#define PIANOMODE 5
#define REPEATMODE 6
#define SWEEPMODE 7
#define PLACEMODE 8
// its gotta be last
#define SELECTOPTIONMODE 9
#define SETTEMPOMODE 10
#define SETFILTERMODE 11
#define SETVOLMODE 12
#define SETDELAYMODE 13
#define SETDETUNEMODE 14

// display modes
#define COOL 0
#define SHOWMODE 1
#define PLAY 2
#define WEIRD 3
#define IDLE 4
#define LOADING 5
#define ROTATE 6
#define SHOWROTARY 7
#define SHOWCHORDSCHEMA 8
#define SHOWHARMMODE 9
#define SHOWSELECT 10
#define SHOWTEMPO 11
#define SHOWFILTER 12
#define SHOWVOL 13
#define SHOWDELAY 14
#define SHOWDETUNE 15
#define SHOWINTRO 16
#define SHOWPLACEOSC 17

#define OPTION0COLOR_R 245
#define OPTION0COLOR_G 11
#define OPTION0COLOR_B 196

#define OPTION1COLOR_R 44
#define OPTION1COLOR_G 242
#define OPTION1COLOR_B 237

#define OPTION2COLOR_R 242
#define OPTION2COLOR_G 204
#define OPTION2COLOR_B 19

#define OPTION3COLOR_R 5
#define OPTION3COLOR_G 204
#define OPTION3COLOR_B 0

#define OPTION4COLOR_R 19
#define OPTION4COLOR_G 19
#define OPTION4COLOR_B 242


bool keylock = false;

#define INTRODELAY 1618
EventDelay introDelay = EventDelay(INTRODELAY);

EventDelay pixdelay = EventDelay(PIXDELAY);
#define IDLEDELAY 3000
EventDelay display_idle_delay = EventDelay( IDLEDELAY );
EventDelay display_aux_delay = EventDelay();

// for +white version
Adafruit_NeoPixel pixel(NUMPIXELS, PIXELPIN, NEO_GRBW + NEO_KHZ800);
// Adafruit_NeoPixel pixel(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);
// r,g,b,r,g,b... NUMPIX * 3
byte pixel_colors[21];

// which arp shape
byte harmonicMode = 0;
const byte harmonicModeIntervals[] = { 0,2,2,1,2,2,2,1 };
// this is goddamn fuckin major c bitch!
byte key = 0;

byte display_mode;
byte pixel_counter = 0;
bool pixel_flag = true;
bool pixel_flag2 = true;

bool displayPlayNotes[4] = { false, false, false, false };

byte tempo = 88;
// we are in set tempo mode
// a beat is happening right now
bool onBeat = false;
// perhaps a beat happened once beforee
bool lastOnBeat = false;
// this is used to tell when we're on beat
unsigned long setTempoTime;
// this is used to actually track time tbetween tempo taps
unsigned long tempoTapTimer = false;
byte beatBrightness = 180;
byte altColorBrightness = 0;

// envelope modes wow!
#define SHORT_ENV 0
#define LONG_ENV 1
#define BEAUTY_SHORT_ENV 2
#define BEAUTY_LONG_ENV 3
#define PIANO_SHORT_ENV 4
#define PIANO_LONG_ENV 5
byte envelope_mode = SHORT_ENV;

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <8192, AUDIO_RATE> aSin0(SIN8192_DATA);
Oscil <8192, AUDIO_RATE> aSin1(SIN8192_DATA);
Oscil <8192, AUDIO_RATE> aSin2(SIN8192_DATA);
Oscil <8192, AUDIO_RATE> aSin3(SIN8192_DATA);

// davibrato :D
// Oscil <COS2048_NUM_CELLS, AUDIO_RATE> aVibrato(COS2048_DATA);

// deelay
#define DELAY_BUFFER_LENGTH 256
byte delayIndex = 0;
int delayBuffer[DELAY_BUFFER_LENGTH];

#define MAX_DELAY_TIME 480
unsigned int delayTime = 240;
unsigned long delayTimer = mozziMicros();

// ReverbTank reverb;

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 1024 // Hz, powers of 2 are most reliable
ADSR <AUDIO_RATE, AUDIO_RATE> envelope0;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope1;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope2;
ADSR <AUDIO_RATE, AUDIO_RATE> envelope3;
// byte[4] gains = {0,0,0,0};

// Effects objects
bool toggles[4] = {false,false,false,false};

// to know if we should reset enveleopes
bool last_env_toggle = false;

LowPassFilter lpf;
byte cutoff = 196;
float volume = 1.0;

int freqHeat = 0;

int detune = 0;

// for flanger
// AudioDelay <256> aDel;

// Weirdmode stuff
// 4 seconds?
// #define BUFFER_LENGTH 128
// #define BUFFSTEP_LENGTH 128

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

// chordschemamode stuff
#define MAJOR 0
#define MINOR 1
#define MAJORSEVENTH 2
#define MINORSEVENTH 3
#define MINORTHIRDS 4
#define MAJORTHIRDS 5
#define STEPFIFTH 6

int8_t current_note = 0;

// select which chord schema
byte chord_schema = 0;

// select which osc (or 4 == all) to edit
byte placeOsc = 4;

// set via setup_mode in setup()
byte mode = REGNOTEMODE;
int8_t lastMode = -1;
// modes with lots of voices need to be SHUT UP
bool dimThisMode = false;

#define BIGBUTTONPIN 2
#define BIGBUTTON_LED_PIN 10

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
    byte currentNote;
  public:
    // for now just freq
    // void initialize(int, int);
    Note(byte);
    void note_on();
    void note_off(bool);

    bool is_playing();
    bool is_available();
    void set_available(bool);

    float get_frequency();
    void set_frequency(float);
    void update_envelope();
    int osc_next();
    // int osc_phmod_next(Q15n16);
    unsigned int env_next();
    byte get_osc_index();
    void store_note(byte);
    byte get_note();
};

Note::Note(byte init_osc_index){
  osc_index = init_osc_index;
}

void Note::store_note(byte newNote){
  // midi note #
  if(newNote>=0 && newNote<128){
    currentNote = newNote;
  }
}

byte Note::get_note(){
  // midi note
  return currentNote;
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
    envelope0.noteOn();
  } else if(oi == 1){

    envelope1.noteOn();
  } else if(oi == 2){

    envelope2.noteOn();
  } else if(oi == 3){

    envelope3.noteOn();
  }
}

float Note::get_frequency(){
  return freq;
}

void Note::set_frequency(float new_freq){
  freq = new_freq;
  if(osc_index == 0){
    return aSin0.setFreq(freq);
  } else if(osc_index == 1){
    return aSin1.setFreq(freq);
  } else if(osc_index == 2){
    return aSin2.setFreq(freq);
  } else if(osc_index == 3){
    return aSin3.setFreq(freq);
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

Note *notes[4];
EventDelay *note_delays[4];

Note note0 = Note(0);
Note note1 = Note(1);
Note note2 = Note(2);
Note note3 = Note(3);

EventDelay qa_delay = EventDelay();

byte arp_note_index = 0;
byte arp_next_osc = 0;
// this is for timing out arp notes
EventDelay arp_delay = EventDelay(300);

uint8_t repeat_notes[4] = {0,0,0,0};


// for each chord
// EventDelay chord_delay = EventDelay(1300);

// long rando;

// button stuff
bool button_state = false;
bool last_button_state = false;
// long last_debounce_time = mozziMicros();
EventDelay last_debounce_delay = EventDelay();

// 10ms
#define DEBOUNCE_DELAY 10
byte bigButtonBrightness = 0;

// rotary button
bool rbstate = false;

// unsigned long last_rbdebounce_time = mozziMicros();
EventDelay last_rbdebounce_delay = EventDelay();
// 10ms
#define RBDEBOUNCE_DELAY 10
// time to hold down, 800ms
#define RB_DELAY 0

EventDelay rb_hold_delay = EventDelay();

bool rb_hold_once = false;
bool rb_block_mode_increment = false;

// rotary -127 to 127
#define ROTARYMIN -32000
#define ROTARYMAX 32000
int rotary_position = 12;
int8_t last_rp_move = 0;
// rotarys a pin reading
bool last_a = digitalRead(ROTARY_A_PIN);
// know whether to fade down in showrotary
bool rotaryMoved = false;

// on adafruit encoder, one 'click' == two moves
// this is not a bounce from interference because if you hold it between clicks it makes one move
// so wait for two moves before making one
byte rotaryState = 0;

// keep in micros cause its actually two timers
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
      // Serial.print(i);
      // Serial.println(F(" was available to play..."));
      return i;
      break;
    }
  }

  return 0;
}

void play_note(byte new_note, unsigned int delay_time, uint8_t available_slot){

  // Serial.println(F("NOTE IS"));
  // Serial.println(new_note);
  // float freq = note_to_freq(new_note);
  // Serial.println("FREQ IS");
  // Serial.println(freq);
  notes[available_slot]->set_frequency( (float) note_to_freq(new_note) + detune );
  // save which midi note
  notes[available_slot]->store_note(new_note);

  if(mode == REGNOTEMODE || mode == REPEATMODE){
    // this is the which notes[i] slot were playing from in regular mode

    // not used in beauty or piano mode cause we already use all 4
    current_note = available_slot;  
  }
  
  // Serial.println(F("Running ::note_on for "));
  // Serial.println(available_slot);
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
  // Serial.print(F("Start display for "));
  // Serial.println(available_slot);
  // keep resetting disp timer os play stay (rather than go back to cool)
  display_idle_delay.start(IDLEDELAY);
}

float note_to_freq(int midi_note){
  // internet trash
  // return (pow (2.0, ((float)(noter-69)/12.0))) * 440.0;
  // return (pow (2.0, ((float)(noter-69.0)/12.0))) * 440.0;
  return 440.0 * ( pow( 2.0, ((midi_note-69.0)/12.0) ) );
}

// byte next_arp_note(){
//   switch (arp_note_index) {
//     case 0: return 70;
//         break;
//     case 1: return 72;
//         break;
//     case 2: return 74;
//         break;
//     case 3: return 75;
//         break;
//     case 4: return 77;
//         break;
//     case 5: return 79;
//         break;
//     case 6: return 81;
//         break;
//     case 7: return 82;
//         break;
//     case 8: return 84;
//         break;
//     case 9: return 86;
//         break;
//     case 10: return 87;
//         break;
//     case 11: arp_note_index = 0;
//         return 70;
//         break;
//     default: return 70;
//   }
// }



// default
//   add rotary position for true offset
// modelock
//   currently were not in a 'key'

//   if you could lock root note
//     (need a separate way to change root note)

//     you can change knob to set starting note
//       if lockmode
//         when change rotary_position, change rp_move to adjacent harmonicmodeoffset value
//         that way you only make legal moves iwthin the current key

//     you can change mode to set intervals

// void saveSettings(){
//   // for(byte i=0; i<7; i++){
//   //   // save each thing in settings into eeprom beeetch!
//   //   EEPROM.put(SETTINGS_ADDRESS+i, userSettings[i])
//   // }
//   int address = SETTINGS_ADDRESS;
//   EEPROM.put(address, tempo);
//   address += sizeof(tempo);
//   EEPROM.put(address, detune);
//   address += sizeof(detune);
//   EEPROM.put(address, keylock);
//   address += sizeof(keylock);
//   EEPROM.put(address, harmonicMode);
//   address += sizeof(harmonicMode);
//   EEPROM.put(address, key);
//   address += sizeof(key);
//   // this is lastmode because the save happens during selectoptionmode
//   EEPROM.put(address, lastMode);
//   address += sizeof(lastMode);
//   EEPROM.put(address, chord_schema);
//   address += sizeof(chord_schema);
// }

// void readSettings(){
//   byte tempoCheck;
//   EEPROM.get(SETTINGS_ADDRESS, tempoCheck);
//   // Serial.print("Fucking got stupid ");
//   // Serial.println(tempoCheck);
//   if(tempoCheck > 0){
//     tempo = tempoCheck;
    
//     int address = SETTINGS_ADDRESS;
//     EEPROM.get(address, tempo);
//     address += sizeof(tempo);
//     EEPROM.get(address, detune);
//     address += sizeof(address);
//     EEPROM.get(address, keylock);
//     address += sizeof(address);
//     EEPROM.get(address, harmonicMode);
//     address += sizeof(address);
//     EEPROM.get(address, key);
//     address += sizeof(address);
//     EEPROM.get(address, lastMode);
//     address += sizeof(address);
//     EEPROM.get(address, chord_schema);
//     // yeah baby
//     // setup_mode(mode);
//     // Serial.println(detune);
//     // Serial.println(keylock);
//     // Serial.println(harmonicMode);
//     // Serial.println(key);
//     // Serial.println(mode);
//     // Serial.println(chord_schema);

//   }  
// }

void safeRotaryChange(int8_t change){
  // change might be negative! so only add
  if(rotary_position+change < ROTARYMAX && rotary_position+change > ROTARYMIN){
    rotary_position += change;
  }
}

int8_t moveRotaryWithinScale(int8_t direction){
  int8_t change = 0;
  if(direction > 0){

    // if going up, use the CURRENT arp inde xto get move size
    change = harmonicModeIntervals[arp_note_index];
    if(change == 0){
      // go up to 'root'
      change = 1;
    }

    // up
    if(arp_note_index+1 > 7){
      arp_note_index = 0;
    } else {
      arp_note_index += 1;
    }

  } else {
    // if going down, switch arp index FIRST, then use the fucking interval to move down
    if(arp_note_index-1<0){
      arp_note_index = 7;
    } else {
      arp_note_index -= 1;
    }

    change = harmonicModeIntervals[arp_note_index];
    if(change == 0){
      // go down to 'root'
      change = 2;
    }
  }

  // return offset to get to note we moved to
  return direction * change;
}

byte next_arp_note(){
  return 72 + rotary_position + getHarmonicModeOffset(arp_note_index);
}

byte getHarmonicModeOffset(byte arp_index){
  byte offset = 0;
  
  // start at our selected modes starting point
  byte modeIntervalIndex = harmonicMode;

  for(byte i=0; i<(arp_index+1); i++){
    // add in the intervals for each step up to our current arp note
    offset += harmonicModeIntervals[ modeIntervalIndex ];

    modeIntervalIndex += 1;
    if(modeIntervalIndex > 7){
      // 
      modeIntervalIndex = 0;
    }
  }

  return offset;
}

void play_arp_notes(){
  // ARPMODE
  if(arp_delay.ready()){
    byte next_note = next_arp_note();

    // offset note by current rotary value!
    // next_note += rotary_position;

    arp_note_index += 1;
    if(arp_note_index > 7){
      arp_note_index = 0;
    }


    notes[arp_next_osc]->note_off();
    play_note(next_note, getReleaseTime( envelope_mode ), arp_next_osc);

    // rotate through voices
    arp_next_osc++;
    if(arp_next_osc>3){
      arp_next_osc = 0;
    }

    // uint16_t arp_time;
    // keep this, not same thing as envelope setting
    // if(short_env_disabled()){
    //   arp_time = 300;
    // } else {
    //   arp_time = 1200;
    // }
    
    // tempo 60
    // delay would be 1000

    // time_of_one_beat = 60s * 1(beat)/tempo;
    // arp_time = 60000 / tempo;

    // YOOO add 4 / measure (tempo/4) option with toggles

    // beginning of each beat
    arp_delay.start( getArpTime(envelope_mode) );
  }
}

void set_sweep_freq(){

  // exponentially increase with rot pos
  // float sweep_freq = 440.0 + ( pow(rotary_position, 2) );
  aSin0.setFreq( calcFreqOffset(rotary_position) );
}

void setPlaceFreq(byte osc_index, int8_t rpMove){
  if(osc_index == 4) {

    // do a knob change, so only change by 1 chunk
    if(notes[0]->get_frequency() != 0){
      notes[0]->set_frequency( notes[0]->get_frequency() + calcKnobOffset( rpMove ) );
    }
    if(notes[1]->get_frequency() != 0){
      notes[1]->set_frequency( notes[1]->get_frequency() + calcKnobOffset( rpMove ) );
    }
    if(notes[2]->get_frequency() != 0){
      notes[2]->set_frequency( notes[2]->get_frequency() + calcKnobOffset( rpMove ) );
    }
    if(notes[3]->get_frequency() != 0){
      notes[3]->set_frequency( notes[3]->get_frequency() + calcKnobOffset( rpMove ) );
    }
  } else {

    // set for tuning one osc - use entire offset here
    notes[osc_index]->set_frequency(440.0 + calcFreqOffset(rotary_position));
  } 
}

float calcFreqOffset(int rpMoves){
  // freq heat just serves to pump up size of each move (1 rpmove) if you're realllly turnin

  Serial.print("Im settin to ");
  Serial.println(rpMoves + (8 * freqHeat) + detune);

  return rpMoves + (8 * freqHeat) + detune;
}

float calcKnobOffset(int rpMoves){
  // freq heat just serves to pump up size of each move (1 rpmove) if you're realllly turnin

  return 16 * rpMoves + detune;
}

void handle_rotary_button(){

  if( last_rbdebounce_delay.ready() ){

    // goes low when pressed
    bool readin = !digitalRead(ROTARY_BUTTON_PIN);

    // doing stuff after debounce

    if(readin != rbstate){

      // rb reading actually changed

      // Serial.println(F("Got Redin"));
      rbstate = readin;

      // this is wher da magik happens
      if(rbstate){
        // button held down
        rb_hold_delay.start( 300 );
        rb_hold_once = true;
        // Serial.println(F("Set hold timer."));
      } else {
        // rb released

        if(button_state){

          // RB SUBOPTIONS
          if(mode == CHORDSCHEMAMODE){

            // if big button is held, and its chordschema, use rb to increment chordschema
            // Serial.println(F("I incremented chordschema "));
            chord_schema++;
            if(chord_schema > 6){
              chord_schema = 0;
            }
            setDisplayMode(SHOWCHORDSCHEMA);            
          } else if(mode == ARPMODE){
            harmonicMode++;
            if(harmonicMode > 6){
              harmonicMode = 0;
            }
            setDisplayMode(SHOWHARMMODE);
          } else if(mode == PLACEMODE){
            placeOsc++;
            if(placeOsc > 4){
              placeOsc = 0;
            }
            setDisplayMode(SHOWPLACEOSC);
          }

        } else {
          // otherwise, increment mode normally

          if(lastMode<0 && mode < SELECTOPTIONMODE && !rb_block_mode_increment){
            mode++;
            if(mode == SELECTOPTIONMODE){
              mode = 0;
            }
            setup_mode(mode);
            // Serial.print(F("I incremented MODE to "));
            // Serial.println(mode);  
          }

          // released button from hold, dont block mode hcange anymore
          rb_block_mode_increment = false;

        }
      }
    }

    last_rbdebounce_delay.start(RBDEBOUNCE_DELAY);
  }

  if(rbstate && rb_hold_once && rb_hold_delay.ready() ){
    // Serial.print(F("LASMODEHELLO "));
    // Serial.println(lastMode);
    
    if(mode < SELECTOPTIONMODE){
      // track mode to switch back to
      lastMode = mode;

      // held rb for long enough (3s), start tempo setting mode
      setup_mode(SELECTOPTIONMODE);
      setDisplayMode(SHOWSELECT);
      // Serial.println(F("Set freakin selectoptionmode"));

    } else if(lastMode >= 0){
      // if already in settempo, switch back to prev mode
      // Serial.println(F("All lastmode baby"));
      setup_mode(lastMode);
      lastMode = -1;

      // block mode changing once so we can release button without incrementing mode
      rb_block_mode_increment = true;
    }

    rb_hold_once = false;
    // dont want to lose trakc of this!
    rotaryState = 0;
  }

}

bool isOnBeat(unsigned long startTime, unsigned long now, short tempo){
  // in microseconds
  // is duration between settempotime and now, on a beat or less than 300ms after
  return (startTime - now) % (60000000 / tempo) < 30000;
}

void handle_note_button(){
  // read the state of the switch into a local variable:
  bool reading = !digitalRead(BIGBUTTONPIN);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != last_button_state) {
    // reset the debouncing timer
    last_debounce_delay.start(DEBOUNCE_DELAY);
    // Serial.println("Raeding was note same");
  }

  if ( last_debounce_delay.ready() ) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != button_state) {
      button_state = reading;
      // Serial.println(F("button changed"));

      // only do osmething if the new button state is HIGH (uh)
      if(button_state && current_note<0) {

        byte available_slot = available_note_slot();

        // HERE MEANS I"M GOING TO start PLAYing SOMTHING
        if(mode == REGNOTEMODE){
          // next note is C3 offset by current rotary value!
          play_note(72 + rotary_position, 0, available_slot);
        } else if(mode == ARPMODE){

          play_continuing = PLAYING;
        } else if(mode == CHORDMODE){

          // play a chord
          play_seq_chord(rotary_position);
        } else if(mode == CHORDSCHEMAMODE){

          // in this case, were thinking of input as a note were modifying
          play_schema_chord(60+rotary_position);
        } else if(mode == SWEEPMODE){

          start_play_sweep();
          if(display_mode != ROTATE){
            setDisplayMode(ROTATE);
          }
        } else if(mode == REPEATMODE){

          play_note(72 + rotary_position, getArpTime(envelope_mode), available_slot);
          setRepeatNote(72 + rotary_position);

        } else if(mode == BEAUTYMODE){
          // kind of violinish
          // next note is C3 offset by current rotary value!

          // play overtones fool
          // Serial.print(F("trying hard to play beauty... "));
          // for(byte i=0; i<4; i++){
          //   notes[i]->note_off();
          // }

          play_note(72 + rotary_position, 0, 0);
          play_note(84 + rotary_position, 0, 1);
          play_note(96 + rotary_position, 0, 2);
          play_note(103 + rotary_position, 0, 3);
        } else if(mode == PIANOMODE){
          // next note is C3 offset by current rotary value!

          // play overtones fool
          // Serial.println(F("trying hard to play piano..."));
          // fund
          play_note(72 + rotary_position, false, 0);
          // +perfect fifth
          play_note(79 + rotary_position, false, 1);
          //  +octave+4
          play_note(88 + rotary_position, false, 2);
          // +3octave+7 semitones? seemed like strong harmonic in spectrum
          play_note(115 + rotary_position, false, 3);
        } else if(mode == SELECTOPTIONMODE){
          // pressed button, so select mode based on rotary position % 3
          byte optionMode = abs(rotary_position) % 6 + 10;
          // Serial.print(F("I have selected option "));
          // Serial.println(optionMode);
          
          // last light is actually a toggle
          // Serial.print("omode is ");
          // Serial.println(optionMode);
          if(optionMode == 15){
            // this is just more than the last seelct option -> not really a mode

            keylock = !keylock;
            // Serial.println(F("Hey  bitch"));
            // fuck it
            // saveSettings();
          } else {
            setup_mode( optionMode );

          }
        } else if(mode == PLACEMODE){
          // just start em up at whatever freq they're at
          notes[0]->note_on();
          notes[1]->note_on();
          notes[2]->note_on();
          notes[3]->note_on();
        } else if(mode == SETTEMPOMODE){
          // use button pushes to set tempo

          unsigned long now = mozziMicros();
          if(tempoTapTimer){
            // second tap, compare time, set tempo, then record time
            // t = timeof1beat/min

            // tempo = 60s * 1beat / tapTime
            // double it up, seems very slow
            tempo = 60000000/(now - tempoTapTimer);
            // Serial.print(F("Bitch I set the beat to "));
            // Serial.println(60000000/(now - tempoTapTimer));
          }

          // first tap just sets timer to compare on second
          tempoTapTimer = now;

          // dont go back to idle if were tapping
          display_idle_delay.start( IDLEDELAY );
        }
        
      } else if(!button_state) {

        // HEY - here is where button state off directly triggeres noteoffs - most used for modes where all 4 voices are used at the same time

        // Serial.println("No more note held");
        // no more note held
        // regular note held is set in play_note
        // chord root held is set in play_seq_chord
        current_note = -1;

        if(mode == CHORDMODE || mode == CHORDSCHEMAMODE){
          for(byte i=0; i<4; i++){
            // start noteoffs because button was released
            notes[i]->note_off();
            displayPlayNotes[i] = false;
          }
        } else if(mode == SWEEPMODE){

          stop_play_sweep();
        } else if(mode == BEAUTYMODE || mode == PIANOMODE || mode == PLACEMODE){

          for(byte i=0; i<4; i++){
            // start noteoffs because button was released
            notes[i]->note_off();
            note_delays[i]->start( getReleaseTime(envelope_mode) );
            displayPlayNotes[i] = false;
          }
        } else if(mode == ARPMODE){
          arp_note_index = harmonicMode;
        }

        play_continuing = TAILING;
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
      // Serial.print(i);
      // Serial.println(F(" was available to set repeat..."));
      available_slot = i;
      break;
    }
  }

  // wait 4 beats ... then play elsewheref  
  note_delays[available_slot]->start( oneBeat() * 8 );

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

  // Serial.print(F("Chord schema is "));
  // Serial.println(chord_schema); 

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
  play_note(note1, oneBeat() * 8, 0);

  if(note2 > 0){
    play_note(note2, oneBeat() * 8, 1);
  }

  if(note3 > 0){
    play_note(note3, oneBeat() * 8, 2);
  }

  if(note4 > 0){
    play_note(note4, oneBeat() * 8, 3);
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

  // Serial.print(F("Octave Offset... "));
  // Serial.println(octave_offset);

  int r_time = getReleaseTime( envelope_mode );
  
  byte available_slot = available_note_slot();
  play_note(note1 + octave_offset, r_time, available_slot);
  
  available_slot = available_note_slot();
  play_note(note2 + octave_offset, r_time, available_slot);
  
  available_slot = available_note_slot();
  play_note(note3 + octave_offset, r_time, available_slot);
}

void start_play_sweep(){
  // Serial.println(F("I start play Sweep")); 
  envelope0.noteOn();
  // start an extra timer so that we can know to bump noteOn again if we're still holding down
  note_delays[1]->start( getReleaseTime(envelope_mode) );
  play_continuing = PLAYING;
}

void stop_play_sweep(){
  // Serial.println(F("I Stop play Sweep"));
  envelope0.noteOff();
  play_continuing = TAILING;
  // set up env timer for the one Note we're using (0)
  note_delays[0]->start( getReleaseTime(envelope_mode) );
}
// effects helpers
// bool flanger_enabled(){
//   // YOOO
//   return toggles[0];
// }

bool compressor_enabled(){
  return toggles[1];
}

bool short_env_disabled(){
  // reverse this so that button down is LONG_ENV
  return !toggles[0];
}

// too big for memory
// bool reverb_enabled(){
//   return toggles[2];
// }

bool delay_enabled(){
  return toggles[2];
}

// bool waveshaper_enabled(){
//   return toggles[1] == 1;
// }

bool vibrato_enabled(){
  return false;
  // defined near osc_next
  return toggles[2];
}

void set_effects() {
  for(byte i=0; i<3; i++){
    // bool newtoggle =!digitalRead(i+3);
    // if(newtoggle != toggles[i]){
    //   Serial.print(i);
    //   Serial.print(" Changed to ");
    //   Serial.println(newtoggle);
    // }
    
    // comes in backwards... (and noexistent ones are true)
    toggles[i] = !digitalRead(i+3);
    // Serial.print("TOgg ");
    // Serial.print(i);
    // Serial.print(" ");
    // Serial.println(toggles[i]);
  }

}

// hardcoded compressor settings
#define THRESHOLD -18.4
#define WIDTH 2.0
#define LOWER -30.0
#define UPPER -17.0
#define RATIO 3.0
#define SLOPE -2.0
// #define SOFTSLOPEFACTOR -1.0/6.0;

int render_delay(int signal){
  if( mozziMicros() - delayTimer > delayTime ){
    // get old sample to play back (if avail)
    int toPlay = delayBuffer[delayIndex];

    // store new sample at same index (gets played next time around bbuffer)
    delayBuffer[delayIndex] = signal;

    // increment for next tick
    delayIndex++;
    if(delayIndex == DELAY_BUFFER_LENGTH){
      delayIndex = 0;
    }

    delayTimer = mozziMicros();

    // play whatever is in the buffer at delayIndex 
    return dimThisMode ? toPlay>>2 : toPlay>>1;
  } else {
    return 0;
  }
  
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
    // Serial.println(F("Too low!"));
    return 0;
  } else if (sig_db <= UPPER){
    // original
    // float soft_slope = SLOPE * ((sig_db - LOWER) / WIDTH) * 0.5;

    // return (int) ((1.0/6.0) * (sig_db-LOWER)) * (LOWER - sig_db);
    // Serial.println(F("Just right!"));
    return (int) (sig_db + 23.0)/17.0;
  } else {
    // Serial.println(F("High brow!"));
    return (int) SLOPE * (THRESHOLD - sig_db);
  }
}

int render_effects(int signal) {
  // its not enabled
  // if(flanger_enabled()){
  //   // do delay
  //   signal = signal + aDel.next((int8_t)signal, 512);
  // }

  if(delay_enabled()){
    signal = signal + render_delay(signal);
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
  //   signal = signal + ( reverb.next(signal) >> 3);
  // }

  return (int) signal;
}

float get_slope(float y2, float x2, int y1, int x1){
  return (y2-y1/x2-x1);
}

void setup(){
  Serial.begin(9600);

  // start somewhere random in the 'random seq'
  randomSeed(analogRead(0));
  
  // load settings from EEP <3 on startup
  // readSettings();

  // start up the neopix
  pixel.begin();
  // show black
  pixel.show();

  // initialize colors
  for(byte i=0; i<NUMPIXELS*3; i++){
    pixel_colors[i] = 0;
    // from_pixel_colors[i] = 0;
    // dest_pixel_colors[i] = 0;
  }

  pinMode(BIGBUTTONPIN, INPUT_PULLUP);

  pinMode(TOGGLE_0_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_1_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_2_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_3_PIN, INPUT_PULLUP);

  pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);

  // for ada encoder, set this, doesnt have power pin
  pinMode(ROTARY_A_PIN, INPUT_PULLUP);
  pinMode(ROTARY_B_PIN, INPUT_PULLUP);
  
  // top left button signal
  // top right gnd
  // bottom left signal A
  // bottom middle gnd
  // bottom right signal B


  arp_delay.start(300);

  // lpf currently applied to EVERYTHING, maybe dump if corrected passive filter sounds better
  lpf.setResonance(16);
  // cutoff is 0-255 represnting 0-8191Hz (audiorate/2)
  lpf.setCutoffFreq(cutoff);


  // float phase_freq = 55.f;
  // aPhasor1.setFreq(phase_freq);
  
  // byte vib_intensity = 255;
  // aVibrato.setFreq(3.f);

  
  // -128 to 127
  // for udpatecontrol
  // aDel.setFeedbackLevel(-111);
  // this is how used in updateaudio
  // return asig/8 + aDel.next(asig, deltime); // mix some straight signal with the delayed signal

  // seed dat
  // randomSeed(analogRead(0));

  startMozzi(CONTROL_RATE); // :)
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

  // INITIAL MODEFOOL
  setup_mode(mode);

  setDisplayMode(SHOWINTRO);
  introDelay.start(INTRODELAY);


  // load settings from EEP <3 on startup
  // readSettings();
}

unsigned int oneBeat(){
  return 60000 / tempo;
}

unsigned int getReleaseTime(byte env_mode){
  if(env_mode == SHORT_ENV){
    return oneBeat() / 4;
  } else if(env_mode == LONG_ENV){
    return oneBeat() / 2;
    
  } else if(env_mode == BEAUTY_SHORT_ENV){
    return oneBeat() / 8;
  } else if(env_mode == BEAUTY_LONG_ENV){
    return oneBeat();
  } else if(env_mode == PIANO_SHORT_ENV){
    return oneBeat() / 8;
  } else if(env_mode == PIANO_LONG_ENV){
    return oneBeat() / 2;
  }
}

unsigned int getArpTime(byte env_mode){
  if(env_mode == SHORT_ENV){
    return oneBeat() / 2;
  } else if(env_mode == LONG_ENV){
    // twice as long delay between notes in longenv
    return oneBeat();
  }
}

unsigned int getNoteTime(byte env_mode){
  if(env_mode == BEAUTY_SHORT_ENV){
    return 1600;
  } else if(env_mode == BEAUTY_LONG_ENV){
    return 3200;
  } else if(env_mode == PIANO_SHORT_ENV){
    return 480;
  } else if(env_mode == PIANO_LONG_ENV){
    return 3000;
  } else if(env_mode == SHORT_ENV){
    return 200;
  } else if(env_mode == LONG_ENV){
    return 12200;
  } 
}

void setup_envelopes(byte env_mode){
  // theses are in ms baby
  if(env_mode == SHORT_ENV || env_mode == LONG_ENV){
    unsigned int a_t;
    unsigned int d_t;
    unsigned int s_t;
    unsigned int r_t;
    byte a_level;
    byte d_level;

    // most modes use the same ADSR for each Note because they only have one voice
    if(env_mode == SHORT_ENV){
      a_t = 2;
      d_t = 60;
      s_t = 60000;
      r_t =  getReleaseTime(env_mode) ;

      a_level = 245;
      d_level = 255;
    } else if(env_mode == LONG_ENV){
      a_t = 60;
      d_t = 80;
      s_t = 60000;
      r_t =  getReleaseTime(env_mode) ;

      a_level = 200;
      d_level = 255;
    }

    envelope0.setADLevels(a_level,d_level);
    // not milliseconds
    envelope0.setTimes(a_t,d_t,s_t,r_t);
    envelope1.setADLevels(a_level,d_level);
    // not milliseconds
    envelope1.setTimes(a_t,d_t,s_t,r_t);
    envelope2.setADLevels(a_level,d_level);
    // not milliseconds
    envelope2.setTimes(a_t,d_t,s_t,r_t);
    envelope3.setADLevels(a_level,d_level);
    // not milliseconds
    envelope3.setTimes(a_t,d_t,s_t,r_t);
  } else if(env_mode == BEAUTY_SHORT_ENV) {
    envelope0.setADLevels(180,225);
    envelope1.setADLevels(180,225);
    envelope2.setADLevels(200,225);
    envelope3.setADLevels(200,225);

    envelope0.setTimes(2, 40, 60000, 100 );

    envelope1.setTimes(2, 60, 60000, 100 );
    
    envelope2.setTimes(20, 80, 3000, 100 );
    
    envelope3.setTimes(30, 60, 2000, 100 );

    // Serial.println(F("I love bein in here!"));
  } else if(env_mode == BEAUTY_LONG_ENV) {
    envelope0.setADLevels(255,225);
    envelope1.setADLevels(255,225);
    envelope2.setADLevels(255,225);
    envelope3.setADLevels(255,225);

    envelope0.setTimes(80, 60, 60000, 300 );

    envelope1.setTimes(40, 60, 60000, 160 );
    
    envelope2.setTimes(160, 120, 60000, 200 );
    
    envelope3.setTimes(88, 80, 60000, 300 );

  } else if(env_mode == PIANO_SHORT_ENV) {
    envelope0.setADLevels(180,225);
    envelope1.setADLevels(180,225);
    envelope2.setADLevels(200,225);
    envelope3.setADLevels(200,225);

    // fundamental
    envelope0.setTimes(80, 50, 60000, 200 );

    // fifth - 2/3 length
    envelope1.setTimes(80, 50, 60000, 120 );
    
    // 2x fundamental - half length
    envelope2.setTimes(80, 50, 60000, 100 );
    
    // 21 semitone idk - 2/3 lengh of fund
    envelope3.setTimes(80, 50, 60000, 120 );

  } else if(env_mode == PIANO_LONG_ENV) {
    envelope0.setADLevels(180,225);
    envelope1.setADLevels(180,225);
    envelope2.setADLevels(200,225);
    envelope3.setADLevels(200,225);

    envelope0.setTimes(40, 180, 60000, 1800 );

    envelope1.setTimes(40, 180, 60000, 2400 );
    
    envelope2.setTimes(40, 180, 60000, 900 );
    
    envelope3.setTimes(40, 180, 60000, 2400 );
  }

  envelope0.update();
  envelope1.update();
  envelope2.update();
  envelope3.update();
}

unsigned int msToMozziTime(unsigned int ms){
  return (unsigned int) ms / 4;
}

void setup_mode(byte newmode){
  
  mode = newmode;
  // turn this off so we dont carry over from other mode
  play_continuing = STOPPED;

  // most modes follow short vs long env setting
  if(short_env_disabled()){
    envelope_mode = SHORT_ENV;
  } else {
    envelope_mode = LONG_ENV;
  }

  if(mode == REGNOTEMODE){

    // gets done by setup_envelopes
    // envelope0.update();
    // envelope1.update();
    // envelope2.update();
    // envelope3.update();
    dimThisMode = false;
  } else if(mode == ARPMODE){

    dimThisMode = false;
  } else if(mode == SWEEPMODE){

    // not needed because of setupenvelopes
    // envelope0.update();

    set_sweep_freq();
  } else if(mode == BEAUTYMODE){
    // yucky
    envelope_mode = short_env_disabled() ? BEAUTY_SHORT_ENV : BEAUTY_LONG_ENV;

    for(byte i=0; i<4; i++){
      notes[i]->note_off();
    }
    
    dimThisMode = true;
  } else if(mode == PIANOMODE){
    
    envelope_mode = short_env_disabled() ? PIANO_SHORT_ENV : PIANO_LONG_ENV;
    dimThisMode = true;
  } else if(mode == CHORDMODE || mode == CHORDSCHEMAMODE){
    dimThisMode = true;
  } else if(mode == SETTEMPOMODE){
    // flag this down to show that we dont set tempo before recording tap 1
    tempoTapTimer = false;
    setTempoTime = mozziMicros();
  } else if(mode == PLACEMODE){
    dimThisMode = true;
    for(byte i=0; i<4; i++){
      // start at 0 freq so its 'off'
      notes[i]->set_frequency(0);
    }
  }

  // show the new mode, unless its an option
  if(mode < SELECTOPTIONMODE){
    setDisplayMode(SHOWMODE);
  } else {
    // otherwise show option mode
    if(mode == SETTEMPOMODE){
      setDisplayMode(SHOWTEMPO);
    } else if(mode == SETFILTERMODE){
      // Serial.println(F("Setting filter mode display"));
      setDisplayMode(SHOWFILTER);
    } else if(mode == SETVOLMODE){
      setDisplayMode(SHOWVOL);
    } else if(mode == SETDELAYMODE){
      setDisplayMode(SHOWDELAY);
    } else if(mode == SETDETUNEMODE){
      setDisplayMode(SHOWDETUNE);
    }
  }
  // set envelopes for this mode
  setup_envelopes(envelope_mode);
}

void setDisplayMode(byte dispMode){
  bool zeroOutDisplay = false;
  if(dispMode == SHOWMODE){
    // Serial.println(F("I started showmode"));
  } else if(dispMode == PLAY){
    // Serial.println(F("I started playmode"));
    // set colors to 0 to get rid of cool blue
    zeroOutDisplay = true;
  } else if(dispMode == COOL){
    // Serial.println(F("I started coolmode"));
    zeroOutDisplay = true;
  } else if (dispMode == WEIRD){
    // Serial.println(F("I started weridispMode"));
  } else if(dispMode == IDLE){
    // zeroOutDisplay = true;
    pixel_flag = true;
    // set random colors for init on IDLE
    // Serial.println(F("I started IDLEmode"));
    zeroOutDisplay = true;
    // for(byte i=0; i<21; i++){
    //   pixel_colors[i] = rand() % 10;
    // }
  } else if(dispMode == LOADING){
    // Serial.println(F("I started loadingmode"));
    zeroOutDisplay = true;
  } else if(dispMode == SHOWROTARY){
    // zeroOutDisplay = true;
    // Serial.println(F("I started showrotarymode"));
    
    // reset this so we know when to fade down
    rotaryMoved = false;

  } else if(dispMode == SHOWCHORDSCHEMA){
    zeroOutDisplay = true;
    // Serial.println(F("I started showchordschemamode"));
  } else if(dispMode == SHOWHARMMODE){
    zeroOutDisplay = true;
    // Serial.println(F("I started showchordschemamode"));
  } else if(dispMode == SELECTOPTIONMODE){
    zeroOutDisplay = true;
    // Serial.println(F("I started selectoptionmode"));
  } else if(dispMode == SHOWTEMPO){
    zeroOutDisplay = true;
    // Serial.println(F("I started showtempomode"));
  } else if(dispMode == SHOWFILTER){
    zeroOutDisplay = true;
    // Serial.println(F("I started showfiltermode"));
  } else if(dispMode == SHOWVOL){
    zeroOutDisplay = true;
    // Serial.println(F("I started showvolmode"));
  } else if(dispMode == SHOWDELAY){
    zeroOutDisplay = true;
    // Serial.println(F("I started showvolmode"));
  } else if(dispMode == SHOWDETUNE){
    zeroOutDisplay = true;
    // Serial.println(F("I started showvolmode"));
  } else if(dispMode == SHOWINTRO){
    // Serial.println(F("I started showvolmode"));
    for(byte i=0; i<NUMPIXELS; i++){
      if(i == 0 || i>=5){
        pixel_colors[i*3] = 12;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 255;  
      } else {
        pixel_colors[i*3] = 12;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 12;  
      }
    }
    bigButtonBrightness = 255;
  } else if(dispMode == SHOWPLACEOSC){
    zeroOutDisplay = true;
  }

  if(zeroOutDisplay){
    for(byte i=0; i<21; i++){
      pixel_colors[i] = 0;
    }
  }

  display_idle_delay.start(IDLEDELAY);

  // reset all this reusable shit  
  pixel_counter = 0;
  // display_aux_timer = mozziMicros();
  // reset to nothing so we hit its set in display__ method
  display_aux_delay.start();
  pixel_flag = false;

  display_mode = dispMode;
}

void updatePlayingNotes(byte rpMove){
  for(byte i=0; i<4; i++){
    if( notes[i]->is_playing() ){
      // if its playing, change it by rpmove semitones
      byte newNote = notes[i]->get_note() + rpMove;
      // acutally change freq
      notes[i]->set_frequency( note_to_freq( newNote ) );
      // record for next moves :)
      notes[i]->store_note(newNote);
    }
  }
}

void updateControl() {
  bool aVal = digitalRead(ROTARY_A_PIN);
  bool bVal = digitalRead(ROTARY_B_PIN);
  if( aVal != last_a ){

    // Means the knob is rotating// if the knob is rotating, we need to determine direction// We do that by reading pin B.

    // hold the increment were gonna move by
    int8_t rp_move = 0;

    if( (aVal ^ last_a) == (aVal ^ bVal) ){

      // Means pin A Changed first -We're RotatingClockwise.
      // Serial.println(F("CCW"));
      rp_move = 1;

      // show rotary UP
      pixel_flag = true;
    } else {
      // Otherwise B changedfirst and we're moving CCW
      // Serial.println(F("CW"));
      rp_move = -1;

      // show rotary DOWN
      pixel_flag = false;
    }

    // heat up to avoid double moves
    rotaryState += 1;

    // either way show rotary
    if(mode < SELECTOPTIONMODE && (display_mode != SHOWROTARY || rp_move != last_rp_move) && introDelay.ready() ){
      setDisplayMode(SHOWROTARY);
    }

    // if(mozziMicros() - rotary_debounce_timer < ROTARY_DEBOUNCE_DELAY ){
    //   // rp move blocked, timer wasnt ready
    //   rp_move = 0;
    // } else {
    //   // rp move allowed, set timer to avoid bounceback
    //   rotary_debounce_timer = mozziMicros();
    // }

    last_rp_move = rp_move;

    // is option
    if(mode > SELECTOPTIONMODE){

      // is option
      if(mode == SETFILTERMODE){
        if(cutoff + 8*rp_move >= 0 && cutoff + 8*rp_move <= 255 ){
          cutoff += 8*rp_move;
        }
        // actually set it here
        lpf.setCutoffFreq( cutoff );
        // Serial.print(F("cutoff "));
        // Serial.println(cutoff);
      } else if(mode == SETVOLMODE){
        if( volume + 0.02*rp_move > 0 && volume + 0.02*rp_move <= 1.0 ){
          // changed here, gets relfected in updateaudio
          volume += 0.02*rp_move;
        }

      } else if(mode == SETDELAYMODE){
        if( delayTime + rp_move*10 > 0 && delayTime + rp_move*10 <= MAX_DELAY_TIME ){
          delayTime += rp_move*10;
        }

      } else if(mode == SETDETUNEMODE){
        if( detune + rp_move > -50 && detune + rp_move <= 50 ){
          detune += rp_move;
        }

      }

    } else if(rotaryState == 2 && mode <= SELECTOPTIONMODE){
      // need rp to move to select an option!

      // only set when changing freq
      if(mode == PLACEMODE){
        int oldFreqHeat = freqHeat;
        freqHeat += rp_move * 2;
        if( (freqHeat ^ oldFreqHeat) < 0 ){
          // do they have opposite signs (ie did we cross 0)
          // dont go negative if so
          freqHeat = 0;
        }
        // reflect the rotary change
        safeRotaryChange(rp_move);
          
        // set the frequency for the currently-selected place osc
        setPlaceFreq( placeOsc, rp_move );
      } else if(mode != SWEEPMODE) {

        if(keylock){
          safeRotaryChange( moveRotaryWithinScale(rp_move) );
        } else {
          // is regular
          safeRotaryChange(rp_move);
        }

        // change currently playing freqs
        updatePlayingNotes(rp_move);
      }
      
      rotaryState = 0;
    }

    if(mode == SWEEPMODE){
      pixel_flag = !pixel_flag;
      // too chunky for place
      freqHeat += rp_move;
    }

    last_a = aVal;
  }
  
  handle_rotary_button();
  // check if note buttons down/ready for action -> if so, play note!
  handle_note_button();
  // this just sets em into toggles[]
  set_effects();

  bool this_env_toggle = short_env_disabled();
  if(last_env_toggle != this_env_toggle){
    // when env length switch gets changed

    if(mode == BEAUTYMODE){
      // each env has its own setting
      envelope_mode = this_env_toggle ? BEAUTY_SHORT_ENV : BEAUTY_LONG_ENV;
    } else if(mode == PIANOMODE){
      // each env has its own setting
      envelope_mode = this_env_toggle ? PIANO_SHORT_ENV : PIANO_LONG_ENV;
    } else if(mode != PLACEMODE) {
      // shared adsr values for envs
      envelope_mode = this_env_toggle ? SHORT_ENV : LONG_ENV;
    }
  
    setup_envelopes( envelope_mode );

    // only set this when 
    last_env_toggle = this_env_toggle;
  }

  if(mode == ARPMODE && play_continuing){
    // keep playin that arp
    play_arp_notes();
  }

  // handle events for oscillatorsz
  bool anyPlaying = false;
  for(byte i=0; i<4; i++){

    if(notes[i]->is_playing()){
      anyPlaying = true;
    }
    
    if(mode == REGNOTEMODE){
      
      if(notes[i]->is_playing() && current_note != i){

        // do regular note off if button not held for this note
        notes[i]->note_off();
        note_delays[i]->start( getReleaseTime(envelope_mode) );
        
        // get rid of that pip
        displayPlayNotes[i] = false;

      } else if(!notes[i]->is_available() && note_delays[i]->ready()){
        // Serial.print(i);
        // Serial.println(F("Was ready!"));
        notes[i]->set_available(true);
      }

    } else if(mode == ARPMODE) {
      
      if(note_delays[i]->ready()){
        notes[i]->note_off();
        notes[i]->set_available(true);
        displayPlayNotes[i] = false;
      }

    } else if(mode == REPEATMODE){

      if(i < 2){

        if(notes[i]->is_playing() && current_note != i){

          // do regular note off if button not held for this note
          notes[i]->note_off();
          displayPlayNotes[i] = false;
          note_delays[i]->start( oneBeat() * 32 );
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
            // Serial.print(F("I payed a repeat dummy "));
            // Serial.println(note);
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
      // fuck!
      if(!notes[i]->is_available() && note_delays[i]->ready()){
        // Serial.print(i);
        // Serial.println(F("Was ready!"));
        notes[i]->set_available(true);
      }
    }

    // this doesnt work, may need ONE timer that retriggers all 4
    // // for all modes, if still holding the button refresh the note
    if(button_state && notes[i]->is_playing() && mode != ARPMODE){
      if(note_delays[i]->ready()){
        notes[i]->note_on();
        note_delays[i]->start( getReleaseTime(envelope_mode) );
      }
    }
  } 

  if(!anyPlaying && play_continuing == TAILING){
    play_continuing = STOPPED;
  }

  if(mode == SWEEPMODE){
    // sweepers only

    // wait 2ms to change sweep freq
    //  oops 200 is 2 microseconds
    unsigned long now = mozziMicros();
    if(now-sweep_timer > 1800000){

      if(freqHeat > 0 ){
        freqHeat -= 1;
      } else if(freqHeat < 0){
        freqHeat += 1;
      }
    }
    if((now - sweep_timer) > 20000){
      set_sweep_freq();
      sweep_timer = now;
    }

    if(button_state){
      if(note_delays[1]->ready()){
        notes[0]->note_on();
        note_delays[1]->start( getReleaseTime(envelope_mode) );
      }
    }
  } else if(mode == PLACEMODE) {
    unsigned long now = mozziMicros();
    if(now-sweep_timer > 16000000){

      if(freqHeat > 0 ){
        freqHeat -= 1;
      } else if(freqHeat < 0){
        freqHeat += 1;
      }
    }
  }


  if(mode == SETTEMPOMODE){
    // set onbeat so displaymode will visualize the pulse

    unsigned long now = mozziMicros();
    onBeat = isOnBeat(setTempoTime, now, tempo);
    if(onBeat != lastOnBeat){
      // if we hit the first tick of a beat, resset the tempotime so it still fits in a unsigned long
      setTempoTime = now;
      // reset so display is bright for new beat
      beatBrightness = 180;
    }

    lastOnBeat = onBeat;
  }

  // set pixels now that we've changed state
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
  } else if(display_mode == IDLE){
    showIdle();
  } else if(display_mode == LOADING){
    showLoading();
  } else if(display_mode == ROTATE){
    showRotate();
  } else if(display_mode == SHOWROTARY){
    showRotary();
  } else if(display_mode == SHOWCHORDSCHEMA){
    showChordSchema();
  } else if(display_mode == SHOWHARMMODE){
    showHarmMode();
  } else if(display_mode == SHOWSELECT){
    showSelectOption();
  } else if(display_mode == SHOWTEMPO){
    showTempo();
  } else if(display_mode == SHOWFILTER){
    showFilter();
  } else if(display_mode == SHOWVOL){
    showVol();
  } else if(display_mode == SHOWDELAY){
    showDelay();
  } else if(display_mode == SHOWDETUNE){
    showDetune();
  } else if(display_mode == SHOWINTRO){
    showIntro();
  } else if(display_mode == SHOWPLACEOSC){
    showPlaceOsc();
  }

  if(!button_state && display_mode != IDLE && mode < SELECTOPTIONMODE && display_idle_delay.ready() ){
    // go back after 2s if button not held
    setDisplayMode(IDLE);
    // Serial.println(F("Set IDLE again."));
  }
}

void writeDisplay(){
// set neopixel
  // unsigned long now = mozziMicros();
  // if(now - pixel_timer >= PIXDELAY){

  if(mode == SETTEMPOMODE || pixdelay.ready()){
    handleDisplay();

    pixel.clear();
    for(byte i=0; i<NUMPIXELS; i++) {
      // uint8_t r = rand() % 6;
      // uint8_t g = rand() % 6;
      // uint8_t b = rand() % 6;
      // Serial.print("Setting pix to ");
      // Serial.print(pixel_colors[pixcolorindex]);
      // Serial.print(" ");
      // Serial.print(pixel_colors[pixcolorindex+1]);
      // Serial.print(" ");
      // Serial.println(pixel_colors[pixcolorindex+2]);
        pixel.setPixelColor( i, pixel.Color(pixel_colors[(i * 3)], pixel_colors[(i * 3)+1], pixel_colors[(i * 3)+2]) );
    }

    // use the pix timer to write to the button as well
    handleArcadeButton();

    // pixel_timer = now;
    pixel.show();
    pixdelay.start(PIXDELAY);
  }
}

void handleArcadeButton(){
  analogWrite(BIGBUTTON_LED_PIN, bigButtonBrightness);
}

bool inRange(byte low, byte high, byte value){
  // dont page me
  return low <= value && value <= high;
}

bool colorCloseEnough(byte color, byte destColor){
  return inRange(destColor-4, destColor+4, color);
}

void showPlaceOsc(){
  for(byte i=0; i<=placeOsc; i++){
    if(i==0){

      if(placeOsc == 4){
        pixel_colors[i*3] = 120;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 120;
      } else {
        pixel_colors[i*3] = 40;
        pixel_colors[i*3+1] = 0;
        pixel_colors[i*3+2] = 40;
      }
      
    }
  
    if(notes[i]->get_frequency() != 0){
      pixel_colors[(i+1)*3] = 12;
      pixel_colors[(i+1)*3+1] = 12;
      pixel_colors[(i+1)*3+2] = 120;
    }
    
  }
}

void showIntro(){
  if(display_aux_delay.ready()){
    for(byte i=0; i<NUMPIXELS*3; i++){
      if(pixel_colors[i] > 0){
        pixel_colors[i] -= 1;
      }

      // if(i == 0 || i>=5){
      //   if(pixel_colors[i*3]<255){
      //     pixel_colors[i*3] += 1;
      //   }

      // } else {
      //   if(pixel_colors[i*3]<255){
      //     pixel_colors[i*3] += 1;
      //   }
      //   if(pixel_colors[i*3+2]<255){
      //     pixel_colors[i*3+2] += 1;  
      //   }
      // }
    }

    if(bigButtonBrightness>0){
      bigButtonBrightness -= 1;
    }
    display_aux_delay.start(64);
  }
}

void showPlay(){
  // just the meter, 

  // well hi
  byte pixcolorindex;
  bool someonePlayin = false;
  for(byte i=0; i<4; i++){
    pixcolorindex = i * 3;
    if(displayPlayNotes[i]){
      someonePlayin = true;
    } else {

      // if it aint playin, fade it down
      // safeFadePixel(pixcolorindex, 5);
      // safeFadePixel(pixcolorindex+1, 5);
      // safeFadePixel(pixcolorindex+2, 5);

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

  // bad VU meter
  // if(vuTotal+bigButtonBrightness<255){
  //   vuTotal += bigButtonBrightness; 
  // } else {
  //   vuTotal = 255;
  // }
  // if(vuTotal-16 > 0 ){
  //   vuTotal -= 16;
  // } else {
  //   vuTotal = 0;
  // }
  // pixel_colors[12] = vuTotal>80 ? vuTotal : 0;
  // pixel_colors[13] = 0;
  // pixel_colors[14] = 0;
  // pixel_colors[15] = vuTotal>40 ? vuTotal : 0;
  // pixel_colors[16] = vuTotal>40 ? vuTotal : 0;
  // pixel_colors[17] = 0;
  // pixel_colors[18] = 0;
  // pixel_colors[19] = vuTotal;
  // pixel_colors[20] = 0;
}

// void safeFadePixel(byte pixIndex, byte decrement){
//   if(pixel_colors[pixIndex] - decrement > 0){
//     pixel_colors[pixIndex] -= decrement;
//   } else {
//     pixel_colors[pixIndex] = 0;
//   }
// }

void displayPlayNote(byte note_index){
  // well hi
  byte pixcolorindex = note_index * 3;
  displayPlayNotes[note_index] = true;

  // note is playin but not already lit

  // set rand color on corresponding jewel
  // Serial.print(F("Setting pix at "));
  // Serial.println(pixcolorindex);

  pixel_colors[pixcolorindex] = 8;
  pixel_colors[pixcolorindex+1] = 4;
  pixel_colors[pixcolorindex+2] = 16;
  // pixel_colors[pixcolorindex] = mode * 12;
  // pixel_colors[pixcolorindex+1] = 64 / mode;
  // pixel_colors[pixcolorindex+2] = 128 - (mode * 12);

  for(byte i=4; i<NUMPIXELS; i++){
    // if its notes playin, turn on the meter lgiths
    pixel_colors[i*3] = 25;
    pixel_colors[i*3+1] = 0;
    pixel_colors[i*3+2] = 125;
  }

    // // gyr
    // pixel_colors[12] = 255;
    // pixel_colors[13] = 0;
    // pixel_colors[14] = 0;
    // pixel_colors[15] = 255;
    // pixel_colors[16] = 255;
    // pixel_colors[17] = 0;
    // pixel_colors[18] = 0;
    // pixel_colors[19] = 255;
    // pixel_colors[20] = 0;
}

void showRotary(){
  if(!rotaryMoved){
    if(pixel_flag){
      pixel_colors[6] = 10;
      pixel_colors[7] = 10;
      pixel_colors[8] = 10;
      pixel_colors[9] = 0;
      pixel_colors[10] = 200;
      pixel_colors[11] = 0;
    } else {
      pixel_colors[6] = 200;
      pixel_colors[7] = 0;
      pixel_colors[8] = 0;
      pixel_colors[9] = 10;
      pixel_colors[10] = 10;
      pixel_colors[11] = 10;
    }

    rotaryMoved = true;
  } else {

    for(byte i=6; i<12; i++){
      if(pixel_colors[i]-20 > 0){
        pixel_colors[i] -= 20;
      } else {
        pixel_colors[i] = 0;
      }
    }
  }
}

void showSelectOption(){
  byte selected = abs(rotary_position) % 6;
  for(byte i=1; i<NUMPIXELS; i++){

    byte brightness;

    if(i == 1){
      brightness = selected == 0 ? 1 : 16;
      // 12
      pixel_colors[i*3] = OPTION0COLOR_R/brightness;
      pixel_colors[i*3+1] = OPTION0COLOR_G/brightness;
      pixel_colors[i*3+2] = OPTION0COLOR_B/brightness;
    } else if(i == 2 ){
      brightness = selected == 1 ? 1 : 16;
      // 34
      pixel_colors[i*3] = OPTION1COLOR_R/brightness;
      pixel_colors[i*3+1] = OPTION1COLOR_G/brightness;
      pixel_colors[i*3+2] = OPTION1COLOR_B/brightness;
    } else if(i == 3){
      brightness = selected == 2 ? 1 : 16;
      pixel_colors[i*3] = OPTION2COLOR_R/brightness;
      pixel_colors[i*3+1] = OPTION2COLOR_G/brightness;
      pixel_colors[i*3+2] = OPTION2COLOR_B/brightness;
    } else if(i == 4) {
      brightness = selected == 3 ? 1 : 16;
      // 56
      pixel_colors[i*3] = OPTION3COLOR_R/brightness;
      pixel_colors[i*3+1] = OPTION3COLOR_G/brightness;
      pixel_colors[i*3+2] = OPTION3COLOR_B/brightness;
    } else if(i == 5) {
      brightness = selected == 4 ? 1 : 16;
      // 56
      pixel_colors[i*3] = OPTION4COLOR_R/brightness;
      pixel_colors[i*3+1] = OPTION4COLOR_G/brightness;
      pixel_colors[i*3+2] = OPTION4COLOR_B/brightness;
    } else if(i == 6) {
      brightness = selected == 5 ? 1 : 16;
      // 56
      if(keylock){
        pixel_colors[i*3] = 130/brightness;
        pixel_colors[i*3+1] = 20/brightness;
        pixel_colors[i*3+2] = 0/brightness; 
      } else {
        pixel_colors[i*3] = 16/brightness;
        pixel_colors[i*3+1] = 16/brightness;
        pixel_colors[i*3+2] = 16/brightness;
      }
      
    } else {
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    }
  }
}

void showFilter(){
  for(byte i=1; i<NUMPIXELS; i++){
    if( i < ( floor( (float) cutoff/255 * 8) ) ){
      // show pix proportional to cutttoffff
      pixel_colors[i*3] = OPTION1COLOR_R;
      pixel_colors[i*3+1] = OPTION1COLOR_G;
      pixel_colors[i*3+2] = OPTION1COLOR_B;
    } else {
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    }
  }
}

void showVol(){
   for(byte i=1; i<NUMPIXELS; i++){
    if( i < floor( (float) volume/1.0 * 8 ) ){
      // show pix proportional to vol oom
      pixel_colors[i*3] = OPTION2COLOR_R;
      pixel_colors[i*3+1] = OPTION2COLOR_G;
      pixel_colors[i*3+2] = OPTION2COLOR_B;
    } else {
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    }
  } 
}

void showDelay(){
   for(byte i=1; i<NUMPIXELS; i++){
    if( i < floor( (float) delayTime/1000 * 8 ) ){
      // show pix proportional to vol oom
      pixel_colors[i*3] = OPTION3COLOR_R;
      pixel_colors[i*3+1] = OPTION3COLOR_G;
      pixel_colors[i*3+2] = OPTION3COLOR_B;
    } else {
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    }
  } 
}

void showDetune(){
   for(byte i=1; i<NUMPIXELS; i++){
    if( i < floor(((float) (detune+50)/100 ) * 8) ){
      // show pix proportional to vol oom
      pixel_colors[i*3] = OPTION4COLOR_R;
      pixel_colors[i*3+1] = OPTION4COLOR_G;
      pixel_colors[i*3+2] = OPTION4COLOR_B;
    } else {
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    }
  } 
}

void showTempo(){
  // for(byte i=0; i<21; i++){
  //   if(onBeat){
  //     pixel_colors[i] = beatBrightness;

  //     // fade off after beat so less harsh
  //     if(beatBrightness > 0){
  //       beatBrightness -= 1;
  //     }
  //   } else {
  //     pixel_colors[i] = 0;
  //   }
  // }
  for(byte i=0; i<NUMPIXELS; i++){
    if(onBeat){
      pixel_colors[i*3] = beatBrightness;

      // fade off after beat so less harsh
      if(beatBrightness > 0){
        beatBrightness -= 1;
      }
    } else {
      pixel_colors[i*3] = 0;
    }

    pixel_colors[i*3+1] = altColorBrightness/64;
    pixel_colors[i*3+2] = (255 - altColorBrightness)/64;
  }

  altColorBrightness++;
  if(altColorBrightness == 200){
    altColorBrightness = 0;
  }
}

void showRotate(){
  // for(byte i=0; i<NUMPIXELS; i++){
  //   pixel_colors[i*3] = i * 17;
  //   pixel_colors[i*3+1] = i * 17;
  //   pixel_colors[i*3+2] = i * 17;
  // }
  // return;
  if( display_aux_delay.ready() ){

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

    display_aux_delay.start(160);
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
  if( display_aux_delay.ready() ){

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

    display_aux_delay.start(10);
  }
}

void showIdle(){
  if( display_aux_delay.ready() ){
    
    byte choice = rand() % 21;
    if(pixel_colors[choice] < 10 && ( (choice+1) % 3 == 0 || choice%3 == 0 ) ){

      pixel_colors[choice] += 1;

    } else {
      pixel_colors[choice] = 0;
    }

    display_aux_delay.start(40);
  }
}

void showChordSchema(){
  for(byte i=0; i<NUMPIXELS; i++){

    if(i <= chord_schema){
      // red for chordschema count
      pixel_colors[i*3] = 200;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 0;
    } else {
      // otherwise background color
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 0;
      pixel_colors[i*3+2] = 3;
    }

  }
}

void showHarmMode(){
  for(byte i=0; i<NUMPIXELS; i++){

    if(i <= harmonicMode){
      // red for chordschema count
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 50;
      pixel_colors[i*3+2] = 200;
    } else {
      // otherwise background color
      pixel_colors[i*3] = 0;
      pixel_colors[i*3+1] = 3;
      pixel_colors[i*3+2] = 0;
    }

  }
}

void showCool(){
  // // check if we arrived at dest colors
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

  //   if(pixel_counter == i){
  //     // allowed to get reassigned
  //     if( colorCloseEnough(pixel_colors[pixcolorindex], dest_pixel_colors[pixcolorindex]) && colorCloseEnough(pixel_colors[pixcolorindex+1], dest_pixel_colors[pixcolorindex+1]) && colorCloseEnough(pixel_colors[pixcolorindex+2], dest_pixel_colors[pixcolorindex+2]) ){

  //       // Serial.print("I set new color for ");
  //       // Serial.println(pixcolorindex);

  //       // if we reached the rgb desgination for this pixel
  //       // preserve dest color as from color so we can fade propaly
  //       from_pixel_colors[pixcolorindex] = dest_pixel_colors[pixcolorindex];
  //       // dest_pixel_colors[pixcolorindex] = 0;
  //       dest_pixel_colors[pixcolorindex] = rand() % 255;
  //       pixel_lerp_steps[pixcolorindex] = 0;

  //       from_pixel_colors[pixcolorindex+1] = dest_pixel_colors[pixcolorindex+1];
  //       // dest_pixel_colors[pixcolorindex+1] = 0;
  //       dest_pixel_colors[pixcolorindex+1] = rand() % 255;
  //       pixel_lerp_steps[pixcolorindex+1] = 0;

  //       from_pixel_colors[pixcolorindex+2] = dest_pixel_colors[pixcolorindex+2];
  //       dest_pixel_colors[pixcolorindex+2] = rand() % 255;
  //       pixel_lerp_steps[pixcolorindex+2] = 0;

  //       // Serial.print("The new destination is.. ");
  //       // Serial.print(dest_pixel_colors[pixcolorindex]);
  //       // Serial.print(" ");
  //       // Serial.print(dest_pixel_colors[pixcolorindex+1]);
  //       // Serial.print(" ");
  //       // Serial.print(dest_pixel_colors[pixcolorindex+2]);
  //     }
  //   } else {
    
  //     // otherwise just keep fadin
  //     if( !(colorCloseEnough(pixel_colors[pixcolorindex], dest_pixel_colors[pixcolorindex]) && colorCloseEnough(pixel_colors[pixcolorindex+1], dest_pixel_colors[pixcolorindex+1]) && colorCloseEnough(pixel_colors[pixcolorindex+2], dest_pixel_colors[pixcolorindex+2])) ){

  //       for(byte x=0; x<3; x++){
  //         // loop through r,g,b for each pixel
  //         compoindex = pixcolorindex+x;
  //         // track u as byte, turn into float here -> only 7 so same u steps for each whole rgb
  //         pixel_colors[compoindex] = lerp(pixel_colors[compoindex], dest_pixel_colors[compoindex], pixel_lerp_steps[ pixcolorindex ] );
  //       }

  //       pixel_lerp_steps[ pixcolorindex ] += 1;
  //     }
      
  //   }
  // }

  // unsigned long now = mozziMicros();
  // if(now - display_aux_timer > 10000000){
  //   // increment which pixel can be recolored
  //   pixel_counter++;
  //   if(pixel_counter==NUMPIXELS){
  //     pixel_counter = 0;
  //   }
  //   display_aux_timer = now;
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
  byte r = 200;
  byte g = 0;
  byte b = 0;
  byte numToLight = mode;
  if(mode == REGNOTEMODE){
    // nothin!
  } else if(mode <= CHORDMODE){
    r = 32;
    g = 43;
    b = 160;
  } else if(mode <= PIANOMODE){
    r = 0;
    g = 250;
    b = 154;
  } else if(mode <= SWEEPMODE){
    r = 135;
    g = 100;
    b = 0;
  }

  if(mode >= NUMPIXELS){
    // for big mode (7 or more aka more than 0..6), different color
    // r = 0;
    // g = 255;
    // b = 0;
    numToLight = mode - NUMPIXELS;
  }

  byte pixcolorindex;
  float pixmag = pixel_counter/255.0;
  for(byte i=0; i<NUMPIXELS; i++){
    pixcolorindex = i * 3;
    if(i <= numToLight){
      // light up each pixel up to mode number
      pixel_colors[pixcolorindex] = (byte) r*pixmag;
      pixel_colors[pixcolorindex+1] = (byte) g*pixmag;
      pixel_colors[pixcolorindex+2] = (byte) b*pixmag;
    } else {
      // dark it out
      pixel_colors[pixcolorindex] = 0;
      pixel_colors[pixcolorindex+1] = 0;
      pixel_colors[pixcolorindex+2] = 0;
    }
  }

  if(display_aux_delay.ready()){

    // starts false
    if(!pixel_flag){
      pixel_counter += 1;
    } else {
      pixel_counter -= 1;
    }

    if(pixel_counter == 255){
      pixel_flag = true;
    } else if(pixel_counter == 0){
      pixel_flag = false;
    }  

    display_aux_delay.start(1); 
  }
 
}

int updateAudio(){
  int sig = 0;
  // -if button down, play and fill buffer
  // -if button was down, now up, start wait
  // -if wait complete, play buffer

  // get vals from all playing notes

  if(mode == SWEEPMODE){

    // if we're still holding the button, or TAILING
    if(play_continuing != STOPPED){
      notes[0]->update_envelope();
      sig = (int) envelope0.next() * ( aSin0.next() >> 1);
    }
  } else if(mode < SELECTOPTIONMODE) {
    // dont play notes during OPTION modes

    // REGNOTEMODE, ARPMODE, BEAUTYMODE...

    for(byte i=0; i<4; i++){
      notes[i]->update_envelope();
      int next_sample = notes[i]->osc_next();
      // if(vibrato_enabled()){
      //   // intensity value * phase offset value
      //   // Serial.println("Vibrato active...");
      //   Q15n16 vibrato = (Q15n16) 160 * aVibrato.next();
      //   next_sample = notes[i]->osc_phmod_next(vibrato);
      // } else {
      //   next_sample = notes[i]->osc_next();
      // }

      // gain * filter(oscnext)
      // this was arpmode... mby put back?
      if(mode == REGNOTEMODE || mode == ARPMODE || mode == REPEATMODE){
        // if( notes[i]->is_playing() ){
        // }
          sig += ( notes[i]->env_next() * (next_sample >> 2) );
        // sig += ( notes[i]->env_next() * next_sample );

      // } else if(mode == ARPMODE){
        // // huh?! not sure if this makes sense (which Note does arp note play on)
        // sig += (int) ( notes[i]->env_next() * lpf.next( next_sample )  );
        // // sig += (int) ( notes[i]->env_next() * next_sample * (1/(i+1) * 0.89)  );
      } else if(mode == CHORDMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) ( notes[i]->env_next() * ( ( next_sample * (1/i+1) ) >> 2 )  );
        // sig += (int) ( notes[i]->env_next() * next_sample * (1/(i+1) * 0.89)  );
      } else if(mode == CHORDSCHEMAMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += (int) notes[i]->env_next() * ( (next_sample * (1/i+1) ) >> 2);
        // sig += (int) ( notes[i]->env_next() * next_sample * (1/(i^2) * 0.89)  ); 
      } else if(mode == BEAUTYMODE){

        // yikes
        sig += notes[i]->env_next() * ( ( next_sample * (1/i+1) ) >> 2);
        // if(qa_delay.ready()){
        //   // Serial.println(sig/256);
        //   Serial.print(F("I added "));
        //   Serial.println(val);
        //   Serial.println(sig);
        //   qa_delay.start(1000);
        // }

      } else if(mode == PIANOMODE){

        // quiet these boys down a bit, 0 is root, make successive Notes in chord a little quieter
        sig += notes[i]->env_next() * ( ( next_sample * (1/i+1) ) >> 2);
        // sig += (int) ( notes[i]->env_next() * next_sample * (1/(i+1) * 0.68)  );
        // sig += (int) ( notes[i]->env_next() );
      } else if(mode == PLACEMODE){
        sig += ( notes[i]->env_next() * (next_sample >> 3) );
      }
    }
  }

  // dry will just be passed along if not enabled
  sig = render_effects(sig);

  // set volume
  // Serial.println(sig);
  // sig = (int) (sig * volume)>>8;
  // Serial.println(sig);

  // really fucks up the audio
  bigButtonBrightness = abs(sig);

  return (int) lpf.next ( (int) (sig * volume)>>8 );
}

void loop(){
  audioHook(); // required here
}