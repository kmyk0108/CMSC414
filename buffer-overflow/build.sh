#! /bin/bash

docker run --rm -v "$(pwd):/opt" baseline make -C /opt

