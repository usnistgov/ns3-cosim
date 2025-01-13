## How to Run
Make sure you have the ds_ros message dependencies (see external document).

Execute the following commands from the `av-ns3` directory:
-  Start the DataSpeed Bridge python code: `python3 rosbog/dataspeed-bridge.py` 
-  Start the ns-3 model for the NIST Automated Vehicle: `./ns3 run nist-vehicle`
-  Start the rosbag data playback (rosbag not included in repo): `ros2 bag play <NAME_OF_BAG>.db3`

## Adjust the IPv4 Address
To change the IPv4 Address for the gateway:
-  Change the value of the `IP_ADDRESS` variable in the `rosbag/dataspeed-bridge.py` file
-  Set the `gatewayAddress` command line option when running the ns-3 simulation (example: `./ns3 run "nist-vehicle --gatewayAddress=127.0.0.1"`)

## Transmit BSMs when Braking
By default, the vehicle will not transmit BSMs when braking. This can be enabled using the `transmitOnBrake` command line option when running the ns-3 simulation (example: `./ns3 run "nist-vehicle --transmitOnBrake=true"`). However, note the visualization will be difficult to understand when this option is enabled if other nodes are also sending BSMs.

## How to Stop
The code will not exit automatically, even when the rosbag completes playback. You should never terminate the ns-3 code manually as it will fail to log its output data.

Use the following command to terminate the bridge, which will cleanly shutdown the ns-3 model:

`ros2 topic pub --once /ds_bridge/terminate std_msgs/msg/Bool "{data: 'True'}"`

## How to Visualize
Load the `nist-vehicle.json` file that appears in the `av-ns3` directory into NetSimulyzer to start playback.
