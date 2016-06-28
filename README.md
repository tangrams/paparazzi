![](imgs/5846.jpg)

# Tangram Paparazzi

Sneaky version of [Tangram-ES](https://github.com/tangrams/tangram-es) that loads a YAML scene file, take snapshots and runs away. Currently compiles in RaspberryPi and Ubuntu.

## Install

```bash
git clone --recursive https://github.com/tangrams/paparazzi.git
cd paparazzi
./install.sh
```

## Use paparazzi Node.js `server.js`

The Node.js `server.js` set a HTTP server that listen for calls and use the query calls to the URL path as arguments to construct a picture of a map using `paparazzi` binary.

To run the server simply do:

```bash
cd paparazzi
nam start
```

Then on a browser do something like

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

## Use `paparazzi` binary

You can use the paparazzi binary (`build/bin/./paparazzi`) with the following arguments. It will return an image of the specify YAML scene file at the given position, zoom, tilt and rotation.

| Argument       | Description                                |
|----------------|--------------------------------------------|
| `-s file.yaml` | Mandatory. Specify the YAML scene file |
| `-o image.png` | Mandatory. Output image file |
| `-w [number]`  | Width of the final image |
| `-h [number]`  | Height of the final image |
| `-lat [LAT]`	 | Latitud	  |
| `-lot [LON]`   | Longitud |
| `-z [zoom]`    | Zoom Level |
| `-t [tilt]`    | Tilt degree of the camera |
| `-r [rot]`     | Rotation degree of the map |


## Thins to read

- http://www.songho.ca/opengl/gl_fbo.html
- http://www.glfw.org/docs/latest/context.html#context_offscreen
- http://stackoverflow.com/questions/4041682/android-opengl-es-framebuffer-objects-rendering-to-texture
- http://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl
- http://stackoverflow.com/questions/12662227/opengl-es2-0-offscreen-context-for-fbo-rendering
- https://github.com/raspberrypi/userland/blob/master/interface/khronos/egl/egl_client.c#L680

- http://github.prideout.net/headless-rendering
- http://arrayfire.com/remote-off-screen-rendering-with-opengl/
- https://github.com/shehzan10/glContext
- http://renderingpipeline.com/2012/05/windowless-opengl/
- http://renderingpipeline.com/2012/05/remote-opengl/
- https://aws.amazon.com/marketplace/pp/B00FYCDDTE
