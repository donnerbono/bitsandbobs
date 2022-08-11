// adsr envelope example

#include "daisysp.h"
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

// Shortening long macro for sample rate
#ifndef sample_rate

#endif

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)


using namespace daisysp;
using namespace daisy;
using namespace std;

static MidiUsbHandler midi;
static DaisySeed  hw;
Chorus                chorus;
static Adsr       env[6];
static Oscillator osc[6];
static MoogLadder filter;
static BlOsc blosc[6];
bool              mygate [6];
int voices = 6;   // if you want more than 6 voices then change the numbers of set a Voice variable to change for them all
bool filteroff;
	
uint8_t oscnext;
uint8_t notemidi[6];
float notefreq[6];
static ReverbSc   DSY_SDRAM_BSS verb;
float myattack;
float myrelease;
float dry;
float wet;
int oscprimchoice = 0;
int oscsecdchoice = 0;
float oscmix;
float secdmix;
float myvolume = 1;
int myadc4;
int myadc5;
float mychdepth, mychfreq;
float myvelocity[6];
int oscmixdisp = -1;

OledDisplay<SSD130xI2c128x32Driver> display;

std::string osc1screen = "";
std::string osc2screen = "";
const char *char_array = osc1screen.c_str();
const char *char_array2 = osc2screen.c_str();

// declaring end functions here as code confusing if have these at the start
void primaryoscillator(int num1);
void secondaryoscillator(int num2);
void screenupdate();
void setbloscwaveform(uint8_t wave);
void setoscwaveform(uint8_t wave2);
void SynthNoteOn(uint8_t midinote,float midivelocity);
void SynthNoteOff(uint8_t midinote);


static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float sig, env_out, sigwet;
    for( size_t i = 0; i < size; i += 2 ){
        
       for (uint8_t i = 0; i < voices; i++)
        {
        
        // audio stuff for envelope
        env_out = env[i].Process(mygate[i]);
        osc[i].SetAmp(env_out);
        blosc[i].SetAmp(env_out);

        // need to process each of the 6 voices

        // I couldn't get += to work to avoid the if / else

        if (i == 0)
            {
                sig =  ((blosc[0].Process() * oscmix)  + (osc[0].Process() * secdmix)) * myvelocity[0];
            }
        else
            {
                sig = sig + (((blosc[i].Process() * oscmix)  + (osc[i].Process() * secdmix)) * myvelocity[i]);
            } 

        }
       
 
        if (filteroff == false) 
        {
        // turn filter knob up to full it turns filter off so this roughly compensates for volume change
        sig = filter.Process( sig ) * 3;
            hw.SetLed(false);
        }
        else
        {
            hw.SetLed(true);
        
         }
        sig = chorus.Process( sig );
        verb.Process( sig, 0, &sigwet, 0);
       
   
        out[i] = (sig * dry + sigwet * wet) * myvolume;
        out[i + 1] = (sig * dry + sigwet * wet)* myvolume;
    }
}

