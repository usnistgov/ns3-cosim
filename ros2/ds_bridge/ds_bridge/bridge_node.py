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

from rclpy.node import Node
from std_msgs.msg import String
from rcl_interfaces.msg import SetParametersResult
from transforms3d.euler import quat2euler
from functools import total_ordering

from ds_dbw_msgs.msg import BrakeInfo
from ds_dbw_msgs.msg import VehicleVelocity
from nav_msgs.msg import Odometry
from std_msgs.msg import Bool

# local address
IP_ADDRESS = "127.0.0.1"
# NIST Vehicle PC
#IP_ADDRESS = "192.168.0.10"

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

        if nanoseconds >= 1000000000:
            seconds += 1
            nanoseconds -= 1000000000
        return TimeStamp(seconds, nanoseconds)

    # absolute difference
    def __sub__(self, other: TimeStamp) -> TimeStamp:
        if self == other:
            return TimeStamp(0,0)
        if self > other:
            if self.nanoseconds >= other.nanoseconds:
                return TimeStamp(self.seconds - other.seconds, self.nanoseconds - other.nanoseconds)
            else:
                return TimeStamp(self.seconds - other.seconds - 1, 1000000000 + self.nanoseconds - other.nanoseconds)
        else: # self < other
            if other.nanoseconds >= self.nanoseconds:
                return TimeStamp(other.seconds - self.seconds, other.nanoseconds - self.nanoseconds)
            else:
                return TimeStamp(other.seconds - self.seconds - 1, 1000000000 + other.nanoseconds - self.nanoseconds)

    def get_seconds(self):
        return self.seconds
    
    def get_nanoseconds(self):
        return self.nanoseconds

class Position:
    def __init__(self, x, y, z):
        self.x = x
        self.y = y
        self.z = z
    
    def __str__(self):
        return "(%s,%s,%s)" % (self.x, self.y, self.z)
    
    def __add__(self, other: Position) -> Position:
        return Position(self.x + other.x, self.y + other.y, self.z + other.z)
    
    def __sub__(self, other: Position) -> Position:
        return Position(self.x - other.x, self.y - other.y, self.z - other.z)
    
    def get_x(self):
        return self.x
    
    def get_y(self):
        return self.y
    
    def get_z(self):
        return self.z

