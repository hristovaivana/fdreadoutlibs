# Quick and dirty code for Absolute Running sum computation

import numpy as np
import math
#import matplotlib.pyplot as plt
#from dataclasses import dataclass


#@dataclass
class Hit :
    start:   float =0
    end :    float =0
    peakT:   float =0
    TOT:     float =0
    peakADC: float =0
    SADC:    float =0
   
#-------------------------------------------
#Primitive hit finding
#-------------------------------------------
def FindHits(waveform, thresh):
    is_hit = False

    hit_charge =[]    
    this_hit = Hit()
   
    hits = []

    for tick, adc  in enumerate(waveform):
        if (adc > thresh and is_hit==False):
            is_hit = True
            this_hit.start =tick
           
        if(is_hit == True):
            hit_charge.append(adc)
           
        if (is_hit and adc<thresh):
            this_hit.end  = tick
            is_hit = False
           
            for index, ADC in enumerate(hit_charge):
                if (ADC == np.max(hit_charge)):
                    this_hit.peakADC = ADC
                    this_hit.peakT = index
            this_hit.SADC = np.sum(hit_charge)
            this_hit.TOT = this_hit.end - this_hit.start
            hits.append(this_hit)

            #clean up for next hit
            hit_charge = []
            this_hit = Hit()


    return hits#return set of hits for waveform



#Estimate the pedestal
def frugal_update(raw_in, ncontig):
   
    median = raw_in[0]
    runningDiff = 0
    pedsub = []
   
    for i in range(0,len(raw_in)):
        sample = raw_in[i]
       
        if(sample>median): runningDiff +=1
        if(sample<median): runningDiff -=1
       
        if (runningDiff > ncontig):
            median +=1
            runningDiff = 0
        if(runningDiff < -1*ncontig):
            median -=1
            runningDiff = 0
        pedsub.append( median)
        #print(i, median)
    return pedsub
   



#Do the actual pedestal subtraction
def ped_sub(waveform):
    ncontig = 10
    ped = frugal_update(waveform,ncontig)
   
    pedestal_subtracted = []
   
    for i in range(0,len(waveform)):
        pedestal_subtracted.append(waveform[i] - ped[i])
   
    return pedestal_subtracted

   
def single_frugal_update(median, raw_input, runningDiff, ncontig):
    pedsub = []
   
    sample = raw_input
    
    if(sample>median): runningDiff +=1
    if(sample<median): runningDiff -=1
    
    if (runningDiff > ncontig):
        median +=1
        runningDiff = 0
    if(runningDiff < -1*ncontig):
        median -=1
        runningDiff = 0
    
    return median, runningDiff

def AbsRunningSum(data, R, s):
    RS = []
    I_RS = data[0]

    for i in data:
        print(int(R*I_RS), int(abs(i/s)))

        I_RS = int(R*I_RS) + int(abs(i/s))
        RS.append(I_RS)

    return np.array(RS)



# Performs the Absolute Running Sum and 
# then the pedestal subtraction on the RS values
def AbsRunningSumComplete(data, R, s):
    RS = []
    I_RS = data[0]
    accumulator = 0
    medianRS = 0 
   
    counter = 0
    for i in data:
        first_part = R*(I_RS-medianRS)
        second_part = abs(i/s)
        I_RS = round(first_part + second_part)
        
        medianRS, accumulator = single_frugal_update(medianRS, I_RS, accumulator, 10)

        RS.append(I_RS-medianRS)
  
   

        print("First part: ", float("{:.2f}".format(first_part)), "\tSecond part: ", second_part, "\tRS_before: ", I_RS, "  \tMedianRS ", medianRS, "\tRS: ", I_RS-medianRS)
        counter += 1    

        if counter % 12 ==0:
            print("="*24, "New superchunk")

    return np.array(RS)




#FIR filtering
def apply_fir_filter(waveform):
   
    taps = [2,9,23,31,23,9,2]
    ntaps = len(taps)
    filtered = []
   
    for i in range(0,len(waveform)):
        filt = 0
        for j in range(0,ntaps):
           
            index = 0
            if (i>j):
                index = i-j
            else:
                index = 0
            filt += (waveform[index]*taps[j])
        filtered.append(filt)
    return filtered



PATH_FILE = "/nfs/sw/work_dirs/aabedabu/for_klaudia/raw_adc_221108/all_channels_RSNaive_data08-11-2022_09-12.txt"
data = np.loadtxt(PATH_FILE, delimiter = ',')


  
single_chan_waveform = []
channel = 0
for i in data:
    if i[0] == channel:
        single_chan_waveform.append(i[1])
       
ped = ped_sub(single_chan_waveform)
AbsRS = AbsRunningSumComplete(ped, 0.8,2)
#ped2 = ped_sub(AbsRS)

print('-------------')
for i in range(0,len(ped)):
    print(single_chan_waveform[i], "\t", ped[i], "\t", AbsRS[i])


# Plotting
#for j in range (0,1):
#    waveform = []

#    for i in data:
#        if (i[0] == j):
#            waveform.append(i[1])
#    plt.plot(waveform)
#    plt.show()
   
 