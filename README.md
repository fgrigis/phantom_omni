phantom_omni
============

This package was forked from https://github.com/danepowell/phantom_omni. The original README.md can be found in the original repo. The README.md here is mainly intended to facilitate the installation/setup and use of the Phantom Omni device in the context of the research work of ETH Zurich's MSRL and it's research projects.

ROS Node for GeomMagic Touch devicePhantom Omni devices.

Requires the [omni_description](https://github.com/danepowell/omni_description) package. 

Parameters:
- ~omni_name (default: omni1)

Publishes:
- OMNI_NAME_joint_states (sensor_msgs/JointState): The state of each of the omni's joints.
- OMNI_NAME_button (phantom_omni/PhantomButtonEvent): Events for the grey and white buttons.

Subscribes:
- OMNI_NAME_force_feedback (phantom_omni/OmniFeedback): Force feedback to be displayed on the omni. Takes a force and a position. If you simultaneously click the grey and white buttons, the omni will 'lock' to the position.

Installation
============

Environment:
Ubuntu 16.04
ROS Kinetic


Go to https://3dssupport.microsoftcrmportals.com/knowledgebase/article/KA-03284/en-us and download the OpenHaptics Installer Haptic device driver Installer. 

Extract the downloaded files to the home folder. Then follow the installation instructions in the respective folders (see README_INSTALL files in the folders). The computer will reboot during both installation processes.

IMPORTANT: There is a weird QT related conflict between RVIZ and GeomagicTouch, which requires the removal of some loaders during boot. Therefore, after completion of the installation go to /etc/profile.d/ and open the file geomagic.sh (you need root privileges). Comment the three lines of the file such that it's content looks like this:

#export GTDD_HOME=/opt/geomagic_touch_device_driver

#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/geomagic_touch_device_driver/lib

#export QT_PLUGIN_PATH=/opt/geomagic_touch_device_driver/lib/plugins



Set-Up
============

Connect the power cable and the USB-Ethernet dongle to hook the device up to the computer. Once the device is connected to the computer, follow the instructions in the file 'Linux_Helper_Notes.pdf' to set up the ethernet connection properly.

Next, in a new terminal window, run the following three commands, one after another, make sure you haven't sourced anything in this terminal before:

- export GTDD_HOME=/opt/geomagic_touch_device_driver

- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/geomagic_touch_device_driver/lib

- export QT_PLUGIN_PATH=/opt/geomagic_touch_device_driver/lib/plugins

Then, in the same terminal window:

- cd into /geomagic_touch_device_driver_2016.1-1-amd64/opt/geomagic_touch_device_driver

- ./Geomagic_Touch_Setup

This opens up the Geomagic Setup application. Select Geomagic Touch (Device Model). Then click on the pairing button in the application right after that, push the pairing button on the back of the haptic device within 10 sec. A window should pop up to confirm the succesful pairing. Klick ok, then Apply. Click OK again and close the application. Next you can open the Diagnostics app with:

- ./Geomagic_Touch_Diagnostics

IMPORTANT NOTES:
- Whenever you change to another computer with the device, you need to pair the haptic device again. If the device is used with the same computer, the setup procedure doesn't have to be repeated.
- When using the haptic device, remember to first execute the export commands listed above. These commands have to be run in the terminal window in which the Phantom_Omni node will be running. 
- It is important to execute the 'export' commands before sourcing the ROS environmentwith ROS
- If your applications uses rviz, make sure to launch the application that uses rviz from another terminal windo where the export commands have NOT been executed.




