#include <stdint.h>

#include <system_ch32v00x.h>
#include <ch32v00x.h>
#include <ch32v00x_adc.h>
#include <ch32v00x_misc.h>
#include <debug.h>

#define DUTY_CH ADC_Channel_2
#define FREQ_CH ADC_Channel_0

/**
 * Init ADC in injected sequence mode for potentiometer readings
 */
static void ADC_Function_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {
        .GPIO_Mode = GPIO_Mode_AIN
    };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div24);

    // Setup gpio inputs used for analog readings
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitTypeDef ADC_InitStructure = {
        .ADC_Mode = ADC_Mode_Independent,
        .ADC_ScanConvMode = DISABLE,
        .ADC_ContinuousConvMode = DISABLE,
        .ADC_ExternalTrigConv = ADC_ExternalTrigInjecConv_None,
        .ADC_DataAlign = ADC_DataAlign_Right,
        .ADC_NbrOfChannel = 1
    };
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_InjectedSequencerLengthConfig(ADC1, 1); // configure the sequence channel later, right before conv is triggered
    ADC_ExternalTrigInjectedConvCmd(ADC1, DISABLE);

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    ADC_AutoInjectedConvCmd(ADC1, ENABLE);
    // ADC_InjectedDiscModeCmd(ADC1, ENABLE);
    // ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    // run calibration for (slightly) more accurate results
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
 * Configure Timer1 to drive LED with PWM for brightness control
 */
static void TIM1_PWM_In(u16 arr, u16 psc, u16 ccp) {
    TIM_OCInitTypeDef TIM_OCInitStructure={0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure={0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);
    GPIO_PinRemapConfig(AFIO_PCFR1_PA12_REMAP, DISABLE); // Disable remapping to external crystal

    GPIO_InitTypeDef GPIO_LEDInit = {
        .GPIO_Pin = GPIO_Pin_1,
        .GPIO_Speed = GPIO_Speed_30MHz,
        .GPIO_Mode = GPIO_Mode_AF_PP
    };
    GPIO_Init(GPIOA, &GPIO_LEDInit);

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; // Edge aligned
    // TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2; // Center aligned

    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; // output high when timer value > cmp
    TIM_OC2Init(TIM1, &TIM_OCInitStructure); // Output channel 2 (PA1)

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

static uint16_t adc_read(uint8_t ch) {
    ADC_ClearFlag(ADC1, ADC_JEOC);
    ADC_InjectedChannelConfig(ADC1, ch, 1, ADC_SampleTime_241Cycles); // set injected channel to take reading from
    ADC_SoftwareStartInjectedConvCmd(ADC1, ENABLE); // trigger injected conversion
    while (!ADC_GetFlagStatus(ADC1, ADC_JEOC)); // wait for injected reading complete flag
    return ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_1);
}

int main() {
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    ADC_Function_Init();
    TIM1_PWM_In(1000, 240 - 1, 0); // Period, timer prescaler, initial compare value

    uint16_t duty_pot, freq_pot, period, on_t, off_t;
    uint16_t i;

    for (;;) {
        // ADC is 10bit so raw readings go from 0-0x3ff
        duty_pot = adc_read(DUTY_CH);
        freq_pot = adc_read(FREQ_CH);

        // Calculate timings for next cycle
        period = (freq_pot << 2) + 500; // approx 1-5s (avoid mul and div if possible due to lack of hw instruction for these)
        on_t = period * duty_pot / 0x3ff;
        off_t = period - on_t;

        // Update PWM cycle
        for (i = 0; i < 1000; ++i) { // brightness ramp up
            TIM_SetCompare2(TIM1, i);
            Delay_Us(75);
        }
        TIM_SetCompare2(TIM1, 1000); // always on
        Delay_Ms(on_t); // On time
        for (i = 1000; i > 0; --i) { // brightness ramp down
            TIM_SetCompare2(TIM1, i);
            Delay_Us(75);
        }
        TIM_SetCompare2(TIM1, 0); // always off
        Delay_Ms(off_t); // Off time
    }
}
