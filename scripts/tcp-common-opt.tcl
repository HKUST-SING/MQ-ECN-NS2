Class TCP_pair

#Variables:
#tcps tcpr:  Sender TCP, Receiver TCP 
#sn dn:	source/dest node which TCP sender/receiver exist
#aggr_ctrl:  Agent_Aggr_pair for callback
#start_cbfunc:  callback at start
#fin_cbfunc:  callback at fin
#group_id:  group id "src->Dst"
#pair_id:  index of connection among the group
#fid:  unique flow identifier for this connection (group_id, pair_id)
#service_id: id of service that this connection belongs to
#rttimes: number of TCP timeouts

#Public Functions:
#setup{snode dnode}  
#setgid {gid} <- set group id
#setpairid {pid} <- set pair id
#setfid {fid} <- set flow id
#serserviceid {sid} <- set service id
#start { nr_bytes } <- let start sending nr_bytes 
#set_debug_mode { mode } <- change to debug_mode

TCP_pair instproc init {args} {
    $self instvar pair_id group_id fid debug_mode rttimes
    $self instvar tcps tcpr;# Sender TCP,  Receiver TCP
	eval $self next $args
	
	$self set tcps [new Agent/TCP/FullTcp/Sack]  
    $self set tcpr [new Agent/TCP/FullTcp/Sack]  
	$tcps set_callback $self
	
	$self set pair_id	0
    $self set group_id	0
    $self set service_id	0
    $self set debug_mode	1
    $self set rttimes	0
}

TCP_pair instproc set_debug_mode { mode } {
    $self instvar debug_mode
    $self set debug_mode $mode
}

TCP_pair instproc setup {snode dnode} {
    global ns
	$self instvar tcps tcpr; # Sender TCP,  Receiver TCP
	$self instvar sn dn; #Sender node, Receiver node
		
	$self set sn $snode
	$self set dn $dnode
	
	$ns attach-agent $sn $tcps;
    $ns attach-agent $dn $tcpr;

    $tcpr listen
    $ns connect $tcps $tcpr
}

TCP_pair instproc set_fincallback { controller func} {
    $self instvar aggr_ctrl fin_cbfunc
    $self set aggr_ctrl  $controller
    $self set fin_cbfunc  $func
}

TCP_pair instproc set_startcallback { controller func} {
    $self instvar aggr_ctrl start_cbfunc
    $self set aggr_ctrl $controller
    $self set start_cbfunc $func
}

TCP_pair instproc setgid { gid } {
    $self instvar group_id
    $self set group_id $gid
}

TCP_pair instproc setpairid { pid } {
    $self instvar pair_id
    $self set pair_id $pid
}

TCP_pair instproc setfid { flow_id } {
    $self instvar tcps tcpr
    $self instvar fid
    $self set fid $flow_id
    $tcps set fid_ $flow_id;
    $tcpr set fid_ $flow_id;
}

TCP_pair instproc setserviceid {sid} {
	$self instvar tcps tcpr
	$self instvar service_id
	$self set service_id $sid
	$tcps set serviceid_ $sid;
    $tcpr set serviceid_ $sid;
}

TCP_pair instproc start { nr_bytes } {
    global ns sim_end flow_gen
    $self instvar tcps tcpr fid group_id
    $self instvar start_time bytes
    $self instvar aggr_ctrl start_cbfunc

    $self instvar debug_mode

    $self set start_time [$ns now] 
    $self set bytes $nr_bytes  

    if {$flow_gen >= $sim_end} {
		return
    }
    #if {$start_time >= 0.2} 
	#{
	set flow_gen [expr $flow_gen + 1]
    #}

    if { $debug_mode == 1 } {
		puts "stats: [$ns now] start grp $group_id fid $fid $nr_bytes bytes service $service_id"
    }

    if { [info exists aggr_ctrl] } {
		$aggr_ctrl $start_cbfunc
    }
  
    #$tcpr set flow_remaining_ [expr $nr_bytes]
    $tcps set signal_on_empty_ TRUE
    $tcps advance-bytes $nr_bytes
}

TCP_pair instproc stop {} {
    $self instvar tcps tcpr
    $tcps reset
    $tcpr reset
}

TCP_pair instproc fin_notify {} {    
	global ns
	$self instvar sn dn	rttimes
	$self instvar tcps tcpr
	$self instvar aggr_ctrl fin_cbfunc
	$self instvar pair_id service_id
	$self instvar bytes

    $self instvar dt
    $self instvar bps
	
    $self flow_finished
	
	set old_rttimes $rttimes
    $self set rttimes [$tcps set nrexmit_]
	
	if { [info exists aggr_ctrl] } {
		$aggr_ctrl $fin_cbfunc $pair_id $bytes $dt $bps [expr $rttimes - $old_rttimes]
    }
}