int main(void)
{
    // initialize seed hardware and daisysp modules
    float sample_rate;
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();
    
          /** Initialize USB Midi 
		 *  by default this is set to use the built in (USB FS) peripheral.
		 * 
		 *  by setting midi_cfg.transport_config.periph = MidiUsbTransport::Config::EXTERNAL
		 *  the USB HS pins can be used (as FS) for MIDI 
		 */
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    //Set envelope parameters
    myrelease = 1;
    myattack = 1;

    for (uint8_t i = 0; i < voices; i++)
	{
		// oscillator, filter and blosciallotor for all the voices.
	
		osc[i].Init(sample_rate);
		osc[i].SetWaveform(osc[i].WAVE_SIN);
		osc[i].SetAmp(0.25f); // default
		osc[i].SetFreq(440.0f); // default

        env[i].Init(sample_rate);
        env[i].SetTime(ADSR_SEG_ATTACK, myattack);
        env[i].SetTime(ADSR_SEG_DECAY, .25);
        env[i].SetTime(ADSR_SEG_RELEASE, myrelease);
        env[i].SetSustainLevel(.625);

        blosc[i].Init( sample_rate );
        blosc[i].SetWaveform(blosc[i].WAVE_SQUARE);
        blosc[i].SetFreq( 440 );
        blosc[i].SetAmp( 0.325 );


    }

    // initialising the non-per voice things.

    filter.Init(sample_rate);
    filter.SetFreq( 880 );
    filter.SetRes( 0.7 );

    chorus.Init(sample_rate);
    chorus.SetLfoFreq(.33f, .2f);
    chorus.SetLfoDepth(1.f, 1.f);
    chorus.SetDelay(.75f, .9f);


    verb.Init(sample_rate);
    verb.SetFeedback(0.95f);
    verb.SetLpFreq(16000.0f);
    // was 18000 if too dark

    /** OLED display configuration obviously this used the SSD130 screen and 4 pins - very cheap!!*/
	OledDisplay<SSD130xI2c128x32Driver>::Config display_config;

	display_config.driver_config.transport_config.i2c_address = 0x3C;
	display_config.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
	display_config.driver_config.transport_config.i2c_config.speed  = I2CHandle::Config::Speed::I2C_100KHZ;
	display_config.driver_config.transport_config.i2c_config.mode = I2CHandle::Config::Mode::I2C_MASTER;
 	display_config.driver_config.transport_config.i2c_config.pin_config.scl = {DSY_GPIOB, 8};
	display_config.driver_config.transport_config.i2c_config.pin_config.sda = {DSY_GPIOB, 9};

	//display_config.driver_config.transport_config.Defaults();
	display.Init(display_config);

    // start callback
    hw.StartAudio(AudioCallback);

    // adc stuff so they're ready to receive knob info
    int numberOfADCChannels = 10;
    AdcChannelConfig adcConfig[ numberOfADCChannels ];
    adcConfig[ 0 ].InitSingle( daisy::seed::A0 );
    adcConfig[ 1 ].InitSingle( daisy::seed::A1 );
    adcConfig[ 2 ].InitSingle( daisy::seed::A2 );
    adcConfig[ 3 ].InitSingle( daisy::seed::A3 );
    adcConfig[ 4 ].InitSingle( daisy::seed::A4 );
    adcConfig[ 5 ].InitSingle( daisy::seed::A5 );
    adcConfig[ 6 ].InitSingle( daisy::seed::A6 );
    adcConfig[ 7 ].InitSingle( daisy::seed::A7 );
    adcConfig[ 8 ].InitSingle( daisy::seed::A8 );
    adcConfig[ 9 ].InitSingle( daisy::seed::A9 );
    
    hw.adc.Init( adcConfig, numberOfADCChannels );
    hw.adc.Start();

    while(1)
     {
              
        /** Listen to MIDI for new changes */
        midi.Listen();

        /** When there are messages waiting in the queue... */
        while(midi.HasEvents())
        {
            /** Pull the oldest one from the list... */
            auto msg = midi.PopEvent();
            switch(msg.type)
            {
                case NoteOn: 
                {
                    /** and get the note on info */
                    auto note_msg = msg.AsNoteOn();
                    

                    if(note_msg.velocity != 0)
                        {
                       // send note on and velocity on to function to handle all the voices stuff
                        SynthNoteOn(note_msg.note, note_msg.velocity);
                        }
                }
                break;
                //
               case NoteOff:
                {
                    
                // turn a note off
                        auto note_msg = msg.AsNoteOn();
                        SynthNoteOff(note_msg.note); 
                }
                break;
                    // Since we only care about note-on messages in this example
                    // we'll ignore all other message types
                default: break;
            }
        }
        // fmap gives the range from knob to values you want to use in one tidy line!
        filter.SetFreq( fmap(
            hw.adc.GetFloat( 1 ),
            50.0,
            8000.0
        ) );
        // rather than add a switch the filter just goes into a filter off 
        if ((hw.adc.GetFloat (1)) > 0.98)
        {
            filteroff = true;
        }
        else 
        {
            filteroff = false;
        }
        
         // handling attack and release only as there's only so many knobs   
       myattack = (fmap(
           hw.adc.GetFloat( 2 ),
           0.1,
           2)
        );

        myrelease = (fmap(
           hw.adc.GetFloat( 3),
          0.1,
          3) 
       );
       // have to update values for every voice. 
        for (uint8_t i = 0; i < voices; i++)
	{ 
        env[i].SetTime(ADSR_SEG_ATTACK, myattack);
        env[i].SetTime(ADSR_SEG_RELEASE, myrelease);
    }
       
       // reverb knob just goes dry / off to wet / full on
         wet = (fmap(
           hw.adc.GetFloat( 0),
          0,
          1) 
        );
        dry = (-1 + wet) * -1;
        // using knobs to select waveforms for oscillators
        myadc4 = int(fmap(
           hw.adc.GetFloat( 4 ),
           0,
           2.3)
        );
        // if statements are to stop knobs continually giving values (as they do) and the screen updating lots 
        // as it steals all the processer time. only sends to screen if new waveform selected
        if (myadc4 != oscprimchoice) {
            oscprimchoice = myadc4;
            primaryoscillator(int(oscprimchoice));
        }

        myadc5 = int(fmap(
           hw.adc.GetFloat( 5 ),
           0,
           4.45)
        );
        if (myadc5 != oscsecdchoice) {
            oscsecdchoice = myadc5;
            secondaryoscillator(int(oscsecdchoice));
        }
        // mixing between the 2
        oscmix = (fmap(
           hw.adc.GetFloat( 6 ),
           0,
           1)
        );
        if (oscmixdisp != int(oscmix * 100)) {
            oscmixdisp = int(oscmix * 100);
            screenupdate();

        }
        secdmix = (-1 + oscmix)* -1;
        // overall volume
        myvolume = (fmap(
           hw.adc.GetFloat( 7 ),
           0,
           1)
        );
        // chorus stuff
        mychdepth = (fmap(
           hw.adc.GetFloat( 8 ),
           0,
           1)
        );
        mychfreq = (fmap(
            hw.adc.GetFloat( 9 ),
           0,
           1)
        );
        chorus.SetLfoFreq(mychfreq, (mychfreq * 0.9));
        chorus.SetLfoDepth(mychdepth, mychdepth); 
     } 
}
// credit to Staffan Melin. https://www.oscillator.se/opensource/  oscnext gives the next free voice

 void SynthNoteOn(uint8_t midinote, float midivelocity)

