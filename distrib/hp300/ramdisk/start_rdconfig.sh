echo rdconfig ${1} ${2}
rdconfig ${1} ${2} &
echo  $! >rd.pid 

