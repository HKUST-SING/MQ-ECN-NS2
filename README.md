# MQ-ECN NS2 Simulation
## Software requirements
To reproduce simulation results (Figure 9-13) of [MQ-ECN paper](http://www.cse.ust.hk/~kaichen/papers/mqecn-nsdi16.pdf), you need following softwares:
  - [Network Simulator (NS) 2.35](https://sourceforge.net/projects/nsnam/)
  - [mqecn.patch](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/mqecn.patch) (Modify TCP & Add ECMP) 
  - [MQ-ECN code](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/queue) (implement DWRR and WRR with different ECN marking schemes)
  - [Simulation scripts](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/scripts) (run simulations in parallel and parse results)

## Installation
Download [Network Simulator (NS) 2.35](https://sourceforge.net/projects/nsnam/) and unzip it.
```
$ tar -zxvf ns-allinone-2.35.tar.gz
```
  
Copy [mqecn.patch](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/mqecn.patch) to the *top ns-2.35 folder* (```ns-allinone-2.35```) and apply the patch. Then install NS2.
```
$ cd ns-allinone-2.35
$ patch -p1 --ignore-whitespace -i mqecn.patch
$ ./install
```

Copy files of [MQ-ECN code](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/queue) to ```ns-allinone-2.35/ns-2.35/queue/```

Add ```queue/wrr.o queue/dwrr.o``` to ```ns-allinone-2.35/ns-2.35/Makefile```
 
Run ```make``` on ```/ns-allinone-2.35/ns-2.35```

## Running Basic Simulations
Before running large-scale simulations, you can run several small-scale simulations to understand the basic properties of MQ-ECN. The scripts for basic simulations are [test-wrr.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/test-wrr.tcl), [test-wrr-2.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/test-wrr-2.tcl), [test-dwrr.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/test-dwrr.tcl) and [test-dwrr-2.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/test-dwrr-2.tcl). The basic simulation scripts will generate three files: ```throughput.tr```, ```qlenfile.tr``` and ```tot_qlenfile.tr```.

- The ```throughput.tr``` gives aggregate goodputs of different services. It has the following format:
  ```
  Time, Goodput of Service 1, ..., Goodput of Service N
  ```
  
- The ```qlenfile.tr``` gives queue lengths (in bytes) of different service queues. It has the following format:
  ```
  Time, Queue Length of Service Queue 1, ..., Queue Length of Service N
  ```
  
- The ```tot_qlenfile.tr``` gives the total queue length (in bytes) of the congested switch port. It has the following format:
  ```
  Time, Queue Length of Switch Port
  ```
  
## Running Large-Scale Simulations
At the beginning, we briefly introduce the usage of each file in [scripts](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/scripts) directory.
- [CDF_cache.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/CDF_cache.tcl), [CDF_dctcp.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/CDF_dctcp.tcl), [CDF_hadoop.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/CDF_hadoop.tcl) and [CDF_vl2.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/CDF_vl2.tcl) give flow size distributions used in our paper.

- [spine_empirical_diffserv.tcl](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/spine_empirical_diffserv.tcl) and [tcp-common-opt-raw.tcl] (https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/tcp-common-opt-raw.tcl) are NS2 TCL simulation scripts.  

- [run_simulation_diffserv.py](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/run_simulation_diffserv.py) is used to run NS2 simulations in parallel. 

- [result.py](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/result.py) is used to parse final results.  

Hence, to run simulations in parallel:
```
python run_simulation_diffserv.py
```

There are many parameters (e.g., transport protocol, the number of queues, etc) to configue in [run_simulation_diffserv.py](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/run_simulation_diffserv.py). By default, it runs 20x simulations in parallel and iterates over the per-queue ECN with the standard threshold, per-queue ECN with the minimum threshold and MQ-ECN. For each simulation, it will create a directory with the following name:
```
diffserv_[scheduler]_[transport]_scheme_[ECN scheme]_load_[network load]_service_[number of queues]
```

Each direcotry consists of two files: ```flow.tr``` and ```logFile.tr```. The ```flow.tr``` gives flow completion time results with the following format:
```
number of packets, flow completion time, number of timeouts, src ID, dst ID, service (queue) ID
```

You can use [result.py](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/scripts/result.py) to parse ```flow.tr``` files as follows:
```
$ python result.py -a -i [directory_path]/flow.tr
```

## Contact
If you have any question about MQ-ECN simulation code, please contact [Wei Bai](http://sing.cse.ust.hk/~wei/).

## Acknowledgements
We thank [Mohammad Alizadeh](https://people.csail.mit.edu/alizadeh/) for sharing pFabric simulation code and DCTCP patch.  



