source "tcp-common-opt-raw.tcl"

set ns [new Simulator]
set sim_start [clock seconds]
puts "Host: [exec uname -a]"

if {$argc != 24} {
    puts "wrong number of arguments $argc"
    exit 0
}

set service_num [lindex $argv 0]
set sim_end [lindex $argv 1]
set link_rate [lindex $argv 2]
set mean_link_delay [lindex $argv 3]
set host_delay [lindex $argv 4]
set queueSize [lindex $argv 5]
set load [lindex $argv 6]
set connections_per_pair [lindex $argv 7]

#### Multipath
set enableMultiPath [lindex $argv 8]
set perflowMP [lindex $argv 9]

#### Transport settings options
set init_window [lindex $argv 10] ; # TCP initial window
set sourceAlg [lindex $argv 11] ; # Sack or DCTCP-Sack
set ackRatio [lindex $argv 12]
set slowstartrestart [lindex $argv 13]
set DCTCP_g [lindex $argv 14] ;	# DCTCP alpha estimation gain
set min_rto [lindex $argv 15]

#### Switch side options
#per-queue standard (0), per-port (1), MQ-ECN (2) and per-queue-min (3)
set ECN_scheme [lindex $argv 16]
set DCTCP_K [lindex $argv 17]
set switchAlg [lindex $argv 18]

#### topology
set topology_spt [lindex $argv 19]
set topology_tors [lindex $argv 20]
set topology_spines [lindex $argv 21]
set topology_x [lindex $argv 22]

#### FCT log file
set fct_log [lindex $argv 23]

#### Flow size distribution CDF file
set pktSize 1460;	#packet size in bytes
set dwrr_quantum [expr $pktSize + 40];	#quantum for each queue (DWRR)
set wrr_quantum [expr $pktSize + 40]; #weight for each queue (WRR)
set link_capacity_unit Gb

puts "Simulation input:"
puts "Dynamic Flow - Pareto"
puts "topology: spines server per rack = $topology_spt, x = $topology_x"
puts "service number $service_num"
puts "sim_end $sim_end"
puts "link_rate $link_rate Gbps"
puts "link_delay $mean_link_delay sec"
puts "RTT  [expr $mean_link_delay * 2.0 * 6] sec"
puts "host_delay $host_delay sec"
puts "queue size $queueSize pkts"
puts "load $load"
puts "connections_per_pair $connections_per_pair"
puts "enableMultiPath=$enableMultiPath, perflowMP=$perflowMP"
puts "initial window: $init_window"
puts "source algorithm: $sourceAlg"
puts "ackRatio $ackRatio"
puts "DCTCP_g $DCTCP_g"
puts "slow-start Restart $slowstartrestart"
puts "ECN marking scheme $ECN_scheme"
puts "DCTCP_K_ $DCTCP_K"
puts "switchAlg $switchAlg"
puts "pktSize(payload) $pktSize Bytes"
puts "pktSize(include header) [expr $pktSize + 40] Bytes"
puts "service number $service_num"
puts "FCT log file $fct_log"
puts " "

################# Transport Options ####################
Agent/TCP set windowInit_ $init_window
Agent/TCP set window_ 1256
Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ $pktSize
Agent/TCP/FullTcp set segsize_ $pktSize
Agent/TCP/FullTcp set spa_thresh_ 0

Agent/TCP set slow_start_restart_ $slowstartrestart
Agent/TCP set windowOption_ 0
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set minrto_ $min_rto
Agent/TCP set rtxcur_init_ $min_rto;    #RTO init value
Agent/TCP set maxrto_ 64

Agent/TCP/FullTcp set nodelay_ true;    #disable Nagle
Agent/TCP/FullTcp set segsperack_ $ackRatio
Agent/TCP/FullTcp set interval_ 0


if {[string compare $sourceAlg "DCTCP-Sack"] == 0} {
	Agent/TCP set dctcp_ true
	Agent/TCP set dctcp_g_ $DCTCP_g
}

