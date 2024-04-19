#include "pipeline.h"
#include "payload.h"
#include "debug.h"

vp::vp(bool n_enable,
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
       bool n_full_policy) {
    // Set default values
    n_ineligible_type = 0;
    n_ineligible_drop = 0;
    n_miss = 0;
    n_conf_corr = 0;
    n_conf_incorr = 0;
    n_unconf_corr = 0;
    n_unconf_incorr = 0;

    // Set private variables equal to the user defined variables
    enable = n_enable;
    perf = n_perf;
    size = n_size;
    oracle = n_oracle;
    index = n_index;
    tag = n_tag;
    confmax = n_confmax;
    confinc = n_confinc;
    confdec = n_confdec;
    replace_stride = n_replace_stride;
    replace = n_replace;
    predINTALU = n_predINTALU;
    predFPALU = n_predFPALU;
    predLOAD = n_predLOAD;
    full_policy = n_full_policy;

    // Data struc allocation and initialization
    entries = pow(2, index);

    stridevaluepred str;
    svp.resize(entries, str);

    valuepredqueue val;
    vpq.resize(size, val);
    vpq_h = 0;
    vpq_t = 0;
}

vp::~vp() {
}

void vp::vp_stats(uint64_t num_instr, FILE* fp) {
    unsigned int all_ineligible = (n_ineligible_type + n_ineligible_drop);
    unsigned int all_eligible = (n_miss + n_conf_corr + n_conf_incorr + n_unconf_corr + n_unconf_incorr);
    fprintf(fp, "VPU MEASUREMENTS-----------------------------------\n");
    fprintf(fp, "vpmeas_ineligible         : %10d (%6.2f%%) // Not eligible for value prediction.\n", 
            all_ineligible,
            100.0*(double)all_ineligible/(double)num_instr);
    fprintf(fp, "   vpmeas_ineligible_type : %10d (%6.2f%%) // Not eligible because of type.\n",
            n_ineligible_type,
            100.0*(double)n_ineligible_type/(double)num_instr);
    fprintf(fp, "   vpmeas_ineligible_drop : %10d (%6.2f%%) // VPU dropped otherwise-eligible instr. (neither predict nor train)\n                                                 // due to unavailable resource (e.g., VPQ_full_policy=1 and no free VPQ entry).\n",
            n_ineligible_drop,
            100.0*(double)n_ineligible_drop/(double)num_instr);
    fprintf(fp, "vpmeas_eligible           : %10d (%6.2f%%) // Eligible for value prediction.\n",
            all_eligible,
            100.0*(double)all_eligible/(double)num_instr);
    fprintf(fp, "   vpmeas_miss            : %10d (%6.2f%%) // VPU was unable to generate a value prediction (e.g., SVP miss).\n",
            n_miss,
            100.0*(double)n_miss/(double)num_instr);
    fprintf(fp, "   vpmeas_conf_corr       : %10d (%6.2f%%) // VPU generated a confident and correct value prediction.\n",
            n_conf_corr,
            100.0*(double)n_conf_corr/(double)num_instr);
    fprintf(fp, "   vpmeas_conf_incorr     : %10d (%6.2f%%) // VPU generated a confident and incorrect value prediction. (MISPREDICTION)\n",
            n_conf_incorr,
            100.0*(double)n_conf_incorr/(double)num_instr);
    fprintf(fp, "   vpmeas_unconf_corr     : %10d (%6.2f%%) // VPU generated an unconfident and correct value prediction. (LOST OPPORTUNITY)\n",
            n_unconf_corr,
            100.0*(double)n_unconf_corr/(double)num_instr);
    fprintf(fp, "   vpmeas_unconf_incorr   : %10d (%6.2f%%) // VPU generated an unconfident and incorrect value prediction.\n",
            n_unconf_incorr,
            100.0*(double)n_unconf_incorr/(double)num_instr);
}

void vp::debug(FILE* fp, uint64_t count) {
    fprintf(fp, "-------- DEBUG: retired instruction count = %lu\n", count);
    fprintf(fp, "SVP entry #:   tag(hex)   conf   retired_value   stride   instance\n");
    for(int i = 0; i < svp.size(); i++){
        fprintf(fp, "%11d:%11X%7lu%16lu%9d%11lu\n", i, svp[i].tag, svp[i].conf, svp[i].retired_value, svp[i].stride, svp[i].instance);
    }
    fprintf(fp, "\n");
}

