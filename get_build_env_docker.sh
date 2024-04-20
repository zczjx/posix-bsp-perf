#!/bin/bash

docker run -it -P --mount type=bind,source=/nvidia/code/clarencez/docker_path,target=/mnt zczjx/bsp-perf-build-env-x86:v0.0.1 /bin/bash

docker run -it -v /nvidia/code/clarencez/docker_path:/mnt zczjx/bsp-perf-build-env-x86:v0.0.1 /bin/bash

docker run -it -v /nvidia/code/clarencez/docker_path:/mnt bsp-perf-build-env-x86:latest /bin/bash

docker run -it -P --mount type=bind,source=/nvidia/code/clarencez/docker_path,target=/mnt bsp-perf-build-env-x86:latest /bin/bash


docker run -it -P --mount type=bind,source=/home/build,target=/mnt ubuntu:20.04 /bin/bash

docker run -it -P --mount type=bind,source=/home/clarencez,target=/mnt ubuntu:20.04 /bin/bash

docker run -it -P --mount type=bind,source=/nvidia/code/clarencez,target=/mnt ubuntu:20.04 /bin/bash