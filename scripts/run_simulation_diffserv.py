import threading
import os
import Queue

def worker():
	while True:
		try:
			j = q.get(block = 0)
		except Queue.Empty:
			return
		#Make directory to save results
		os.system('mkdir '+j[1])
		os.system(j[0])

q = Queue.Queue()
service_num_arr = [8, 32]
sim_end = 50000
link_rate = 10
mean_link_delay = 0.0000002
host_delay = 0.000020
queueSize = 200
load_arr = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
connections_per_pair = 1

enableMultiPath = 1
perflowMP = 1

init_window = 16
sourceAlg = 'Sack' #DCTCP-Sack or Sack
ackRatio = 1
slowstartrestart = 'true'
DCTCP_g = 0.0625
min_rto = 0.005

#per-queue-standard(0), per-port(1), MQ-ECN(2) and per-queue-min(3)
ECN_scheme_arr = [0, 2, 3]
DCTCP_K = 84.0
switchAlgs = ['DWRR','WRR']

topology_spt = 12
topology_tors = 12
topology_spines = 12
topology_x = 1

ns_path = '../ns-allinone-2.35/ns-2.35/ns'
sim_script = 'spine_empirical_diffserv.tcl'
specialStr = ''

#For different scheduling algorithms
for switchAlg in switchAlgs:
	#For different numbers of queues (services):
	for service_num in service_num_arr:
		#For different loads
		for load in load_arr:
			#For different ECN marking schemes
			for ECN_scheme in ECN_scheme_arr:
				#Transport protocol: TCP(ECN*) or DCTCP
				transport='tcp'
				if 'DCTCP' in sourceAlg:
					transport = 'dctcp'

				#Directory name: workload_transport_scheme_[ECN_scheme]_load_[load]_service_[service_num]
				directory_name = 'diffserv_'+specialStr+switchAlg+'_'+transport+'_scheme_'+str(ECN_scheme)+'_load_'+str(int(load*10))+'_service_'+str(service_num)
				directory_name = directory_name.lower()
				#Simulation command
				cmd = ns_path+' '+sim_script+' '\
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
					+str(init_window)+' '\
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

				q.put([cmd, directory_name])

# create all worker threads
threads = []
number_worker_threads = 20

# start threads to process jobs
for i in range(number_worker_threads):
	t = threading.Thread(target = worker)
	threads.append(t)
	t.start()

# join all completed threads
for t in threads:
	t.join()
