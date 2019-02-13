#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Vector3.h>
#include <tf/transform_listener.h>

#include "mag_msgs/FieldStamped.h"

#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/TransformStamped.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <eigen_conversions/eigen_msg.h>

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <sstream>

#include <HL/hl.h>
#include <HD/hd.h>
#include <HDU/hduError.h>
#include <HDU/hduVector.h>
#include <HDU/hduMatrix.h>

#include "phantom_omni/PhantomButtonEvent.h"
#include "phantom_omni/OmniFeedback.h"
#include <pthread.h>
#include <ros/publisher.h>

#define GRAVITY_COMPENSATION_FORCE 0.705


int calibrationStyle;

struct OmniState {
	hduVector3Dd position;  //3x1 vector of position
	hduVector3Dd velocity;  //3x1 vector of velocity
	hduVector3Dd inp_vel1; //3x1 history of velocity used for filtering velocity estimate
	hduVector3Dd inp_vel2;
	hduVector3Dd inp_vel3;
	hduVector3Dd out_vel1;
	hduVector3Dd out_vel2;
	hduVector3Dd out_vel3;
	hduVector3Dd pos_hist1; //3x1 history of position used for 2nd order backward difference estimate of velocity
	hduVector3Dd pos_hist2;
	hduVector3Dd rot;
	hduVector3Dd joints;
	hduVector3Dd force;   //3 element double vector force[0], force[1], force[2]
	float thetas[7];
	int buttons[2];
	int buttons_prev[2];
	bool lock;
	hduVector3Dd lock_pos;

	double masterForces[3] = {0,0,0};
};

class PhantomROS {

public:
	ros::NodeHandle n;
	ros::Publisher joint_pub;
	ros::Publisher cursor_location_pub;
	ros::Publisher button_pub;
	ros::Publisher target_mf_pub_;
	ros::Subscriber haptic_sub;
	std::string omni_name;

    tf::TransformListener tfListener;
    tf::StampedTransform transformStylusToBase;

	Eigen::Quaterniond rotationalData;
	Eigen::Matrix3d rotMatEuler;
	Eigen::Vector3d desFieldStylus; //this is just the orientation of the x axis of the frame of the stylus of the phantom omni in the stylus frame. Gets transformed into base frame
	Eigen::Vector3d magFieldBase_Eigen;

    //tf2_ros::Buffer tfBuffer;
    //tf2_ros::TransformListener tfListener;
    //geometry_msgs::TransformStamped transformStamped;
    //geometry_msgs::Vector3 desMagField; //this is just the orientation of the x axis of the frame of the stylus of the phantom omni in the stylus frame. Gets transformed into base frame
    geometry_msgs::Vector3 magFieldBase_msg; // this is the transformed desMgField vector aka stylus orientation represented in base frame

	OmniState *state;

	void init(OmniState *s) {
		ros::param::param(std::string("~omni_name"), omni_name,
				std::string("omni1"));

                // Publish joint states for robot_state_publisher,
                // and anyone else who wants them.
		std::ostringstream joint_topic;
		joint_topic << omni_name << "_joint_states";
		joint_pub = n.advertise<sensor_msgs::JointState>(joint_topic.str(), 1);

		// Publish button state on NAME_button.
		std::ostringstream button_topic;
		button_topic << omni_name << "_button";
		button_pub = n.advertise<phantom_omni::PhantomButtonEvent>(button_topic.str(), 100);

		// Publish cursor location on NAME_Cursor_location. --> TODO: change this to cursor pose later
		std::ostringstream cursor_location_topic;
		cursor_location_topic << omni_name << "_cursor_location";
		cursor_location_pub = n.advertise<geometry_msgs::PoseStamped>(cursor_location_topic.str(), 1);

        std::ostringstream target_mf_topic;
        //target_mf_topic << "const_curv_sim/field_at_magnet";
        target_mf_topic << "/desired_field";
        target_mf_pub_ = n.advertise<mag_msgs::FieldStamped>(target_mf_topic.str(),1);


		// Subscribe to NAME_force_feedback.
		std::ostringstream force_feedback_topic;
		force_feedback_topic << omni_name << "_force_feedback";
		haptic_sub = n.subscribe(force_feedback_topic.str(), 100,
				&PhantomROS::force_callback, this);

		state = s;
		state->buttons[0] = 0;
		state->buttons[1] = 0;
		state->buttons_prev[0] = 0;
		state->buttons_prev[1] = 0;
		hduVector3Dd zeros(0, 0, 0);
		state->velocity = zeros;
		state->inp_vel1 = zeros;  //3x1 history of velocity
		state->inp_vel2 = zeros;  //3x1 history of velocity
		state->inp_vel3 = zeros;  //3x1 history of velocity
		state->out_vel1 = zeros;  //3x1 history of velocity
		state->out_vel2 = zeros;  //3x1 history of velocity
		state->out_vel3 = zeros;  //3x1 history of velocity
		state->pos_hist1 = zeros; //3x1 history of position
		state->pos_hist2 = zeros; //3x1 history of position
		state->lock = false;
		state->lock_pos = zeros;

	}

