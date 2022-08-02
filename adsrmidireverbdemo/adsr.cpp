// adsr envelope example

#include "daisysp.h"
#include "daisy_seed.h"

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

    float sig, env_out;
    for( size_t i = 0; i < size; i += 2 ){
        env_out = env.Process(mygate);
        osc.SetAmp(env_out);
        primary.SetAmp(env_out);

        float lfoState = osc.Process();
        primary.SetPw( ( lfoState * 0.3 ) + 0.5 );
        sig = primary.Process();
        sig = filter.Process( sig );
        verb.Process(sig, sig, &out[i], &out[i + 1]);
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
    osc.SetAmp(0.15);

    filter.Init(sample_rate);
    filter.SetFreq( 880 );
    filter.SetRes( 0.7 );

//setup reverb
    verb.Init(sample_rate);
    verb.SetFeedback(0.9f);
    verb.SetLpFreq(18000.0f);
    

    // start callback
    hw.StartAudio(AudioCallback);

    // adc stuff
    int numberOfADCChannels = 4;
    AdcChannelConfig adcConfig[ numberOfADCChannels ];
    adcConfig[ 0 ].InitSingle( daisy::seed::A0 );
    adcConfig[ 1 ].InitSingle( daisy::seed::A1 );
    adcConfig[ 2 ].InitSingle( daisy::seed::A2 );
    adcConfig[ 3 ].InitSingle( daisy::seed::A3 );
    
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
                 hw.SetLed(true);
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
        verb.SetFeedback( fmap(
            hw.adc.GetFloat( 0 ),
            0.0,
            0.95
        ) ); 
      
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
    }
}
