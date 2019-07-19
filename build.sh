#!/bin/bash
set -x
set -e

# git submodule init
# git submodule update --recursive
# git -C nxdk submodule init
# git -C nxdk submodule update --recursive

DOCKER_IMAGE_NAME="dcdoom"

if [[ `docker images | grep -o $DOCKER_IMAGE_NAME` != "$DOCKER_IMAGE_NAME" ]]; then
	echo "[*] Docker image not found... building"
	docker build -t dcdoom .
fi

echo "[*] Building WAD"
docker run -it --rm -v $PWD:/work -w /work $DOCKER_IMAGE_NAME make DEUTEX=/usr/games/deutex -C freedoom

echo "[*] Building..."
docker run -it --rm -v $PWD:/work -w /work $DOCKER_IMAGE_NAME bash -c "source ./init_tree.sh && make V=1"
docker run -it --rm -v $PWD:/work -w /work $DOCKER_IMAGE_NAME bash -c "source ./init_tree.sh && make -C agent V=1"

echo "[*] Build complete"
echo "    > xbe: bin/default.xbe"
echo "    > wad: bin/freedm.wad"
echo "    > iso: dcdoom.iso"
