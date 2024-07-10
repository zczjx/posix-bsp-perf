# posix-bsp-perf
bsp perf test for posix OS(such as Linux/QNX/Rtthread)

## Docker Build Env Setup

please reference the cmd in [get_build_env_docker.sh](./get_build_env_docker.sh)

- Dockerfile.base is for the base docker image generating
- Dockerfile will pull the full docker image of Dockerfile.base and config the local env


## Local native build Env Setup

please reference the cmd in [local_env_pkgs_intall.sh](./local_env_pkgs_intall.sh) to install the packages to setup
the local native build env if required(or you don't like to use docker)

## Reference Build cmd

please reference the cmd in [reference_build_cmd.sh ](./reference_build_cmd.sh ) to get the reference build cmd

## BSP Trace Event Visualization

upload the *.perfetto profiler file to [perfetto](https://ui.perfetto.dev/) to analysis the perf data at local web browser

- BspTrace visualization demo

  ![Perfetto demo](image/perfetto.PNG)

## Tracing Branch

Tracing branch(``git checkout -b tracing origin/tracing``) is for deep tracing in the src code, will leave so many trace event tags in the code for analysis
User could do ``git pull origin master --rebase`` to sync and merge the latest master ToT to tracing branch
