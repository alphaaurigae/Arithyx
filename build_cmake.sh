#!/bin/bash


BOLD='\033[1m'
BRIGHT_WHITE='\033[1;37m'
RESET='\033[0m' 

BUILD_DIR="build"
BIN_DIR='bin'
BIN_NAME='universe'


configure() {
    echo -e "${BOLD}${BRIGHT_WHITE}Create build directories and config cmake${RESET}"
    mkdir -p ${BUILD_DIR}
sync
    cmake -S . -B ${BUILD_DIR} -DCMAKE_VERBOSE_MAKEFILE=ON
sync
}

build() {
    echo -e "${BOLD}${BRIGHT_WHITE}Building project...${RESET}"
sync
    cmake --build ${BUILD_DIR} --target all -- -j$(nproc) --debug
sync
}


./clean_cmake.sh

configure
build
