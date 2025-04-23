/*
 RA4M1 Sine Wave Output Test
    A0 : DAC Output Pin
    Samplerate : 44100Hz
    Sine Wave  : 440Hz

    Output Pin = A0
    AGT1 Timer => DMA => DAC12
      The output buffer is placed in the extended repeat area (aligned to 512byte boundary)
      and output to the DAC12 as a ring buffer from the timer event.

    2025/04/23 g200kg
*/
#include <FspTimer.h>

#define SAMPLERATE 44100
#define SINE_FREQ 440.0

static FspTimer fsp_timer;

class DACOut {
public:
  constexpr static int buffSize = 256;
  static uint16_t buff[];
  static int buffWIndex;
  // timer
  constexpr static uint8_t timerType = AGT_TIMER;
  constexpr static uint8_t timerChannel = 1;
  // register
  constexpr static uint32_t mstp_mstpcrd    = 0x40047008; // Module Stop Control Register D
  constexpr static uint32_t dac12_dadr0     = 0x4005e000; // D/A Data Register 0
  constexpr static uint32_t dac12_dacr      = 0x4005e004; // D/A Control register
  constexpr static uint32_t dac12_dadpr     = 0x4005e005; // DADR0 Format Select Register
  constexpr static uint32_t dac12_daadscr   = 0x4005e006; // D/A A/D Synchronouse Start Control Register
  constexpr static uint32_t dac12_davrefcr  = 0x4005e007; // D/A VREF Control Register
  constexpr static uint32_t pfs_p014pfs     = 0x40040838; // Port 014 Pin Function Select Register
  constexpr static uint32_t dmac0_dmsar     = 0x40005000; // DMA0 Source Address Register
  constexpr static uint32_t dmac0_dmdar     = 0x40005004; // DMA0 Destination Address Register
  constexpr static uint32_t dmac0_dmcra     = 0x40005008; // DMA0 Transfer Count Register
  constexpr static uint32_t dmac0_dmcrb     = 0x4000500c; // DMA0 Block Transfer Count Register
  constexpr static uint32_t dmac0_dmtmd     = 0x40005010; // DMA0 Transfer Mode Register
  constexpr static uint32_t dmac0_dmint     = 0x40005013; // DMA0 Interrupt Setting Register
  constexpr static uint32_t dmac0_dmamd     = 0x40005014; // DMA0 Address Mode Register
  constexpr static uint32_t dmac0_dmofr     = 0x40005018; // DMA0 Offset Register
  constexpr static uint32_t dmac0_dmcnt     = 0x4000501c; // DMA0 Transfer Enable Register
  constexpr static uint32_t dmac0_dmreq     = 0x4000501d; // DMA0 Software Start Register
  constexpr static uint32_t dmac0_dmsts     = 0x4000501e; // DMA0 Status Register
  constexpr static uint32_t dma_dmast       = 0x40005200; // DMA Module activation Register
  constexpr static uint32_t icu_delsr0      = 0x40006280; // ICU DMAC EVent Link Setting Register 0

  DACOut(int sampleRate) {
    Serial.println("DACOut.Init");
    pinMode(DAC, OUTPUT);
    analogWriteResolution(12);

    fsp_timer.begin(TIMER_MODE_PERIODIC, timerType, static_cast<uint8_t>(timerChannel), (float)sampleRate, 25.0, nullptr, nullptr);
    fsp_timer.open();

    *(volatile uint32_t*)mstp_mstpcrd &= ~(0x100000);   // Module Stop Control D : DAC12 Module Enable
    *(volatile uint8_t*)dac12_dacr = 0x5f;              // DAC12 Output Enable : 0, DAOE0=1(enable), 0, 11111
    *(volatile uint8_t*)dac12_dadpr = 0;                // DAC12 Data Format : DPSEL=0(right align), 0000000
    *(volatile uint8_t*)dac12_daadscr = 0;              // DA AD Sync Start : DAADST=0(disable), 0000000
    *(volatile uint8_t*)dac12_davrefcr = 1;             // DA VREF Control : 00000, REF=001(AVCC0/AVSS0)
    *(volatile uint32_t*)pfs_p014pfs = 0x00000000;      // P014 Port Function Select : ASEL=DA0

    *(volatile uint8_t*)dma_dmast    = 1;                     // DMA Module Enable : DMST=1(enable)
    *(volatile uint32_t*)dmac0_dmsar = (uint32_t)buff;        // From buff
    *(volatile uint32_t*)dmac0_dmdar = dac12_dadr0;           // To dac12_dadr0
    *(volatile uint32_t*)dmac0_dmcra = 0x00000000;            // Count=0(freerun)
//    *(volatile uint16_t*)dmac0_dmtmd = 0b0010000100000000;  // Transfer Mode : MD=0b00(normal), DTS=0b10(norepeat,noblock), 00, SZ=0b01(16bit), 000000, DCTG=0b00(software)
    *(volatile uint16_t*)dmac0_dmtmd = 0b0010000100000001;    // Transfer Mode : MD=0b00(normal), DTS=0b10(norepeat,noblock), 00, SZ=0b01(16bit), 000000, DCTG=0b01(module)
    *(volatile uint16_t*)dmac0_dmamd = 0b1000100100000000;    // Address Mode : SM=0b10(src adrs inc), 0, SARA=0b01001(512byte), DM=0b00(dst adrs fix), 0, DARA=0b00000(none)
    *(volatile uint16_t*)icu_delsr0  = 0x21;                  // ICU DMA0 link from event AGT1_AGTI
    *(volatile uint8_t*)dmac0_dmcnt  = 1;                     // DMAC0 DTE=1, Transfer Enable

    fsp_timer.start();

    Serial.println("DACOut.Init OK");
  }
  void output(int val) {
    buff[buffWIndex++] = 2048 + val;
    if(buffWIndex >= buffSize)
      buffWIndex = 0;
  }
  static int getRIndex() {
    return (*(volatile uint32_t*)dmac0_dmsar - (int)DACOut::buff)>>1;
  }
  int outBuffAvailable() {
    int sz = buffWIndex - getRIndex();
    if(sz < 0)
      sz += buffSize;
    return buffSize - sz - 1;
  }
};
uint16_t DACOut::buff[DACOut::buffSize] __attribute__((aligned(512)));
int DACOut::buffWIndex = 0;

class Osc {
public:
  int* waveForm;
  int sampleRate;
  int delta, phase;
  Osc(int fs) {
    waveForm = new int[256];
    for(int i = 0; i < 256; ++i) {
      waveForm[i] = 2047 * (sin(2.0 * M_PI * i/256)); // -2047 to +2047
    }
    delta = phase = 0;
    sampleRate = fs;
    setFreq(SINE_FREQ);
  }
  void setFreq(double f) {
    delta = (int)(f / sampleRate * (256 * 4096));
    Serial.print("setFreq:");
    Serial.println(f,1);
  }
  int getData(void) {
    phase += delta;
    return waveForm[(phase >> 12) & 0xff];
  }
};

Osc* osc1;
DACOut* dacout;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Serial.begin");

  dacout = new DACOut(SAMPLERATE);
  osc1 = new Osc(SAMPLERATE);

  Serial.print("DACOut::buff address ");
  Serial.println((int)DACOut::buff, HEX);

  Serial.println("started.");
  osc1->setFreq(440.0);

}

unsigned long prev = 0;
void loop() {
  int available;
  if((available = dacout->outBuffAvailable()) > 0) {
    dacout->output(osc1->getData());
  }
  int now = millis();
  if(now > prev + 3000) {
    prev = now;
    Serial.print("OutBuff available:");
    Serial.println(available, DEC);
  }
}
