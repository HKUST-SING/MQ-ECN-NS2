import threading
import os

class SimThread(threading.Thread):
	def __init__(self, cmd, directory_name):
		self.cmd=cmd
		self.directory_name=directory_name
		threading.Thread.__init__(self)

	def run(self):
		#Make directory to save results
		os.system('mkdir '+self.directory_name)
		os.system(self.cmd)

service_num_arr=[8,32]
sim_end=50000
link_rate=10
mean_link_delay=0.0000002
host_delay=0.000020
queueSize=240
load_arr=[0.1,0.8,0.9]
connections_per_pair=1
enableMultiPath=1
perflowMP=1
sourceAlg='DCTCP-Sack'
ackRatio=1
slowstartrestart='true'
DCTCP_g=0.0625
min_rto=0.005
ECN_scheme_arr=[0,2,4]
DCTCP_K=65.0
switchAlg='DWRR'
topology_spt=16
topology_tors=9
topology_spines=4
topology_x=1

ns_path='/home/wei/buffer_management/ns-allinone-2.35/ns-2.35/ns'
sim_script='spine_empirical_diffserv.tcl'

threads=[]
max_thread_num=18

#For different numbers of queues (services):
for service_num in service_num_arr:
	#For different loads
	for load in load_arr:
		#For different ECN marking schemes
		for ECN_scheme in ECN_scheme_arr:
			#Transport protocol: TCP(ECN) or DCTCP
			transport='tcp'
			if 'DCTCP' in sourceAlg:
				transport='dctcp'

			#Directory name: workload_transport_scheme_[ECN_scheme]_load_[load]_service_[service_num]
			directory_name='diffserv_'+switchAlg+'_'+transport+'_scheme_'+str(ECN_scheme)+'_load_'+str(int(load*10))+'_service_'+str(service_num)
			directory_name=directory_name.lower()
			#Simulation command
			cmd=ns_path+' '+sim_script+' '\
				+str(service_num)+' '\
				+str(sim_end)+' '\
				+str(link_rate)+' '\
				+str(mean_link_delay)+' '\
				+str(host_delay)+' '\
				+str(queueSize)+' '\
				+str(load)+' '\
				+str(connections_per_pair)+' '\
				+str(enableMultiPath)+' '\
				+str(perflowMP)+' '\
				+str(sourceAlg)+' '\
				+str(ackRatio)+' '\
				+str(slowstartrestart)+' '\
				+str(DCTCP_g)+' '\
				+str(min_rto)+' '\
				+str(ECN_scheme)+' '\
				+str(DCTCP_K)+' '\
				+str(switchAlg)+' '\
				+str(topology_spt)+' '\
				+str(topology_tors)+' '\
				+str(topology_spines)+' '\
				+str(topology_x)+' '\
				+str('./'+directory_name+'/flow.tr')+'  >'\
				+str('./'+directory_name+'/logFile.tr')

			#Start thread to run simulation
			newthread=SimThread(cmd,directory_name)
			threads.append(newthread)

#Thread id
thread_i=0
#A temporary array to store running threads
tmp_threads=[]
#The number of concurrent running threads
concurrent_thread_num=0

while True:
	#If it is a legal thread and 'tmp_threads' still has capacity
	if thread_i<len(threads) and len(tmp_threads)<max_thread_num:
		tmp_threads.append(threads[thread_i])
		concurrent_thread_num=concurrent_thread_num+1
		thread_i=thread_i+1
	#No more thread or 'tmp_threads' does not have any capacity
	#'tmp_threads' is not empty
	elif len(tmp_threads)>0:
		print 'Start '+str(len(tmp_threads))+' threads\n'
		#Run current threads in 'tmp_threads' right now!
		for t in tmp_threads:
			t.start()
		#Wait for all of them to finish
		for t in tmp_threads:
			t.join()
		#Clear 'tmp_threads'
		del tmp_threads[:]
		#Reset
		concurrent_thread_num=0
	#'tmp_threads' is empty
	else:
		break
