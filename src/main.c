#include <stdint.h>

#include <system_ch32v00x.h>
#include <ch32v00x.h>
#include <ch32v00x_adc.h>
#include <ch32v00x_misc.h>
#include <debug.h>

#define DUTY_CH ADC_Channel_2
#define FREQ_CH ADC_Channel_0

static void ADC_Function_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {
        .GPIO_Mode = GPIO_Mode_AIN
    };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

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

    ADC_InjectedSequencerLengthConfig(ADC1, 2);
    ADC_InjectedChannelConfig(ADC1, DUTY_CH, 1, ADC_SampleTime_241Cycles);
    ADC_InjectedChannelConfig(ADC1, FREQ_CH, 2, ADC_SampleTime_241Cycles);

    // ADC_DiscModeChannelCountConfig(ADC1, 1);
    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    // ADC_InjectedDiscModeCmd(ADC1, ENABLE);
    // ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);
    ADC_AutoInjectedConvCmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
 * 
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
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    // TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_OC2Init(TIM1, &TIM_OCInitStructure); // Output channel 2 (PA1)

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

static void adc_trigger() {
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_JEOC));
    ADC_ClearFlag(ADC1, ADC_FLAG_JEOC);
}


int main() {
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    // LED_GPIO_Init();
    ADC_Function_Init();
    TIM1_PWM_In(1000, 48000 - 1, 0); // Period, timer prescaler, initial compare value

    uint32_t duty_pot, freq_pot, period, on_t, off_t;
    uint16_t i;

    for (;;) {
        adc_trigger();
        // ADC is 10bit so raw idata readings go from 0-0x3ff
        duty_pot = ADC1->IDATAR1;
        freq_pot = ADC1->IDATAR2;

        // Calculate timings for next cycle
        period = (freq_pot << 3) + 1000; // approx 1-9s
        on_t = period * duty_pot / 0x3ff;
        off_t = period - on_t;

        // Update PWM cycle
        for (i = 0; i < 1000; ++i) {
            TIM_SetCompare2(TIM1, i);
            Delay_Us(100);
        }
        TIM_SetCompare2(TIM1, 1000); // always on
        Delay_Ms(on_t); // On time
        for (i = 1000; i > 0; --i) {
            TIM_SetCompare2(TIM1, i);
            Delay_Us(100);
        }
        TIM_SetCompare2(TIM1, 0); // always off
        Delay_Ms(off_t); // Off time
    }
}
