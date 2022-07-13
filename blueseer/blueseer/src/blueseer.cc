/*
Using a predefined neural network we predict the environment to a given data sample.
*/

#include "module_config.h"

#ifdef WITH_CLASSIFICATION

#include "blueseer.h"

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "blueseer_model.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
//#include "tensorflow/lite/version.h"
#include <sys/printk.h>
#include <math.h>

#include <zephyr.h>

#define DATA_LINE_LENGTH 23
#define DATA_ROWS 5

static float normalized_vector[DATA_LINE_LENGTH*DATA_ROWS];


namespace
{
tflite::ErrorReporter *error_reporter = nullptr;
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

// Create an area of memory to use for input, output, and intermediate arrays.
// const int kModelArenaSize = 6064;
// Extra headroom for model + alignment + future interpreter changes.
// const int kExtraArenaSize = 560 + 16 + 160;
const int kTensorArenaSize = 2048; //kModelArenaSize + kExtraArenaSize;
static uint8_t tensor_arena[kTensorArenaSize];
} 

/*
normalize data sample; must be done since neural network is trained with normalized data
*/
void normalize_input(int raw_vector[], int length, float *normalized_vector)
{
	for (int i = 0; i < length; i++) {
		normalized_vector[i] = (raw_vector[i] - mean_list[i]) / std_list[i];
	}
}

/*
initialize neural network
*/
void blueseer_setup()
{

	static tflite::MicroErrorReporter micro_error_reporter;
	error_reporter = &micro_error_reporter;

	model = tflite::GetModel(blueseer_model);

	if (model->version() != TFLITE_SCHEMA_VERSION) {
		#ifdef WITH_LOGGING
		TF_LITE_REPORT_ERROR(error_reporter,
				     "Model provided is schema version %d not equal "
				     "to supported version %d.",
				     model->version(), TFLITE_SCHEMA_VERSION);
		printk("model not supported\n");
		#endif /* WITH_LOGGING */
		return;
	}

	//static tflite::AllOpsResolver resolver;
	static tflite::MicroMutableOpResolver<5> resolver;
	resolver.AddFullyConnected();
	resolver.AddSoftmax();
	resolver.AddRelu();
	resolver.AddQuantize();
	resolver.AddDequantize();

	// Build an interpreter to run the model with.
	static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena,
							   kTensorArenaSize, error_reporter);
	interpreter = &static_interpreter;

	// Allocate memory from the tensor_arena for the model's tensors.
	TfLiteStatus allocate_status = interpreter->AllocateTensors();

	if (allocate_status != kTfLiteOk) {
		#ifdef WITH_LOGGING
		TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
		printk("tensor allocation failed\n");
		#endif /* WITH_LOGGING */
		return;
	}

	// Obtain pointers to the model's input and output tensors.
	input = interpreter->input(0);
	output = interpreter->output(0);

	//print expected and actual input size and used memory 
	int expected = input->dims->data[1];
	#ifdef WITH_LOGGING
	printk("sample input: %d, expected input: %d, used tensor bytes: %d\n", DATA_LINE_LENGTH*DATA_ROWS, expected, interpreter->arena_used_bytes());
	#endif /* WITH_LOGGING */
}

/*
predict data sample with pretrained neural network
*/
void blueseer_infer(int data_sample[DATA_LINE_LENGTH*DATA_ROWS], struct classification *ptr)
{

	normalize_input(data_sample, DATA_LINE_LENGTH*DATA_ROWS, normalized_vector);

	for (int i = 0; i < DATA_LINE_LENGTH*DATA_ROWS; i++) {
		input->data.f[i] = normalized_vector[i];
	}

	//execute network
	interpreter->Invoke();
	float max_value = 0;
	int env_index_pred = -1;

	//find environment with highest probability
	for (int i = 0; i<available_env_len; i++){
		float pred = output->data.f[i];
		if( pred> max_value){
			max_value = pred;
			env_index_pred = i;
		}
	}
			#ifdef WITH_LOGGING
			printk("Pred: ");
			for (int i=0;i<7;++i){
				printk("%i->%i.%i; ", i, (int) (output->data.f[i]*100), ((int)(output->data.f[i]*10000))%100);
			}
			printk("\n");
			#endif /* WITH_LOGGING */
	

	ptr->index = env_index_pred;
	ptr->probability = max_value;
}

#endif /* WITH_CLASSIFICIATION */
