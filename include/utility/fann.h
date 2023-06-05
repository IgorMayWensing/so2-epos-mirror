/*
    This code is a port of:

    Fast Artificial Neural Network Library (fann)
    Copyright (C) 2003-2016 Steffen Nissen (steffen.fann@gmail.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
*/

#include <utility/string.h>
#include <system.h>
#include <utility/ostream.h>
#include <utility/math.h>

#ifndef __fann_h
#define __fann_h

__BEGIN_UTIL

class FANN
{
public:
    typedef float fann_type;
    typedef char FILE;

public:
    static const unsigned int NUM_LAYERS_CONFIG           = 4;
    static constexpr float    LEARNING_RATE_CONFIG        = 0.35;
    static constexpr float    CONNECTION_RATE_CONFIG      = 1.000000;
    static const unsigned int NETWORK_TYPE_CONFIG         = 0;
    static constexpr float    LEARNING_MOMENTUM_CONFIG    = 0.000000;
    static const unsigned int TRAINING_ALGORITHM_CONFIG   = 0; // 0 = INCREMENTAL - for online training support
    static const unsigned int TRAIN_ERROR_FUNCTION_CONFIG = 0;
    static const unsigned int TRAIN_STOP_FUNCTION_CONFIG  = 0;
    static constexpr double   BIT_FAIL_LIMIT_CONFIG       = 3.49999994039535522461e-01;

private: // Internal Macros
    #define fann_mult(x,y) (x*y)
    #define fann_abs(value) (((value) > 0) ? (value) : -(value))
    #define fann_clip(x, lo, hi) (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

    #define fann_linear_func(v1, r1, v2, r2, sum) (((((r2)-(r1)) * ((sum)-(v1)))/((v2)-(v1))) + (r1))
    #define fann_stepwise(v1, v2, v3, v4, v5, v6, r1, r2, r3, r4, r5, r6, min, max, sum) (sum < v5 ? (sum < v3 ? (sum < v2 ? (sum < v1 ? min : fann_linear_func(v1, r1, v2, r2, sum)) : fann_linear_func(v2, r2, v3, r3, sum)) : (sum < v4 ? fann_linear_func(v3, r3, v4, r4, sum) : fann_linear_func(v4, r4, v5, r5, sum))) : (sum < v6 ? fann_linear_func(v5, r5, v6, r6, sum) : max))


    /* FANN_LINEAR */
    /* #define fann_linear(steepness, sum) fann_mult(steepness, sum) */
    #define fann_linear_derive(steepness, value) (steepness)

    /* FANN_SIGMOID */
    /* #define fann_sigmoid(steepness, sum) (1.0f/(1.0f + exp(-2.0f * steepness * sum))) */
    // #define fann_sigmoid_real(sum) (1.0f/(1.0f + FANN_EXP(-2.0f * sum)))
    #define fann_sigmoid_derive(steepness, value) (2.0f * steepness * value * (1.0f - value))

    /* FANN_SIGMOID_SYMMETRIC */
    /* #define fann_sigmoid_symmetric(steepness, sum) (2.0f/(1.0f + exp(-2.0f * steepness * sum)) - 1.0f) */
    // #define fann_sigmoid_symmetric_real(sum) (2.0f/(1.0f + FANN_EXP(-2.0f * sum)) - 1.0f)
    #define fann_sigmoid_symmetric_derive(steepness, value) steepness * (1.0f - (value*value))

    /* FANN_GAUSSIAN */
    /* #define fann_gaussian(steepness, sum) (exp(-sum * steepness * sum * steepness)) */
    // #define fann_gaussian_real(sum) (FANN_EXP(-sum * sum))
    #define fann_gaussian_derive(steepness, value, sum) (-2.0f * sum * value * steepness * steepness)

    /* FANN_GAUSSIAN_SYMMETRIC */
    /* #define fann_gaussian_symmetric(steepness, sum) ((exp(-sum * steepness * sum * steepness)*2.0)-1.0) */
    // #define fann_gaussian_symmetric_real(sum) ((FANN_EXP(-sum * sum)*2.0f)-1.0f)
    #define fann_gaussian_symmetric_derive(steepness, value, sum) (-2.0f * sum * (value+1.0f) * steepness * steepness)

    /* FANN_ELLIOT */
    /* #define fann_elliot(steepness, sum) (((sum * steepness) / 2.0f) / (1.0f + fann_abs(sum * steepness)) + 0.5f) */
    #define fann_elliot_real(sum) (((sum) / 2.0f) / (1.0f + fann_abs(sum)) + 0.5f)
    #define fann_elliot_derive(steepness, value, sum) (steepness * 1.0f / (2.0f * (1.0f + fann_abs(sum)) * (1.0f + fann_abs(sum))))

    /* FANN_ELLIOT_SYMMETRIC */
    /* #define fann_elliot_symmetric(steepness, sum) ((sum * steepness) / (1.0f + fann_abs(sum * steepness)))*/
    #define fann_elliot_symmetric_real(sum) ((sum) / (1.0f + fann_abs(sum)))
    #define fann_elliot_symmetric_derive(steepness, value, sum) (steepness * 1.0f / ((1.0f + fann_abs(sum)) * (1.0f + fann_abs(sum))))

    #define fann_activation_switch(activation_function, value, result) \
    switch(activation_function) \
    { \
        case FANN_LINEAR: \
            result = (fann_type)value; \
            break; \
        case FANN_LINEAR_PIECE: \
            result = (fann_type)((value < 0) ? 0 : (value > 1) ? 1 : value); \
            break; \
        case FANN_LINEAR_PIECE_SYMMETRIC: \
            result = (fann_type)((value < -1) ? -1 : (value > 1) ? 1 : value); \
            break; \
        case FANN_SIGMOID_SYMMETRIC_STEPWISE: \
            result = (fann_type)fann_stepwise(((fann_type)-2.64665293693542480469e+00), ((fann_type)-1.47221934795379638672e+00), ((fann_type)-5.49306154251098632812e-01), ((fann_type)5.49306154251098632812e-01), ((fann_type)1.47221934795379638672e+00), ((fann_type)2.64665293693542480469e+00), ((fann_type)-9.90000009536743164062e-01), ((fann_type)-8.99999976158142089844e-01), ((fann_type)-5.00000000000000000000e-01), ((fann_type)5.00000000000000000000e-01), ((fann_type)8.99999976158142089844e-01), ((fann_type)9.90000009536743164062e-01), -1, 1, value); \
            break; \
        case FANN_SIGMOID_STEPWISE: \
            result = (fann_type)fann_stepwise(((fann_type)-2.64665246009826660156e+00), ((fann_type)-1.47221946716308593750e+00), ((fann_type)-5.49306154251098632812e-01), ((fann_type)5.49306154251098632812e-01), ((fann_type)1.47221934795379638672e+00), ((fann_type)2.64665293693542480469e+00), ((fann_type) 4.99999988824129104614e-03), ((fann_type) 5.00000007450580596924e-02), ((fann_type) 2.50000000000000000000e-01), ((fann_type)7.50000000000000000000e-01), ((fann_type)9.49999988079071044922e-01), ((fann_type)9.95000004768371582031e-01), 0, 1, value); \
            break; \
        case FANN_THRESHOLD: \
            result = (fann_type)((value < 0) ? 0 : 1); \
            break; \
        case FANN_THRESHOLD_SYMMETRIC: \
            result = (fann_type)((value < 0) ? -1 : 1); \
            break; \
        case FANN_ELLIOT: \
            result = (fann_type)fann_elliot_real(value); \
            break; \
        case FANN_ELLIOT_SYMMETRIC: \
            result = (fann_type)fann_elliot_symmetric_real(value); \
            break; \
        default: \
            result = 0; \
            break; \
    }

