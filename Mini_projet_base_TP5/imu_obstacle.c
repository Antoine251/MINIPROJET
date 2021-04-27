#include "ch.h"
#include "hal.h"
#include <usbcfg.h>
#include <chprintf.h>
#include <sensors/imu.h>

#include <mailboxe.h>
#include <imu_obstacle.h>

#define Z_AXIS 							2
#define NUMBER_SAMPLE_IMU 				100
#define NUMBER_SAMPLE_IMU_CALIBRATION 	255
#define ACC_Z_TILT_THRESHOLD			30
#define MAX_TILT_COUNTER				3

#define NO_BUMP							0
#define BUMP_DETECTED					1
#define BUMP_PASSED						2


static int16_t offset_acc_z = 0;

static THD_WORKING_AREA(obs_thd_wa, 256); //256 ou moins ?
static THD_FUNCTION(obs_thd, arg){

	chRegSetThreadName(__FUNCTION__);
	(void)arg;

	int16_t z_acc = 0;
	uint8_t tilt_counter = 0;
	uint8_t flat_counter = 0;

	uint8_t tilt_state = NO_BUMP;

	msg_t turn_state = NO_TURN_TO_DO;


	while(1){



		z_acc = get_acc_filtered(Z_AXIS, NUMBER_SAMPLE_IMU) - offset_acc_z;


		//pour qu'une bosse/plat soit d�tect�, il faut que 3 valeurs audessus/endessous du seuil soit lues
		if(z_acc >= ACC_Z_TILT_THRESHOLD){
			flat_counter = 0;
			tilt_counter++;
			if(tilt_counter == MAX_TILT_COUNTER){
				tilt_counter = MAX_TILT_COUNTER - 1;
				tilt_state = BUMP_DETECTED;
			}
		}else{
			tilt_counter = 0;
			flat_counter++;
			if(flat_counter == MAX_TILT_COUNTER){
				flat_counter = MAX_TILT_COUNTER - 1;
				if(tilt_state == BUMP_DETECTED){
					tilt_state = BUMP_PASSED;
				}

			}
		}

		if(tilt_state == BUMP_PASSED){
			tilt_state = NO_BUMP;
			turn_state = DO_A_TURN;
		}

		//envoi d'information "bosse" via mailboxe
		chSysLock();
		chMBPostI(get_mailboxe_imu_adr(), turn_state);
		chSysUnlock();

		//chprintf((BaseSequentialStream *)&SDU1, "imu values z_axis : %d \n", z_acc);

		chThdSleepMilliseconds(100); //10x par seconde /!\ doit �tre plus lent que la thread moteur
	}



}

void imu_init(void){ //ordre ?
	imu_start();
	chThdCreateStatic(obs_thd_wa, sizeof(obs_thd_wa), NORMALPRIO, obs_thd, NULL);
	calibrate_acc();
	offset_acc_z = get_acc_filtered(Z_AXIS, NUMBER_SAMPLE_IMU_CALIBRATION);
}

