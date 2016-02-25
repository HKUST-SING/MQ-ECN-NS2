# MQ-ECN NS2 Simulation

## Software requirements
To reproduce simulation results (Figure 9-13) of [MQ-ECN paper](http://www.cse.ust.hk/~kaichen/papers/mqecn-nsdi16.pdf), you need following softwares:
  - [NS2 Network Simulator 2.35](https://sourceforge.net/projects/nsnam/)
  - [mqecn.patch](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/mqecn.patch) (include DCTCP and ECMP support) 
  - [MQ-ECN code](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/queue) (implement DWRR and WRR with different ECN marking schemes)
  - [Simulation scripts](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/scripts) (run simulations in parallel and parse results)

## Installation
- Download [NS2 Network Simulator 2.35](https://sourceforge.net/projects/nsnam/) and unzip it.
  ```
  $ tar -zxvf ns-allinone-2.35.tar.gz
  ```
  
- Copy [mqecn.patch](https://github.com/HKUST-SING/MQ-ECN-NS2/blob/master/mqecn.patch) to the *top ns-2.35 folder* (```ns-allinone-2.35```) and apply the patch. Then install NS2.
  ```
  $ cd ns-allinone-2.35
  $ patch -p1 --ignore-whitespace -i mqecn.patch
  $ ./install
  ```
 
- Copy files of [MQ-ECN code](https://github.com/HKUST-SING/MQ-ECN-NS2/tree/master/queue) to ```ns-allinone-2.35/ns-2.35/queue/```
- Add ```queue/wrr.o queue/dwrr.o``` to ```ns-allinone-2.35/ns-2.35/Makefile```
- Run ```make``` on ```/ns-allinone-2.35/ns-2.35```

## Acknowledgements
We thank [Mohammad Alizadeh](https://people.csail.mit.edu/alizadeh/) for sharing pFabric simulation codes and DCTCP patch.  



