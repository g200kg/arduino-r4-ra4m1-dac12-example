# arduino-r4-ra4m1-dac12-example
Arduino R4 RA4M1 Wave Generation with 12bit DAC and DMA

## RA4M1_DAC12_RegisterWrite
 This code is a test to generate a sine wave by directly manipulating the registers of a 12-bit DAC. It is faster than analogWrite() in the standard library because it manipulates the register directly.

> Sampling rate : 44100Hz.  
> Sine wave frequency : 440Hz.

## RA4M1_DAC12_DMA
This code is a test to generate a sine wave by kicking a 12bit DAC with DMA. No interrupt processing is required, so the CPU load is reduced.

> Sampling rate : 44100Hz  
> Sine wave frequency : 440Hz

---

Tested with XIAO RA4M1
![](./images/20250420_ra4m1_0.jpeg)

Generated wave
![](./images/pic_199_1.png)

RA4M1_DAC12_RegisterWrite D1 output is :  
 LO=interrupt  
 HI=mainloop  
 
The duty of the D1 output indicates the load of interrupt processing. It is about 10% of the total.
![](./images/pic_199_2.png)

