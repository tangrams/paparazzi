![](imgs/5846.jpg)

# Tangram Paparazzi

Sneaky version of [Tangram-ES](https://github.com/tangrams/tangram-es) that loads a YAML scene file, take snapshots and runs away. Currently compiles in RaspberryPi and Ubuntu.

## Install

```bash
git clone --recursive https://github.com/tangrams/paparazzi.git
cd paparazzi
./setup
./start
```

## Use

The Node.js `server.js` set a HTTP server that listen for calls and use the query calls to construct a picture of a map. 

How that looks like? After running paparazzi with `./start` go to your browser 

```
http://localhost:8080/?zoom=17&lat=40.7086&lon=-73.9924&scene=https://dl.dropboxusercontent.com/u/335522/openframe/tangram/blueprint.yaml
```

Here is a list of arguments to pass to the URL

| Query Arguments   | Description                                |
|-------------------|------------------------------------------|
| `scene=[url]`   | Specify a valid url to the YAML scene file |
| `width=[number]`  | Width of the final image |
| `heigth=[number]` | Height of the final image |
| `lat=[LAT]`       | Latitud    |
| `lot=[LON]`       | Longitud |
| `zoom=[zoom]`     | Zoom Level |
| `tilt=[tilt deg]` | Tilt degree of the camera |
| `rot=[rot deg]`   | Rotation degree of the map |
| `key=[API_KEY]`   | Mapzen API_KEY |