// Search the SVP using the PC tag (if there is one) and check confidence.
// Return true if the predictor is confident. If the predictor is not confident or the instruction
// is not yet in the SVP, return false.
bool vp::get_confidence(uint64_t PC) {
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);

    if(svp[index_n].conf == confmax && (svp[index_n].tag == tag_n || tag==0))
        return true;
    else
        return false;
}

// This function checks value-prediction eligibility.
// predINTALU, predFPALU, and predLOAD are all "bool" types of my stride predictor class,
// and are configured to be true or false based on the --vp-svp instruction type arguments being 1 or 0,
// respectively.
bool vp::eligible(unsigned int flags) {
   if (IS_INTALU(flags))
      return(predINTALU);     // instr. is INTALU type.  It is eligible if predINTALU is configured "true".
   else if (IS_FPALU(flags))
      return(predFPALU);      // instr. is FPALU type.  It is eligible if predFPALU is configured "true".
   else if (IS_LOAD(flags) && !IS_AMO(flags))
      return(predLOAD);      // instr. is a normal LOAD (not rare load-with-reserv).  It is eligible if predLOAD is configured "true".
   else
      return(false);     // instr. is none of the above major types, so it is never eligible
}

uint64_t vp::predict(uint64_t PC) {
    uint64_t prediction;
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);

    if(svp[index_n].tag == tag_n || tag==0) {
        svp[index_n].instance++;
        prediction = svp[index_n].retired_value + (svp[index_n].instance * svp[index_n].stride);
        return prediction;
    }
    else {
        miss = true;
        return 0;
    }
}

void vp::train(uint64_t PC, uint64_t val) {
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);
    uint64_t new_stride;
    uint64_t tmp_tail;
    uint64_t i;
    bool j = true;

    printf("Index in SVP: %d\n", index_n);
    printf("Tag of SVP: %X\n", svp[index_n].tag);
    printf("Tag of instruction: %X\n", tag_n);
    if(svp[index_n].tag == tag_n || tag==0) {
        printf("TAGS MATCH IN THE TRAIN\n");
        new_stride = val-svp[index_n].retired_value;
        if(new_stride==svp[index_n].stride) {
            svp[index_n].conf += confinc;
            // saturate the confidence
            if(svp[index_n].conf > confmax)
                svp[index_n].conf = confmax;
        }
        else {
            if(svp[index_n].conf <= replace_stride)
                svp[index_n].stride = new_stride;
            if(confdec > 0){
                svp[index_n].conf -= confdec;
                //saturate the confidence
                if(svp[index_n].conf < 0)
                    svp[index_n].conf = 0;
            }
            else
                svp[index_n].conf = 0;
        }
        svp[index_n].retired_value = val;
        svp[index_n].instance--;
        if(svp[index_n].instance < 0)
            svp[index_n].instance = 0;
    }
    else if(svp[index_n].conf <= replace) {
        printf("In the else if of train");
        // Initialize
        svp[index_n].tag = tag_n;
        svp[index_n].conf = 0;
        svp[index_n].retired_value = val;
        svp[index_n].stride = val;
        i = vpq_h+1;
        if(i == size)
            i = 0;
        while(j) {
            if(vpq[i].PC == PC) {
                svp[index_n].instance++;
            }
            i++;
            if(i == size)
                i = 0;
            if(i == vpq_t + 1)
                j = false;
        }
    }
    vpq_h++;
    if(vpq_h == size) {
        vpq_h = 0;
        vpq_hp = !vpq_hp;
    }
}

// This function allocates a spot for the eligible instruction into the vpq
// and increments the tail pointer for future allocations.
uint64_t vp::vpq_allocate(uint64_t PC) {
    vpq[vpq_t].PC = PC;
    vpq_t++;
    if(vpq_t == size) {
        vpq_t = 0;
        vpq_tp = !vpq_tp;
    }
    return vpq_t-1;
}

bool vp::stall_vpq(uint64_t bundle_instr){
    if(vpq_tp == vpq_hp){
        if(size - (vpq_t - vpq_h) >= bundle_instr){
            return false;
        }
        return true;
    }

    if(vpq_h - vpq_t >= bundle_instr){
       return false;
    }
    return true;
}

void vp::vpq_deposit(uint64_t index, uint64_t value) {
    vpq[index].val = value;
}

void vp::squash(){
    vpq_t = vpq_h;
    vpq_tp = vpq_hp;
}
