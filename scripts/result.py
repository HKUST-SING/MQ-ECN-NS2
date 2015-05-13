import os
import sys
import string

#This function is to print usage of this script
def usage():
	sys.stderr.write('result.py [file]\n')

#Get average FCT
def avg(flows):
	sum=0.0
	for f in flows:
		sum=sum+f
	return sum/len(flows)

#GET mean FCT
def mean(flows):
	flows.sort()
	return flows[50*len(flows)/100]
	
#GET 99% FCT
def max(flows):
	flows.sort()
	return flows[99*len(flows)/100]
	
	
if len(sys.argv)==2:
	file=sys.argv[1]

	#All the flows
	flows=[]
	flows_notimeouts=[];	#Flows without timeouts
	#Short flows (0,100KB)
	short_flows=[]
	short_flows_notimeouts=[];	
	#Large flows (10MB,)
	large_flows=[]
	large_flows_notimeouts=[];	
	#Median flows (100KB, 10MB)
	median_flows=[]
	median_flows_notimeouts=[];	
	#The number of total timeouts
	timeouts=0
	
	fp = open(file)
	#Get overall average normalized FCT
	while True:
		line=fp.readline()
		if not line:
			break
		if len(line.split())<3:
			continue
		pkt_size=int(float(line.split()[0]))
		byte_size=float(pkt_size)*1460
		time=float(line.split()[1])
		result=time*1000
		
		#TCP timeouts
		timeouts_num=int(line.split()[2])
		timeouts+=timeouts_num
		
		flows.append(result)
		#If the flow is a short flow
		if byte_size<100*1024:
			short_flows.append(result)
		#If the flow is a large flow
		elif byte_size>10*1024*1024:
			large_flows.append(result)
		else:
			median_flows.append(result)
		
		#If this flow does not have timeout
		if timeouts_num==0:
			flows_notimeouts.append(result)
			#If the flow is a short flow
			if byte_size<100*1024:
				short_flows_notimeouts.append(result)
			#If the flow is a large flow
			elif byte_size>10*1024*1024:
				large_flows_notimeouts.append(result)
			else:
				median_flows_notimeouts.append(result)
		
	fp.close()
	print "There are "+str(len(flows))+" flows in total"
	print "There are "+str(timeouts)+" TCP timeouts in total"
	print "Overall average FCT: "+str(avg(flows))
	print "Average FCT (0,100KB): "+str(len(short_flows))+" flows "+str(avg(short_flows))
	print "99th percentile FCT (0,100KB): "+str(len(short_flows))+" flows "+str(max(short_flows))
	print "Average FCT (100KB,10MB): "+str(len(median_flows))+" flows "+str(avg(median_flows))
	print "Average FCT (10MB,): "+str(len(large_flows))+" flows "+str(avg(large_flows))
	
	print "There are "+str(len(flows_notimeouts))+" flows w/o timeouts in total"
	print "Overall average FCT w/o timeouts: "+str(avg(flows_notimeouts))
	print "Average FCT (0,100KB) w/o timeouts: "+str(len(short_flows_notimeouts))+" flows "+str(avg(short_flows_notimeouts))
	print "99th percentile FCT (0,100KB): "+str(len(short_flows_notimeouts))+" flows "+str(max(short_flows_notimeouts))
	print "Average FCT (100KB,10MB) w/o timeouts: "+str(len(median_flows_notimeouts))+" flows "+str(avg(median_flows_notimeouts))
	print "Average FCT (10MB,) w/o timeouts: "+str(len(large_flows_notimeouts))+" flows "+str(avg(large_flows_notimeouts))
