FROM zczjx/bsp-perf-build-env-x86-base:latest

# 定义构建参数
ARG USER_ID
ARG GROUP_ID

# 创建一个新的用户组和用户，使用传入的用户ID和组ID
RUN groupadd -g ${GROUP_ID} builder && \
    useradd -l -u ${USER_ID} -g builder builder

# 设置工作目录
WORKDIR /home/builder

# 切换到新用户
USER builder

# 容器启动命令
CMD ["echo", "posix-bsp-perf cross build docker launch!"]