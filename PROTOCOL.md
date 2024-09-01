Anycubic ACE Pro Protocol
=========================

The ACE Pro talks over as USB CDC device using a JSON API.

**WARNING**: None of this has been tested and may be entirely wrong.

Framing
=======

Each JSON command is packed in a frame of the following format:

- 2 bytes: 0xFF 0xAA
- 2 bytes: Payload length (little endian)
- The JSON itself
- 2 bytes: A CRC code of the JSON
- 1 byte: 0xFE

RPC
===

Each request is sent to the ACE Pro containing the following JSON data:

- id: The message number
- method: A string identifying the method
- params: Dictionary of method-specific parameters

Each response is sent from the ACE containing the following JSON data:

- id: The request's message number
- result: Dictionary of method-specific return data
- code: Method-specific return code
- msg: Method-specific message

Methods
=======

This section documents the methods you can call and the data you'll get
back. For values not known, static values are listed.

This is not a comprehensive API documentation as we don't control the
firmware or have any authority over the ACE or its future updates.

get_status
----------

Request params:

- None

Response data:
- msg: "ready"
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
- status: "stop"
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
