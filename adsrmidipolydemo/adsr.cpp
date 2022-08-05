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

static MidiUsbHandler midi;
static DaisySeed  hw;
static Adsr       env;
static Oscillator osc;
static MoogLadder filter;
static BlOsc primary;
bool              mygate;
static ReverbSc   verb;
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

OledDisplay<SSD130xI2c128x32Driver> display;

std::string osc1screen = "Start";
std::string osc2screen = "2ndStart";
const char *char_array = osc1screen.c_str();
const char *char_array2 = osc2screen.c_str();

// declaring end functions here as code confusing if have these at the start
void primaryoscillator(int num1);
void secondaryoscillator(int num2);
void screenupdate();

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
   // float osc_out, env_out;
  //  for(size_t i = 0; i < size; i += 2)
  //  {
        

        // Use envelope to control the amplitude of the oscillator.
   //     env_out = env.Process(mygate);
        
   //     osc.SetAmp(env_out);
   //     osc_out = filter.Process(osc.Process());
        
   //     verb.Process(osc_out, osc_out, &out[i], &out[i + 1]);

    float sig, env_out, sigwet;
    for( size_t i = 0; i < size; i += 2 ){
        env_out = env.Process(mygate);
        osc.SetAmp(env_out);
        primary.SetAmp(env_out);

       // float lfoState = osc.Process();
       // primary.SetPw( ( lfoState * 0.3 ) + 0.5 );

      //  sig = (((primary.Process() * oscmix)  + (osc.Process() * secdmix)) * myvolume);
        sig = ((primary.Process() * oscmix)  + (osc.Process() * secdmix));
      //  sig = primary.Process();
        sig = filter.Process( sig );
        verb.Process( sig, 0, &sigwet, 0);
       
        //verb.Process((sig * dry + sigwet * wet), (sig * dry + sigwet * wet), &out[i], &out[i + 1]);
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
    
    env.Init(sample_rate);
    osc.Init(sample_rate);

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
    env.SetTime(ADSR_SEG_ATTACK, myattack);
    env.SetTime(ADSR_SEG_DECAY, .25);
    env.SetTime(ADSR_SEG_RELEASE, myrelease);

    env.SetSustainLevel(.5);

    primary.Init( sample_rate );
    primary.SetWaveform( primary.WAVE_SQUARE);
    primary.SetFreq( 440 );
    primary.SetAmp( 0.325 ); 
    // Set parameters for oscillator
    osc.SetWaveform(osc.WAVE_SIN);
    osc.SetFreq(330);
    osc.SetAmp(0.325);

    filter.Init(sample_rate);
    filter.SetFreq( 880 );
    filter.SetRes( 0.7 );

//setup reverb
    verb.Init(sample_rate);
    verb.SetFeedback(0.9f);
    verb.SetLpFreq(13000.0f);
    // was 18000 if too dark and 0.9 for feedback if too weedy

    /** OLED display configuration */
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

    // adc stuff
    int numberOfADCChannels = 8;
    AdcChannelConfig adcConfig[ numberOfADCChannels ];
    adcConfig[ 0 ].InitSingle( daisy::seed::A0 );
    adcConfig[ 1 ].InitSingle( daisy::seed::A1 );
    adcConfig[ 2 ].InitSingle( daisy::seed::A2 );
    adcConfig[ 3 ].InitSingle( daisy::seed::A3 );
    adcConfig[ 4 ].InitSingle( daisy::seed::A4 );
    adcConfig[ 5 ].InitSingle( daisy::seed::A5 );
    adcConfig[ 6 ].InitSingle( daisy::seed::A6 );
    adcConfig[ 7 ].InitSingle( daisy::seed::A7 );
    
    
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
                    /** and change the frequency of the oscillator */
                    auto note_msg = msg.AsNoteOn();

                    if(note_msg.velocity != 0)
                        {
                        mygate = true;
                        osc.SetFreq(mtof(note_msg.note));
                        primary.SetFreq(mtof(note_msg.note));
                
                        }
                        
                    
                 
                }
                break;
                //
               case NoteOff:
                {
                 mygate = false;
                }
                break;
                    // Since we only care about note-on messages in this example
                    // we'll ignore all other message types
                default: break;
            }
        }
        filter.SetFreq( fmap(
            hw.adc.GetFloat( 1 ),
            50.0,
            2000.0
        ) );
        // get rid of this and only use the dry wet when need knobs!
       /* verb.SetFeedback( fmap(
            hw.adc.GetFloat( 0 ),
            0.0,
            0.95
        ) ); */
      
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
        env.SetTime(ADSR_SEG_ATTACK, myattack);
        env.SetTime(ADSR_SEG_RELEASE, myrelease);

         wet = (fmap(
           hw.adc.GetFloat( 0),
          0,
          1) 
        );
        dry = (-1 + wet) * -1;

        myadc4 = int(fmap(
           hw.adc.GetFloat( 4 ),
           0,
           2.3)
        );
        
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
    
        oscmix = (fmap(
           hw.adc.GetFloat( 6 ),
           0,
           1)
        );
        secdmix = (-1 + oscmix)* -1;
        
        myvolume = (fmap(
           hw.adc.GetFloat( 7 ),
           0,
           1)
        );
        
     } 
}

void primaryoscillator(int num1)
    {
    
   switch (num1) {
    
    case 0:
    primary.SetWaveform( primary.WAVE_SQUARE);
    osc1screen = "Square";
    screenupdate();
    break;

    case 1:
    primary.SetWaveform( primary.WAVE_TRIANGLE);
    osc1screen = "Triangle";
    screenupdate();
    break;
    
    case 2:
    primary.SetWaveform( primary.WAVE_SAW);
    osc1screen = "Saw";
    screenupdate();
    break;
    }
    }
void secondaryoscillator(int num2)
    {
    switch (num2) {
    case 0:
    osc.SetWaveform( osc.WAVE_SIN);
    osc2screen = "Sine";
    screenupdate();
    break;

    case 1:
    osc.SetWaveform( osc.WAVE_TRI);
    osc2screen = "Triangle";
    screenupdate();
    break;
    
    case 2:
    osc.SetWaveform( osc.WAVE_SAW);
    osc2screen = "Saw";
    screenupdate();
    break;

    case 3:
    osc.SetWaveform( osc.WAVE_RAMP);
    osc2screen = "Ramp";
    screenupdate();
    break;

    case 4:
    osc.SetWaveform( osc.WAVE_SQUARE);
    osc2screen = "Square";
    screenupdate();
    break;

   
    }
    }
void screenupdate() {

    display.Fill(true);
		display.SetCursor(0, 2);
		display.WriteString(char_array, Font_7x10, false);
		display.SetCursor(0, 17);
		display.WriteString(char_array2, Font_7x10, false);
        display.Update();
        
}
