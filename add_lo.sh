#!/bin/bash

if [ $# -ne "1" ]
then
    echo "Provide an outside interface as a single argument."
    exit
fi

sudo ifconfig lo:1 1.1.1.1 netmask 255.255.255.255 up

sudo /sbin/iptables -t nat -A POSTROUTING -o $1 -j MASQUERADE
sudo /sbin/iptables -A FORWARD -i $1 -o lo:1 -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo /sbin/iptables -A FORWARD -i lo:1 -o $1 -j ACCEPT