	/*******************************************************************************
	 ROS node callback.
	 *******************************************************************************/
	void force_callback(const phantom_omni::OmniFeedbackConstPtr& omnifeed) {
		////////////////////Some people might not like this extra damping, but it
		////////////////////helps to stabilize the overall force feedback. It isn't
		////////////////////like we are getting direct impedance matching from the
		////////////////////omni anyway
		state->force[0] = omnifeed->force.x - 0.001 * state->velocity[0];
		state->force[1] = omnifeed->force.y - 0.001 * state->velocity[1];
		state->force[2] = omnifeed->force.z - 0.001 * state->velocity[2];

		state->lock_pos[0] = omnifeed->position.x;
		state->lock_pos[1] = omnifeed->position.y;
		state->lock_pos[2] = omnifeed->position.z;
	}

	void publish_cursor_location() {
        geometry_msgs::PoseStamped cursorPose;
        cursorPose.header.stamp = ros::Time::now();
       // cursorPose.pose.position.x = state->position[0];
       // cursorPose.pose.position.y = state->position[1];
       // cursorPose.pose.position.z = state->position[2];


        cursorPose.pose.position.x = 0;
        cursorPose.pose.position.y = 0;
        cursorPose.pose.position.z = 0;

        float roh = 1; // used to calculate vector that represents

        //cursorPose.pose.orientation.x = roh*sin(state->thetas[4])*cos(state->thetas[5]);
        //cursorPose.pose.orientation.y = roh*sin(state->thetas[4])*sin(state->thetas[5]);
        //cursorPose.pose.orientation.z = roh*cos(state->thetas[4]);

        try{
            tfListener.lookupTransform("stylus", "base",
                                     ros::Time(0), transformStylusToBase);
        }
        catch (tf::TransformException ex){
            ROS_ERROR("%s",ex.what());
            ros::Duration(1.0).sleep();
        }


		desFieldStylus << 0.1,0,0;

        tf::Quaternion quat;
        quat = transformStylusToBase.getRotation();
        double yaw =0, pitch =0, roll =0;
        tf::Matrix3x3(quat).getEulerZYX(yaw, pitch, roll);



		rotMatEuler << 	cos(pitch) * cos(yaw), cos(pitch) * sin(yaw), (-1)* sin(pitch),
				sin(roll) * sin(pitch) * cos(yaw)- cos(roll) * sin(yaw), sin(roll) * sin(pitch) * sin(yaw)+ cos(roll) * cos(yaw), sin(roll) * cos(pitch),
				cos(roll)* sin(pitch) * cos(yaw) + sin(roll) * sin(yaw),cos(roll)* sin(pitch) * sin(yaw) - sin(roll) * cos(yaw), cos(roll)* cos(pitch);

        magFieldBase_Eigen = rotMatEuler * (-1)*desFieldStylus;


        //norm vector; Magnitude corresponds to desired field strength in Tesla. Max is 0.040 T (40 mTesla).
        //try to work with 'as low as possible'

		//magFieldBase_Eigen * (0.8 / magFieldBase_Eigen.norm()); // setting field (aka vector length) to static 0.02 T for first tests.



        magFieldBase_msg.x = magFieldBase_Eigen(0);
        magFieldBase_msg.y = magFieldBase_Eigen(1);
        magFieldBase_msg.z = magFieldBase_Eigen(2);

        mag_msgs::FieldStamped mf_target;
        mf_target.header.frame_id = "mns";
        mf_target.header.stamp = ros::Time::now();
        mf_target.field.vector.x = magFieldBase_msg.x;
        mf_target.field.vector.y = magFieldBase_msg.y;
        mf_target.field.vector.z = magFieldBase_msg.z;
        mf_target.field.position.x = 0.0;
        mf_target.field.position.y = 0.0;
        mf_target.field.position.z = 0.05;

        target_mf_pub_.publish(mf_target);


        //const Eigen::Vector3d magFieldBase_Eigen_= magFieldBase_Eigen;

        //tf::vectorEigenToMsg(&magFieldBase_Eigen_,magFieldBase_msg);

        //quaternionStampedTFToMsg(quat,cursorPose.pose.orientation);
        //cursorPose.pose.orientation.y = transformStamped.rotation.y;
        //cursorPose.pose.orientation.z = transformStamped.rotation.z;

        cursorPose.header.frame_id = "/base";

        cursor_location_pub.publish(cursorPose);

    };