    inline static void * malloc(size_t bytes) { return operator new(bytes, SYSTEM); }

public: // Structs and Enums

    enum fann_errno_enum
    {
        FANN_E_NO_ERROR = 0,
        FANN_E_CANT_OPEN_CONFIG_R,
        FANN_E_CANT_OPEN_CONFIG_W,
        FANN_E_WRONG_CONFIG_VERSION,
        FANN_E_CANT_READ_CONFIG,
        FANN_E_CANT_READ_NEURON,
        FANN_E_CANT_READ_CONNECTIONS,
        FANN_E_WRONG_NUM_CONNECTIONS,
        FANN_E_CANT_OPEN_TD_W,
        FANN_E_CANT_OPEN_TD_R,
        FANN_E_CANT_READ_TD,
        FANN_E_CANT_ALLOCATE_MEM,
        FANN_E_CANT_TRAIN_ACTIVATION,
        FANN_E_CANT_USE_ACTIVATION,
        FANN_E_TRAIN_DATA_MISMATCH,
        FANN_E_CANT_USE_TRAIN_ALG,
        FANN_E_TRAIN_DATA_SUBSET,
        FANN_E_INDEX_OUT_OF_BOUND,
        FANN_E_SCALE_NOT_PRESENT,
        FANN_E_INPUT_NO_MATCH,
        FANN_E_OUTPUT_NO_MATCH,
        FANN_E_WRONG_PARAMETERS_FOR_CREATE
    };

    enum fann_nettype_enum
    {
        FANN_NETTYPE_LAYER = 0, /* Each layer only has connections to the next layer */
        FANN_NETTYPE_SHORTCUT /* Each layer has connections to all following layers */
    };

    enum fann_errorfunc_enum
    {
        FANN_ERRORFUNC_LINEAR = 0,
        FANN_ERRORFUNC_TANH
    };

    enum fann_activationfunc_enum
    {
        FANN_LINEAR = 0,
        FANN_THRESHOLD,
        FANN_THRESHOLD_SYMMETRIC,
        FANN_SIGMOID,
        FANN_SIGMOID_STEPWISE,
        FANN_SIGMOID_SYMMETRIC,
        FANN_SIGMOID_SYMMETRIC_STEPWISE,
        FANN_GAUSSIAN,
        FANN_GAUSSIAN_SYMMETRIC,
        /* Stepwise linear approximation to gaussian.
        * Faster than gaussian but a bit less precise.
        * NOT implemented yet.
        */
        FANN_GAUSSIAN_STEPWISE,
        FANN_ELLIOT,
        FANN_ELLIOT_SYMMETRIC,
        FANN_LINEAR_PIECE,
        FANN_LINEAR_PIECE_SYMMETRIC,
        FANN_SIN_SYMMETRIC,
        FANN_COS_SYMMETRIC,
        FANN_SIN,
        FANN_COS
    };


    enum fann_stopfunc_enum
    {
        FANN_STOPFUNC_MSE = 0,
        FANN_STOPFUNC_BIT
    };

    enum fann_train_enum
    {
        FANN_TRAIN_INCREMENTAL = 0,
        FANN_TRAIN_BATCH,
        FANN_TRAIN_RPROP,
        FANN_TRAIN_QUICKPROP,
        FANN_TRAIN_SARPROP
    };

    struct fann_neuron
    {
        /* Index to the first and last connection
        * (actually the last is a past end index)
        */
        unsigned int first_con;
        unsigned int last_con;
        /* The sum of the inputs multiplied with the weights */
        fann_type sum;
        /* The value of the activation function applied to the sum */
        fann_type value;
        /* The steepness of the activation function */
        fann_type activation_steepness;
        /* Used to choose which activation function to use */
        enum fann_activationfunc_enum activation_function;
    };

    struct fann_connection
    {
        /* Unique number used to identify source neuron */
        unsigned int from_neuron;
        /* Unique number used to identify destination neuron */
        unsigned int to_neuron;
        /* The numerical value of the weight */
        fann_type weight;
    };

    /* A single layer in the neural network.
    */
    struct fann_layer
    {
        /* A pointer to the first neuron in the layer
        * When allocated, all the neurons in all the layers are actually
        * in one long array, this is because we want to easily clear all
        * the neurons at once.
        */
        struct fann_neuron *first_neuron;

        /* A pointer to the neuron past the last neuron in the layer */
        /* the number of neurons is last_neuron - first_neuron */
        struct fann_neuron *last_neuron;
    };

    struct fann
    {
        /* The type of error that last occured. */
        enum fann_errno_enum errno_f;

        /* Where to log error messages. */
        FILE *error_log;

        /* A string representation of the last error. */
        char *errstr;

        /* the learning rate of the network */
        float learning_rate;

        /* The learning momentum used for backpropagation algorithm. */
        float learning_momentum;

        /* the connection rate of the network
        * between 0 and 1, 1 meaning fully connected
        */
        float connection_rate;

        /* is 1 if shortcut connections are used in the ann otherwise 0
        * Shortcut connections are connections that skip layers.
        * A fully connected ann with shortcut connections are a ann where
        * neurons have connections to all neurons in all later layers.
        */
        enum fann_nettype_enum network_type;

        /* pointer to the first layer (input layer) in an array af all the layers,
        * including the input and outputlayers
        */
        struct fann_layer *first_layer;

        /* pointer to the layer past the last layer in an array af all the layers,
        * including the input and outputlayers
        */
        struct fann_layer *last_layer;

        /* Total number of neurons.
        * very useful, because the actual neurons are allocated in one long array
        */
        unsigned int total_neurons;

        /* Number of input neurons (not calculating bias) */
        unsigned int num_input;

        /* Number of output neurons (not calculating bias) */
        unsigned int num_output;

        /* The weight array */
        fann_type *weights;

        /* The connection array */
        struct fann_neuron **connections;

        /* Used to contain the errors used during training
        * Is allocated during first training session,
        * which means that if we do not train, it is never allocated.
        */
        fann_type *train_errors;

        /* Training algorithm used when calling fann_train_on_..
        */
        enum fann_train_enum training_algorithm;

        /* Total number of connections.
        * very useful, because the actual connections
        * are allocated in one long array
        */
        unsigned int total_connections;

        /* used to store outputs in */
        fann_type *output;

        /* the number of data used to calculate the mean square error.
        */
        unsigned int num_MSE;

        /* the total error value.
        * the real mean square error is MSE_value/num_MSE
        */
        float MSE_value;

        /* The number of outputs which would fail (only valid for classification problems)
        */
        unsigned int num_bit_fail;

        /* The maximum difference between the actual output and the expected output
        * which is accepted when counting the bit fails.
        * This difference is multiplied by two when dealing with symmetric activation functions,
        * so that symmetric and not symmetric activation functions can use the same limit.
        */
        fann_type bit_fail_limit;

        /* The error function used during training. (default FANN_ERRORFUNC_TANH)
        */
        enum fann_errorfunc_enum train_error_function;

        /* The stop function used during training. (default FANN_STOPFUNC_MSE)
        */
        enum fann_stopfunc_enum train_stop_function;

        /* The callback function used during training. (default NULL)
        */
        unsigned int callback;

