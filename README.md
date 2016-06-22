# Tangram Paparazzi

Sneaky version of [Tangram-ES](https://github.com/tangrams/tangram-es) to take snapshots of Tangram using a RaspberryPi.


## Install

Compile `tangramPaparazzi`:

```bash
sudo apt-get update
sudo apt-get install make libcurl4-openssl-dev
export CXX=/usr/bin/g++-4.9

cd ~
git clone --recursive https://github.com/tangrams/paparazzi.git
cd paparazzi
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make
cd bin
sudo cp tangramPaparazzi /usr/local/bin
```

Install and run a PHP server:

```bash
sudo apt-get install php5-common php5-cgi php5
cd ~/paparazzi
php -S localhost:8000
```

## Use

Run `tangramPaparazzi` with the following arguments and will give you back an image of the specify YAML scene file at the given position, zoom, tilt and rotation.

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

