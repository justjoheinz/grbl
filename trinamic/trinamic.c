#include "trinamic.h"
#include "spi/spi.h"

//some default values used in initialization
#define DEFAULT_MICROSTEPPING_VALUE 32

//TMC26X register definitions
#define DRIVER_CONTROL_REGISTER 0x0ul
#define CHOPPER_CONFIG_REGISTER 0x80000ul
#define COOL_STEP_REGISTER  0xA0000ul
#define STALL_GUARD2_LOAD_MEASURE_REGISTER 0xC0000ul
#define DRIVER_CONFIG_REGISTER 0xE0000ul

#define REGISTER_BIT_PATTERN 0xFFFFFul

//definitions for the driver control register
#define MICROSTEPPING_PATTERN 0xFul
#define STEP_INTERPOLATION 0x200ul
#define DOUBLE_EDGE_STEP 0x100ul
#define VSENSE 0x40ul
#define READ_MICROSTEP_POSTION 0x0ul
#define READ_STALL_GUARD_READING 0x10ul
#define READ_STALL_GUARD_AND_COOL_STEP 0x20ul
#define READ_SELECTION_PATTERN 0x30ul

//definitions for the chopper config register
#define CHOPPER_MODE_STANDARD 0x0ul
#define CHOPPER_MODE_T_OFF_FAST_DECAY 0x4000ul
#define T_OFF_PATTERN 0xful
#define RANDOM_TOFF_TIME 0x2000ul
#define BLANK_TIMING_PATTERN 0x18000ul
#define BLANK_TIMING_SHIFT 15
#define HYSTERESIS_DECREMENT_PATTERN 0x1800ul
#define HYSTERESIS_DECREMENT_SHIFT 11
#define HYSTERESIS_LOW_VALUE_PATTERN 0x780ul
#define HYSTERESIS_LOW_SHIFT 7
#define HYSTERESIS_START_VALUE_PATTERN 0x78ul
#define HYSTERESIS_START_VALUE_SHIFT 4
#define T_OFF_TIMING_PATERN 0xFul

//definitions for cool step register
#define MINIMUM_CURRENT_FOURTH 0x8000ul
#define CURRENT_DOWN_STEP_SPEED_PATTERN 0x6000ul
#define SE_MAX_PATTERN 0xF00ul
#define SE_CURRENT_STEP_WIDTH_PATTERN 0x60ul
#define SE_MIN_PATTERN 0xful

//definitions for stall guard2 current register
#define STALL_GUARD_FILTER_ENABLED 0x10000ul
#define STALL_GUARD_TRESHHOLD_VALUE_PATTERN 0x17F00ul
#define CURRENT_SCALING_PATTERN 0x1Ful
#define STALL_GUARD_CONFIG_PATTERN 0x17F00ul
#define STALL_GUARD_VALUE_PATTERN 0x7F00ul

//definitions for the input from the TCM260
#define STATUS_STALL_GUARD_STATUS 0x1ul
#define STATUS_OVER_TEMPERATURE_SHUTDOWN 0x2ul
#define STATUS_OVER_TEMPERATURE_WARNING 0x4ul
#define STATUS_SHORT_TO_GROUND_A 0x8ul
#define STATUS_SHORT_TO_GROUND_B 0x10ul
#define STATUS_OPEN_LOAD_A 0x20ul
#define STATUS_OPEN_LOAD_B 0x40ul
#define STATUS_STAND_STILL 0x80ul
#define READOUT_VALUE_PATTERN 0xFFC00ul

//default values
#define INITIAL_MICROSTEPPING 0x3ul //32th microstepping

//debuging output
//#define DEBUG

