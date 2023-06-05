// EPOS CPU Energy-aware PEDF Scheduler Component Implementation

#include <process.h>
#include <clerk.h>

#ifndef __mode_builtin__
__BEGIN_SYS

// EA PEDF Statistics
TSC::Time_Stamp EA_PEDF::Statistics::_cpu_time[Traits<Build>::CPUS]; // accumulated cpu time at each hyperperiod
TSC::Time_Stamp EA_PEDF::Statistics::_last_hyperperiod[Traits<Build>::CPUS];     // Time Stamp of last hyperperiod
TSC::Time_Stamp EA_PEDF::Statistics::_hyperperiod;                               // Global Hyperperiod
TSC::Time_Stamp EA_PEDF::Statistics::_hyperperiod_cpu_time[Traits<Build>::CPUS]; // idle time in last hyperperiod
EA_PEDF::Activity EA_PEDF::Statistics::_activity_cpu[Traits<Build>::CPUS];
EA_PEDF::Activity EA_PEDF::Statistics::_last_activity_cpu[Traits<Build>::CPUS];

// EA PEDF control features
bool EA_PEDF::_cooldown[Traits<Build>::CPUS];                              // not actuate on cooldown
bool EA_PEDF::_decrease_frequency_vote[Traits<Build>::CPUS];                    // CPU votes
bool EA_PEDF::_vote_ready[Traits<Build>::CPUS];                      // vote ready
bool EA_PEDF::_to_learn[Traits<Build>::CPUS];                              // online learning
Hertz EA_PEDF::_min_clock;
Hertz EA_PEDF::_max_clock;

// EA PEDF Migration control features
bool EA_PEDF::_imbalanced;
//bool EA_PEDF::_swapped; // CPU id of last swap from == to, last swap to == from (this is only used to check if a swap was made)
// in this new configuration a reference to the criterions swapped or migrated is required -- TODO
Hertz EA_PEDF::_last_freq;
unsigned int EA_PEDF::_last_migration_from; // CPU id of last from
unsigned int EA_PEDF::_last_migration_to; // CPU id of last swap from
EA_PEDF* EA_PEDF::_last_migration;
EA_PEDF* EA_PEDF::_last_swap;


float EA_PEDF::_migration_optimization_threshold;
unsigned long long EA_PEDF::_imbalance_threshold;
float EA_PEDF::_activity_weights[PMU_EVENTS]
            = {0.463849127,0.46623829,0.521283507,0.482989371,0.496628821,0.486082405}; //{0.5,0.5,0.5,0.5,0.5,0.5};
;    // Migration feature weights

// EA PEDF ANN control features
struct FANN::fann * EA_PEDF::_ann[Traits<Build>::CPUS];           // ANN used by this Criterion
unsigned int EA_PEDF::_train_count[Traits<Build>::CPUS];
FANN::fann_type *EA_PEDF::_train_input[Traits<Build>::CPUS][SAMPLES_PER_TRAIN_LIMIT];
FANN::fann_type EA_PEDF::_desired_output[Traits<Build>::CPUS][SAMPLES_PER_TRAIN_LIMIT];
FANN::fann_type EA_PEDF::_utilization_prediction[Traits<Build>::CPUS];

// EA PEDF Collect control
unsigned int EA_PEDF::_collected[Traits<Build>::CPUS];

// EA PEDF PMU Events
Clerk<PMU>* EA_PEDF::_bus_access_st_ca53_v8[Traits<Build>::CPUS];
Clerk<PMU>* EA_PEDF::_data_write_stall_st_buffer_full_ca53_v8[Traits<Build>::CPUS];
Clerk<PMU>* EA_PEDF::_immediate_branches_ca[Traits<Build>::CPUS];
Clerk<PMU>* EA_PEDF::_l2d_writeback[Traits<Build>::CPUS];
Clerk<PMU>* EA_PEDF::_cpu_cycles[Traits<Build>::CPUS];
Clerk<PMU>* EA_PEDF::_l1_cache_hits[Traits<Build>::CPUS];


