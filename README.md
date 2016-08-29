![](imgs/5846.jpg)

# Tangram Paparazzi

Repurposed version of [Tangram-ES](https://github.com/tangrams/tangram-es) to take pictures of maps (static maps). Designed to run on servers in a reliable, multi-thread and headless fashonable way. How? Thanks to the imperssive [```prime_server```](https://github.com/kevinkreiser/prime_server) develop by [Kevin Kreiser](https://twitter.com/kevinkreiser).

Currently compiles in Amazon GPU Server, Rasbian, Ubuntu and Darwin OSX.

## Install Paparazzi

```bash
git clone --recursive https://github.com/tangrams/paparazzi.git
cd paparazzi
./paparazzi.sh install
```

If you are trying things on the code and just want to compile do

```bash
./paparazzi.sh make
```

**Note**: if you want to make a xcode project do: `./paparazzi.sh make xcode`

## Runing Paparazzi

Once paparazzi is compile you can use the `paparazzi.sh` script to:

* **start** (`./paparazzi.sh start [N_THREADS]`) runs [```prime_server```](https://github.com/kevinkreiser/prime_server), ```prime_proxy```, and N instances of ```paparazzi_thread```
* **stop** (`./paparazzi.sh stop`) stops [```prime_server```](https://github.com/kevinkreiser/prime_server), ```prime_proxy```, and all ```paparazzi_thread```
* **restart** (`./paparazzi.sh restart`) do `stop` and `start`
* **add** (`./paparazzi.sh add [N_THREADS]`) add N instances of ```paparazzi_thread```
* **status** (`./paparazzi.sh status`) do a `ps` for [```prime_server```](https://github.com/kevinkreiser/prime_server), ```prime_proxy``` and ```paparazzi_thread```

## URL calls 

```
http://localhost:8000/?lat=40.7053&lon=-74.0098&zoom=16&width=1000&height=1000&scene=https://tangrams.github.io/tangram-sandbox/styles/default.yaml
```

### Query arguments

| Arguments         | Req | Description                                   |
|-------------------|-----|-----------------------------------------------|
| `scene=[url]`     |  Y  | Specify a valid url to the YAML scene file    |
| `width=[number]`  |  Y  | Width of the final image                      |
| `heigth=[number]` |  Y  | Height of the final image                     |
| `lat=[LAT]`       |  Y  | Latitud                                       |
| `lot=[LON]`       |  Y  | Longitud                                      |
| `zoom=[zoom]`     |  Y  | Zoom Level                                    |
| `tilt=[deg]`      |  N  | Tilt degree of the camera                     |
| `rotation=[deg]`  |  N  | Rotation degree of the map                    |
