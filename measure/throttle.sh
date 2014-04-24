if [ -z "$1" ]; then echo "Enter [throttle speed] or rm!"; exit 0; fi
sudo tc qdisc del dev lo root
if [ "$1" = "rm" ]; then exit 0; fi

sudo tc qdisc add dev lo root handle 1: htb default 1
sudo tc class add dev lo parent 1: classid 1:1 htb rate $1
if [ -n "$2" ]; then
	sudo tc qdisc add dev lo parent 1:1 netem delay ${2}ms
fi
#sudo tc filter add dev lo parent 1: protocol ip prio 1 u32 match ip dst 0.0.0.0/0 flowid 1:1
tc -s qdisc ls dev lo
tc qdisc show dev lo
tc class show dev lo
tc filter show dev lo