EA_PEDF *EA_PEDF::_first_criterion[Traits<Build>::CPUS];
EA_PEDF *EA_PEDF::_last_criterion[Traits<Build>::CPUS];

void EA_PEDF::init()
{
    _min_clock = CPU::min_clock();
    _max_clock = CPU::max_clock();
    _last_freq = _max_clock;
    _imbalanced = false;
    _last_freq = 0;
    _last_migration_from = 0;
    _last_migration_to = 0;
    _migration_optimization_threshold = 0.05;
    _imbalance_threshold = 0;

    unsigned int cpu = CPU::id();

    _ann[cpu] = FANN::fann_create_from_config(ANN_DEVIATION_THRESHOLD);
    Statistics::_hyperperiod_cpu_time[cpu] = 0;
    db<Thread>(TRC) << "ANN on = " << _ann[cpu] << endl;

    _bus_access_st_ca53_v8[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::BUS_ACCESS_ST_CA53_v8, 0, false);
    _data_write_stall_st_buffer_full_ca53_v8[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::DATA_WRITE_STALL_ST_BUFFER_FULL_CA53_v8, 0, false);
    _immediate_branches_ca[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::IMMEDIATE_BRANCHES_CA, 0, false);
    _l2d_writeback[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::L2D_WRITEBACK, 0, false);
    _cpu_cycles[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::CPU_CYCLES, 0, false);
    _l1_cache_hits[cpu] = new (SYSTEM) Clerk<PMU>(Traits_Tokens::PMU_Event::L1_CACHE_HITS, 0, false);
}

bool EA_PEDF::collect(bool end)
{
    if(!end) {
        unsigned int cpu = CPU::id();
        TSC::Time_Stamp ts = TSC::time_stamp();

        // idle never calls this method
        if(_collected[cpu] == 0) { // previous
            // Timing
            _statistics.thread_execution_time      += ts - _statistics.last_thread_dispatch;
            Statistics::_cpu_time[cpu]             += ts - _statistics.last_thread_dispatch;
            _statistics.last_thread_dispatch        = 0;

            // PMU Account per task: Fix overflow and store increase in counter during its execution
            PMU::Event aux = _bus_access_st_ca53_v8[cpu]->read();
            _statistics.pmu_accumulated[0]         += (aux < _statistics.pmu_accumulated[0]) ? aux + (ANY - _statistics.pmu_accumulated[0]) : aux - _statistics.last_pmu_read[0];

            aux = _data_write_stall_st_buffer_full_ca53_v8[cpu]->read();
            _statistics.pmu_accumulated[1]         += (aux < _statistics.pmu_accumulated[1]) ? aux + (ANY - _statistics.pmu_accumulated[1]) : aux - _statistics.last_pmu_read[1];

            aux = _immediate_branches_ca[cpu]->read();
            _statistics.pmu_accumulated[2]         += (aux < _statistics.pmu_accumulated[2]) ? aux + (ANY - _statistics.pmu_accumulated[2]) : aux - _statistics.last_pmu_read[2];

            aux = _l2d_writeback[cpu]->read();
            _statistics.pmu_accumulated[3]         += (aux < _statistics.pmu_accumulated[3]) ? aux + (ANY - _statistics.pmu_accumulated[3]) : aux - _statistics.last_pmu_read[3];

            aux = _cpu_cycles[cpu]->read();
            _statistics.pmu_accumulated[4]         += (aux < _statistics.pmu_accumulated[4]) ? aux + (ANY - _statistics.pmu_accumulated[4]) : aux - _statistics.last_pmu_read[4];

            aux = _l1_cache_hits[cpu]->read();
            _statistics.pmu_accumulated[5]         += (aux < _statistics.pmu_accumulated[5]) ? aux + (ANY - _statistics.pmu_accumulated[5]) : aux - _statistics.last_pmu_read[5];
        } else if(_collected[cpu] == 1) { // next
            // Timing
            _statistics.last_thread_dispatch = ts;

            // PMU Account per task: next stores current value to accumulate the increase in each counter during its execution
            _statistics.last_pmu_read[0] = _bus_access_st_ca53_v8[cpu]->read();
            _statistics.last_pmu_read[1] = _data_write_stall_st_buffer_full_ca53_v8[cpu]->read();
            _statistics.last_pmu_read[2] = _immediate_branches_ca[cpu]->read();
            _statistics.last_pmu_read[3] = _l2d_writeback[cpu]->read();
            _statistics.last_pmu_read[4] = _cpu_cycles[cpu]->read();
            _statistics.last_pmu_read[5] = _l1_cache_hits[cpu]->read();
        }

        _collected[cpu]++;

    } else {
        _collected[CPU::id()] = 0;
    }

    return true;
}

