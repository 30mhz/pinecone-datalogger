/*
 * Dendro.h
 *
 * Created: 10/18/2016 5:30:58 PM
 *  Author: tim.anderson
 */ 

#ifndef DENDRO_H_
#define DENDRO_H_

#include<stdint.h>

void ConfigureDendroADC(struct adc_module *adcModule, const uint32_t dendAnalogPin);
double ReadDendro(const struct adc_module *adcModule);

#endif /* DENDRO_H_ */