TCP_pair instproc flow_finished {} {
    global ns
    $self instvar start_time bytes fid group_id service_id
	$self instvar dt bps
    $self instvar debug_mode

    set ct [$ns now]
    $self set dt  [expr $ct - $start_time]
    if {$dt == 0} {
		puts "dt = 0"
		flush stdout
	}
	$self set bps [expr $bytes * 8.0 / $dt ]
	
    if {$debug_mode == 1} {
		puts "stats: $ct fin grp $group_id fid $fid size $bytes bytes fct $dt sec service $service_id"
    }
}

Agent/TCP/FullTcp instproc set_callback {tcp_pair} {
    $self instvar ctrl
    $self set ctrl $tcp_pair
}

Agent/TCP/FullTcp instproc done_data {} {
    $self instvar ctrl
    if { [info exists ctrl] } 
	{
		$ctrl fin_notify
    }
}

Class Agent_Aggr_pair
#Note: Contoller and placeholder of Agent_pairs. Let Agent_pairs to arrives according to random process. 

#Variables:
#apair:	array of Agent_pair (TCP_pair)
#nr_pairs: the number of pairs
#rv_flow_intval: (r.v.) flow interval
#rv_nbytes: (r.v.) the number of bytes within a flow
#last_arrival_time: the last flow starting time
#logfile: log file (should have been opend)
#stat_nr_finflow: statistics nr  of finished flows
#stat_sum_fldur: statistics sum of finished flow durations
#last_arrival_time: last flow arrival time
#actfl: nr of current active flow

#Public functions:
#attach-logfile {logf}  <- call if want logfile
#setup {snode dnode gid nr} <- must 
#set_PCarrival_process {lambda cdffile rands1 rands2} 
#init_schedule {} <- must 
#fin_notify { pid bytes fldur bps } <- Callback
#start_notify {} <- Callback

#Private functions:
#init {args}
#resetvars {}

Agent_Aggr_pair instproc init {args} {
    eval $self next $args
}

Agent_Aggr_pair instproc attach-logfile { logf } {
    $self instvar logfile
    $self set logfile $logf
}

Agent_Aggr_pair instproc setup {snode dnode gid nr init_fid sid agent_pair_type} {
#Note:
#Create nr pairs of Agent_pair
#and connect them to snode and dnode 
#We may refer this pair by group_id gid
#All Agent_pairs have the same gid and service id (sid) 
#and each of them has its own flow id: init_fid + [0 .. nr-1]

    $self instvar apair     ;# array of Agent_pair
    $self instvar group_id  ;# group id of this group (given)
    $self instvar nr_pairs  ;# nr of pairs in this group (given)
	$self instvar service_id ;# service id of this group (given) 
    $self instvar s_node d_node apair_type ;

    $self set group_id $gid 
    $self set nr_pairs $nr
	$self set service_id $sid
    $self set s_node $snode
    $self set d_node $dnode
    $self set apair_type $agent_pair_type
	
	for {set i 0} {$i < $nr_pairs} {incr i} {
		$self set apair($i) [new $agent_pair_type]
		$apair($i) setup $snode $dnode 
		$apair($i) setgid $group_id	;# let each pair know our group id
		$apair($i) setpairid $i	;# let each pair know his pair id
		$apair($i) setfid $init_fid	;# assign next fid
		$apair($i) setserviceid $service_id	;# let each pair know his service id
		incr init_fid
	}
	
	$self resetvars	;# other initialization
}

Agent_Aggr_pair instproc resetvars {} {
#Private
#Reset variables
    $self instvar tnext ;# last flow arrival time
    $self instvar actfl	;# nr of current active flow
   
    $self set tnext 0.0
    $self set actfl 0
}

Agent_Aggr_pair instproc init_schedule {} {
#Public
#Note:
#Initially schedule flows for all pairs
#according to the arrival process.
    global ns
    $self instvar nr_pairs apair
	$self instvar tnext rv_flow_intval
	
	set dt [$rv_flow_intval value]
    $self set tnext [expr [$ns now] + $dt]
	
	for {set i 0} {$i < $nr_pairs} {incr i} {

		#### Callback Setting ########################
		$apair($i) set_fincallback $self fin_notify
		$apair($i) set_startcallback $self start_notify
		###############################################

		$self schedule $i
    }
}

Agent_Aggr_pair instproc set_PCarrival_process {lambda cdffile rands1 rands2} {
#public
#setup random variable rv_flow_intval and rv_npkts.
# PCarrival:
# flow arrival: poisson with rate $lambda
# flow length: custom defined expirical cdf
	$self instvar rv_flow_intval rv_nbytes

	set rng1 [new RNG]; #Random Number Generator
	$rng1 seed $rands1

	$self set rv_flow_intval [new RandomVariable/Exponential]
	$rv_flow_intval use-rng $rng1
	$rv_flow_intval set avg_ [expr 1.0/$lambda]; # inter-arrival time (second) of Possion process

	set rng2 [new RNG]; #Random Number Generator
	$rng2 seed $rands2

	$self set rv_nbytes [new RandomVariable/Empirical]
	$rv_nbytes use-rng $rng2
	$rv_nbytes set interpolation_ 2
	$rv_nbytes loadCDF $cdffile
}