bool EA_PEDF::charge(bool end)
{
    // Run only at hyperperiod and idle shall never calls this method
    if(TSC::time_stamp() < Statistics::_hyperperiod + Statistics::_last_hyperperiod[CPU::id()] || Statistics::_hyperperiod == 0)
        return false; // returning false stops for_all iterations

    // Set last hyperperiod cpu time
    if(!end) {
        unsigned int cpu = CPU::id();
        if(!cpu)
            return false;

        if(Statistics::_cpu_time[cpu] != 0) {
            Statistics::_hyperperiod_cpu_time[cpu] = Statistics::_cpu_time[cpu];
            Statistics::_cpu_time[cpu] = 0;
        }

        // On cooldown reset features, on learn reset accumulators
        if(_cooldown[cpu] || _to_learn[cpu]){
            for(unsigned int i = 0; i < PMU_EVENTS; i++) {
                _statistics.pmu_accumulated[i] = 0;
                _statistics.last_activity.vector[i] = 0;
            }

            if(_cooldown){
                _statistics.thread_execution_time = 0;
                return false;
            } else { // on learn set training
                CPU::fpu_save();
                if(_train_count[cpu] < SAMPLES_PER_TRAIN_LIMIT) {
                    _train_input[cpu][_train_count[cpu]] = _statistics.input;
                    _desired_output[cpu][_train_count[cpu]++] = (_statistics.thread_execution_time*1.0)/Statistics::_hyperperiod;
                }
                _statistics.thread_execution_time = 0;
                CPU::fpu_restore();
                return false;
            }
        }

        CPU::fpu_save();
        // normal charge flow: collect input and predict
        _statistics.input[6] = (_statistics.thread_execution_time*1.0)/Statistics::_hyperperiod;
        _statistics.input[7] = CPU::clock();
        _statistics.activity.utilization = _statistics.input[6];

        for(unsigned int i = 0; i < PMU_EVENTS; i++) {
            _statistics.input[i] = _statistics.pmu_accumulated[i];
            _statistics.pmu_accumulated[i] = 0;
        }

        _statistics.output = *FANN::fann_run(_ann[cpu], _statistics.input, true);
        _utilization_prediction[cpu] += _statistics.output;
        CPU::fpu_restore();
        return true;
    } else {
        unsigned int cpu = CPU::id();

        if(!cpu)
            return false;

        if(_cooldown[cpu]) {
            _cooldown[cpu] = false;
            return false;
        }

        CPU::fpu_save();

        if(_to_learn[cpu]) {
            if(_train_count[cpu] > 0) {
                FANN::fann_reset_MSE(_ann[cpu]);
                float error = 0;
                int max_train = MAX_TRAINS;
                bool end = false;
                //int count = 0;

                // incremental train loop
                while(!end && max_train > 0) {
                    end = true;
                    for(unsigned int i = 0; i < _train_count[cpu]; ++i)
                    {
                        error = FANN::fann_train_data_incremental(_ann[cpu], _train_input[cpu][i], &_desired_output[cpu][i]);
                        if(error > ANN_DEVIATION_THRESHOLD) {
                            error = FANN::fann_train_data_incremental(_ann[cpu], _train_input[cpu][i], &_desired_output[cpu][i]);
                        }
                        end &= error <= ANN_DEVIATION_THRESHOLD;
                        max_train--;
                    }
                    error = FANN::fann_train_data_incremental(_ann[cpu], _train_input[cpu][0], &_desired_output[cpu][0]);
                    if(error > ANN_DEVIATION_THRESHOLD)
                        end = false;
                }
            }
            _to_learn[cpu] = false;
            _train_count[cpu] = 0;
            return false;
        }

        _decrease_frequency_vote[cpu] = _utilization_prediction[cpu] < (1 - SAFETY_MARGIN);
        _vote_ready[cpu] = true;
        CPU::fpu_restore();
        return true;
    }
}