class DataSpeedNetworkBridge(Node):
    def __init__(self, timestep_ms, ignore_position_z):
        super().__init__('dataspeed_network_bridge')
        self.timestep_ms = timestep_ms
        self.ignore_position_z = ignore_position_z

        # Create Subscriptions
        self.create_subscription(Odometry, '/novatel/odom', self.callback_odometry, 10)
        self.create_subscription(BrakeInfo, '/vehicle/brake/info', self.callback_brake, 10)
        self.create_subscription(VehicleVelocity, '/vehicle/vehicle_velocity', self.callback_velocity, 10)
        self.create_subscription(Bool, '/ds_bridge/terminate', self.callback_terminate, 10)

        # Create Publications
        self.stop_publisher = self.create_publisher(String, 'stop_cmd', 10)

        self.next_time = self.__get_timestep()
        self.next_position = Position(0,0,0)
        self.next_orientation = [0,0,0]
        self.next_brake_torque = 0.0
        self.next_velocity = 0.0

        self.start_time = TimeStamp(0,0)
        self.is_clock_set = False

        self.origin_position = Position(308223.4547550826,4334006.677150687,107.5224222894758) # NIST Gaithersburg
        self.is_position_set = True # comment to use first value received from ns-3

        # Setup the TCP Server for ns-3
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # allow immediate re-use of address if code restarted
        self.server_socket.bind((IP_ADDRESS, 1111))
        self.server_socket.listen(1)
        self.get_logger().info("TCP/IP server at {}:1111 waiting for client connection...".format(IP_ADDRESS))
        # self.client_socket, client_address = self.server_socket.accept()
        #self.get_logger().info("accepted client with address {}".format(client_address))

    def callback_odometry(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)
        position = Position(message.pose.pose.position.x, message.pose.pose.position.y, message.pose.pose.position.z)

        if not self.is_clock_set:
            self.start_time = timestamp
            self.get_logger().info("Using %s as the reference start time" % self.start_time)
            self.is_clock_set = True

        timestamp_adjusted = timestamp - self.start_time
        if timestamp_adjusted > self.next_time: # TODO: check edge cases for when things are uninitialized
            self.advance_time() # this is not called when equal to allow messages for other topics to arrive

        if not self.is_position_set:
            self.origin_position = position
            self.get_logger().info("Using %s as the reference start position" % self.origin_position)
            self.is_position_set = True
        position -= self.origin_position

        if self.ignore_position_z:
            # the car model origin is ~1.5 units above the ground, and the map plane will be drawn at 1 unit elevation
            position -= Position(0,0,position.get_z()-3)
        
        self.next_position = position
        self.get_logger().debug("received position %s at time %s" % (self.next_position, timestamp_adjusted))

        roll, pitch, yaw = quat2euler([
            message.pose.pose.orientation.w,
            message.pose.pose.orientation.x,
            message.pose.pose.orientation.y,
            message.pose.pose.orientation.z])
        self.next_orientation = [0, 0, math.degrees(yaw) + 90] # flat 2D map with the model offset by 90 degrees
        self.get_logger().debug("received orientation %s at time %s" % (self.next_orientation, timestamp_adjusted))

    def callback_brake(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)
        brake_torque = message.brake_torque_request

        if not self.is_clock_set:
            self.start_time = timestamp
            self.get_logger().info("Using %s as the reference start time" % self.start_time)
            self.is_clock_set = True

        timestamp_adjusted = timestamp - self.start_time
        if timestamp_adjusted > self.next_time: # TODO: check edge cases for when things are uninitialized
            self.advance_time() # this is not called when equal to allow messages for other topics to arrive
        
        self.next_brake_torque = brake_torque
        self.get_logger().debug("received brake torque %s at time %s" % (self.next_brake_torque, timestamp_adjusted))
        
    def callback_velocity(self, message):
        timestamp = TimeStamp(message.header.stamp.sec, message.header.stamp.nanosec)
        velocity = message.vehicle_velocity_propulsion

        if not self.is_clock_set:
            self.start_time = timestamp
            self.get_logger().info("Using %s as the reference start time" % self.start_time)
            self.is_clock_set = True

        timestamp_adjusted = timestamp - self.start_time
        if timestamp_adjusted > self.next_time: # TODO: check edge cases for when things are uninitialized
            self.advance_time() # this is not called when equal to allow messages for other topics to arrive
        
        self.next_velocity = velocity
        self.get_logger().debug("received velocity %s at time %s" % (self.next_velocity, timestamp_adjusted))

    def callback_terminate(self, message):
        if message.data:
            self.get_logger().info("Exiting due to /ds_bridge/terminate")
            # self.client_socket.send("-1\r\n".encode())
            sys.exit()

    def advance_time(self):
        packet_data = [
            self.next_position.get_x(),
            self.next_position.get_y(),
            self.next_position.get_z(),
            self.next_orientation[0], # x
            self.next_orientation[1], # y
            self.next_orientation[2], # z
            self.next_velocity,
            self.next_brake_torque]
        packet_string = str(self.next_time.get_seconds()) + ',' + str(self.next_time.get_nanoseconds()) + '\n'
        packet_string += ','.join(str(d) for d in packet_data)
        packet_string += "\r\n"

        # Send data to ns-3 and receive the response
        # self.client_socket.send(packet_string.encode())
        self.get_logger().debug("sent packet: %s" % packet_string)
        # response = self.client_socket.recv(4096).decode()
        # self.get_logger().info("received response: %s" % response)

        #if response == '1':
        #    stop_msg = String()
        #    stop_msg.data = 'stop'
        #    self.stop_publisher.publish(stop_msg)

        self.next_time += self.__get_timestep()
        self.get_logger().info("Waiting until clock advances to %s" % self.next_time)
    
    def __get_timestep(self):
        return TimeStamp(0, self.timestep_ms * 1000000)

def main(args=None):
    rclpy.init(args=args)
    node = DataSpeedNetworkBridge(100, True)
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