Agent_Aggr_pair instproc schedule { pid } {
#Private
#Note:
#Schedule  pair (having pid) next flow time
#according to the flow arrival process.
	global ns flow_gen sim_end
    $self instvar apair
	$self instvar tnext
    $self instvar rv_flow_intval rv_nbytes
	
	if {$flow_gen >= $sim_end} {
		return
    }  
	
	set t [$ns now]
    
    if { $t > $tnext } {
		puts "Error, Not enough flows ! Aborting! pair id $pid"
		flush stdout
		exit 
    }
	
	#Generate flow size
	set tmp_ [expr ceil ([$rv_nbytes value])];	#Flow size in packets (MSS)
    set tmp_ [expr $tmp_ * 1460];	#Flow size in bytes
	#Start flow at tnext 
    $ns at $tnext "$apair($pid) start $tmp_"
	
	set dt [$rv_flow_intval value]
    $self set tnext [expr $tnext + $dt]
    $ns at [expr $tnext - 0.0000001] "$self check_if_behind"
}

Agent_Aggr_pair instproc check_if_behind {} {
    global ns
    global flow_gen sim_end
    $self instvar apair
    $self instvar nr_pairs
    $self instvar apair_type s_node d_node group_id service_id
    $self instvar tnext

    set t [$ns now]
    if { $flow_gen < $sim_end && $tnext < [expr $t + 0.0000002] } { #create new flow
		puts "[$ns now]: creating new connection $nr_pairs $s_node -> $d_node"
		flush stdout
		$self set apair($nr_pairs) [new $apair_type]
		$apair($nr_pairs) setup $s_node $d_node
		$apair($nr_pairs) setgid $group_id ;
		$apair($nr_pairs) setpairid $nr_pairs ;
		$apair($nr_pairs) setserviceid $service_id	;# let each pair know his service id
	
		#### Callback Setting #################
		$apair($nr_pairs) set_fincallback $self fin_notify
		$apair($nr_pairs) set_startcallback $self start_notify
		#######################################
		$self schedule $nr_pairs
		incr nr_pairs
	}
}

Agent_Aggr_pair instproc fin_notify { pid bytes fldur bps rttimes } {
#Callback Function
#pid  : pair_id
#bytes : nr of bytes of the flow which has just finished
#fldur: duration of the flow which has just finished
#bps  : avg bits/sec of the flow which has just finished
#Note:
#If we registor $self as "setcallback" of 
#$apair($id), $apair($i) will callback this
#function with argument id when the flow between the pair finishes.
#i.e.
#If we set:  "$apair(13) setcallback $self" somewhere,
#"fin_notify 13 $bytes $fldur $bps" is called when the $apair(13)'s flow is finished.
# 
    global ns flow_gen flow_fin sim_end
    $self instvar logfile
    $self instvar group_id
	$self instvar service_id
    $self instvar actfl
    $self instvar apair

    #Here, we re-schedule $apair($pid).
    #according to the arrival process.

    $self set actfl [expr $actfl - 1]

    set fin_fid [$apair($pid) set fid]
    
    ###### OUPUT STATISTICS #################
    if { [info exists logfile] } {
        #puts $logfile "flow_stats: [$ns now] gid $group_id pid $pid fid $fin_fid bytes $bytes fldur $fldur actfl $actfl bps $bps"
        set tmp_pkts [expr $bytes / 1460]
	
		#puts $logfile "$tmp_pkts $fldur $rttimes"
		puts $logfile "$tmp_pkts $fldur $rttimes $service_id $group_id"
		flush stdout
    }
    set flow_fin [expr $flow_fin + 1]
    if {$flow_fin >= $sim_end} {
		finish
    } 
    if {$flow_gen < $sim_end} {
		$self schedule $pid ;# re-schedule a pair having pair_id $pid. 
    }
}

Agent_Aggr_pair instproc start_notify {} {
#Callback Function
#Note:
#If we registor $self as "setcallback" of 
#$apair($id), $apair($i) will callback this
#function with argument id when the flow between the pair finishes.
#i.e.
#If we set:  "$apair(13) setcallback $self" somewhere,
#"start_notyf 13" is called when the $apair(13)'s flow is started.
    $self instvar actfl;
    incr actfl;
}

proc finish {} {
    global ns flowlog
    global sim_start

    $ns flush-trace
    close $flowlog

    set t [clock seconds]
    puts "Simulation Finished!"
    puts "Time [expr $t - $sim_start] sec"
    exit 0
}