bool EA_PEDF::award(bool end)
{
    // This will make the last CPU to run charge to be the cpu to actuate on award
    // Or, if configured to CPU 0, CPU 0 will do it

    if(TSC::time_stamp() < Statistics::_hyperperiod + Statistics::_last_hyperperiod[CPU::id()] || Statistics::_hyperperiod == 0)
        return false; // returning false stops for_all iterations

    unsigned int cpu = CPU::id();

    if(!end) {
        if(Traits<Build>::CPUS > 1) {
            if(cpu) {
                // make criterions of each CPU to be linked in a doubly linked list fashion
                if(_first_criterion[cpu] == 0) {
                    _first_criterion[cpu] = this;
                    _last_criterion[cpu] = this;
                } else {
                    _last_criterion[cpu]->_next = this;
                    _last_criterion[cpu] = this;
                }
            }
        }
        return true;

    } else {
        if(cpu) {// This will make only CPU 0 to run award instead of the last CPU to run charge
            // set last link to (tail) to null
            _last_criterion[cpu]->_next = 0;
            _last_criterion[cpu] = 0;
            return false;
        }

        bool votes_ready = true;

        for(unsigned int c = 1; c < Traits<Build>::CPUS; c++)
            votes_ready &= _vote_ready[c];

        if(!votes_ready) {
            return false;
        }
        // float usage by FANN and Activity vector
        CPU::fpu_save();

        bool consensus = true;
        bool safety_margin_not_broke = true;
        unsigned int max_utilization = 0;
        unsigned int min_utilization = Statistics::_hyperperiod;

        TSC::Time_Stamp reservation = SAFETY_MARGIN * Statistics::_hyperperiod;

        for(unsigned int c = 1; c < Traits<Build>::CPUS; c++) {
            consensus &= _decrease_frequency_vote[c];
            safety_margin_not_broke &= Statistics::_hyperperiod_cpu_time[c] - Statistics::_hyperperiod > reservation;
            if(Statistics::_hyperperiod_cpu_time[c] < min_utilization)
                min_utilization = Statistics::_hyperperiod_cpu_time[c];

            if(Statistics::_hyperperiod_cpu_time[c] > max_utilization)
                max_utilization = Statistics::_hyperperiod_cpu_time[c];

            _vote_ready[c] = false;
        }

        if(!safety_margin_not_broke) { // withdraw one frequency level as one CPU has idle time < SAFETY MARGIN
            Hertz freq = CPU::clock();
            if(freq < _max_clock) {
                for(unsigned int i = 1; i < Traits<Build>::CPUS; ++i) {
                    _cooldown[i] = true;
                    _vote_ready[i] = false;
                }
                CPU::clock(freq + FREQUENCY_LEVEL);
                _imbalanced = false;
                _imbalance_threshold = 0;
                CPU::fpu_restore();
                return true;
            }
            CPU::fpu_restore();
            return true;
        }

        if(consensus) { // all CPUS voted for frequency decrease
            Hertz freq = CPU::clock();
            if(freq > _min_clock) {
                for(unsigned int i = 1; i < Traits<Build>::CPUS; ++i) {
                    _to_learn[i] = true;
                    _cooldown[i] = true;
                }
                CPU::clock(freq - FREQUENCY_LEVEL);
            }
            CPU::fpu_restore();
            return true;
        } else if( Traits<Build>::CPUS > 1 && (!_imbalanced || max_utilization - min_utilization > _imbalance_threshold)) { // check for migrations and run the best one
            // if already at lowest frequency
            Hertz freq = CPU::clock();
            if(freq == _min_clock) { // reached lowest frequency without breaking safety margin, update imbalance threshold
                _imbalanced = true;
                _last_migration_from = 0;
                _last_migration_to = 0;
                _last_migration = 0;
                _last_swap = 0;
                _last_freq = freq;
                //_swap_migration = false;

                _imbalance_threshold = max_utilization - min_utilization;
                db<Thread>(TRC) << "end - freq=" << freq << endl;
                CPU::fpu_restore();
                return false; // did not actuated
            }

            // calculates activity
            for(unsigned int c = 1; c < Traits<Build>::CPUS; c++) {
                Statistics::_activity_cpu[c].reset();
                EA_PEDF *ea = _first_criterion[c];
                while (ea != 0) {
                    for(unsigned int i = 0; i < PMU_EVENTS; i++) {
                        ea->_statistics.activity.vector[i] = ea->_statistics.input[i] * _activity_weights[i] * ea->_statistics.activity.utilization;
                    }
                    Statistics::_activity_cpu[cpu] += ea->_statistics.activity;
                    ea = ea->_next;
                }
            }

            if(TRAIN_WEIGHTS && _last_migration_from > 0) { // train because there was a migration
                float error;
                float update[PMU_EVENTS];
                for(unsigned int c = 1; c < Traits<Build>::CPUS; c++) {
                    error = Statistics::_activity_cpu[c].sum() - Statistics::_last_activity_cpu[c].sum();
                    for(unsigned int p = 0; p < PMU_EVENTS; p++)
                        update[p] = Statistics::_last_activity_cpu[c].vector[p]*error;
                }
                for(unsigned int p = 0; p < PMU_EVENTS; p++) {
                    update[p] /= Traits<Build>::CPUS > 1 ? (Traits<Build>::CPUS-1) : 1;
                    _activity_weights[p] -= (LEARNING_RATE * -update[p]);
                    db<Thread>(TRC) << "weights[" << p << "]=" << _activity_weights[p];
                }

                for(unsigned int c = 1; c < Traits<Build>::CPUS; c++) {
                    Statistics::_activity_cpu[c].reset();
                    EA_PEDF *ea = _first_criterion[c];
                    while (ea != 0) {
                        for(unsigned int i = 0; i < PMU_EVENTS; i++) {
                            ea->_statistics.activity.vector[i] = ea->_statistics.input[i] * _activity_weights[i] * ea->_statistics.activity.utilization;
                        }
                        Statistics::_activity_cpu[c] += ea->_statistics.activity;
                        ea = ea->_next;
                    }
                }
            }

            // calculate current variance
            float digest_avg = 0;
            float digest_cpu[Traits<Build>::CPUS];
            float activity_variance = 0;
            float digest_avg_temp = 0;
            // variance of activity vector per CPU
            for(unsigned int c = 1; c < Traits<Build>::CPUS; ++c)
            {
                digest_cpu[c] = Statistics::_activity_cpu[c].sum();
                digest_avg += digest_cpu[c];
            }
            digest_avg /= Traits<Build>::CPUS > 1 ? (Traits<Build>::CPUS-1) : 1;
            // variance calculation (metric)
            for(unsigned int c = 1; c < Traits<Build>::CPUS; ++c)
                activity_variance += (digest_avg - digest_cpu[c]) * (digest_avg - digest_cpu[c]);

            // Evaluate migrations
            unsigned int migration_from = 0;
            unsigned int migration_to = 0;
            unsigned int best_migration_score = 0;
            EA_PEDF* best_migration = 0;
            EA_PEDF* best_swap = 0;

            for(unsigned int current_cpu = 1; current_cpu < Traits<Build>::CPUS; current_cpu++) {
                EA_PEDF *ea = _first_criterion[current_cpu];
                while (ea != 0) {
                    Statistics::_activity_cpu[current_cpu] -= ea->_statistics.activity; // remove this thread from the current cpu
                    float current_cpu_digest = Statistics::_activity_cpu[current_cpu].sum(); // get new digest

                    for(unsigned int target_cpu = 1; target_cpu < Traits<Build>::CPUS; target_cpu++)
                    {
                        if(target_cpu == current_cpu) // skip current cpu
                            continue;

                        db<Thread>(TRC) << "In=" << current_cpu << "," << ea << "," << target_cpu << endl;
                        // check if fits in target_cpu
                        if(ea->_statistics.migration_locked[target_cpu]
                                                            || !Statistics::_activity_cpu[target_cpu].fits(ea->_statistics.activity))
                            continue;

                        Statistics::_activity_cpu[target_cpu] += ea->_statistics.activity;
                        digest_avg_temp = (digest_avg*(Traits<Build>::CPUS > 1 ? (Traits<Build>::CPUS-1) : 1) + current_cpu_digest) - digest_cpu[current_cpu];
                        digest_avg_temp += Statistics::_activity_cpu[target_cpu].sum() - digest_cpu[target_cpu];
                        digest_avg_temp /= Traits<Build>::CPUS > 1 ? (Traits<Build>::CPUS-1) : 1;

                        float score = 0;
                        // variance^2 calculation (metric)
                        for(unsigned int c = 1; c < Traits<Build>::CPUS; ++c)
                            score += (digest_avg_temp - digest_cpu[c]) * (digest_avg_temp - digest_cpu[c]);

                        if(score < best_migration_score && score < activity_variance * (1 - _migration_optimization_threshold)) {
                            best_migration_score = score;
                            migration_from = current_cpu;
                            migration_to = target_cpu;
                            best_migration = ea;
                            //_swap_migration = false;
                        }
                    }
                    Statistics::_activity_cpu[current_cpu] += ea->_statistics.activity;
                    ea = ea->_next;
                }
            }

            // Revocation -- TODO
            if((_last_freq < freq && _last_migration_from > 0 // worse frequency configuration and there was a migration
                && (migration_from == 0 || _last_swap == 0 // no swap or no simple migration
                    || ((migration_to != _last_migration_from && migration_from != _last_migration_to) && _last_swap != 0))) // if last migration was a swap and currently a simple migration between the two cpus involved in the last swap is the best, then no revocation is done
                || (_last_freq == freq && (migration_to == _last_migration_from && migration_from == _last_migration_to && _last_swap == 0 && migration_from != 0))) /// algorithm is automatically undoing last migartion, then we block it as a revocation
            {
                // worsen frequency with no swap, undo
                // worsen frequency with a swap, if swap members are not involved, undo
                // same frequnecy already undoing last migration
                // worsen frequency through a simple migration (enters if worsen freq, not first round and no further migration or next migration is a simple undo of last migration (swap is ok to undo one part))
                _last_migration->_statistics.destination_cpu = _last_migration_from;
                _last_migration->_statistics.migration_locked[_last_migration_to] = true;
                Statistics::_activity_cpu[_last_migration_from] += _last_migration->_statistics.activity;
                Statistics::_activity_cpu[_last_migration_to]   -= _last_migration->_statistics.activity;

                if(_last_swap != 0) { // last migration was a swap
                    // in swap: last_from == swto and last_to == swfrom
                    _last_swap->_statistics.destination_cpu = _last_migration_to;
                    _last_swap->_statistics.migration_locked[_last_migration_from] = true;
                    Statistics::_activity_cpu[_last_migration_to] += _last_swap->_statistics.activity;
                    Statistics::_activity_cpu[_last_migration_from]   -= _last_swap->_statistics.activity;
                }
                _migration_optimization_threshold += 0.15;
                _cooldown[_last_migration_from] = true;
                _cooldown[_last_migration_to] = true;
                _last_swap = 0;
                _last_migration_from = 0;
                _last_migration_to = 0;
                _last_migration = 0;

                db<Thread>(TRC) << "undo migration" << endl;

                CPU::clock(_max_clock);
                CPU::fpu_restore();
                return true;
            }

            if(migration_from == 0) { // no simple migrations found, then evaluate swaps
                for(unsigned int current_cpu = 1; current_cpu < Traits<Build>::CPUS; current_cpu++) {
                    EA_PEDF *ea = _first_criterion[current_cpu];
                    while (ea != 0) {
                        Statistics::_activity_cpu[current_cpu] -= ea->_statistics.activity; // remove this thread from the current cpu

                        for(unsigned int target_cpu = current_cpu+1; target_cpu < Traits<Build>::CPUS; target_cpu++) {
                            EA_PEDF *ea_swap = _first_criterion[target_cpu];
                            while (ea_swap != 0) {
                                Statistics::_activity_cpu[target_cpu] -= ea_swap->_statistics.activity;

                                // check if fits and not blocked
                                if( ea->_statistics.migration_locked[target_cpu]
                                                                     || !Statistics::_activity_cpu[target_cpu].fits(ea->_statistics.activity)
                                                                     || ea_swap->_statistics.migration_locked[current_cpu]
                                                                                                              || !Statistics::_activity_cpu[current_cpu].fits(ea_swap->_statistics.activity)) {
                                    Statistics::_activity_cpu[target_cpu] += ea_swap->_statistics.activity;
                                    ea_swap = ea_swap->_next;
                                    continue;
                                }

                                Statistics::_activity_cpu[target_cpu]  += ea->_statistics.activity;
                                Statistics::_activity_cpu[current_cpu] += ea_swap->_statistics.activity;

                                digest_avg_temp = (digest_avg*(Traits<Build>::CPUS-1) + Statistics::_activity_cpu[current_cpu].sum()) - digest_cpu[current_cpu];
                                digest_avg_temp += Statistics::_activity_cpu[target_cpu].sum() - digest_cpu[target_cpu];
                                digest_avg_temp /= Traits<Build>::CPUS > 1 ? (Traits<Build>::CPUS-1) : 1;

                                float score = 0;
                                // variance calculation (metric)
                                for(unsigned int c = 1; c < Traits<Build>::CPUS; ++c)
                                    score += (digest_avg_temp - digest_cpu[c]) * (digest_avg_temp - digest_cpu[c]);

                                if(score < best_migration_score && score < activity_variance * (1 - _migration_optimization_threshold)) {
                                    best_migration_score = score;
                                    migration_from = cpu;
                                    migration_to = target_cpu;
                                    best_migration = ea;
                                    best_swap = ea_swap;
                                    //_swap_migration = true;
                                }

                                Statistics::_activity_cpu[target_cpu]  -= ea->_statistics.activity;
                                Statistics::_activity_cpu[target_cpu]  += ea_swap->_statistics.activity;
                                Statistics::_activity_cpu[current_cpu] -= ea_swap->_statistics.activity;
                                ea_swap = ea_swap->_next;
                            }
                        }
                        Statistics::_activity_cpu[current_cpu] += ea->_statistics.activity;
                        ea = ea->_next;
                    }
                }
            }

            // Do Migration
            if(migration_from == 0) { // no Migration
                _imbalanced = true;
                _last_migration_from = 0;
                _last_migration_to = 0;
                _last_freq = freq;
                _last_migration = 0;
                _last_swap = 0;

                _imbalance_threshold = max_utilization - min_utilization;
                CPU::fpu_restore();
                return true;

            } else { // Migration

                Statistics::_activity_cpu[migration_from] -= _last_migration->_statistics.activity;
                Statistics::_activity_cpu[migration_to]   += _last_migration->_statistics.activity;
                // change criterion queue of _last_migration
                best_migration->_statistics.destination_cpu = migration_to;
                _last_migration = best_migration;
                _last_migration_from = migration_from;
                _last_migration_to = migration_to;

                if(_last_swap != 0) {
                    Statistics::_activity_cpu[migration_to]   -= _last_swap->_statistics.activity;
                    Statistics::_activity_cpu[migration_from] += _last_swap->_statistics.activity;
                    // change criterion queue of _best_swap
                    best_swap->_statistics.destination_cpu = migration_from;
                    _last_swap = best_swap;
                }

                for(unsigned int c = 1; c < Traits<Build>::CPUS; ++c) {
                    _first_criterion[c] = 0;
                    Statistics::_last_activity_cpu[c] = Statistics::_activity_cpu[c];
                }

                _cooldown[_last_migration_from] = true;
                _cooldown[_last_migration_to] = true;
                _last_freq = freq;
                CPU::clock(_max_clock);
                CPU::fpu_restore();
                return true;
            }
        }
    }
    CPU::fpu_restore();
    return false;
}

__END_SYS

#endif
