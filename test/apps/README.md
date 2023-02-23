# Testing SWTPG algorithms


## Compare two outputs 
```sh
diff -y <(sort TP_dump_SwtpgAvx_02-11-2022_09-03.txt) <(sort TP_dump_SwtpgNaive_02-11-2022_09-03.txt) | less
```


# Number of elements
256 channels 
15360 ADC values in total 
5 superchunks

# Running sum formula 


RS = (R_factor * RS) + std::abs(filt)/scale; 

where:

filt = filtered ADC value 
R_factor = 0.7
scale = 2.0


# Removing the FIR filter

With FIR: core utilization oscillates between 65% to 75%. Average is 70%
No FIR: core utilization oscillates between 55% to 65%. Average is 60% 

Noticible difference in core utilization, even when pinning to the cores. Tested with two parallel processing tasks using 8 registers 

Performance gain when removing the FIR is 14% 

Executed perf tests: 

With FIR: 

```sh
[aabedabu@np04-srv-018 ~]$ perf stat -p 1133297 sleep 600

 Performance counter stats for process id '1133297':

      2,082,745.78 msec task-clock                #    3.471 CPUs utilized          
        21,203,151      context-switches          #   10.180 K/sec                  
           229,583      cpu-migrations            #  110.231 /sec                   
        14,151,257      page-faults               #    6.795 K/sec                  
 4,705,432,416,209      cycles                    #    2.259 GHz                    
 6,364,422,554,812      instructions              #    1.35  insn per cycle         
   570,564,684,128      branches                  #  273.948 M/sec                  
     6,223,592,434      branch-misses             #    1.09% of all branches        

     600.018631655 seconds time elapsed

```

Without FIR: 
```sh
[aabedabu@np04-srv-018 ~]$ perf stat -p 1081773 sleep 240 

 Performance counter stats for process id '1081773':

        606,241.77 msec task-clock                #    2.526 CPUs utilized          
         7,308,471      context-switches          #   12.055 K/sec                  
            39,935      cpu-migrations            #   65.873 /sec                   
         4,334,180      page-faults               #    7.149 K/sec                  
 1,360,126,667,294      cycles                    #    2.244 GHz                    
 1,571,704,106,380      instructions              #    1.16  insn per cycle         
    93,998,680,118      branches                  #  155.051 M/sec                  
       542,586,726      branch-misses             #    0.58% of all branches        

     240.001483766 seconds time elapsed



[aabedabu@np04-srv-018 ~]$ perf stat -p 1135715 sleep 600

 Performance counter stats for process id '1135715':

      1,457,805.78 msec task-clock                #    2.430 CPUs utilized          
        19,551,915      context-switches          #   13.412 K/sec                  
           114,509      cpu-migrations            #   78.549 /sec                   
        30,687,607      page-faults               #   21.051 K/sec                  
 3,254,573,242,917      cycles                    #    2.233 GHz                    
 3,991,683,911,841      instructions              #    1.23  insn per cycle         
   247,803,688,352      branches                  #  169.984 M/sec                  
     1,525,207,699      branch-misses             #    0.62% of all branches        

     600.003658762 seconds time elapsed


```

Looking at the instructions per cycle, there is a noticeable gain of approximately 10%-14% when removing the FIR filter

# Unapcking

## Processing in the naive code 

First loop through all the channels (--> up to 256)
Then loop through all the wib frames for that channel

To do this we do the crazy reverse indexing of the RegisterArray structure

Result is something like this. For each channel there are 12 messages

```sh
Executing superchunk number 2 out of 24000
CHANNEL: 0
ADC value: 8447 sigma: 0        Median: 0       Filter: 0       is_over: 0
ADC value: 8443 sigma: 0        Median: 0       Filter: 0       is_over: 0
ADC value: 8445 sigma: 0        Median: 0       Filter: 511     is_over: 1
ADC value: 8445 sigma: 0        Median: 0       Filter: 3577    is_over: 1
ADC value: 8432 sigma: 0        Median: 0       Filter: 11242   is_over: 1
ADC value: 8421 sigma: 0        Median: 0       Filter: 21462   is_over: 1
ADC value: 8417 sigma: 0        Median: 0       Filter: 29127   is_over: 1
ADC value: 8419 sigma: 0        Median: 0       Filter: 32193   is_over: 1
ADC value: 8431 sigma: 0        Median: 0       Filter: 32704   is_over: 1
ADC value: 8449 sigma: 0        Median: 0       Filter: 32704   is_over: 1
ADC value: 8456 sigma: 1        Median: 1       Filter: 32704   is_over: 1
ADC value: 8451 sigma: 1        Median: 1       Filter: 32704   is_over: 1
CHANNEL: 1
ADC value: 8492 sigma: 0        Median: 0       Filter: 0       is_over: 0
ADC value: 8484 sigma: 0        Median: 0       Filter: 0       is_over: 0
ADC value: 8469 sigma: 0        Median: 0       Filter: 511     is_over: 1
ADC value: 8459 sigma: 0        Median: 0       Filter: 3577    is_over: 1
ADC value: 8455 sigma: 0        Median: 0       Filter: 11242   is_over: 1
ADC value: 8459 sigma: 0        Median: 0       Filter: 21462   is_over: 1
ADC value: 8465 sigma: 0        Median: 0       Filter: 29127   is_over: 1
ADC value: 8461 sigma: 0        Median: 0       Filter: 32193   is_over: 1
ADC value: 8465 sigma: 0        Median: 0       Filter: 32704   is_over: 1
ADC value: 8470 sigma: 0        Median: 0       Filter: 32704   is_over: 1
ADC value: 8478 sigma: 1        Median: 1       Filter: 32704   is_over: 1
ADC value: 8484 sigma: 1        Median: 1       Filter: 32704   is_over: 1
CHANNEL: 2
```


## Processing in the AVX code

Loop through all the registers (1 to 16). We have 16 registers of 16 samples per register which corresponds to 256 which are the channels 


For each superchunk you should have 16 lines like this: 

```sh
Register: 0
s:              +8447  +8492  +8502  +8586  +8471  +8587  +8514  +8460  +8554  +8415  +8479  +8564  +8648  +8467  +8541  +8547 
s:               +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511   +511 
median:            +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0 
sigma:             +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0 
filt:              +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0 
is_over:              +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0     +0 
```
Each register has 12 rows because they represent the 12 messages of the superchunk. After that you have the new register with other 16 channels with its 12 rows.


# Notes

Skipping superchunk will alter the results. This is because it will change the values of the the sigma and medians that are used for the frugal streaming. Changing the sigma affects the resulting number of TPs that are found from the algorithm because the last step is `bool is_over = filt > 5 * sigma * info.multiplier;`


Tested: 
Start from offset 0 
Start from offset 1 
Start from offset 100 


Noted that uin16_t has a maximum of 32k . If we assume that filt is the maximum value of 32k and sassume info..multiplier is 64 then sigma can be at most 100. If sigma gets above a certain threshold then we will never be able to hits. In the AVX code we do something like this `sigma = _mm256_min_epi16(sigma, sigmaMax);`. 



Setting the state
Loop through all the registers, loop through all the channels, look at the first message of the superchunk and read the ADC value. This will be used as the pedestal for the channel state. 


# Notes on the number of hits found

In the AVX code once the RS value is above the threhsold then you take the whole register. This means that if 1 of the 16 entries is above then you save the whole register. This means that nhits in the ProcessRSAVX2 code is misleading on the total number of hits.