	void publish_omni_state() {
		sensor_msgs::JointState joint_state;
		joint_state.header.stamp = ros::Time::now();
		joint_state.name.resize(6);
		joint_state.position.resize(6);
		joint_state.name[0] = "waist";
		joint_state.position[0] = -state->thetas[1];
		joint_state.name[1] = "shoulder";
		joint_state.position[1] = state->thetas[2];
		joint_state.name[2] = "elbow";
		joint_state.position[2] = state->thetas[3];
		joint_state.name[3] = "wrist1";
		joint_state.position[3] = -state->thetas[4] + M_PI; // TODO find correct encoder Mapping  (- 14*(M_PI/180); //added -15*(M_PI/180) to compensate encoder offset seen in rviz)
		joint_state.name[4] = "wrist2";
		joint_state.position[4] = -state->thetas[5] - 3*M_PI/4 + 18*(M_PI/180); // TODO find correct encoder Mapping (added -18*(M_PI/180) to compensate encoder offset seen in rviz)
		joint_state.name[5] = "wrist3";
		joint_state.position[5] = -state->thetas[6] - M_PI + 18*(M_PI/180); //
		joint_pub.publish(joint_state);

		if ((state->buttons[0] != state->buttons_prev[0])
				or (state->buttons[1] != state->buttons_prev[1])) {

			if ((state->buttons[0] == state->buttons[1])
					and (state->buttons[0] == 1)) {
				state->lock = !(state->lock);
			}
			phantom_omni::PhantomButtonEvent button_event;
			button_event.grey_button = state->buttons[0];
			button_event.white_button = state->buttons[1];
			state->buttons_prev[0] = state->buttons[0];
			state->buttons_prev[1] = state->buttons[1];
			button_pub.publish(button_event);
		}
	}
};

HDCallbackCode HDCALLBACK omni_state_callback(void *pUserData) {
	OmniState *omni_state = static_cast<OmniState *>(pUserData);
	if (hdCheckCalibration() == HD_CALIBRATION_NEEDS_UPDATE) {
	  ROS_DEBUG("Updating calibration...");
	    hdUpdateCalibration(calibrationStyle);
	  }
	hdBeginFrame(hdGetCurrentDevice());
	//Get angles, set forces
	hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, omni_state->rot);
	hdGetDoublev(HD_CURRENT_POSITION, omni_state->position);
	hdGetDoublev(HD_CURRENT_JOINT_ANGLES, omni_state->joints);

	hduVector3Dd vel_buff(0, 0, 0);
	vel_buff = (omni_state->position * 3 - 4 * omni_state->pos_hist1
			+ omni_state->pos_hist2) / 0.002;  //mm/s, 2nd order backward dif
	omni_state->velocity = (.2196 * (vel_buff + omni_state->inp_vel3)
			+ .6588 * (omni_state->inp_vel1 + omni_state->inp_vel2)) / 1000.0
			- (-2.7488 * omni_state->out_vel1 + 2.5282 * omni_state->out_vel2
					- 0.7776 * omni_state->out_vel3);  //cutoff freq of 20 Hz
	omni_state->pos_hist2 = omni_state->pos_hist1;
	omni_state->pos_hist1 = omni_state->position;
	omni_state->inp_vel3 = omni_state->inp_vel2;
	omni_state->inp_vel2 = omni_state->inp_vel1;
	omni_state->inp_vel1 = vel_buff;
	omni_state->out_vel3 = omni_state->out_vel2;
	omni_state->out_vel2 = omni_state->out_vel1;
	omni_state->out_vel1 = omni_state->velocity;
	if (omni_state->lock == true) {
		omni_state->force = 0.04 * (omni_state->lock_pos - omni_state->position)
				- 0.001 * omni_state->velocity;
	}

	hduVector3Dd AppliedForce;

	AppliedForce[0] = omni_state->masterForces[0];
	AppliedForce[1] = omni_state->masterForces[1] + GRAVITY_COMPENSATION_FORCE;
	AppliedForce[2] = omni_state->masterForces[2];

	hdSetDoublev(HD_CURRENT_FORCE, AppliedForce);


	//Get buttons
	int nButtons = 0;
	hdGetIntegerv(HD_CURRENT_BUTTONS, &nButtons);
	omni_state->buttons[0] = (nButtons & HD_DEVICE_BUTTON_1) ? 1 : 0;
	omni_state->buttons[1] = (nButtons & HD_DEVICE_BUTTON_2) ? 1 : 0;

	hdEndFrame(hdGetCurrentDevice());

	HDErrorInfo error;
	if (HD_DEVICE_ERROR(error = hdGetError())) {
		hduPrintError(stderr, &error, "Error during main scheduler callback");
		if (hduIsSchedulerError(&error))
			return HD_CALLBACK_DONE;
	}

	float t[7] = { 0., omni_state->joints[0], omni_state->joints[1],
			omni_state->joints[2] - omni_state->joints[1], omni_state->rot[0],
			omni_state->rot[1], omni_state->rot[2] };
	for (int i = 0; i < 7; i++)
		omni_state->thetas[i] = t[i];
	return HD_CALLBACK_CONTINUE;
}

