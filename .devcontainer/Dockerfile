FROM --platform=linux/amd64 ubuntu:rolling

ARG USER

RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
RUN echo "Types: deb \n\
URIs: http://apt.llvm.org/questing/ \n\
Suites: llvm-toolchain-questing \n\
Components: main \n\
Signed-By: /etc/apt/trusted.gpg.d/apt.llvm.org.asc\n" > /etc/apt/sources.list.d/llvm.sources
RUN cat /etc/apt/sources.list.d/llvm.sources
RUN apt update
RUN apt install -y sudo wget git pkgconf clangd build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev libsox-dev git libasound2-dev nasm g++-14
RUN deluser ubuntu || echo
RUN useradd -m ${USER}
RUN echo ${USER} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USER} \
    && chmod 0440 /etc/sudoers.d/${USER}
