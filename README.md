# Tangram Paparazzi

Sneaky version of [Tangram-ES](https://github.com/tangrams/tangram-es) to take snapshots of Tangram using a RaspberryPi.


## Install

Compile `tangramPaparazzi`:

```bash
git clone --recursive https://github.com/tangrams/paparazzi.git
cd paparazzi
./install.sh
cd build
make
```

## Use `tangramPaparazzi`

Run `build/bin/./paparazzi` with the following arguments and will give you back an image of the specify YAML scene file at the given position, zoom, tilt and rotation.

| Argument       | Description                                |
|----------------|--------------------------------------------|
| `-s file.yaml` | Mandatory. Specify the YAML scene file |
| `-o image.png` | Mandatory. Output image file |
| `-w [number]`  | Width of the final image |
| `-w [number]`  | Height of the final image |
| `-lat [LAT]`	 | Latitud	  |
| `-lot [LON]`   | Longitud |
| `-z [zoom]`    | Zoom Level |
| `-t [tilt]`    | Tilt degree of the camera |
| `-r [rot]`     | Rotation degree of the map |

## Use Node `server.js`