        /* A pointer to user defined data. (default NULL)
        */
        void *user_data;

        /* Variables for use with Cascade Correlation */

        /* The error must change by at least this
        * fraction of its old value to count as a
        * significant change.
        */
        float cascade_output_change_fraction;

        /* No change in this number of epochs will cause
        * stagnation.
        */
        unsigned int cascade_output_stagnation_epochs;

        /* The error must change by at least this
        * fraction of its old value to count as a
        * significant change.
        */
        float cascade_candidate_change_fraction;

        /* No change in this number of epochs will cause
        * stagnation.
        */
        unsigned int cascade_candidate_stagnation_epochs;

        /* The current best candidate, which will be installed.
        */
        unsigned int cascade_best_candidate;

        /* The upper limit for a candidate score
        */
        fann_type cascade_candidate_limit;

        /* Scale of copied candidate output weights
        */
        fann_type cascade_weight_multiplier;

        /* Maximum epochs to train the output neurons during cascade training
        */
        unsigned int cascade_max_out_epochs;

        /* Maximum epochs to train the candidate neurons during cascade training
        */
        unsigned int cascade_max_cand_epochs;

        /* Minimum epochs to train the output neurons during cascade training
        */
        unsigned int cascade_min_out_epochs;

        /* Minimum epochs to train the candidate neurons during cascade training
        */
        unsigned int cascade_min_cand_epochs;

        /* An array consisting of the activation functions used when doing
        * cascade training.
        */
        enum fann_activationfunc_enum *cascade_activation_functions;

        /* The number of elements in the cascade_activation_functions array.
        */
        unsigned int cascade_activation_functions_count;

        /* An array consisting of the steepnesses used during cascade training.
        */
        fann_type *cascade_activation_steepnesses;

        /* The number of elements in the cascade_activation_steepnesses array.
        */
        unsigned int cascade_activation_steepnesses_count;

        /* The number of candidates of each type that will be present.
        * The actual number of candidates is then
        * cascade_activation_functions_count *
        * cascade_activation_steepnesses_count *
        * cascade_num_candidate_groups
        */
        unsigned int cascade_num_candidate_groups;

        /* An array consisting of the score of the individual candidates,
        * which is used to decide which candidate is the best
        */
        fann_type *cascade_candidate_scores;

        /* The number of allocated neurons during cascade correlation algorithms.
        * This number might be higher than the actual number of neurons to avoid
        * allocating new space too often.
        */
        unsigned int total_neurons_allocated;

        /* The number of allocated connections during cascade correlation algorithms.
        * This number might be higher than the actual number of neurons to avoid
        * allocating new space too often.
        */
        unsigned int total_connections_allocated;

        /* Variables for use with Quickprop training */

        /* Decay is used to make the weights not go so high */
        float quickprop_decay;

        /* Mu is a factor used to increase and decrease the stepsize */
        float quickprop_mu;

        /* Variables for use with with RPROP training */

        /* Tells how much the stepsize should increase during learning */
        float rprop_increase_factor;

        /* Tells how much the stepsize should decrease during learning */
        float rprop_decrease_factor;

        /* The minimum stepsize */
        float rprop_delta_min;

        /* The maximum stepsize */
        float rprop_delta_max;

        /* The initial stepsize */
        float rprop_delta_zero;

        /* Defines how much the weights are constrained to smaller values at the beginning */
        float sarprop_weight_decay_shift;

        /* Decides if the stepsize is too big with regard to the error */
        float sarprop_step_error_threshold_factor;

        /* Defines how much the stepsize is influenced by the error */
        float sarprop_step_error_shift;

        /* Defines how much the epoch influences weight decay and noise */
        float sarprop_temperature;

        /* Current training epoch */
        unsigned int sarprop_epoch;

        /* Used to contain the slope errors used during batch training
        * Is allocated during first training session,
        * which means that if we do not train, it is never allocated.
        */
        fann_type *train_slopes;

        /* The previous step taken by the quickprop/rprop procedures.
        * Not allocated if not used.
        */
        fann_type *prev_steps;

        /* The slope values used by the quickprop/rprop procedures.
        * Not allocated if not used.
        */
        fann_type *prev_train_slopes;

        /* The last delta applied to a connection weight.
        * This is used for the momentum term in the backpropagation algorithm.
        * Not allocated if not used.
        */
        fann_type *prev_weights_deltas;

        /* Arithmetic mean used to remove steady component in input data.  */
        float *scale_mean_in;

        /* Standart deviation used to normalize input data (mostly to [-1;1]). */
        float *scale_deviation_in;

        /* User-defined new minimum for input data.
        * Resulting data values may be less than user-defined minimum.
        */
        float *scale_new_min_in;

        /* Used to scale data to user-defined new maximum for input data.
        * Resulting data values may be greater than user-defined maximum.
        */
        float *scale_factor_in;

        /* Arithmetic mean used to remove steady component in output data.  */
        float *scale_mean_out;

        /* Standart deviation used to normalize output data (mostly to [-1;1]). */
        float *scale_deviation_out;

        /* User-defined new minimum for output data.
        * Resulting data values may be less than user-defined minimum.
        */
        float *scale_new_min_out;

        /* Used to scale data to user-defined new maximum for output data.
        * Resulting data values may be greater than user-defined maximum.
        */
        float *scale_factor_out;

        /* Incremental training threshold for stopping training */
        float _train_error_threshold;
    };

private: //Fann Allocate Methods

    static struct fann* fann_allocate_structure(unsigned int num_layers) {
        struct fann* ann = (struct fann *) malloc(sizeof(struct fann));

        ann->errno_f = FANN_E_NO_ERROR;
        ann->error_log = 0;
        ann->errstr = 0;
        ann->learning_rate = 0.7f;
        ann->learning_momentum = 0.0;
        ann->total_neurons = 0;
        ann->total_connections = 0;
        ann->num_input = 0;
        ann->num_output = 0;
        ann->train_errors = 0;
        ann->train_slopes = 0;
        ann->prev_steps = 0;
        ann->prev_train_slopes = 0;
        ann->prev_weights_deltas = 0;
        ann->training_algorithm = FANN_TRAIN_INCREMENTAL;
        ann->num_MSE = 0;
        ann->MSE_value = 0;
        ann->num_bit_fail = 0;
        ann->bit_fail_limit = (fann_type)0.35;
        ann->network_type = FANN_NETTYPE_LAYER;
        ann->train_error_function = FANN_ERRORFUNC_TANH;
        ann->train_stop_function = FANN_STOPFUNC_MSE;
        ann->callback = 0;
        ann->user_data = 0; /* User is responsible for deallocation */
        ann->weights = 0;
        ann->connections = 0;
        ann->output = 0;
        ann->scale_mean_in = 0;
        ann->scale_deviation_in = 0;
        ann->scale_new_min_in = 0;
        ann->scale_factor_in = 0;
        ann->scale_mean_out = 0;
        ann->scale_deviation_out = 0;
        ann->scale_new_min_out = 0;
        ann->scale_factor_out = 0;

