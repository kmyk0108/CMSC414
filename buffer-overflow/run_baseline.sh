#! /bin/bash

docker run -d --name stack -v "$(pwd):/opt" -v "$(pwd)/secret:/var/secret" baseline tail -f /etc/issue

