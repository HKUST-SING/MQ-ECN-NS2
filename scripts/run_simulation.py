import threading
import os

class SimThread(threading.Thread):
	def __init__(self, cmd):
		self.cmd=cmd
		threading.Thread.__init__(self)

	def run(self):
		os.system(self.cmd)
			
sim_end=50000
link_rate=10
mean_link_delay=0.0000002
host_delay=0.000020
queueSize=240
load=0.8
connections_per_pair=1
meanFlowSize=1661480
paretoShape=1.05
enableMultiPath=1
perflowMP=1
sourceAlg='DCTCP-Sack'
ackRatio=1
slowstartrestart='true'
DCTCP_g=0.0625
min_rto=0.005
ECN_scheme=[0,2,3]
DCTCP_K=72
topology_spt=16
topology_tors=9
topology_spines=4
topology_x=1
fct_log=['flow_0.tr', 'flow_1.tr', 'flow_2.tr']
sim_log=['logFile_0.tr','logFile_1.tr','logFile_2.tr']

ns_path='/home/wei/buffer_management/ns-allinone-2.35/ns-2.35/ns'
sim_script='spine_empirical_dctcp.tcl'

threads=[]

if len(ECN_scheme)==len(fct_log) and len(fct_log)==len(sim_log):
	for i in range(len(ECN_scheme)):
		cmd=ns_path+' '+sim_script+' '\
		+str(sim_end)+' '\
		+str(link_rate)+' '\
		+str(mean_link_delay)+' '\
		+str(host_delay)+' '\
		+str(queueSize)+' '\
		+str(load)+' '\
		+str(connections_per_pair)+' '\
		+str(meanFlowSize)+' '\
		+str(paretoShape)+' '\
		+str(enableMultiPath)+' '\
		+str(perflowMP)+' '\
		+str(sourceAlg)+' '\
		+str(ackRatio)+' '\
		+str(slowstartrestart)+' '\
		+str(DCTCP_g)+' '\
		+str(min_rto)+' '\
		+str(ECN_scheme[i])+' '\
		+str(DCTCP_K)+' '\
		+str(topology_spt)+' '\
		+str(topology_tors)+' '\
		+str(topology_spines)+' '\
		+str(topology_x)+' '\
		+str(fct_log[i])+'  >'\
		+str(sim_log[i])
		
		#Start thread to run simulation
		newthread=SimThread(cmd)
		threads.append(newthread)
		newthread.start()
	
	# Wait for all of them to finish
	for t in threads:
		t.join()
		
else:
	print 'wrong inputs'