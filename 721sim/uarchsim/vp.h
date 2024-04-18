#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

class stridevaluepred {
public:
    uint64_t tag = 0;
    uint64_t conf = 0;
    uint64_t retired_value = 0;
    uint64_t stride = 0;
    uint64_t instance = 0;
};

class valuepredqueue {
public:
    uint64_t PC = 0;
    uint64_t val = 0;
};
class vp {
private:
    bool enable;
    bool perf;

    uint64_t size;
    bool oracle;
    uint64_t index;
    uint64_t tag;
    uint64_t confmax;
    uint64_t confinc;
    uint64_t confdec;
    uint64_t replace_stride;
    uint64_t replace;
    bool predINTALU;
    bool predFPALU;
    bool predLOAD;
    bool full_policy;

    unsigned int n_ineligible_type;
    unsigned int n_ineligible_drop;

    unsigned int n_miss;
    unsigned int n_conf_corr;
    unsigned int n_conf_incorr;
    unsigned int n_unconf_corr;
    unsigned int n_unconf_incorr;

    std::vector<stridevaluepred> svp;
    uint64_t entries;
    
    std::vector<valuepredqueue> vpq;
    uint64_t vpq_h = 0;
    uint64_t vpq_hp = 0;
    uint64_t vpq_t = 0;
    uint64_t vpq_tp = 0;
public:
    // This is the constructor for the value predictor
    vp(bool n_enable,
       bool n_perf,
       uint64_t n_size,
       bool n_oracle,
       uint64_t n_index,
       uint64_t n_tag,
       uint64_t n_confmax,
       uint64_t n_confinc,
       uint64_t n_confdec,
       uint64_t n_replace_stride,
       uint64_t n_replace,
       bool n_predINTALU,
       bool n_predFPALU,
       bool n_predLOAD,
       bool n_full_policy);

    // Deconstructor
    ~vp();

    // Stat printer function
    void vp_stats(uint64_t num_instr, FILE* fp);

    // Increment ineligible due to type count
    void inc_ineligible_type(){n_ineligible_type++;}

    // Increment ineligible due to drop count
    void inc_ineligible_drop(){n_ineligible_drop++;}

    // Increment confident correct count
    void inc_conf_corr(){n_conf_corr++;}

    // Increment confident incorrect count
    void inc_conf_incorr(){n_conf_incorr++;}

    // Increment unconfident correct count
    void inc_unconf_corr(){n_unconf_corr++;}

    // Increment unconfident incorrect count
    void inc_unconf_incorr(){n_unconf_incorr++;}

    // Increment miss count
    void inc_miss(){n_miss++;}

    // Get vp enable
    bool get_enable(){return enable;}

    // Get vp type
    bool get_perf(){return perf;}

    // Get vpq size
    bool get_size(){return size;}

    // Get the confidence of the value prediction
    bool get_confidence(uint64_t PC);

    // Check value-prediction eligibility
    bool eligible(unsigned int flags);
    
    // Get the prediction of the instruction
    uint64_t predict(uint64_t PC, bool& miss);

    // Allocate entry in VPQ
    uint64_t vpq_allocate(uint64_t PC);
};