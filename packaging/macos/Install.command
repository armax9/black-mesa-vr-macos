#!/bin/bash
set -e
cd "$(dirname "$0")"
python3 install.py
printf '\nPress Return to close.\n'
read -r
