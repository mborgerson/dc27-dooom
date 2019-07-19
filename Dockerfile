FROM ubuntu:18.04

RUN apt-get update
RUN apt-get install -y \
	build-essential \
	flex \
	bison \
	clang \
	lld \
	mingw-w64 \
	mingw-w64-tools \
	mingw-w64-i686-dev \
	git \
	cmake \
	python \
	python-pil \
	deutex \
	sudo

RUN useradd -m user
RUN echo "user:user" | chpasswd && adduser user sudo

USER user
