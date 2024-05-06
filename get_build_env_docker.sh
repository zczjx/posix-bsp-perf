#!/bin/bash

# build base docker image(optional)
docker build -t zczjx/bsp-perf-build-env-x86-base:latest -f Dockerfile.base .

# build docker image
docker pull zczjx/bsp-perf-build-env-x86-base:latest

docker build --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) --build-arg DIR_SRC_PATH=tools_conf -t bsp-perf-build-env-x86:v0.0.1 -f Dockerfile .

# run build docker image

docker run --network=host --gpus all --privileged --user $(id -u):$(id -g) -it -v /build:/build -v /dev/bus/usb:/dev/bus/usb bsp-perf-build-env-x86:v0.0.1 /bin/bash

docker exec -it <container-id-or-name> /bin/bash

# run base docker image(if required to update base image for enviroment modification)
docker run --network=host --gpus all --privileged  -it -v /build:/build -v /dev/bus/usb:/dev/bus/usb zczjx/bsp-perf-build-env-x86-base:latest /bin/bash

docker commit -m "update docker base" <container_id> zczjx/bsp-perf-build-env-x86-base:latest

docker login

docker push zczjx/bsp-perf-build-env-x86-base:latest