        /* variables used for cascade correlation (reasonable defaults) */
        ann->cascade_output_change_fraction = 0.01f;
        ann->cascade_candidate_change_fraction = 0.01f;
        ann->cascade_output_stagnation_epochs = 12;
        ann->cascade_candidate_stagnation_epochs = 12;
        ann->cascade_num_candidate_groups = 2;
        ann->cascade_weight_multiplier = (fann_type)0.4;
        ann->cascade_candidate_limit = (fann_type)1000.0;
        ann->cascade_max_out_epochs = 150;
        ann->cascade_max_cand_epochs = 150;
        ann->cascade_min_out_epochs = 50;
        ann->cascade_min_cand_epochs = 50;
        ann->cascade_candidate_scores = 0;
        ann->cascade_activation_functions_count = 10;
        ann->cascade_activation_functions =
            (enum fann_activationfunc_enum *)malloc(ann->cascade_activation_functions_count * sizeof(enum fann_activationfunc_enum));

        ann->cascade_activation_functions[0] = FANN_SIGMOID;
        ann->cascade_activation_functions[1] = FANN_SIGMOID_SYMMETRIC;
        ann->cascade_activation_functions[2] = FANN_GAUSSIAN;
        ann->cascade_activation_functions[3] = FANN_GAUSSIAN_SYMMETRIC;
        ann->cascade_activation_functions[4] = FANN_ELLIOT;
        ann->cascade_activation_functions[5] = FANN_ELLIOT_SYMMETRIC;
        ann->cascade_activation_functions[6] = FANN_SIN_SYMMETRIC;
        ann->cascade_activation_functions[7] = FANN_COS_SYMMETRIC;
        ann->cascade_activation_functions[8] = FANN_SIN;
        ann->cascade_activation_functions[9] = FANN_COS;

        ann->cascade_activation_steepnesses_count = 4;
        ann->cascade_activation_steepnesses =
            (fann_type *)malloc(ann->cascade_activation_steepnesses_count * sizeof(fann_type));

        ann->cascade_activation_steepnesses[0] = (fann_type)0.25;
        ann->cascade_activation_steepnesses[1] = (fann_type)0.5;
        ann->cascade_activation_steepnesses[2] = (fann_type)0.75;
        ann->cascade_activation_steepnesses[3] = (fann_type)1.0;

        /* Variables for use with with Quickprop training (reasonable defaults) */
        ann->quickprop_decay = -0.0001f;
        ann->quickprop_mu = 1.75;

        /* Variables for use with with RPROP training (reasonable defaults) */
        ann->rprop_increase_factor = 1.2f;
        ann->rprop_decrease_factor = 0.5;
        ann->rprop_delta_min = 0.0;
        ann->rprop_delta_max = 50.0;
        ann->rprop_delta_zero = 0.1f;

        /* Variables for use with SARPROP training (reasonable defaults) */
        ann->sarprop_weight_decay_shift = -6.644f;
        ann->sarprop_step_error_threshold_factor = 0.1f;
        ann->sarprop_step_error_shift = 1.385f;
        ann->sarprop_temperature = 0.015f;
        ann->sarprop_epoch = 0;

        ann->first_layer = (struct fann_layer *) malloc(num_layers * sizeof(struct fann_layer));
        memset(ann->first_layer, 0, num_layers * sizeof(struct fann_layer));

        ann->last_layer = ann->first_layer + num_layers;

        return ann;
    }

    static void fann_allocate_neurons(struct fann* ann) {
        struct fann_layer *layer_it;
        struct fann_neuron *neurons;
        unsigned int num_neurons_so_far = 0;
        unsigned int num_neurons = 0;

        /* all the neurons is allocated in one long array (calloc clears mem) */
        neurons = (struct fann_neuron *) malloc(ann->total_neurons * sizeof(struct fann_neuron));
        memset(neurons, 0, ann->total_neurons * sizeof(struct fann_neuron));
        ann->total_neurons_allocated = ann->total_neurons;

        for(layer_it = ann->first_layer; layer_it != ann->last_layer; layer_it++)
        {
            num_neurons = (unsigned int)(layer_it->last_neuron - layer_it->first_neuron);
            layer_it->first_neuron = neurons + num_neurons_so_far;
            layer_it->last_neuron = layer_it->first_neuron + num_neurons;
            num_neurons_so_far += num_neurons;
        }

        ann->output = (fann_type *) malloc(num_neurons * sizeof(fann_type));
        memset(ann->output, 0, num_neurons * sizeof(fann_type));
    }

    static void fann_allocate_connections(struct fann* ann) {
        ann->weights = (fann_type *) malloc(ann->total_connections * sizeof(fann_type));
        memset(ann->weights, 0, ann->total_connections * sizeof(fann_type));

        ann->total_connections_allocated = ann->total_connections;

        /* TODO make special cases for all places where the connections
        * is used, so that it is not needed for fully connected networks.
        */
        ann->connections =
            (struct fann_neuron **) malloc(ann->total_connections_allocated * sizeof(struct fann_neuron *));
        memset(ann->connections, 0, ann->total_connections_allocated * sizeof(struct fann_neuron *));
    }

public: // Create and Run and Online Train
    static struct fann* fann_create_from_config(float train_error_threshold = 0) {
        // The following const definitions must be customized when using an ANN to enable the desired model configuration.
        // Moreover, those configurations follows the FANN model output and can be obtained through the .net output file

        const unsigned int layers_sizes[] = { 9, 4, 4, 2 }; // each layer has a bias neuron

        const float neurons_config[][3] = {
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {0,0,0.00000000000000000000e+00},
            {9,6,5.00000000000000000000e-01},
            {9,6,5.00000000000000000000e-01},
            {9,6,5.00000000000000000000e-01},
            {0,6,0.00000000000000000000e+00},
            {4,6,5.00000000000000000000e-01},
            {4,6,5.00000000000000000000e-01},
            {4,6,5.00000000000000000000e-01},
            {0,6,0.00000000000000000000e+00},
            {4,0,5.00000000000000000000e-01},
            {0,0,0.00000000000000000000e+00}
        };

        const float weights_config[][3] = {
            {0,    -2.58305281400680541992e-01},
            {1,    -1.11155845224857330322e-01},
            {2,    1.64187774062156677246e-01},
            {3,    -3.31179410219192504883e-01},
            {4,    -4.47285175323486328125e-01},
            {5,    3.41278463602066040039e-01},
            {6,    4.07331657409667968750e+00},
            {7,    -2.46524259448051452637e-01},
            {8,    -1.57653665542602539062e+00},
            {0,    -1.09554767608642578125e-01},
            {1,    3.33400480449199676514e-02},
            {2,    -5.32809682190418243408e-02},
            {3,    8.05078208446502685547e-01},
            {4,    2.57287472486495971680e-01},
            {5,    -9.26923081278800964355e-02},
            {6,    -1.82686161994934082031e+00},
            {7,    8.04656863212585449219e-01},
            {8,    6.39719784259796142578e-01},
            {0,    -1.23890288174152374268e-01},
            {1,    -4.38192427158355712891e-01},
            {2,    -7.90347397327423095703e-01},
            {3,    4.21088933944702148438e-01},
            {4,    -7.68854200839996337891e-01},
            {5,    -9.53153491020202636719e-01},
            {6,    -1.28460872173309326172e+00},
            {7,    -1.37730360031127929688e+00},
            {8,    -4.99627619981765747070e-01},
            {9,    -1.61492601037025451660e-01},
            {10,    -5.23944757878780364990e-02},
            {11,    1.52116239070892333984e-01},
            {12,    2.70557720214128494263e-02},
            {9,    3.51071286201477050781e+00},
            {10,    -1.98799991607666015625e+00},
            {11,    6.62951543927192687988e-02},
            {12,    -1.45197105407714843750e+00},
            {9,    -1.79416465759277343750e+00},
            {10,    -2.07665041089057922363e-01},
            {11,    1.85924898833036422729e-02},
            {12,    -2.44105071760714054108e-03},
            {13,    -1.39736264944076538086e-01},
            {14,    2.59535789489746093750e-01},
            {15,    -1.14086031913757324219e+00},
            {16,    1.05494141578674316406e+00}
        };