{

oscnext = (oscnext + 1) % voices;
notemidi[oscnext] = midinote;
notefreq[oscnext] = mtof(notemidi[oscnext]);
mygate[oscnext] = true;
osc[oscnext].SetFreq(notefreq[oscnext]);
blosc[oscnext].SetFreq(notefreq[oscnext]);
myvelocity[oscnext] = midivelocity / 100;
}


void SynthNoteOff(uint8_t midinote)
{
    for (uint8_t i = 0; i < voices; i++)
	{
		if (notemidi[i] == midinote)
		{
			notemidi[i] = 0;
            mygate[i] = false;
		}
	}

}

// I got up to switches in my c++ learning so used them to manage waveform changes! Time to go back to the books...

void primaryoscillator(int num1)
    {
    
   switch (num1) {
    
    case 0:
    // primary.SetWaveform( primary.WAVE_SQUARE);
    setbloscwaveform(0);
    osc1screen = "Triangle";
    screenupdate();
    break; 

    case 1:
    // primary.SetWaveform( primary.WAVE_TRIANGLE);
    setbloscwaveform(1);
    osc1screen = "Saw";
    screenupdate();
    break;
    
    case 2:
    // primary.SetWaveform( primary.WAVE_SAW);
    setbloscwaveform(2);
    osc1screen = "Square";
    screenupdate();
    break;
    }
    }
void secondaryoscillator(int num2)
    {
    switch (num2) {
    case 0:
    // osc.SetWaveform( osc.WAVE_SIN);
    setoscwaveform(0);
    osc2screen = "Sine";
    screenupdate();
    break;

    case 1:
    // osc.SetWaveform( osc.WAVE_TRI);
    setoscwaveform(1);
    osc2screen = "Triangle";
    screenupdate();
    break;
    
    case 2:
    // osc.SetWaveform( osc.WAVE_SAW);
    setoscwaveform(2);
    osc2screen = "Saw";
    screenupdate();
    break;

    case 3:
    //osc.SetWaveform( osc.WAVE_RAMP);
    setoscwaveform(3);
    osc2screen = "Ramp";
    screenupdate();
    break;

    case 4:
    // osc.SetWaveform( osc.WAVE_SQUARE);
    setoscwaveform(4);
    osc2screen = "Square";
    screenupdate();
    break;

   
    }
    }

//https://forum.electro-smith.com/t/ssd1306-i2c-oled-doesnt-work/2339/10 shows how to edit ssd1306 if your 0,0 pixel is in the middle of screen
// edited to add variable but is messy and needs taking out of this cpp
void screenupdate() {

        display.Fill(false);
        std::string str = "Blosc:" + osc1screen + " ";
        char * cstr = &str[0];
        display.SetCursor(2, 2);
        display.WriteString(cstr,Font_7x10,true);
        std::string strvar1 = std::to_string(static_cast<uint32_t>(oscmixdisp));
        char * cstrvar1 = &strvar1[0];
        display.SetCursor(106, 2);
        display.WriteString(cstrvar1,Font_7x10,true);


        int secdmixdisp = int (secdmix * 100);
        std::string str2 = "Osc:  " + osc2screen + " ";
        char * cstr2 = &str2[0];
        display.SetCursor(2, 17);
        display.WriteString(cstr2,Font_7x10,true);
        std::string strvar2 = std::to_string(static_cast<uint32_t>(secdmixdisp));
        char * cstrvar2 = &strvar2[0];
        display.SetCursor(106, 17);
        display.WriteString(cstrvar2,Font_7x10,true);

        display.Update();


/* working code!
        display.SetCursor(10, 2);
		display.WriteString(char_array, Font_7x10, false);
		display.SetCursor(10, 17);
		display.WriteString(char_array2, Font_7x10, false);
        display.Update();
*/
}

       
// another array for i - to set the oscillator per voice
void setbloscwaveform(uint8_t wave) {
      for (uint8_t i = 0; i < voices; i++)
        {
         blosc[i].SetWaveform(wave);
        }

// enum  	Waveforms { WAVE_TRIANGLE , WAVE_SAW , WAVE_SQUARE , WAVE_OFF }
}
 

void setoscwaveform(uint8_t wave2) {
      for (uint8_t i = 0; i < voices; i++)
        {
         osc[i].SetWaveform(wave2);
        }

/*
  WAVE_SIN , WAVE_TRI , WAVE_SAW , WAVE_RAMP ,
  WAVE_SQUARE , WAVE_POLYBLEP_TRI , WAVE_POLYBLEP_SAW , WAVE_POLYBLEP_SQUARE ,
  WAVE_LAST
*/
}