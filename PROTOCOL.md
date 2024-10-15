Anycubic ACE Pro Protocol
=========================

Transport
=========

The ACE Pro talks over USB using a USB CDC device, with no flow control or data
integrity checking. It seems to share a single ringle buffer for input and
output, and sending packets too fast may drop data. Sending packets before
waiting for a response may lose output the printer was sending. The maximum
safe amount to send within a small time span size seems to be 1024 bytes, but
this may change in the future.

Framing
=======

Each JSON command is packed in a frame of the following format:

- 2 bytes: 0xFF 0xAA
- 2 bytes: Payload length (little endian)
- The JSON itself
- 2 bytes: CRC-16/MCRF4XX code of the JSON (little endian)
- Any number of bytes, ignored for now
- 1 byte: 0xFE

The ACE will disconnect and reconnect if no frame has been completely sent 3
seconds, regardless of whether the frame data has a valid length or CRC. The
keepalive does disregard data from the previous connection: Frames can be split
across multiple connections.

The header is two bytes, so in the case of one of the bytes getting corrupted
the frame will be ignored, unless the frame contains 0xFF 0xAA in it. If the
header gets corrupted and frame contains 0xFF 0xAA in it the ACE may freeze for
a while trying to read a large frame. You can try and prevent this by
re-generating a payload until the CRC does not match 0xFF 0xAA (little endian).

If a long frame (greater than 1024 bytes) is requested either by accident or on
purpose, the ACE seems to freeze and enter an unrecoverable state. No amount of
data send to complete the frame's payload unfreezes the machine.

RPC
===

**WARNING**: None of this has been tested and may be entirely wrong.

Each request is sent to the ACE Pro containing the following JSON data:

- id: The message number
- method: A string identifying the method
- params: Dictionary of method-specific parameters

Each response is sent from the ACE containing the following JSON data:

- id: The request's message number
- result: Dictionary of method-specific return data
- code: Method-specific return code
- msg: Method-specific message

Make sure to lock access to the ACE when sending a request and reading a
response. It's easy to overlook this if you have a background thread that
works to keep the connection alive by sending a command every second.

Methods
=======

This section documents the methods you can call and the data you'll get
back. For values not known, static values are listed.

This is not a comprehensive API documentation as we don't control the
firmware or have any authority over the ACE or its future updates.

enable_rfid
-----------

Request params:

- None

Response data:
- msg: "success"
- code: 0

Response params:
- None

disable_rfid
------------

Request params:

- None

Response data:
- msg: "success"
- code: 0

Response params:
- None

get_info
--------

Request params:

- None

Response data:
- msg: "success"
- code: 0

Response params:

- id: 0
- slots: 4
- model: "Anycubic Color Engine Pro"
- firmware: "V1.3.82"
- boot_firmware: "V1.0.1"

get_filament_info
-----------------

Request params:

- index: Filament slot number

Response data:
- msg: "success"
- code: 0

Response params:

- index: Filament slot number
- sku: ""
- brand: ""
- type: ""
- color: [0,0,0]
- rfid: 1
- extruder_temp: Dictionary of temperature data
- hotbed_temp: Dictionary of temperature data
- diameter: 0.00
- total: 330
- current: 0

Temperature data dictionary:

- min: 0
- max: 0

get_status
----------

Request params:

- None

Response data:
- msg: "success"
- code: 0

Response params:

- status: "ready"
- dryer_status: Dictionary of dryer status
- temp: Dryer temperature in celsius
- enable_rfid: 1
- fan_speed: 7000
- feed_assist_count: 0
- cont_assist_time: 0.0
- slots: Array of dictionary of slot status

Dryer status dictionary:
- status: "stop" or "drying"
- target_temp: 0
- duration: 0
- remain_time: 0

Slot status dictionary:
- index: Filament slot number
- status: "ready"
- sku: ""
- type: ""
- color: [0,0,0]
- rfid: 1

drying
------

Request params:

- temp: Dryer temperature in celsius
- fan_speed: 7000
- duration: 240

Response data:
- msg: "drying"
- code: 0

Response params:
- None

drying_stop
-----------

Request params:

- None

Response data:
- msg: "success"
- code: 0

Response params:
- None

unwind_filament
---------------

Request params:

- index: Filament slot number
- length: 300, 70
- speed: 10, 15
- mode: 0

Response data:
- msg: "success"
- code: 0

Response params:
- None

update_unwinding_speed
----------------------

Request params:

- index: Filament slot number
- speed: 15

Response data:
- msg: "success"
- code: 0

Response params:
- None

stop_unwind_filament
--------------------

Request params:

- index: Filament slot number

Response data:
- msg: "success"
- code: 0

Response params:
- None

feed_filament
-------------

Request params:

- index: Filament slot number
- length: 2000
- speed: 25

Response data:
- msg: "success"
- code: 0

Response params:
- None

update_feeding_speed
--------------------

Request params:

- index: Filament slot number
- speed: 25

Response data:
- msg: "success"
- code: 0

Response params:
- None

stop_feed_filament
------------------

Request params:

- index: Filament slot number

Response data:
- msg: "success"
- code: 0

Response params:
- None

start_feed_assist
-----------------

Request params:

- index: Filament slot number

Response data:
- msg: "success"
- code: 0

Response params:
- None

stop_feed_assist
----------------

Request params:

- index: Filament slot number

Response data:
- msg: ""
- code: 0

Response params:
- None