        unsigned int num_layers, layer_size, input_neuron, i, num_connections;
        unsigned int tmpVal;

        //unsigned int scale_included;

        struct fann_neuron *first_neuron, *neuron_it, *last_neuron, **connected_neurons;
        fann_type *weights;
        struct fann_layer *layer_it;

        num_layers = NUM_LAYERS_CONFIG;
        struct fann* ann = fann_allocate_structure(num_layers);
        ann->learning_rate = LEARNING_RATE_CONFIG;
        ann->connection_rate = CONNECTION_RATE_CONFIG;
        ann->network_type = (enum fann_nettype_enum) NETWORK_TYPE_CONFIG;
        ann->learning_momentum = LEARNING_MOMENTUM_CONFIG;
        ann->training_algorithm = (enum fann_train_enum)TRAINING_ALGORITHM_CONFIG;
        ann->train_error_function = (enum fann_errorfunc_enum)TRAIN_ERROR_FUNCTION_CONFIG;
        ann->train_stop_function = (enum fann_stopfunc_enum)TRAIN_STOP_FUNCTION_CONFIG;

        ann->bit_fail_limit = (fann_type) BIT_FAIL_LIMIT_CONFIG;
        unsigned int iter = 0;

        for(layer_it = ann->first_layer; layer_it != ann->last_layer; layer_it++)
        {
            layer_size = layers_sizes[iter];
            iter++;
            /* we do not allocate room here, but we make sure that
            * last_neuron - first_neuron is the number of neurons */
            layer_it->first_neuron = 0;
            layer_it->last_neuron = layer_it->first_neuron + layer_size;
            ann->total_neurons += layer_size;
        }

        ann->num_input = (unsigned int)(ann->first_layer->last_neuron - ann->first_layer->first_neuron - 1);
        ann->num_output = (unsigned int)((ann->last_layer - 1)->last_neuron - (ann->last_layer - 1)->first_neuron);

        if(ann->network_type == FANN_NETTYPE_LAYER)
        {
            /* one too many (bias) in the output layer */
            ann->num_output--;
        }

        /* SCALE NOT INCLUDED IN OUR FANN */

        fann_allocate_neurons(ann);

        last_neuron = (ann->last_layer - 1)->last_neuron;
        iter = 0;

        for(neuron_it = ann->first_layer->first_neuron; neuron_it != last_neuron; neuron_it++)
        {
            num_connections = neurons_config[iter][0];
            tmpVal = neurons_config[iter][1];
            neuron_it->activation_steepness = neurons_config[iter][2];
            iter++;
            neuron_it->activation_function = (enum fann_activationfunc_enum)tmpVal;
            neuron_it->first_con = ann->total_connections;
            ann->total_connections += num_connections;
            neuron_it->last_con = ann->total_connections;
        }

        fann_allocate_connections(ann);

        connected_neurons = ann->connections;
        weights = ann->weights;
        first_neuron = ann->first_layer->first_neuron;
        iter = 0;

        for(i = 0; i < ann->total_connections; i++)
        {
            input_neuron = weights_config[iter][0];
            weights[i] = weights_config[iter][1];
            iter++;
            connected_neurons[i] = first_neuron + input_neuron;
        }

        ann->_train_error_threshold = train_error_threshold;

        return ann;
    }

    static fann_type* scale_input(fann_type* input, unsigned int input_size) {
        const fann_type fann_max[] = {50.9884, 101.1298, 127.5168, 14.1448, 2391.37, 405.6942, 100., 1200000000.};
        const fann_type fann_min[] = {0.0, 0.0, 0.2632, 0.0, 4.6226, 0.5374, 0, 600000000.};

        for (unsigned int i = 0; i < input_size; i++)
        {
            if(i == 6)
                continue;
            if(input[i] > fann_max[i])
                input[i] = fann_max[i];
            else if (input[i] < fann_min[i])
                input[i] = fann_min[i];
            input[i] = ((input[i] - fann_min[i])/(fann_max[i] - fann_min[i]));
        }

        return input;
    }

    static fann_type * fann_run(struct fann* ann, fann_type * input, bool scale) {
        struct fann_neuron *neuron_it, *last_neuron, *neurons, **neuron_pointers;
        if(scale)
            input = scale_input(input, ann->num_input);
        unsigned int i, num_connections, num_input, num_output;
        fann_type neuron_sum, *output;
        fann_type *weights;
        struct fann_layer *layer_it, *last_layer;
        unsigned int activation_function;
        fann_type steepness;

        /* store some variabels local for fast access */
        struct fann_neuron *first_neuron = ann->first_layer->first_neuron;

        fann_type max_sum = 0;

        /* first set the input */
        num_input = ann->num_input;
        for(i = 0; i != num_input; i++) {
            first_neuron[i].value = input[i];
        }

        /* Set the bias neuron in the input layer */
        (ann->first_layer->last_neuron - 1)->value = 1;

        last_layer = ann->last_layer;
        for(layer_it = ann->first_layer + 1; layer_it != last_layer; layer_it++) {
            last_neuron = layer_it->last_neuron;
            for(neuron_it = layer_it->first_neuron; neuron_it != last_neuron; neuron_it++) {
                if(neuron_it->first_con == neuron_it->last_con)
                {
                    /* bias neurons */
                    neuron_it->value = 1;
                    continue;
                }

                activation_function = neuron_it->activation_function;
                steepness = neuron_it->activation_steepness;

                neuron_sum = 0;
                num_connections = neuron_it->last_con - neuron_it->first_con;
                weights = ann->weights + neuron_it->first_con;

                if(ann->connection_rate >= 1) {
                    if(ann->network_type == FANN_NETTYPE_SHORTCUT) {
                        neurons = ann->first_layer->first_neuron;
                    } else {
                        neurons = (layer_it - 1)->first_neuron;
                    }

                    /* unrolled loop start */
                    i = num_connections & 3;	/* same as modulo 4 */
                    switch (i)
                    {
                        case 3:
                            neuron_sum += fann_mult(weights[2], neurons[2].value);
                        case 2:
                            neuron_sum += fann_mult(weights[1], neurons[1].value);
                        case 1:
                            neuron_sum += fann_mult(weights[0], neurons[0].value);
                        case 0:
                            break;
                    }

                    for(; i != num_connections; i += 4)
                    {
                        neuron_sum +=
                            fann_mult(weights[i], neurons[i].value) +
                            fann_mult(weights[i + 1], neurons[i + 1].value) +
                            fann_mult(weights[i + 2], neurons[i + 2].value) +
                            fann_mult(weights[i + 3], neurons[i + 3].value);
                    }
                    /* unrolled loop end */

                    /*
                    * for(i = 0;i != num_connections; i++){
                    * printf("%f += %f*%f, ", neuron_sum, weights[i], neurons[i].value);
                    * neuron_sum += fann_mult(weights[i], neurons[i].value);
                    * }
                    */
                } else {
                    neuron_pointers = ann->connections + neuron_it->first_con;

                    i = num_connections & 3;	/* same as modulo 4 */
                    switch (i)
                    {
                        case 3:
                            neuron_sum += fann_mult(weights[2], neuron_pointers[2]->value);
                        case 2:
                            neuron_sum += fann_mult(weights[1], neuron_pointers[1]->value);
                        case 1:
                            neuron_sum += fann_mult(weights[0], neuron_pointers[0]->value);
                        case 0:
                            break;
                    }

                    for(; i != num_connections; i += 4)
                    {
                        neuron_sum +=
                            fann_mult(weights[i], neuron_pointers[i]->value) +
                            fann_mult(weights[i + 1], neuron_pointers[i + 1]->value) +
                            fann_mult(weights[i + 2], neuron_pointers[i + 2]->value) +
                            fann_mult(weights[i + 3], neuron_pointers[i + 3]->value);
                    }
                }

                neuron_sum = fann_mult(steepness, neuron_sum);

                max_sum = 150/steepness;
                if(neuron_sum > max_sum)
                    neuron_sum = max_sum;
                else if(neuron_sum < -max_sum)
                    neuron_sum = -max_sum;

                neuron_it->sum = neuron_sum;

                fann_activation_switch(activation_function, neuron_sum, neuron_it->value);
            }
        }
        /* set the output */
        output = ann->output;
        num_output = ann->num_output;
        neurons = (ann->last_layer - 1)->first_neuron;
        for(i = 0; i != num_output; i++)
        {
            output[i] = neurons[i].value;
        }
        return ann->output;
    }

