# NIST-developed software is provided by NIST as a public service. You may use,
# copy, and distribute copies of the software in any medium, provided that you
# keep intact this entire notice. You may improve, modify, and create
# derivative works of the software or any portion of the software, and you may
# copy and distribute such modifications or works. Modified works should carry
# a notice stating that you changed the software and should note the date and
# nature of any such change. Please explicitly acknowledge the National
# Institute of Standards and Technology as the source of the software. 
#
# NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY
# OF ANY KIND, EXPRESS, IMPLIED, IN FACT, OR ARISING BY OPERATION OF LAW,
# INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT, AND DATA ACCURACY. NIST
# NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE
# UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES
# NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR
# THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY,
# RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
# 
# You are solely responsible for determining the appropriateness of using and
# distributing the software and you assume all risks associated with its use,
# including but not limited to the risks and costs of program errors,
# compliance with applicable laws, damage to or loss of data, programs or
# equipment, and the unavailability or interruption of operation. This software 
# is not intended to be used in any situation where a failure could cause risk
# of injury or damage to property. The software developed by NIST employees is
# not subject to copyright protection within the United States.
#
# Author: Thomas Roth <thomas.roth@nist.gov>

from __future__ import annotations

import math
import rclpy
import socket
import sys

from functools import total_ordering
from transforms3d.euler import quat2euler

from rclpy.node import Node
from rcl_interfaces.msg import IntegerRange, ParameterDescriptor, ParameterType

from ds_dbw_msgs.msg import BrakeInfo, VehicleVelocity
from nav_msgs.msg import Odometry
from std_msgs.msg import Bool

# A (second, nanoseconds) pair to represent a ROS2 timestamp
@total_ordering
class TimeStamp:
    def __init__(self, seconds, nanoseconds):
        self.seconds = seconds
        self.nanoseconds = nanoseconds

    def __lt__(self, other: TimeStamp):
        if self.seconds < other.seconds:
            return True
        elif self.seconds > other.seconds:
            return False
        else: # self.seconds == other.seconds
            return self.nanoseconds < other.nanoseconds

    def __eq__(self, other: TimeStamp):
        return self.seconds == other.seconds and self.nanoseconds == other.nanoseconds
    
    def __str__(self):
        return "(%s s + %s ns)" % (self.seconds, self.nanoseconds)
    
    def __add__(self, other: TimeStamp) -> TimeStamp:
        seconds = self.seconds + other.seconds
        nanoseconds = self.nanoseconds + other.nanoseconds

        # ensure the nanoseconds variable is less than 1 second
        if nanoseconds >= 1000000000:
            seconds += 1
            nanoseconds -= 1000000000

        return TimeStamp(seconds, nanoseconds)

    # This returns the absolute difference of the timestamps
    def __sub__(self, other: TimeStamp) -> TimeStamp:
        if self == other:
            return TimeStamp(0,0)
        if self < other:
            minuend = other
            subtrahend = self
        else: # self > other
            minuend = self
            subtrahend = other
        
        if minuend.nanoseconds >= subtrahend.nanoseconds:
            return TimeStamp(minuend.seconds - subtrahend.seconds, minuend.nanoseconds - subtrahend.nanoseconds)
        else: # convert 1 second of the minuend into nanoseconds
            return TimeStamp((minuend.seconds - 1) - subtrahend.seconds, (minuend.nanoseconds + 1000000000) - subtrahend.nanoseconds)

    def get_seconds(self):
        return self.seconds
    
    def get_nanoseconds(self):
        return self.nanoseconds

