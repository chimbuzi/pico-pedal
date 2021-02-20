
/**
 * Copyright (c) 2021 S R Thompson
 *
 * GPLv3
 */


#include "pico/stdlib.h"
#include <hardware/pwm.h>
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "hardware/regs/sio.h" //SIO_CPUID will give core idx
#include "hardware/irq.h"


#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

#define SPI_PORT spi0
#define READ_BIT 0x80


#define US_PER_SAMPLE 10000/220


uint8_t mosi[2];
uint8_t miso[2];
static uint32_t input_signal = 0;
static uint32_t output_signal = 0;
bool write_status = 0;
bool old_write_status = 0;


void core_1_entry(void);
uint16_t process_sample(uint16_t);
bool flip_status(repeating_timer_t *rt);

static inline void cs_select() {
    /*
    | Toggle the CS low (active)
    */
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    /*
    | Toggle the CS high (inactive)
    */
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1);
    asm volatile("nop \n nop \n nop");
}


bool flip_status(repeating_timer_t *rt){
    /*
    | Handy callback function to allow an interrupt to trigger sample output
    | Just flips a bit so a conditional matches
    | Could probably do the I/O from inside here if I could be bothered but
    | that would be a scoping headache.
    */
    write_status = !(write_status & 0x1);
    return true;
}

void on_pwm_wrap_pwm0(){
    pwm_clear_irq(pwm_gpio_to_slice_num(0));
    pwm_set_gpio_level(0, (output_signal >> 6) + (output_signal & 0x3F));
}

int main(){
    // set GPIO_0 and GPIO_1 to be PWM pins
    gpio_set_function(0, GPIO_FUNC_PWM);
    //gpio_set_function(1, GPIO_FUNC_PWM);

    // get the allocated slice numbers
    uint slice_num_pwm0 = pwm_gpio_to_slice_num(0);
    //uint slice_num_pwm1 = pwm_gpio_to_slice_num(1);

    //set PWM pwm_clear_irq
    pwm_clear_irq(slice_num_pwm0);
    pwm_set_irq_enabled(slice_num_pwm0, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap_pwm0);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.f); //half system frequency
    pwm_init(slice_num_pwm0, &config, true);
    
    //set wrap (what does this do?)
    pwm_set_wrap(slice_num_pwm0, 2000);
    //pwm_set_wrap(slice_num_pwm1, 3);
    
    //enable pwm
    //pwm_set_enabled(slice_num_pwm0, true);
    //pwm_set_enabled(slice_num_pwm1, true);
    
    // set the clock div
    //pwm_set_clkdiv(slice_num_pwm1, 4.f);

    //TODO: set PWM mode, range.

    // set the PWM running
    //pwm_config config = pwm_get_default_config();
    //pwm_init(slice_num_pwm0, &config, true);
    //pwm_init(slice_num_pwm1, &config, true);


    //set SPI up
    //frequency
    spi_init(SPI_PORT, 4000*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // chip select
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // set up the mosi buffer to request samples
    mosi[0] = 128;
    mosi[1] = 0x00;
    // and initialise the miso buffer to zero
    miso[0] = 0;

    // set up a timer to trigger writes and reads
    repeating_timer_t timer;
    add_repeating_timer_us((int64_t)US_PER_SAMPLE*(-1), flip_status, NULL, &timer); //negative delay gives spaced starts rather than spaces between

    // Spin up core 1
    multicore_launch_core1(core_1_entry);



    while(1){
        // get new data from SPI
        cs_select(); //should be implicit in r/w calls...
        spi_write_read_blocking(SPI_PORT, mosi, miso, 10);
        cs_deselect();

        // decode input from ADC
        //input_signal = miso[2] + ((miso[1] & 0x0F) << 8);
        input_signal = (miso[0] << 7) + (miso[1] >> 1);


        //do data processing. NB swapped fifo order on purpose to ensure good interleaving
        // fifo structure guarantees thread safety - two implicit fifos, one in each direction.
        //pop next sample to output
        output_signal = multicore_fifo_pop_blocking();


        // push this into the fifo
        multicore_fifo_push_blocking(input_signal);


        // wait for timeout here; maximise available processing time
        // may result in glitch on first iteration
        while (write_status == old_write_status){
            asm volatile ("nop");
        }
        old_write_status = write_status;


        // write the output to PWM
        //pwm_set_gpio_level(0, output_signal & 0x3F);
        //pwm_set_gpio_level(1, output_signal >> 6);
    }


return 0;
}


void core_1_entry(){
    /*
    | Entry-point for second core to run processing task
    */
    uint16_t unprocessed_sample;
    uint16_t processed_sample;
    // output a zero buffer first time around. This also serves as a sync
    multicore_fifo_push_blocking(0);

    // then loop
    while(1){
        // waiting for first data
        unprocessed_sample = multicore_fifo_pop_blocking();

        // then process the data
        processed_sample = process_sample(unprocessed_sample);

        // then return it
        multicore_fifo_push_blocking(processed_sample);
    }
}



uint16_t process_sample(uint16_t input_sample){
    /*
    | Do whatever processing is supposed to be happening.
    | TODO: something clever with function pointers and many FX
    | May require more memory than target has unless allocate stack-space
    | but we probably don't have enough stack space.
    */
    return input_sample; //do nothing for now..
}