################# Switch Options ######################
Queue set limit_ $queueSize

Queue/DWRR set queue_num_ $service_num
Queue/DWRR set mean_pktsize_ [expr $pktSize + 40]
Queue/DWRR set port_thresh_ $DCTCP_K
Queue/DWRR set estimate_round_alpha_ 0.75
Queue/DWRR set estimate_round_idle_interval_bytes_ 1500
Queue/DWRR set link_capacity_ $link_rate$link_capacity_unit
Queue/DWRR set deque_marking_ false
Queue/DWRR set debug_ false

Queue/WRR set queue_num_ $service_num
Queue/WRR set mean_pktsize_ [expr $pktSize + 40]
Queue/WRR set port_thresh_ $DCTCP_K
Queue/WRR set estimate_round_alpha_ 0.75
Queue/WRR set estimate_round_idle_interval_bytes_ 1500
Queue/WRR set link_capacity_ $link_rate$link_capacity_unit
Queue/WRR set deque_marking_ false
Queue/WRR set debug_ false

if {$ECN_scheme != 3} {
	Queue/DWRR set marking_scheme_ $ECN_scheme
	Queue/WRR set marking_scheme_ $ECN_scheme
} else {
	#Per-queue-min
	Queue/DWRR set marking_scheme_ 0
	Queue/WRR set marking_scheme_ 0
}

############## Multipathing ###########################
if {$enableMultiPath == 1} {
	$ns rtproto DV
	Agent/rtProto/DV set advertInterval	[expr 2 * $sim_end]
	Node set multiPath_ 1
	if {$perflowMP != 0} {
		Classifier/MultiPath set perflow_ 1
	}
}

############# Topoplgy #########################
set S [expr $topology_spt * $topology_tors] ; #number of servers
set UCap [expr $link_rate * $topology_spt / $topology_spines / $topology_x] ; #uplink rate
puts "Total server number: $S"
puts "Uplink capacity: $UCap Gbps"

for {set i 0} {$i < $S} {incr i} {
	set s($i) [$ns node]
}

for {set i 0} {$i < $topology_tors} {incr i} {
	set n($i) [$ns node]
}

for {set i 0} {$i < $topology_spines} {incr i} {
	set a($i) [$ns node]
}

for {set i 0} {$i < $S} {incr i} {
	set j [expr $i / $topology_spt]
	$ns duplex-link $s($i) $n($j) [set link_rate]Gb [expr $host_delay + $mean_link_delay]  $switchAlg

##### Configure DWRR/WRR for edge links #####
	set L [$ns link $s($i) $n($j)]
	set q [$L set queue_]

	for {set service_i 0} {$service_i < $service_num} {incr service_i} {

		if {[string compare $switchAlg "DWRR"] == 0} {
			$q set-quantum $service_i $dwrr_quantum
		} elseif {[string compare $switchAlg "WRR"] == 0} {
			$q set-quantum $service_i $wrr_quantum
		}

		#per-queue min (3)
		if {$ECN_scheme == 3} {
			$q set-thresh $service_i [expr $DCTCP_K / $service_num]
		} else {
			$q set-thresh $service_i [expr $DCTCP_K]
		}
	}

	set L [$ns link $n($j) $s($i)]
	set q [$L set queue_]

	for {set service_i 0} {$service_i < $service_num} {incr service_i} {
		if {[string compare $switchAlg "DWRR"] == 0} {
			$q set-quantum $service_i $dwrr_quantum
		} elseif {[string compare $switchAlg "WRR"] == 0} {
			$q set-quantum $service_i $wrr_quantum
		}

		#per-queue min (3)
		if {$ECN_scheme == 3} {
			$q set-thresh $service_i [expr $DCTCP_K / $service_num]
		} else {
			$q set-thresh $service_i [expr $DCTCP_K]
		}
	}
}