    /* Trains the network with the backpropagation algorithm.
     * Online learning is based on incremental learn with only one entry
     * EXECUTION FLOW:
     * 1. At each hyperperiod we execute fun_run() and execute the migraiton/action
     * 2. In the next hyperperiod we compute MSE with the measured reward
     * 3. We backpropagate the error, and update weights
     * 4. With the new configuration, go back to step 1.
     *
     * We do not reset MSE as the online learn is here considered a single train set.
     */
    static float fann_train_data_incremental(struct fann *ann, fann_type *data, fann_type *desired_output)
    {
        fann_type *output = fann_run(ann, data, false);

        // Optimization for error threshold
        if (fann_abs(output[0] - desired_output[0]) <= ann->_train_error_threshold)
            return fann_abs(output[0] - desired_output[0]);

        fann_compute_MSE(ann, desired_output);

        fann_backpropagate_MSE(ann);

        fann_update_weights(ann);
        return fann_abs(output[0] - desired_output[0]);
    }

    static void fann_reset_MSE(struct fann *ann)
    {
        ann->num_MSE = 0;
        ann->MSE_value = 0;
        ann->num_bit_fail = 0;
    }

    static float fann_get_MSE(struct fann *ann)
    {
        if(ann->num_MSE)
        {
            return ann->MSE_value / (float) ann->num_MSE;
        }
        else
        {
            return 0;
        }
    }

    /*
     * Internal train function 
     * WE ASSUME ALREADY SCALED INPUT
     */
    static float fann_train_data_rprop(struct fann *ann, fann_type **data, int data_size, fann_type **desired_output)
    {
        int i;
        if(ann->prev_train_slopes == 0)
        {
            fann_clear_train_arrays(ann);
        }

        fann_reset_MSE(ann);
        for(i = 0; i < data_size; i++)
        {
            fann_run(ann, data[i], false);
            fann_compute_MSE(ann, desired_output[i]);
            fann_backpropagate_MSE(ann);
            fann_update_slopes_batch(ann, ann->first_layer + 1, ann->last_layer - 1);
            fann_update_weights_irpropm(ann, 0, ann->total_connections); // TODO, check update outside of loop.
        }
        
        return fann_get_MSE(ann);
    }

private: // Internal Online Train Methods

    /* INTERNAL FUNCTION
        compute the error at the network output
        (usually, after forward propagation of a certain input vector, fann_run)
        the error is a sum of squares for all the output units
        also increments a counter because MSE is an average of such errors

        After this train_errors in the output layer will be set to:
        neuron_value_derived * (desired_output - neuron_value)
    */
    static void fann_compute_MSE(struct fann *ann, fann_type * desired_output)
    {
        fann_type neuron_value, neuron_diff, *error_it = 0, *error_begin = 0;
        struct fann_neuron *last_layer_begin = (ann->last_layer - 1)->first_neuron;
        const struct fann_neuron *last_layer_end = last_layer_begin + ann->num_output;
        const struct fann_neuron *first_neuron = ann->first_layer->first_neuron;

        /* if no room allocated for the error variabels, allocate it now */
        if(ann->train_errors == 0)
        {
            ann->train_errors = (fann_type *) malloc(ann->total_neurons * sizeof(fann_type));
            memset(ann->train_errors, 0, (ann->total_neurons) * sizeof(fann_type));
        }
        else
        {
            /* clear the error variabels */
            memset(ann->train_errors, 0, (ann->total_neurons) * sizeof(fann_type));
        }
        error_begin = ann->train_errors;

        /* calculate the error and place it in the output layer */
        error_it = error_begin + (last_layer_begin - first_neuron);

        for(; last_layer_begin != last_layer_end; last_layer_begin++)
        {
            neuron_value = last_layer_begin->value;
            neuron_diff = *desired_output - neuron_value;

            neuron_diff = fann_update_MSE(ann, last_layer_begin, neuron_diff);

            if(ann->train_error_function)
            {                       /* TODO make switch when more functions */
                if(neuron_diff < -.9999999)
                    neuron_diff = -17.0;
                else if(neuron_diff > .9999999)
                    neuron_diff = 17.0;
                else
                    neuron_diff = (fann_type) Math::logf((1.0 + neuron_diff) / (1.0 - neuron_diff));
            }

            *error_it = fann_activation_derived(last_layer_begin->activation_function,
                                                last_layer_begin->activation_steepness, neuron_value,
                                                last_layer_begin->sum) * neuron_diff;

            desired_output++;
            error_it++;

            ann->num_MSE++;
        }
    }

    /* INTERNAL FUNCTION
       Helper function to update the MSE value and return a diff which takes symmetric functions into account
    */
    static fann_type fann_update_MSE(struct fann *ann, struct fann_neuron* neuron, fann_type neuron_diff)
    {
        float neuron_diff2;
        
        switch (neuron->activation_function)
        {
            case FANN_LINEAR_PIECE_SYMMETRIC:
            case FANN_THRESHOLD_SYMMETRIC:
            case FANN_SIGMOID_SYMMETRIC:
            case FANN_SIGMOID_SYMMETRIC_STEPWISE:
            case FANN_ELLIOT_SYMMETRIC:
            case FANN_GAUSSIAN_SYMMETRIC:
            case FANN_SIN_SYMMETRIC:
            case FANN_COS_SYMMETRIC:
                neuron_diff /= (fann_type)2.0;
                break;
            case FANN_THRESHOLD:
            case FANN_LINEAR:
            case FANN_SIGMOID:
            case FANN_SIGMOID_STEPWISE:
            case FANN_GAUSSIAN:
            case FANN_GAUSSIAN_STEPWISE:
            case FANN_ELLIOT:
            case FANN_LINEAR_PIECE:
            case FANN_SIN:
            case FANN_COS:
                break;
        }

        neuron_diff2 = (float) (neuron_diff * neuron_diff);
    
        ann->MSE_value += neuron_diff2;

        /*printf("neuron_diff %f = (%f - %f)[/2], neuron_diff2=%f, sum=%f, MSE_value=%f, num_MSE=%d\n", neuron_diff, *desired_output, neuron_value, neuron_diff2, last_layer_begin->sum, ann->MSE_value, ann->num_MSE); */
        if(fann_abs(neuron_diff) >= ann->bit_fail_limit)
        {
            ann->num_bit_fail++;
        }
        
        return neuron_diff;
    }

