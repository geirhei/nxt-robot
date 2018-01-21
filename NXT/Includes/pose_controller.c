#include "pose_controller.h"



void vMainPoseControllerTask( void *pvParameters ) {
    #ifdef DEBUG
        printf("PoseController OK\n");
        uint8_t tellar = 0;
    #endif

    /* Task init */
    cartesian_t Target = {0};
	float radiusEpsilon = 5; //[mm]The acceptable radius from goal for completion
	uint8_t lastMovement = 0;
	
	// Find better values for NXT
	uint8_t maxRotateActuation = 40; //The max speed the motors will run at during rotation (was 75)
	uint8_t maxDriveActuation = 45; //The max speed the motors will run at during drive (was 100)
	uint8_t currentDriveActuation = maxRotateActuation;
	
	/* Controller variables for tuning, probably needs calculation for NXT */
	float rotateThreshold = 0.5235; // [rad] The threshold at which the robot will go from driving to rotation. Equals 10 degrees
	float driveThreshold = 0.0174; // [rad]The threshold at which the robot will go from rotation to driving. In degrees.
	float driveKp = 600; //Proportional gain for theta control during drive
	float driveKi = 10; //Integral gain for theta during drive
	float speedDecreaseThreshold = 300; //[mm] Distance from goal where the robot will decrease its speed inverse proportionally
	
	/* Current position variables */	
	float thetahat = 0;
	float xhat = 0;
	float yhat = 0;
	pose_t GlobalPose = {0};
	
	/* Goal variables*/
	float distance = 0;
	float thetaDiff = 0;
	float xTargt = 0;
	float yTargt = 0;
	
	float leftIntError = 0;
	float rightIntError = 0;
	
	uint8_t doneTurning = TRUE;
	
	int16_t leftWheelTicks = 0;
	int16_t rightWheelTicks = 0;
	wheel_ticks_t WheelTicks = {0};
	
	uint8_t leftEncoderVal = 0;
	uint8_t rightEncoderVal = 0;
	
	uint8_t gLeftWheelDirection = 0;
	uint8_t gRightWheelDirection = 0;
	
	uint8_t idleSent = FALSE;

	uint8_t leftWheelDirection = moveStop;
	uint8_t rightWheelDirection = moveStop;
      
	while(1) {
		// Checking if server is ready
		if (gHandshook) {
			
			vMotorEncoderLeftTickFromISR(gLeftWheelDirection, &leftWheelTicks, leftEncoderVal);
			vMotorEncoderRightTickFromISR(gRightWheelDirection, &rightWheelTicks, rightEncoderVal);
		
			WheelTicks.leftWheel = leftWheelTicks;
			WheelTicks.rightWheel = rightWheelTicks;

			// Send wheel ticks received from ISR to the global wheel tick Q. Wait 0ms - increase this?
			xQueueOverwrite(globalWheelTicksQ, &WheelTicks);

			// Wait for synchronization by direct notification from the estimator task. Blocks indefinetely?
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

			if (xQueuePeek(globalPoseQ, &GlobalPose, 0)) { // Block time?
				thetahat = GlobalPose.theta;
				xhat = GlobalPose.x;
				yhat = GlobalPose.y;
			}
			
			// Check if a new update is received
			if (xQueueReceive(poseControllerQ, &Target, 0)) { // Receive theta and radius set points from com task
				xTargt = Target.x;
				yTargt = Target.y;
			}
			
			distance = sqrt((xTargt-xhat)*(xTargt-xhat) + (yTargt-yhat)*(yTargt-yhat));
			
			//Simple speed controller as the robot nears the target
			if (distance < speedDecreaseThreshold) {
				currentDriveActuation = (maxDriveActuation - 0.32*maxDriveActuation)*distance/speedDecreaseThreshold + 0.32*maxDriveActuation; //Reverse proportional + a constant so it reaches. 
			} else {
				currentDriveActuation = maxDriveActuation;
			}
			
			if (distance > radiusEpsilon) { //Not close enough to target
				idleSent = FALSE;
				
				float xdiff = xTargt - xhat;
				float ydiff = yTargt - yhat;
				float thetaTargt = atan2(ydiff,xdiff); //atan() returns radians
				thetaDiff = thetaTargt - thetahat; //Might be outside pi to -pi degrees
				vFunc_Inf2pi(&thetaDiff);

				//Hysteresis mechanics
				if (fabs(thetaDiff) > rotateThreshold) {
					doneTurning = FALSE;
				} else if (fabs(thetaDiff) < driveThreshold) {
					doneTurning = TRUE;
				}
				
				int16_t LSpeed = 0;
				int16_t RSpeed = 0;
				
				if (doneTurning) { //Start forward movement
					if (thetaDiff >= 0) { //Moving left
						LSpeed = currentDriveActuation - driveKp*fabs(thetaDiff) - driveKi*leftIntError; //Simple PI controller for theta 
						
						//Saturation
						if (LSpeed > currentDriveActuation) {
							LSpeed = currentDriveActuation;
						} else if (LSpeed < 0) {
							LSpeed = 0;
						}
						RSpeed = currentDriveActuation;
					} else { //Moving right
						RSpeed = currentDriveActuation - driveKp*fabs(thetaDiff) - driveKi*rightIntError; //Simple PI controller for theta
						
						//Saturation
						if (RSpeed > currentDriveActuation) {
							RSpeed = currentDriveActuation;
						} else if (RSpeed < 0) {
							RSpeed = 0;
						}
						LSpeed = currentDriveActuation;
					}
					
					leftIntError += thetaDiff;
					rightIntError -= thetaDiff;
					
					gRightWheelDirection = motorForward; //?
					gLeftWheelDirection = motorForward;
					lastMovement = moveForward;
					
				} else { //Turn within 1 degree of target
					if (thetaDiff >= 0) { //Rotating left
						LSpeed = -maxRotateActuation*(0.3 + 0.22*(fabs(thetaDiff)));
						gLeftWheelDirection = motorBackward;
						RSpeed = maxRotateActuation*(0.3 + 0.22*(fabs(thetaDiff)));
						gRightWheelDirection = motorForward;
						lastMovement = moveCounterClockwise;
					} else { //Rotating right
						LSpeed = maxRotateActuation*(0.3 + 0.22*(fabs(thetaDiff)));
						gLeftWheelDirection = motorForward;
						RSpeed = -maxRotateActuation*(0.3 + 0.22*(fabs(thetaDiff)));
						gRightWheelDirection = motorBackward;
						lastMovement = moveClockwise;
					}
					
					leftIntError = 0;
					rightIntError = 0;
				}

				vMotorMovementSwitch(LSpeed, RSpeed, &gLeftWheelDirection, &gRightWheelDirection);
		
			} else {
				if (idleSent == FALSE) {
					send_idle();
					idleSent = TRUE;
				}
				// Set speed of both motors to 0
				vMotorMovementSwitch(0, 0, &gLeftWheelDirection, &gRightWheelDirection);
				lastMovement = moveStop;
			}

			xQueueSendToBack(scanStatusQ, &lastMovement, 0); // Send the current movement to the sensor tower task
			
		}
	}
}