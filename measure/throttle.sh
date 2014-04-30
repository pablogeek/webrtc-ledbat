IF="lo"
if [ -z "$1" ]; then echo "Enter [throttle speed] or rm!"; exit 0; fi
sudo tc qdisc del dev $IF root
if [ "$1" = "rm" ]; then exit 0; fi

sudo tc qdisc add dev $IF root handle 1: htb default 1
sudo tc class add dev $IF parent 1: classid 1:1 htb rate $1

if [ -n "$2" ]; then
	sudo tc qdisc add dev $IF parent 1:1 handle 20: netem delay ${2}ms
    sudo tc qdisc add dev $IF parent 20: handle 10: sfq perturb 5
else 
    sudo tc qdisc add dev $IF parent 1:1 handle 10: sfq perturb 5
fi
#sudo tc filter add dev $IF parent 1: protocol ip prio 1 u32 match ip src 0.0.0.0/0 flowid 1:1

tc -s qdisc ls dev $IF
tc qdisc show dev $IF
tc class show dev $IF
tc filter show dev $IF
