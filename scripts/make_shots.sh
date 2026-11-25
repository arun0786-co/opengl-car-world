#!/bin/sh
# renders the extra readme frames headlessly and saves them as 1200 px jpgs.
# run from the repo root after make (macos, uses sips).
set -e
./car_world --shot docs/img/sky.png --camera 0 --sun 0.35 --seconds 2 --size 1440x900
./car_world --shot docs/img/orbit.png --camera 9 --orbit 40 --sun 0.8 --seconds 2 --size 1440x900
./car_world --shot docs/img/lamp_night.png --camera 4 --moon 0.5 --seconds 3 --size 1440x900
for f in sky orbit lamp_night; do
  sips -Z 1200 -s format jpeg -s formatOptions 85 docs/img/$f.png --out docs/img/$f.jpg >/dev/null
  rm docs/img/$f.png
done
