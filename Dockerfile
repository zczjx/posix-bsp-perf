FROM zczjx/bsp-perf-build-env-x86-base:latest

RUN apt-get update
# 定义构建参数
ARG USER_ID
ARG GROUP_ID
ARG DIR_SRC_PATH

# 创建一个新的用户组和用户，使用传入的用户ID和组ID
# 如果GID/UID已存在，则重用并重命名
RUN getent group ${GROUP_ID} || groupadd -g ${GROUP_ID} builder

RUN if getent passwd ${USER_ID} > /dev/null 2>&1; then \
        EXISTING_USER=$(getent passwd ${USER_ID} | cut -d: -f1); \
        if [ "$EXISTING_USER" != "builder" ]; then \
            usermod -l builder -d /home/builder -m -g ${GROUP_ID} $EXISTING_USER 2>/dev/null || true; \
        fi; \
    else \
        useradd -l -u ${USER_ID} -g ${GROUP_ID} -m -s /bin/bash builder; \
    fi

# 设置用户builder的密码
RUN echo 'builder:builder' | chpasswd

# 配置sudo免密码
RUN echo 'builder ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

# 设置工作目录
WORKDIR /home/builder

RUN chown -R builder:${GROUP_ID} /home/builder


# 切换到新用户
USER builder

# 复制文件到容器(optinal)， 主要是复制shell 和 vim 配置，让终端好看一点
# COPY ${DIR_SRC_PATH}/.bashrc /home/builder/.bashrc
# COPY ${DIR_SRC_PATH}/.profile /home/builder/.profile
# COPY ${DIR_SRC_PATH}/.vimrc /home/builder/.vimrc
# COPY ${DIR_SRC_PATH}/.vim /home/builder/.vim
# COPY ${DIR_SRC_PATH}/.gitconfig  /home/builder/.gitconfig
# 容器启动命令
CMD ["echo", "posix-bsp-perf cross build docker launch!"]