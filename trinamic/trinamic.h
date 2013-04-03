#ifndef TRINAMIC_H
#define TRINAMIC_H

#include <stdbool.h>

typedef struct {
  unsigned long driver_control_register_value;
  unsigned long chopper_config_register;
  unsigned long cool_step_register_value;
  unsigned long stall_guard2_current_register_value;
  unsigned long driver_configuration_register_value;
  //the driver status result
  unsigned long driver_status_result;
        
            
  //the pins for the stepper driver
  unsigned char cs_pin;
  unsigned char dir_pin;
  unsigned char step_pin;
                 
  //status values 
  int microsteps; //the current number of micro steps
 
  // config stuff
  unsigned int resistor;


  // probably not required
  unsigned int direction;
  unsigned int number_of_steps;
  unsigned int steps_left;
  bool cool_step_enabled;
} tos100;

extern void TMC26XStepper_init(int number_of_steps, int cs_pin, int dir_pin, 
			       int step_pin, unsigned int current, 
			       unsigned int resistor, tos100 *tos100);

void TMC26XStepper_start(tos100 *tos100);
void TMC26XStepper_send262(unsigned long datagram, tos100 *tos100);
#endif
