import threading
import os

class SimThread(threading.Thread):
	def __init__(self, cmd):
		self.cmd=cmd
		threading.Thread.__init__(self)

	def run(self):
		os.system(self.cmd)

service_num=32		
sim_end=50000
link_rate=10
mean_link_delay=0.0000002
host_delay=0.000020
queueSize=240
loads=[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9]
connections_per_pair=1
meanFlowSize=1661480
paretoShape=1.05
enableMultiPath=1
perflowMP=1
sourceAlg='Sack' 
ackRatio=1
slowstartrestart='true'
DCTCP_g=0.0625
min_rto=0.005
ECN_scheme=[0,2,3]
DCTCP_K=84
topology_spt=16
topology_tors=9
topology_spines=4
topology_x=1

ns_path='/home/wei/buffer_management/ns-allinone-2.35/ns-2.35/ns'
sim_script='spine_empirical_dctcp.tcl'

threads=[]

for load in loads:
	for i in range(len(ECN_scheme)):
	
		#Transport protocol: TCP(ECN) or DCTCP
		transport='tcp'
		if 'DCTCP' in sourceAlg:
			transport='dctcp'
			
		#Directory name: transport_scheme_[ECN_scheme]_load_[load]_service_[service_num]	
		directory_name=transport+'_scheme_'+str(ECN_scheme[i])+'_load_'+str(int(load*10))+'_service_'+str(service_num)
		#Make directory to save results
		os.system('mkdir '+directory_name)
		
		#CMD
		cmd=ns_path+' '+sim_script+' '\
			+str(service_num)+' '\
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
			+str('./'+directory_name+'/flow.tr')+'  >'\
			+str('./'+directory_name+'/logFile.tr')
		
		#Start thread to run simulation
		newthread=SimThread(cmd)
		threads.append(newthread)
		newthread.start()
	
# Wait for all of them to finish
for t in threads:
	t.join()
