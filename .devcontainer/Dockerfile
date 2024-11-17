FROM ubuntu:rolling

ARG USER

RUN apt update
RUN apt install -y sudo wget git pkgconf
RUN apt install -y build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev libsox-dev git libasound2-dev nasm g++-14
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN echo "deb http://apt.llvm.org/oracular/ llvm-toolchain-oracular main" | tee -a /etc/apt/sources.list
RUN apt update
RUN apt install -y clangd
RUN deluser ubuntu || echo
RUN useradd -m ${USER}
RUN echo ${USER} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USER} \
    && chmod 0440 /etc/sudoers.d/${USER}