for {set i 0} {$i < $topology_tors} {incr i} {
	for {set j 0} {$j < $topology_spines} {incr j} {
		$ns duplex-link $n($i) $a($j) [set UCap]Gb $mean_link_delay $switchAlg

##### Configure DWRR/WRR for core links #####
		set L [$ns link $n($i) $a($j)]
		set q [$L set queue_]
		$q set link_capacity_ $UCap$link_capacity_unit

		for {set service_i 0} {$service_i < $service_num} {incr service_i} {
			if {[string compare $switchAlg "DWRR"] == 0} {
				$q set-quantum $service_i $dwrr_quantum
			} elseif {[string compare $switchAlg "WRR"] == 0} {
				$q set-quantum $service_i $wrr_quantum
			}

			#per-queue min (3)
			if {$ECN_scheme == 3} {
				$q set-thresh $service_i [expr $DCTCP_K / $service_num]
			} else {
				$q set-thresh $service_i [expr $DCTCP_K]
			}
		}

		set L [$ns link $a($j) $n($i)]
		set q [$L set queue_]
		$q set link_capacity_ $UCap$link_capacity_unit

		for {set service_i 0} {$service_i < $service_num} {incr service_i} {
			if {[string compare $switchAlg "DWRR"] == 0} {
				$q set-quantum $service_i $dwrr_quantum
			} elseif {[string compare $switchAlg "WRR"] == 0} {
				$q set-quantum $service_i $wrr_quantum
			}

			#per-queue min (3)
			if {$ECN_scheme == 3} {
				$q set-thresh $service_i [expr $DCTCP_K / $service_num]
			} else {
				$q set-thresh $service_i [expr $DCTCP_K]
			}
		}
	}
}


#############  Agents  #########################
#set lambda [expr ($link_rate*$load*1000000000)/($meanFlowSize*8.0)]
#set lambda [expr ($link_rate*$load*1000000000)/($mean_npkts*($pktSize+40)*8.0)]
#puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
#puts "FlowSize: Pareto with mean = $meanFlowSize, shape = $paretoShape"

puts "Setting up connections ..."; flush stdout

set flow_gen 0
set flow_fin 0

set flowlog [open $fct_log w]
set init_fid 0

for {set j 0} {$j < $S } {incr j} {
	for {set i 0} {$i < $S } {incr i} {
		if {$i != $j} {
			set agtagr($i,$j) [new Agent_Aggr_pair]

			#Assign service ID
			set service_id [expr {(($j + 1) * $S + ($i + 1)) % $service_num}]
			$agtagr($i,$j) setup $s($i) $s($j) "$i $j" $connections_per_pair $init_fid  $service_id "TCP_pair"
			$agtagr($i,$j) attach-logfile $flowlog

			#Assign flow size distribution
			set flow_cdf "CDF_vl2.tcl"
			set meanFlowSize 7495019
			set dist "vl2"

			#Currently, we have 4 flow size distributions in total
			if {$service_id % 4 == 0} {
				set flow_cdf "CDF_cache.tcl"
				set meanFlowSize 913703
				set dist "cache"
			} elseif {$service_id % 4 == 1} {
				set flow_cdf "CDF_dctcp.tcl"
				set meanFlowSize 1671357
				set dist "dctcp"
			} elseif {$service_id % 4 == 2} {
				set flow_cdf "CDF_hadoop.tcl"
				set meanFlowSize 4149016
				set dist "hadoop"
			}

			set lambda [expr ($link_rate * $load * 1000000000) / ($meanFlowSize * 8.0)]
			puts -nonewline "($i,$j) service $service_id $dist "

			#For Poisson
			$agtagr($i,$j) set_PCarrival_process  [expr $lambda/($S - 1)] $flow_cdf [expr 17 * $i + 1244 * $j] [expr 33 * $i + 4369 * $j]
			$ns at 0.1 "$agtagr($i,$j) warmup 0.5 5"
			$ns at 1 "$agtagr($i,$j) init_schedule"

			set init_fid [expr $init_fid + $connections_per_pair];
		}
    }
}

puts "Initial agent creation done";flush stdout
puts "Simulation started!"

$ns run
