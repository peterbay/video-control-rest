
# video-control-rest

Small embedded HTTP server with REST API for querying Linux video devices and setting video controls.


## Dependencies
Both dependencies (Mongoose and mjson) are's included.

[Mongoose - Embedded Web Server / Embedded Networking Library](https://github.com/cesanta/mongoose)

[mjson - a JSON parser + emitter + JSON-RPC engine](https://github.com/cesanta/mjson)

## Build
```
    # Just build
    make

    # Build && execute
    make test
```

## Usage

### Commandline arguments
```
    -d             Enable debug log messages
    -h             Print this help screen and exit
    -i address     IP address for listening
    -p port        Port for listening (number between 80 and 65535)
```

### Default settings
```
    IP address = 0.0.0.0
    PORT       = 8800
```

## REST API

### Available url / commands

| Method | Url | Data | Description |
| :----- | :-- | :--- | :---------- |
|GET|/devices||List devices|
|GET|/device/formats/{device_name}||List available formats for selected device|
|GET|/device/format/{device_name}||Get actual format for selected device|
|GET|/device/control/{device_name}||Get settings for available controls from selected device|
|POST|/device/control/{device_name}|{"brightness": 80, "color_effects": 9}|Set video control to specific value for selected device|

Notice: device_name may be video0 .. videoXX

## -- List devices --

#### REQUEST 
```
curl --request GET http://127.0.0.1:8800/devices
```

#### RESPONSE
Info: the response is shortened due to better readability
```json
 {
   "video0":{
      "driver":"bm2835 mmal",
      "card":"mmal service 16.1",
      "bus_info":"platform:bcm2835-v4l2-0",
      "version":"330265",
      "capabilities":[
         "VIDEO_CAPTURE",
         "VIDEO_OVERLAY",
         "READWRITE",
         "STREAMING",
         "DEVICE_CAPS"
      ]
   },
   "video1":{
      "driver":"g_uvc",
      "card":"20980000.usb",
      "bus_info":"gadget",
      "version":"330265",
      "capabilities":[
         "VIDEO_OUTPUT",
         "STREAMING",
         "DEVICE_CAPS"
      ]
   }
}
```

## -- List available formats for selected device --

#### REQUEST
```
curl --request GET http://127.0.0.1:8800/device/formats/video0
```

#### RESPONSE
Info: the response is shortened due to better readability
```json
 {
   "VIDEO_CAPTURE":{
      "H264":{
         "type":"STEPWISE",
         "min_width":"32",
         "min_height":"32",
         "max_width":"4056",
         "max_height":"3040",
         "step_width":"2",
         "step_height":"2"
      },
      "MJPG":{
         "type":"STEPWISE",
         "min_width":"32",
         "min_height":"32",
         "max_width":"4056",
         "max_height":"3040",
         "step_width":"2",
         "step_height":"2"
      },
   },
   "VIDEO_OVERLAY":{
       "JPEG":{
         "type":"STEPWISE",
         "min_width":"32",
         "min_height":"32",
         "max_width":"4056",
         "max_height":"3040",
         "step_width":"2",
         "step_height":"2"
      },
      "H264":{
         "type":"STEPWISE",
         "min_width":"32",
         "min_height":"32",
         "max_width":"4056",
         "max_height":"3040",
         "step_width":"2",
         "step_height":"2"
      }
   }
}
```

## -- Get actual format for selected device --

#### REQUEST
```
curl --request GET http://127.0.0.1:8800/device/format/video0
```

#### RESPONSE
Info: the response is shortened due to better readability
```json
{
   "VIDEO_CAPTURE":{
      "pix":{
         "width":"1920",
         "height":"1080",
         "pixelformat":"MJPG",
         "field":"NONE",
         "bytesperline":"0",
         "sizeimage":"2088960",
         "colorspace":"SMPTE170M",
         "priv":"-17970434",
         "flags":"0"
      }
   }
}
```

## -- Get settings for available controls from selected device --

#### REQUEST
```
curl --request GET http://127.0.0.1:8800/device/control/video0
```

#### RESPONSE
Info: the response is shortened due to better readability
```json
{
   "brightness":{
      "minimum":"0",
      "maximum":"100",
      "default":"50",
      "step":"1",
      "value":"30",
      "menu":{
         
      }
   },
   "contrast":{
      "minimum":"-100",
      "maximum":"100",
      "default":"0",
      "step":"1",
      "value":"50",
      "menu":{
         
      }
   },
   "color_effects":{
      "minimum":"0",
      "maximum":"15",
      "default":"0",
      "step":"1",
      "value":"0",
      "menu":{
         "0":"None",
         "1":"Black & White",
         "2":"Sepia",
         "3":"Negative",
         "4":"Emboss",
         "5":"Sketch",
         "6":"Sky Blue",
         "7":"Grass Green",
         "8":"Skin Whiten",
         "9":"Vivid",
         "10":"Aqua",
         "11":"Art Freeze",
         "12":"Silhouette",
         "13":"Solarization",
         "14":"Antique",
         "15":"Set Cb/Cr"
      }
   },
   "video_bitrate":{
      "minimum":"25000",
      "maximum":"25000000",
      "default":"10000000",
      "step":"25000",
      "value":"10000000",
      "menu":{
         
      }
   },
}
```

## -- Set video control to specific value for selected device --

#### REQUEST
Set brightness to value 80 and color_effects to value 9 (Vivid)

```
curl --header "Content-Type: application/json" --request POST --data '{"brightness": 80, "color_effects": 9}' http://127.0.0.1:8800/device/control/video0
```

#### RESPONSE
```json
{
   "brightness": 80,
   "color_effects": 9
}
```

## Licences

### video-control-rest
 - video-control-rest is released under GNU General Public License version 2
 - Copyright (c) 2021 Petr Vavřín

### Mongoose
 - Mongoose is released under GNU General Public License version 2
 - Copyright (c) 2004-2013 Sergey Lyubka
 - Copyright (c) 2013-2021 Cesanta Software Limited
 - All rights reserved

### mjson
 - mjson is released under MIT License
 - Copyright (c) 2018 Cesanta Software Limited