    /* INTERNAL FUNCTION
      Calculates the derived of a value, given an activation function
       and a steepness
    */
    static fann_type fann_activation_derived(unsigned int activation_function,
                                      fann_type steepness, fann_type value, fann_type sum)
    {
        switch (activation_function)
        {
            case FANN_LINEAR:
            case FANN_LINEAR_PIECE:
            case FANN_LINEAR_PIECE_SYMMETRIC:
                return (fann_type) fann_linear_derive(steepness, value);
            case FANN_SIGMOID:
            case FANN_SIGMOID_STEPWISE:
                value = fann_clip(value, 0.01f, 0.99f);
                return (fann_type) fann_sigmoid_derive(steepness, value);
            case FANN_SIGMOID_SYMMETRIC:
            case FANN_SIGMOID_SYMMETRIC_STEPWISE:
                value = fann_clip(value, -0.98f, 0.98f);
                return (fann_type) fann_sigmoid_symmetric_derive(steepness, value);
            case FANN_GAUSSIAN:
                /* value = fann_clip(value, 0.01f, 0.99f); */
                return (fann_type) fann_gaussian_derive(steepness, value, sum);
            case FANN_GAUSSIAN_SYMMETRIC:
                /* value = fann_clip(value, -0.98f, 0.98f); */
                return (fann_type) fann_gaussian_symmetric_derive(steepness, value, sum);
            case FANN_ELLIOT:
                value = fann_clip(value, 0.01f, 0.99f);
                return (fann_type) fann_elliot_derive(steepness, value, sum);
            case FANN_ELLIOT_SYMMETRIC:
                value = fann_clip(value, -0.98f, 0.98f);
                return (fann_type) fann_elliot_symmetric_derive(steepness, value, sum);
            case FANN_SIN_SYMMETRIC:
                return 0; //(fann_type) fann_sin_symmetric_derive(steepness, sum);
            case FANN_COS_SYMMETRIC:
                return 0; //(fann_type) fann_cos_symmetric_derive(steepness, sum);
            case FANN_SIN:
                return 0; //(fann_type) fann_sin_derive(steepness, sum);
            case FANN_COS:
                return 0; //(fann_type) fann_cos_derive(steepness, sum);
            case FANN_THRESHOLD:
                return 0;
                //fann_error(NULL, FANN_E_CANT_TRAIN_ACTIVATION);
        }
        return 0;
    }

    /* INTERNAL FUNCTION
       Propagate the error backwards from the output layer.

       After this the train_errors in the hidden layers will be:
       neuron_value_derived * sum(outgoing_weights * connected_neuron)
    */
    static void fann_backpropagate_MSE(struct fann *ann)
    {
        fann_type tmp_error;
        unsigned int i;
        struct fann_layer *layer_it;
        struct fann_neuron *neuron_it, *last_neuron;
        struct fann_neuron **connections;

        fann_type *error_begin = ann->train_errors;
        fann_type *error_prev_layer;
        fann_type *weights;
        const struct fann_neuron *first_neuron = ann->first_layer->first_neuron;
        const struct fann_layer *second_layer = ann->first_layer + 1;
        struct fann_layer *last_layer = ann->last_layer;

        /* go through all the layers, from last to first.
         * And propagate the error backwards */
        for(layer_it = last_layer - 1; layer_it > second_layer; --layer_it)
        {
            last_neuron = layer_it->last_neuron;

            /* for each connection in this layer, propagate the error backwards */
            if(ann->connection_rate >= 1)
            {
                if(ann->network_type == FANN_NETTYPE_LAYER)
                {
                    error_prev_layer = error_begin + ((layer_it - 1)->first_neuron - first_neuron);
                }
                else
                {
                    error_prev_layer = error_begin;
                }

                for(neuron_it = layer_it->first_neuron; neuron_it != last_neuron; neuron_it++)
                {

                    tmp_error = error_begin[neuron_it - first_neuron];
                    weights = ann->weights + neuron_it->first_con;
                    for(i = neuron_it->last_con - neuron_it->first_con; i--;)
                    {
                        /*printf("i = %d\n", i);
                         * printf("error_prev_layer[%d] = %f\n", i, error_prev_layer[i]);
                         * printf("weights[%d] = %f\n", i, weights[i]); */
                        error_prev_layer[i] += tmp_error * weights[i];
                    }
                }
            }
            else
            {
                for(neuron_it = layer_it->first_neuron; neuron_it != last_neuron; neuron_it++)
                {

                    tmp_error = error_begin[neuron_it - first_neuron];
                    weights = ann->weights + neuron_it->first_con;
                    connections = ann->connections + neuron_it->first_con;
                    for(i = neuron_it->last_con - neuron_it->first_con; i--;)
                    {
                        error_begin[connections[i] - first_neuron] += tmp_error * weights[i];
                    }
                }
            }

            /* then calculate the actual errors in the previous layer */
            error_prev_layer = error_begin + ((layer_it - 1)->first_neuron - first_neuron);
            last_neuron = (layer_it - 1)->last_neuron;

            for(neuron_it = (layer_it - 1)->first_neuron; neuron_it != last_neuron; neuron_it++)
            {
                *error_prev_layer *= fann_activation_derived(neuron_it->activation_function, 
                    neuron_it->activation_steepness, neuron_it->value, neuron_it->sum);
                error_prev_layer++;
            }
            
        }
    }

    /* INTERNAL FUNCTION
       Update weights for incremental training
    */
    static void fann_update_weights(struct fann *ann)
    {
        struct fann_neuron *neuron_it, *last_neuron, *prev_neurons;
        fann_type tmp_error, delta_w, *weights;
        struct fann_layer *layer_it;
        unsigned int i;
        unsigned int num_connections;

        /* store some variabels local for fast access */
        const float learning_rate = ann->learning_rate;
        const float learning_momentum = ann->learning_momentum;        
        struct fann_neuron *first_neuron = ann->first_layer->first_neuron;
        struct fann_layer *first_layer = ann->first_layer;
        const struct fann_layer *last_layer = ann->last_layer;
        fann_type *error_begin = ann->train_errors;
        fann_type *deltas_begin, *weights_deltas;

        /* if no room allocated for the deltas, allocate it now */
        if(ann->prev_weights_deltas == 0)
        {
            ann->prev_weights_deltas =
                (fann_type *) malloc(ann->total_connections_allocated * sizeof(fann_type));
            memset(ann->prev_weights_deltas, 0, ann->total_connections_allocated * sizeof(fann_type));
            if(ann->prev_weights_deltas == 0)
            {
                return;
            }       
        }

        deltas_begin = ann->prev_weights_deltas;
        prev_neurons = first_neuron;
        for(layer_it = (first_layer + 1); layer_it != last_layer; layer_it++)
        {
            last_neuron = layer_it->last_neuron;
            if(ann->connection_rate >= 1)
            {
                if(ann->network_type == FANN_NETTYPE_LAYER)
                {
                    prev_neurons = (layer_it - 1)->first_neuron;
                }
                for(neuron_it = layer_it->first_neuron; neuron_it != last_neuron; neuron_it++)
                {
                    tmp_error = error_begin[neuron_it - first_neuron] * learning_rate;
                    num_connections = neuron_it->last_con - neuron_it->first_con;
                    weights = ann->weights + neuron_it->first_con;
                    weights_deltas = deltas_begin + neuron_it->first_con;
                    for(i = 0; i != num_connections; i++)
                    {
                        delta_w = tmp_error * prev_neurons[i].value + learning_momentum * weights_deltas[i];
                        weights[i] += delta_w ;
                        weights_deltas[i] = delta_w;
                    }
                }
            }
            else
            {
                for(neuron_it = layer_it->first_neuron; neuron_it != last_neuron; neuron_it++)
                {
                    tmp_error = error_begin[neuron_it - first_neuron] * learning_rate;
                    num_connections = neuron_it->last_con - neuron_it->first_con;
                    weights = ann->weights + neuron_it->first_con;
                    weights_deltas = deltas_begin + neuron_it->first_con;
                    for(i = 0; i != num_connections; i++)
                    {
                        delta_w = tmp_error * prev_neurons[i].value + learning_momentum * weights_deltas[i];
                        weights[i] += delta_w;
                        weights_deltas[i] = delta_w;
                    }
                }
            }
        }
    }

