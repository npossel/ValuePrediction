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
    valuepredqueue val;
    vpq.resize(size, val);
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

void vp::inc_ineligible_type() {
    n_ineligible_type++;
}

void vp::inc_ineligible_drop() {
    n_ineligible_drop++;
}

void vp::inc_conf_corr() {
    n_conf_corr++;
}

bool vp::get_enable() {
    return enable;
}

bool vp::get_perf() {
    return perf;
}