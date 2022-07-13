
#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_HELLO_WORLD_MAIN_FUNCTIONS_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_HELLO_WORLD_MAIN_FUNCTIONS_H_


#ifdef __cplusplus
extern "C" {
#endif

//Used to pass classification output of neural network
struct classification {
	int index;
	float probability;
};
// Initialize neural network
void blueseer_setup();

// Predict environment of given data sample
void blueseer_infer(int data_sample[230], struct classification *ptr);

#ifdef __cplusplus
}
#endif

#endif 