    /* INTERNAL FUNCTION
    Clears arrays used for training before a new training session.
    Also creates the arrays that do not exist yet.
     */
    static void fann_clear_train_arrays(struct fann *ann)
    {
        unsigned int i;
        fann_type delta_zero;

        /* if no room allocated for the slope variabels, allocate it now
         * (calloc clears mem) */
        if(ann->train_slopes == 0)
        {
            ann->train_slopes =
                (fann_type *) malloc(ann->total_connections_allocated*sizeof(fann_type));
            memset(ann->train_slopes, 0, ann->total_connections_allocated * sizeof(fann_type));
        }
        else
        {
            memset(ann->train_slopes, 0, (ann->total_connections_allocated) * sizeof(fann_type));
        }

        /* if no room allocated for the variabels, allocate it now */
        if(ann->prev_steps == 0)
        {
            ann->prev_steps = (fann_type *) malloc(ann->total_connections_allocated * sizeof(fann_type));
        }

        if(ann->training_algorithm == FANN_TRAIN_RPROP)
        {
            delta_zero = ann->rprop_delta_zero;
            
            for(i = 0; i < ann->total_connections_allocated; i++)
                ann->prev_steps[i] = delta_zero;
        }
        else
        {
            memset(ann->prev_steps, 0, (ann->total_connections_allocated) * sizeof(fann_type));
        }

        /* if no room allocated for the variabels, allocate it now */
        if(ann->prev_train_slopes == 0)
        {
            ann->prev_train_slopes = (fann_type *) malloc(ann->total_connections_allocated*sizeof(fann_type));
            memset(ann->prev_train_slopes, 0, ann->total_connections_allocated * sizeof(fann_type));
        }
        else
        {
            memset(ann->prev_train_slopes, 0, (ann->total_connections_allocated) * sizeof(fann_type));
        }
    }

    /* INTERNAL FUNCTION
       Update slopes for batch training
       layer_begin = ann->first_layer+1 and layer_end = ann->last_layer-1
       will update all slopes.

    */
    static void fann_update_slopes_batch(struct fann *ann, struct fann_layer *layer_begin,
                                  struct fann_layer *layer_end)
    {
        struct fann_neuron *neuron_it, *last_neuron, *prev_neurons, **connections;
        fann_type tmp_error;
        unsigned int i, num_connections;

        /* store some variabels local for fast access */
        struct fann_neuron *first_neuron = ann->first_layer->first_neuron;
        fann_type *error_begin = ann->train_errors;
        fann_type *slope_begin, *neuron_slope;

        /* if no room allocated for the slope variabels, allocate it now */
        if(ann->train_slopes == 0)
        {
            ann->train_slopes =
                (fann_type *) malloc(ann->total_connections_allocated*sizeof(fann_type));
            memset(ann->train_slopes, 0, ann->total_connections_allocated*sizeof(fann_type));
        }

        if(layer_begin == 0)
        {
            layer_begin = ann->first_layer + 1;
        }

        if(layer_end == 0)
        {
            layer_end = ann->last_layer - 1;
        }

        slope_begin = ann->train_slopes;

        prev_neurons = first_neuron;

        for(; layer_begin <= layer_end; layer_begin++)
        {
            last_neuron = layer_begin->last_neuron;
            if(ann->connection_rate >= 1)
            {
                if(ann->network_type == FANN_NETTYPE_LAYER)
                {
                    prev_neurons = (layer_begin - 1)->first_neuron;
                }

                for(neuron_it = layer_begin->first_neuron; neuron_it != last_neuron; neuron_it++)
                {
                    tmp_error = error_begin[neuron_it - first_neuron];
                    neuron_slope = slope_begin + neuron_it->first_con;
                    num_connections = neuron_it->last_con - neuron_it->first_con;
                    for(i = 0; i != num_connections; i++)
                    {
                        neuron_slope[i] += tmp_error * prev_neurons[i].value;
                    }
                }
            }
            else
            {
                for(neuron_it = layer_begin->first_neuron; neuron_it != last_neuron; neuron_it++)
                {
                    tmp_error = error_begin[neuron_it - first_neuron];
                    neuron_slope = slope_begin + neuron_it->first_con;
                    num_connections = neuron_it->last_con - neuron_it->first_con;
                    connections = ann->connections + neuron_it->first_con;
                    for(i = 0; i != num_connections; i++)
                    {
                        neuron_slope[i] += tmp_error * connections[i]->value;
                    }
                }
            }
        }
    }

    /* INTERNAL FUNCTION
       The iRprop- algorithm
    */
    static void fann_update_weights_irpropm(struct fann *ann, unsigned int first_weight, unsigned int past_end)
    {
        fann_type *train_slopes = ann->train_slopes;
        fann_type *weights = ann->weights;
        fann_type *prev_steps = ann->prev_steps;
        fann_type *prev_train_slopes = ann->prev_train_slopes;

        fann_type prev_step, slope, prev_slope, next_step, same_sign;

        float increase_factor = ann->rprop_increase_factor; /*1.2; */
        float decrease_factor = ann->rprop_decrease_factor; /*0.5; */
        float delta_min = ann->rprop_delta_min; /*0.0; */
        float delta_max = ann->rprop_delta_max; /*50.0; */

        unsigned int i = first_weight;

        for(; i != past_end; i++)
        {
            prev_step = Math::max(prev_steps[i], (fann_type) 0.0001);    /* prev_step may not be zero because then the training will stop */
            slope = train_slopes[i];
            prev_slope = prev_train_slopes[i];

            same_sign = prev_slope * slope;

            if(same_sign >= 0.0)
                next_step = Math::min(prev_step * increase_factor, delta_max);
            else
            {
                next_step = Math::max(prev_step * decrease_factor, delta_min);
                slope = 0;
            }

            if(slope < 0)
            {
                weights[i] -= next_step;
                if(weights[i] < -1500)
                    weights[i] = -1500;
            }
            else
            {
                weights[i] += next_step;
                if(weights[i] > 1500)
                    weights[i] = 1500;
            }

            /*if(i == 2){
             * printf("weight=%f, slope=%f, next_step=%f, prev_step=%f\n", weights[i], slope, next_step, prev_step);
             * } */

            /* update global data arrays */
            prev_steps[i] = next_step;
            prev_train_slopes[i] = slope;
            train_slopes[i] = 0.0;
        }
    }

};

__END_UTIL

#endif
