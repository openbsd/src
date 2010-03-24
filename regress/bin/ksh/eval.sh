#!/bin/sh
#
# $Id: eval.sh,v 1.1 2010/03/24 08:29:44 fgsch Exp $

for n in ${a#*=}; do echo ${n}; done                                      
for n in "${a#*=}"; do echo ${n}; done                                      

for n in ${a##*=}; do echo ${n}; done                                      
for n in "${a##*=}"; do echo ${n}; done                                      

for n in ${a%=*}; do echo ${n}; done                                      
for n in "${a%=*}"; do echo ${n}; done                                      

for n in ${a%%=*}; do echo ${n}; done                                      
for n in "${a%%=*}"; do echo ${n}; done                                      
