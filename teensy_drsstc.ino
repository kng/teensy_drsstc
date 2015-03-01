IntervalTimer synthTimer;

const int maxkeys = 4;
int frq[maxkeys];
int pulse[maxkeys];
int frq_cnt[maxkeys];
int pulse_cnt[maxkeys];
int enabled[maxkeys];
int currentnote[maxkeys];
unsigned int pulse_off[maxkeys];
unsigned int pulse_on[maxkeys];

const int srate_pin = 13;
const int pulse_pin = 14;

const int srate = 100000; //44100;    // 44.1kHz standard sampling rate
const int srate_us = 1000000/srate;  // how many microseconds each sample takes
const int holdoff = 150;    // DRSSTC minimum dead time between pulses
const int holdoff_cnt = holdoff / srate_us;  // holdoff in sample ticks
const int frq_min = 10;    // lowest note/frequency that can be activated
const int frq_max = 2000;   // highest -||-
const int maxpulse = 150;

unsigned int pulse_holdoff=0;
// the following values can be adjusted via MIDI CC
int pulse_min = 10;   // DRSSTC minimum pulse to produce the arc
int pulse_max = 150;  // DRSSTC maximum pulse before the cirrent limiting kicks in
int frq_min_start = 100; // frequency at wich to start ramping down the pulse width
int frq_max_start = 1000;

float pitchInFrequency;

void setup(void) {
  usbMIDI.setHandleNoteOff(OnNoteOff);
  usbMIDI.setHandleNoteOn(OnNoteOn) ;
  usbMIDI.setHandleControlChange(OnControlChange);
  //usbMIDI.setHandleProgramChange(OnProgramChange)
  //usbMIDI.setHandleAfterTouch(OnAfterTouch)
  //usbMIDI.setHandlePitchChange(OnPitchChange)
  
  pinMode(srate_pin, OUTPUT);
  pinMode(pulse_pin, OUTPUT);
  synthTimer.begin(dr_pulse, srate_us);

  for(int i=0; i<maxkeys; i++){  // make sure the memory is zerofilled
    enabled[i] = 0;
    pulse_on[i] = 0;
    pulse_off[i] = 0;
    pulse_cnt[i]=100;
    frq_cnt[i]=100;
    currentnote[i]=0;
  }
}

void dr_pulse(void) {    // main synthesizer routine, gets called at SRATE
  int i;
  static int noteon, oldnoteon=0;
  digitalWriteFast(srate_pin, HIGH);  // debugging purposes

  noteon = 0;
  for(i=0; i<maxkeys; i++){
    if(pulse_on[i] == 0){    // check if we're in the delay part or the pulse
      pulse_off[i]++;
      if(pulse_off[i] > frq_cnt[i] && enabled[i] == 1){  // the delay has expired, change to pulse
        pulse_on[i] = 1;
        pulse_off[i] = 0;
      }
    } else {
      pulse_on[i]++;
      if(pulse_holdoff!=0) oldnoteon=1;  // block/trick the pulse entirely if in holdoff mode
      noteon=1;    // we have an active note in pulse mode
      if(pulse_on[i] > pulse_cnt[i]){  // pulse time expired, change to delay mode
        pulse_on[i] = 0;
        pulse_off[i] = 0;
      }
    }
  }
  if(noteon==1 && oldnoteon==0){  // detect rising edge
    digitalWriteFast(pulse_pin, HIGH);
    oldnoteon=1;
  }else if(noteon==0 && oldnoteon==1){  // detect falling edge
    digitalWriteFast(pulse_pin, LOW);
    pulse_holdoff=holdoff_cnt;
    oldnoteon=0;
  }
  if(pulse_holdoff>0) pulse_holdoff--;  // decrement holdoff counter
  digitalWriteFast(srate_pin, LOW);
}

void loop(void) {
  usbMIDI.read();
}

void OnNoteOn (byte channel, byte note, byte velocity) {
  pitchInFrequency = (440 * pow(2,((float)(note - 69)/12)));

  for(int i = 0; i<maxkeys; i++){
    if(enabled[i]==0){
      noInterrupts();
      currentnote[i] = note;
      frq[i] = pitchInFrequency;

      if(frq[i] < frq_min_start){
        pulse[i] = pulse_max;
      }else if(frq[i] > frq_max_start){
        pulse[i] = pulse_min;
      }else{
        pulse[i] = map(frq[i], frq_min_start, frq_max_start, pulse_max, pulse_min);
      }

      pulse_cnt[i] = pulse[i]/srate_us;
      frq_cnt[i] = (srate/frq[i])-pulse_cnt[i];
      if(frq[i] >= frq_min && frq[i] <= frq_max){
        enabled[i] = 1;
      }
      interrupts();
      break;
    }
  }
}

void OnNoteOff(byte channel, byte note, byte velocity) {
  for(int i = 0; i<maxkeys; i++){
    if(currentnote[i] == note){
      enabled[i] = 0;
      currentnote[i] = 0;
    }
  }
}

void OnControlChange(byte channel, byte control, byte value){
  // knobs C10 = 0x16, C11 = 0x17, C12 = 3D, C13 = 17
  if(channel==1){
    if(control==0x16){  // C10
      pulse_min=map(value,0,127,0,maxpulse);
    } else if(control==0x17){ // C11
      pulse_max=map(value,0,127,0,maxpulse);
    }else if(control==0x3D){ // C12
      frq_min_start=map(value,0,127,frq_min,frq_max);
    }else if(control==0x18){  // C13
      frq_max_start=map(value,0,127,frq_min,frq_max);
    } 
  }
}