/*******************************************************************************
 Automatic Calibration of Phantom Device - No character inputs
 *******************************************************************************/
void HHD_Auto_Calibration() {
	int supportedCalibrationStyles;
	HDErrorInfo error;

	hdGetIntegerv(HD_CALIBRATION_STYLE, &supportedCalibrationStyles);
	if (supportedCalibrationStyles & HD_CALIBRATION_ENCODER_RESET) {
		calibrationStyle = HD_CALIBRATION_ENCODER_RESET;
		ROS_INFO("HD_CALIBRATION_ENCODER_RESE..");
	}
	if (supportedCalibrationStyles & HD_CALIBRATION_INKWELL) {
		calibrationStyle = HD_CALIBRATION_INKWELL;
		ROS_INFO("HD_CALIBRATION_INKWELL..");
	}
	if (supportedCalibrationStyles & HD_CALIBRATION_AUTO) {
		calibrationStyle = HD_CALIBRATION_AUTO;
		ROS_INFO("HD_CALIBRATION_AUTO..");
	}
	if (calibrationStyle == HD_CALIBRATION_ENCODER_RESET) {
	  do {
		hdUpdateCalibration(calibrationStyle);
		ROS_INFO("Calibrating.. (put stylus in well)");
		if (HD_DEVICE_ERROR(error = hdGetError())) {
			hduPrintError(stderr, &error, "Reset encoders reset failed.");
			break;
		}
	} while (hdCheckCalibration() != HD_CALIBRATION_OK);
	ROS_INFO("Calibration complete.");
	}
	if (hdCheckCalibration() == HD_CALIBRATION_NEEDS_MANUAL_INPUT) {
	  ROS_INFO("Please place the device into the inkwell for calibration.");
	}
}

void *ros_publish(void *ptr) {
	PhantomROS *omni_ros = (PhantomROS *) ptr;
	int publish_rate;
	omni_ros->n.param(std::string("publish_rate"), publish_rate, 100);
	ros::Rate loop_rate(publish_rate);
	ros::AsyncSpinner spinner(2);
	spinner.start();

	while (ros::ok()) {
		omni_ros->publish_omni_state();
		omni_ros->publish_cursor_location();
		loop_rate.sleep();
	}
	return NULL;
}

int main(int argc, char** argv) {
	////////////////////////////////////////////////////////////////
	// Init Phantom
	////////////////////////////////////////////////////////////////
	HDErrorInfo error;
	HHD hHD;
	hHD = hdInitDevice(HD_DEFAULT_DEVICE);
	if (HD_DEVICE_ERROR(error = hdGetError())) {
		//hduPrintError(stderr, &error, "Failed to initialize haptic device");
		ROS_ERROR("Failed to initialize haptic device"); //: %s", &error);
		return -1;
	}

	ROS_INFO("Found %s.", hdGetString(HD_DEVICE_MODEL_TYPE));
	hdEnable(HD_FORCE_OUTPUT);
	hdStartScheduler();
	if (HD_DEVICE_ERROR(error = hdGetError())) {
		ROS_ERROR("Failed to start the scheduler"); //, &error);
		return -1;
	}
	HHD_Auto_Calibration();

	////////////////////////////////////////////////////////////////
	// Init ROS
	////////////////////////////////////////////////////////////////
	ros::init(argc, argv, "omni_haptic_node");
	OmniState state;
	PhantomROS omni_ros;

	omni_ros.init(&state);
	hdScheduleAsynchronous(omni_state_callback, &state,
			HD_MAX_SCHEDULER_PRIORITY);

	////////////////////////////////////////////////////////////////
	// Loop and publish
	////////////////////////////////////////////////////////////////
	pthread_t publish_thread;
	pthread_create(&publish_thread, NULL, ros_publish, (void*) &omni_ros);
	pthread_join(publish_thread, NULL);

	ROS_INFO("Ending Session....");
	hdStopScheduler();
	hdDisableDevice(hHD);

	return 0;
}

