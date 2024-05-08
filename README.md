# posix-bsp-perf
bsp perf test for posix OS(such as Linux/QNX/Rtthread)

## Docker Build Env Setup

please reference the cmd in [get_build_env_docker.sh](./get_build_env_docker.sh)

- Dockerfile.base is for the base docker image generating
- Dockerfile will pull the full docker image of Dockerfile.base and config the local env


## Local native build Env Setup

please reference the cmd in [local_env_pkgs_intall.sh](./local_env_pkgs_intall.sh) to install the packages to setup
the local native build env if required(or you don't like to use docker)
