/***
 * =================
 * Third Prototype
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

class Color;
class Channel;

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

class Color
/**
 * @brief RGB-Color Class
 *
 * Abstraction for easy color definition and setting.
 *
 * Default constructor gives black (all components 0)
 *
 */
{
public:
    Color()
    {
        red_component = 0;
        green_component = 0;
        blue_component = 0;
    }
    Color(unsigned short in_red_component, unsigned short in_green_component, unsigned short in_blue_component)
    {
        red_component = in_red_component;
        green_component = in_green_component;
        blue_component = in_blue_component;
    }
    int red_component;
    int green_component;
    int blue_component;
};

/**
 * These are the colors to be used.
 *
 * It includes off/black as the first element for convenience. During the note triggering
 * black is never actually chosen (random choice starts at index 1)
 */

Color get_random_color()
/**
 * @brief Get a random color from a pre-defined set of colors
 * Completely random colors would not look nice.
 *
 */
{
    unsigned short NCOLORS = 7;
    Color my_colors[NCOLORS] = {
        Color(0, 0, 4095),
        Color(0, 4095, 0),
        Color(4095, 0, 0),
        Color(4095, 4095, 0),
        Color(4095, 0, 4095),
        Color(0, 4095, 4095),
        Color(4095, 4095, 4095)};

    unsigned short current_color_index = random(0, NCOLORS - 1);
    return my_colors[current_color_index];
};

class Channel
{
public:
    Color current_color;
    bool valid_channel = true;

    Channel(
        unsigned short in_r_pin,
        unsigned short in_g_pin,
        unsigned short in_b_pin,
        unsigned short in_on_cycles = 300,
        unsigned short in_fadecout_cycles = 270)
    {
        pins[0] = in_r_pin;
        pins[1] = in_g_pin;
        pins[2] = in_b_pin;
        on_cycles = in_on_cycles;
        fadeout_cycles = in_fadecout_cycles;
    };

    void Channel::trigger()
    {
        // Only assign a new color, if the old one has completely faded
        // OR if QUICK_COLOR_ASSIGNMENT is true.
        if (remaining_cycles == 0 || QUICK_COLOR_ASSIGNMENT)
        {
            current_color = get_random_color();
        }
        // Restart the counter
        remaining_cycles = on_cycles;

        // Progress for full brightness write to analog
        progress();
    }

    float Channel::get_fadeout_factor()
    /**
     * @brief Get the light fading factor for analog Write.
     *
     * If the fadeout time has not yet started, give full brightness (1.0).
     * During fadeout, reduce linearly from 1.0 to 0.
     */
    {
        if (remaining_cycles > fadeout_cycles)
            return 1.0;
        else
            return remaining_cycles / (1.0 * fadeout_cycles);
    }

    void Channel::progress()
    /**
     * @brief Counts down the cycles and fades out the light
     *
     */
    {
        float fadeout_factor = get_fadeout_factor();

        current_color.red_component *= fadeout_factor;
        current_color.green_component *= fadeout_factor;
        current_color.blue_component *= fadeout_factor;

        write_color_to_analog_out();
        remaining_cycles--;
    }

    void Channel::write_color_to_analog_out()
    {
        set_pin(pins[0], current_color.red_component);
        set_pin(pins[1], current_color.green_component);
        set_pin(pins[2], current_color.blue_component);
    }

private:
    unsigned short pins[3];
    unsigned short on_cycles;
    unsigned short fadeout_cycles;
    unsigned short remaining_cycles;
};

/**
 * Only one channel during development.
 * TODO: Add and configure one channel for every trigger
 */
#define NCHANNELS 1
Channel tom_1(0, 1, 2);

/**
 * Collect all channels in one array for convenient looping later on.
 */
Channel channels[NCHANNELS] = {
    tom_1};

Channel get_channel_from_midi_note(byte note)
{
    if ((note == 0x30) || (note == 0x32))
        return tom_1;

    // In case channel is not found, return replacement value
    Channel notFound(-1, -1, -1, -1);
    notFound.valid_channel = false;
    return notFound;
}

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

void NoteOnHandle(byte channel, byte pitch, byte velocity)
/**
 * Main functionality: What happens, when a midi note is detected
 */
{
    Channel current_output_channel = get_channel_from_midi_note(pitch);

    // If no channel for the mido note was found, return (i.e. do nothing)
    if (current_output_channel.valid_channel == false)
    {
        return;
    }
    current_output_channel.trigger();
}

void loop()
{
    MIDI.read(); // This calls the NoteOnHandle when appropriate
    for (int i = 0; i < NCHANNELS; i++)
    {
        channels[i].progress();
    }
}