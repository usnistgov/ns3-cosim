# Co-Simulation of ns-3 Network Models

This repository contains software that enables the co-simulation of ns-3 network models with other simulation software.
Co-simulation is a modeling and simulation technique to develop high-fidelity simulations through the integration and
joint operation of multiple simulators running in parallel. A co-simulation requires middleware (this repository) to
define a shared time reference and the mechanisms for data exchange between simulators.

For a practical example, [Simulation of Urban Mobility (SUMO)](https://eclipse.dev/sumo/) is a popular traffic simulator
that can model transportation networks of on-road vehicles. An ns-3 model for vehicle-to-vehicle communications could
let SUMO handle node mobility rather than trying to implement its own approach to vehicle motion. This allows ns-3 to
focus on just the communication network, with any domain-specific functionality outsourced to specialized simulators
such as SUMO. In a co-simulation, the integration between ns-3 and SUMO is online where both simulators run in parallel
with periodic synchronization at runtime for data exchange. This is more dynamic than offline approaches that exchange
pre-recorded data, as the performance of the communications network in ns-3 can affect the on-going traffic simulation
in SUMO and vice versa.

The following new classes are provided:
  - a [gateway](model/gateway.h) for integrating ns-3 with other software using a local TCP/IP socket connection
  - a [triggered send application](model/triggered-send-application.h) that lets external code broadcast messages
  - an [external mobility model](model/external-mobility-model.h) that lets external code manage ns-3 node mobility

# Gateway Architecture

(will be updated)

## Data Exchange

(will be updated)

## Time Management

(will be updated)

# Installation

This code was developed for an ns-3 fork that supports vehicle-to-everything (V2X) communications, which is co-located
with the ns-3 [3GPP NR module](https://gitlab.com/cttc-lena). This code was not tested with, and may not support,
other versions of ns-3 (including the official releases).

## Requirements

This code was tested using:
  - Ubuntu 22.04
  - [ns-3-dev-v2x-v1.1](https://gitlab.com/cttc-lena/ns-3-dev/-/tree/ns-3-dev-v2x-v1.1)
  - [nr-v2x-v1.1](https://gitlab.com/cttc-lena/nr/-/tree/v2x-1.1)

These instructions require:
  - cmake 3.13 (or newer)
  - g++ 10.5.0 (or newer)
  - git (any version)
  - make (any version)
  - python 3.8 (or newer)

## Download Dependencies

Run the following commands, ignoring any comments (lines starting with #):

    # install the dependencies for the ns-3 NR module
    apt install libc6-dev libeigen3-dev sqlite sqlite3 libsqlite3-dev

    # download the custom ns-3 development branch for V2X communications
    git clone --branch ns-3-dev-v2x-v1.1 https://gitlab.com/cttc-lena/ns-3-dev.git
    
    # download additional ns-3 modules
    cd ns-3-dev/contrib
    git clone https://github.com/usnistgov/ns3-cosim.git

    # optional (for V2X examples using sidelink)
    git clone --branch v2x-1.1 https://gitlab.com/cttc-lena/nr.git
    # optional (for visualization of network models)
    git clone https://github.com/usnistgov/NetSimulyzer-ns3-module netsimulyzer

Ensure the directory structure matches, exactly:

    ns-3-dev
     - contrib
      - netsimulyzer
      - nr
      - ns3-cosim

## Compile ns-3

From the ns-3-dev directory, run:

    ./ns3 configure --enable-tests --enable-examples

Verify that the output under `Modules configured to be built:` includes `ns3-cosim`.

From the same ns-3-dev directory, run:

    ./ns3 build

# Examples

All examples must be run from the root `ns-3-dev` directory, which is not the directory where this README is located.

## External Mobility Example

The [external mobility example](examples/external-mobility-example.cc) shows how to set the position and velocity of
the new external mobility model. It can be run with the command:

    ./ns3 run external-mobility-example

This example contains two nodes. A periodic function is scheduled to update the mobility of each node. When a mobility
model is updated, its current position and velocity information is output to the ns-3 logger. The updates are:
  - Node 0 is updated every 2 seconds to increase the x-dimension of its position and velocity by 1.
  - Node 1 is updated every 1 second to increase the z-dimension of its position and velocity by 1.

## Triggered Send Example

The [triggered send example](examples/triggered-send-example.cc) shows how to start sending messages using the new
triggered send application, and its behavior when triggered under different conditions. It can be run with the command:

    ./ns3 run triggered-send-example

This example contains two nodes connected by a point-to-point channel. The first node (`192.168.0.1`) is a client with
a triggered send application, and the second node (`192.168.0.2`) is a server with a packet sink application. The first
node is triggered to send messages to the packet sink at different times during the simulation. When the packet sink
receives these messages, it outputs the current time to the ns-3 logger.

The `TriggeredSendApplication::Send` function takes the number of packets to send as an argument. The application also
has a `PacketInterval` attribute that specifies the time delay between sending two consecutive packets. In this example,
the client attempts to send 5 packets with a 200 ms packet interval. Refer to the code for the 4 different cases shown
in this example, and why in some cases the client doesn't send all 5 packets.

## Simple Gateway

This example shows how to create a simple gateway to communicate with external code. It consists of an ns-3 model that
implements the [gateway](examples/simple-gateway.cc) and a simple server that represents the
[external code](examples/simple-gateway-server.cc).

First, run the simple server:

    ./ns3 run simple-gateway-server

Then, run the ns-3 model in a separate terminal:

    ./ns3 run simple-gateway

The server by default runs a 20 time step simulation of 3 vehicles, where the position and velocity information for the
vehicles are randomized each step. Every 5 time steps (starting at step 6), the vehicles have a chance to broadcast a
message to the network. The server starts at time 0, with a step size of 1 second.

The ns-3 model contains 3 nodes to represent the vehicles, and the gateway implementation. Each node has an external
mobility model, a triggered send application for sending packets, and a packet sink for receiving packets. The gateway
implementation connects to the simple server, and uses the position and velocity information received from the server
to update the external mobility models. When the server indicates one of the vehicles has started broadcasting, the
gateway triggers the corresponding triggered send application. When a packet sink receives a packet, it outputs the
current simulation time to the ns-3 logger.

This example includes command line arguments to adjust the behavior of the server and the gateway. To specify the
command line arguments (and to see the list of possible arguments), use the format:

    ./ns3 run "simple-gateway --help"
    ./ns3 run "simple-gateway-server --help"
    ./ns3 run "<program_name> --<option_name>=<value>"

# Additional Information

## Third-Party Licenses

This repository includes modified ns-3 source code distributed under the GNU General Public License, Version 2.
All modified files include their original attributions in a comment header, and their respective licensing statements
can be found in the [LICENSES](licenses) directory.

## Contact Information

This repository is mainted by:
  - Thomas Roth (@tpr1)

## Citation Information

(will be updated when available)

## References

ns-3 NR module
: https://gitlab.com/cttc-lena/nr

ns-3 NetSimulyzer module
: https://github.com/usnistgov/NetSimulyzer-ns3-module

NetSimulyzer standalone
: https://github.com/usnistgov/NetSimulyzer
