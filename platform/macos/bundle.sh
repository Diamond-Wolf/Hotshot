#!/bin/bash

build="${3}/bin/${2}"
plat="${1}/platform/macos"

app="${build}/Hotshot.app"
mkdir "${app}"

app="${app}/Contents"
mkdir "${app}"
mkdir "${app}/MacOS"
mkdir "${app}/Resources"

cp "${build}/Hotshot" "${app}/MacOS/Hotshot" 
cp "${plat}/Info.plist" "${app}/Info.plist"
cp "${plat}/Descent2.icns" "${app}/Resources/Hotshot.icns"