void TMC26XStepper_init(int number_of_steps, int cs_pin, int dir_pin, int step_pin, unsigned int current, unsigned int resistor, tos100 *tos100)
{
  //by default cool step is not enabled
  tos100->cool_step_enabled=false;
        
  //save the pins for later use
  tos100->cs_pin=cs_pin;
  tos100->dir_pin=dir_pin;
  tos100->step_pin = step_pin;
    
  //store the current sense resistor value for later use
  tos100->resistor = resistor;
        
  //initizalize our status values
  tos100->steps_left = 0;
  tos100->direction = 0;
        
  //initialize register values
  tos100->driver_control_register_value=DRIVER_CONTROL_REGISTER | INITIAL_MICROSTEPPING;
  tos100->chopper_config_register=CHOPPER_CONFIG_REGISTER;
        
  //setting the default register values
  tos100->driver_control_register_value=DRIVER_CONTROL_REGISTER|INITIAL_MICROSTEPPING;
  tos100->microsteps = (1 << INITIAL_MICROSTEPPING);
  tos100->chopper_config_register=CHOPPER_CONFIG_REGISTER;
  tos100->cool_step_register_value=COOL_STEP_REGISTER;
  tos100->stall_guard2_current_register_value=STALL_GUARD2_LOAD_MEASURE_REGISTER;
  tos100->driver_configuration_register_value = DRIVER_CONFIG_REGISTER | READ_STALL_GUARD_READING;

  //set the current
  setCurrent(current);
  //set to a conservative start value
  setConstantOffTimeChopper(7, 54, 13,12,1);
  //set a nice microstepping value
  setMicrosteps(DEFAULT_MICROSTEPPING_VALUE);
  //save the number of steps
  tos100->number_of_steps =   number_of_steps;

  TMC26XStepper_start(tos100);
}

/*
 * start & configure the stepper driver
 * just must be called.
 */
void TMC26XStepper_start(tos100 *tos100) {
  //set the pins as output & its initial value
  pinMode(tos100->step_pin, OUTPUT);     
  pinMode(tos100->dir_pin, OUTPUT);     
  pinMode(tos100->cs_pin, OUTPUT);     
  digitalWrite(tos100->step_pin, LOW);     
  digitalWrite(tos100->dir_pin, LOW);     
  digitalWrite(tos100->cs_pin, HIGH);   
        
  //configure the SPI interface
  spi_setBitOrder(MSBFIRST);
  spi_setClockDivider(SPI_CLOCK_DIV8);
  //todo this does not work reliably - find a way to foolprof set it (e.g. while communicating
  //spi_setDataMode(SPI_MODE3);
  spi_begin();
                
  //set the initial values
  TMC26XStepper_send262(tos100->driver_control_register_value, tos100); 
  TMC26XStepper_send262(tos100->chopper_config_register, tos100);
  TMC26XStepper_send262(tos100->cool_step_register_value, tos100);
  TMC26XStepper_send262(tos100->stall_guard2_current_register_value, tos100);
  TMC26XStepper_send262(tos100->driver_configuration_register_value, tos100);
}

/*
 * send register settings to the stepper driver via SPI
 * returns the current status
 */
void TMC26XStepper_send262(unsigned long datagram, tos100 *tos100) {
  unsigned long i_datagram;
    
  //preserver the previous spi mode
  unsigned char oldMode =  SPCR & SPI_MODE_MASK;
        
  //if the mode is not correct set it to mode 3
  if (oldMode != SPI_MODE3) {
    spi_setDataMode(SPI_MODE3);
  }
        
  //select the TMC driver
  digitalWrite(tos100->cs_pin,LOW);

  //ensure that only valid bist are set (0-19)
  //datagram &=REGISTER_BIT_PATTERN;
        
  //write/read the values
  i_datagram = spi_transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= spi_transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= spi_transfer((datagram) & 0xff);
  i_datagram >>= 4;
        
  //deselect the TMC chip
  digitalWrite(tos100->cs_pin,HIGH); 
    
  //restore the previous SPI mode if neccessary
  //if the mode is not correct set it to mode 3
  if (oldMode != SPI_MODE3) {
    spi_setDataMode(oldMode);
  }

        
  //store the datagram as status result
  tos100->driver_status_result = i_datagram;
}

