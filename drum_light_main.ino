/***
 * =================
 * Second Prototype
 * =================
 */

/*
 * MIDI Library and setup
 */
#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

/*
 * Library and Config for the PWM output boards
 */
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm_0 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm_1 = Adafruit_PWMServoDriver(0x41);
unsigned short NPINS = 32;

float set_pin(int pin, int value)
/*
 * This function capsules the fact that we use two PWM boards with 16 pins each,
 * so that we can set pins from 0 to 31.
 */
{
    if (pin <= 15)
        pwm_0.setPin(pin, value);
    if (pin >= 16)
        pwm_1.setPin(pin - 16, value);
}

/**
 * Parameters which define
 * a) how many process cycles the light should be toggled on and
 * b) how many cycles before switch-off, the fadeout should start.
 */
const unsigned long on_cycles_max = 300;
const unsigned long fadeout_length = 270;

/*
 * Should a new color be assigned on every hit (true)
 * or only, when the current color has completely faded (false)
 */
bool QUICK_COLOR_ASSIGNMENT = true;

/**
 * These are the colors to be used. Completely random colors would not look nice.
 *
 * It includes off/black as the first element for convenience. During the note triggering
 * black is never chosen (random choice starts at index 1)
 */
#define NCOLORS 8
static unsigned short my_colors[NCOLORS][3] = {
    {0, 0, 0},
    {0, 0, 4095},
    {0, 4095, 0},
    {4095, 0, 0},
    {4095, 4095, 0},
    {4095, 0, 4095},
    {0, 4095, 4095},
    {4095, 4095, 4095}};

struct channel
/**
 * Channel struct to hold all relevant information about each channels.
 */
{
    const unsigned short channel_id;
    const unsigned short red_pin_number;
    const unsigned short green_pin_number;
    const unsigned short blue_pin_number;
    unsigned short red_component;
    unsigned short green_component;
    unsigned short blue_component;
    unsigned long on_cycles_remaining;
};

/**
 * Only one channel during development.
 * TODO: Add and configure one channel for every trigger
 */
#define NCHANNELS 1
struct channel tom_1 = {4, 0, 1, 2, 0, 0, 0, 0};

/**
 * Collect all channels in one array for convenient looping later on.
 */
struct channel channels[NCHANNELS] = {
    tom_1};

int get_channel_id_from_midi_note(byte note)
/**
 * Multiple MIDI notes belong to one output channel (e.g. center and rim of a tom)
 * The mapping of MIDI-input notes to a channel ID is done here.
 */

{
    // HiHat
    if ((note == 0x16) || (note == 0x1A) || (note == 0x2A) || (note == 0x2C) || (note == 0x2E))
        return 0;

    // Kick
    if (note == 0x24)
        return 1;

    // Snare
    if ((note == 0x25) || (note == 0x26) || (note == 0x28))
        return 2;

    // Ride
    if ((note == 0x33) || (note == 0x35) || (note == 0x3B))
        return 3;

    // Tom 1:
    if ((note == 0x30) || (note == 0x32))
        return 4;

    // Tom 2:
    if ((note == 0x2D) || (note == 0x2F))
        return 5;

    // Tom 3:
    if ((note == 0x2B) || (note == 0x3A))
        return 6;

    // Tom 4:
    if ((note == 0x1B) || (note == 0x1C))
        return 7;

    // Crash 1:
    if ((note == 0x31) || (note == 0x37))
        return 8;

    // Crash 2:
    if ((note == 0x34) || (note == 0x39))
        return 9;

    return -1;
}

void setup()
{
    /**
     * Setup the MIDI shield and the PWM driver boards
     */
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.setHandleNoteOn(NoteOnHandle);

    pwm_0.begin();
    pwm_1.begin();
    pwm_0.setPWMFreq(1600);
    pwm_1.setPWMFreq(1600);

    // Switch all Pins (= all lights) OFF initially
    for (int pin = 0; pin < NPINS; pin++)
    {
        set_pin(pin, 0);
    }
}

float get_fading_factor(struct channel chan)
/**
 * Get the light fading factor for analog Write.
 * If the fadeout time has not yet started, give full brightness (1.0).
 * During fadeout, reduce linearly from 1.0 to 0.
 */
{
    if (chan.on_cycles_remaining > fadeout_length)
    {
        return 1.0;
    }
    return chan.on_cycles_remaining / (1.0 * fadeout_length);
}

void NoteOnHandle(byte channel, byte pitch, byte velocity)
/**
 * Main functionality: What happens, when a midi note is detected
 */
{
    short int current_output_channel = get_channel_id_from_midi_note(pitch);

    for (int i = 0; i < NCHANNELS; i++)
    {
        /**
         * When a channel is triggered ...
         */
        if (channels[i].channel_id == current_output_channel)
        {
            /**
             *  ... start the cycle countdown ...
             */
            channels[i].on_cycles_remaining = on_cycles_max;
            /**
             * ... but only assign a color, when none was assigned yet.
             */
            if (QUICK_COLOR_ASSIGNMENT || ((channels[i].red_component + channels[i].green_component + channels[i].blue_component) == 0))
            {
                unsigned short current_color_index = random(1, NCOLORS - 1);
                channels[i].red_component = my_colors[current_color_index][0];
                channels[i].green_component = my_colors[current_color_index][1];
                channels[i].blue_component = my_colors[current_color_index][2];
            }
        }
    }
}

void loop()
/**
 * Main loop where everything comes together
 */
{
    MIDI.read(); // This calls the NoteOnHandle when appropriate

    for (int i = 0; i < NCHANNELS; i++)
    {
        /**
         * When the cycle countdown has not reached 0 yet ...
         */
        if (channels[i].on_cycles_remaining > 0)
        {
            /**
             *  ... write the appropriate brightness ...
             */
            float fading_factor = get_fading_factor(channels[i]);
            // float fading_factor = 1.0;
            set_pin(channels[i].red_pin_number, int(fading_factor * channels[i].red_component));
            set_pin(channels[i].green_pin_number, int(fading_factor * channels[i].green_component));
            set_pin(channels[i].blue_pin_number, int(fading_factor * channels[i].blue_component));
            /**
             * ... and reduce the counter by one
             */
            channels[i].on_cycles_remaining -= 1;
        }
        else
        {
            /**
             * Switch off when the cycle countdown has elapsed.
             * INVESTIGATE: This might be superfluous, as <fading_factor> will be 0.0, thus setting the pins to 0.
             */
            set_pin(channels[i].red_pin_number, 0);
            set_pin(channels[i].green_pin_number, 0);
            set_pin(channels[i].blue_pin_number, 0);
            /**
             * And reset the color to 0, so that a new color is assigned during the next initial trigger
             */
            channels[i].red_component = 0;
            channels[i].green_component = 0;
            channels[i].blue_component = 0;
        }
    }
}