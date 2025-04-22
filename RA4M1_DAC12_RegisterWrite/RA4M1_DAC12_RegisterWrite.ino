/*
 RA4M1 Sine Wave Output Test
    A0 : DAC Output Pin
    D1 : 1=normal 0=interrupt handler
    LEDBUILTIN ON : wave generation underflow
    Samplerate : 44100Hz
    Sine Wave  : 440Hz

    AGT1 Timer => Write to DAC12 Register
      loop()     : write to DACOut::buff
      timerCallback() : DACOut::buff => DAC12

    2025/04/23 g200kg
*/
#include <FspTimer.h>

#define SAMPLERATE 44100
#define SINE_FREQ 440.0

static FspTimer fsp_timer;

class DACOut {
public:
  constexpr static int buffSize = 256;
  static int16_t* buff;
  static int buffRIndex;
  static int buffWIndex;
  // register
  constexpr static uint32_t mstp_mstpcrd    = 0x40047008; // Module Stop Control Register D
  constexpr static uint32_t dac12_dadr0     = 0x4005e000; // D/A Data Register 0
  constexpr static uint32_t dac12_dacr      = 0x4005e004; // D/A Control register
  constexpr static uint32_t dac12_dadpr     = 0x4005e005; // DADR0 Format Select Register
  constexpr static uint32_t dac12_daadscr   = 0x4005e006; // D/A A/D Synchronouse Start Control Register
  constexpr static uint32_t dac12_davrefcr  = 0x4005e007; // D/A VREF Control Register
  constexpr static uint32_t pfs_p014pfs     = 0x40040838; // Port 014 Pin Function Select Register

  DACOut(int sampleRate) {
    Serial.println("DACOut.Init");
    pinMode(DAC, OUTPUT);
    analogWriteResolution(12);

    *(volatile uint32_t*)mstp_mstpcrd &= ~(0x100000);   // Module Stop Control D : DAC12 Module Enable
    *(volatile uint8_t*)dac12_dacr = 0x5f;              // DAC12 Output Enable : 0, DAOE0=1(enable), 0, 11111
    *(volatile uint8_t*)dac12_dadpr = 0;                // DAC12 Data Format : DPSEL=0(right align), 0000000
    *(volatile uint8_t*)dac12_daadscr = 0;              // DA AD Sync Start : DAADST=0(disable), 0000000
    *(volatile uint8_t*)dac12_davrefcr = 1;             // DA VREF Control : 00000, REF=001(AVCC0/AVSS0)
    *(volatile uint32_t*)pfs_p014pfs = 0x00000000;      // P014 Port Function Select : ASEL=DA0

    uint8_t timer_type;
    int8_t timer_ch = FspTimer::get_available_timer(timer_type);
    if (timer_ch < 0) {
      Serial.println("Error : get_available_timer()");
      return;
    }
    fsp_timer.begin(TIMER_MODE_PERIODIC, timer_type, static_cast<uint8_t>(timer_ch), sampleRate, 25.0, DACOut::timerCallback, nullptr);
    fsp_timer.setup_overflow_irq();
    fsp_timer.open();
    fsp_timer.start();
    Serial.println("DACOut.Init OK");
  }
  void output(int val) {
    buff[buffWIndex++] = 2048 + val;
    if(buffWIndex >= buffSize)
      buffWIndex = 0;
  }
  int outBuffAvailable() {
    int sz = buffWIndex - buffRIndex;
    if(sz < 0)
      sz += buffSize;
    return buffSize - sz - 1;
  }
  static void timerCallback([[maybe_unused]]timer_callback_args_t *arg) {
    digitalWrite(D1, LOW);

    if(buffRIndex == buffWIndex) {  // if data generation underrun
      digitalWrite(LED_BUILTIN, LOW);
      return;
    }
    else {
      digitalWrite(LED_BUILTIN, HIGH);
    }

    *(uint16_t*)dac12_dadr0 = buff[buffRIndex]; // write to DAC12
    if(++buffRIndex >= buffSize)
      buffRIndex = 0;
    digitalWrite(D1, HIGH);
  }
};
int16_t* DACOut::buff = new int16_t[DACOut::buffSize];
int DACOut::buffRIndex = 0;
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

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D1, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on

  dacout = new DACOut(SAMPLERATE);
  osc1 = new Osc(SAMPLERATE);

  Serial.println("started.");
  osc1->setFreq(SINE_FREQ);

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