class DataSpeedBridge(Node):
    def __init__(self):
        super().__init__('dataspeed_bridge')

        # Declare Parameters
        pd_address = ParameterDescriptor(
            name='ip_address',
            type=ParameterType.PARAMETER_STRING,
            description='IPv4 Address for the server socket',
            read_only=True
        )
        self.declare_parameter('ip_address', '127.0.0.1', pd_address)

        pd_port = ParameterDescriptor(
            name='port_number',
            type=ParameterType.PARAMETER_INTEGER,
            description='Port Number for the server socket',
            integer_range=[IntegerRange(from_value=0, to_value=65535, step=1)],
            read_only=True
        )
        self.declare_parameter('port_number', 8080, pd_port)

        pd_timestep = ParameterDescriptor(
            name='timestep_ms',
            type=ParameterType.PARAMETER_INTEGER,
            description='Step size in milliseconds between two iterations',
            integer_range=[IntegerRange(from_value=1, to_value=999, step=1)],
        )
        self.declare_parameter('timestep_ms', 100, pd_timestep)

        # Create Subscriptions
        self.create_subscription(Odometry, '/novatel/odom', self.callback_odometry, 10)
        self.create_subscription(BrakeInfo, '/vehicle/brake/info', self.callback_brake, 10)
        self.create_subscription(VehicleVelocity, '/vehicle/vehicle_velocity', self.callback_velocity, 10)
        self.create_subscription(Bool, '/ds_bridge/terminate', self.callback_terminate, 10)

        # Initialize Variables
        self.next_time = None
        self.position = [0,0,0]
        self.orientation = [0,0,0]
        self.brake_torque = 0.0
        self.velocity = 0.0

    def run(self):
        address = self.get_parameter('ip_address').get_parameter_value().string_value
        port = self.get_parameter('port_number').get_parameter_value().integer_value

        # Setup the server socket
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # allow immediate re-use of address if code restarted
        self.server_socket.bind((address, port))
        self.server_socket.listen(1)

        # Accept client connection
        self.get_logger().info("Server at {}:{} waiting for client connection...".format(address, port))
        self.client_socket, client_address = self.server_socket.accept()
        self.get_logger().info("accepted client with address {}".format(client_address))

    def callback_odometry(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)

        self.__handle_timestamp(timestamp)

        self.position = [message.pose.pose.position.x, message.pose.pose.position.y, message.pose.pose.position.z]
        self.get_logger().debug("received position %s at time %s" % (self.position, timestamp))

        roll, pitch, yaw = quat2euler([
            message.pose.pose.orientation.w,
            message.pose.pose.orientation.x,
            message.pose.pose.orientation.y,
            message.pose.pose.orientation.z])
        self.orientation = [math.degrees(roll), math.degrees(pitch), math.degrees(yaw)]
        self.get_logger().debug("received orientation %s at time %s" % (self.orientation, timestamp))

    def callback_brake(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)

        self.__handle_timestamp(timestamp)
        
        self.brake_torque = message.brake_torque_request
        self.get_logger().debug("received brake torque %s at time %s" % (self.brake_torque, timestamp))

    def callback_velocity(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)

        self.__handle_timestamp(timestamp)

        self.velocity = message.vehicle_velocity_propulsion
        self.get_logger().debug("received velocity %s at time %s" % (self.velocity, timestamp))

    def callback_terminate(self, message):
        if message.data:
            self.get_logger().info("Exiting due to /ds_bridge/terminate")
            # self.client_socket.send("-1\r\n".encode())
            sys.exit()

    def advance_time(self):
        packet_data = [
            self.position[0],
            self.position[1],
            self.position[2],
            self.orientation[0],
            self.orientation[1],
            self.orientation[2],
            self.velocity,
            self.brake_torque
        ]
        packet_string = str(self.next_time.get_seconds()) + ' ' + str(self.next_time.get_nanoseconds()) + ' '
        packet_string += ' '.join(str(d) for d in packet_data)
        packet_string += "\r\n"

        # Send data to ns-3 and receive the response
        self.client_socket.send(packet_string.encode())
        self.get_logger().debug("sent packet: %s" % packet_string)
        #response = self.client_socket.recv(4096).decode()
        #self.get_logger().info("received response: %s" % response)

        #if response == '1':
        #    stop_msg = String()
        #    stop_msg.data = 'stop'
        #    self.stop_publisher.publish(stop_msg)

        self.next_time += self.__get_timestep()
        self.get_logger().info("Waiting until clock advances to %s" % self.next_time)

    def __get_timestep(self):
        return TimeStamp(0, self.get_parameter('timestep_ms').get_parameter_value().integer_value * 1000000)
    
    def __handle_timestamp(self, timestamp):
        if self.next_time is None:
            self.next_time = timestamp + self.__get_timestep()
            self.get_logger().info("Using %s as the reference start time" % timestamp)

        if timestamp > self.next_time:
            self.advance_time() # this is not called when equal to allow messages for other topics to arrive

def main(args=None):
    rclpy.init(args=args)

    ds_bridge = DataSpeedBridge()
    ds_bridge.run()

    rclpy.spin(ds_bridge)
    ds_bridge.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
