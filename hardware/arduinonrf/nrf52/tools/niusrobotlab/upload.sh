#!/bin/sh

mode="$1"
shift

if [ "$mode" = "dfu" ]; then
  exec "$@"
fi

exec "$@"