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

void vp::debugSVP(FILE* fp, uint64_t count) {
    fprintf(fp, "-------- DEBUG: retired instruction count = %lu\n", count);
    fprintf(fp, "SVP entry #:   tag(hex)   conf   retired_value   stride   instance\n");
    for(int i = 0; i < svp.size(); i++){
        fprintf(fp, "%11d: %10lx %6lu %15lu %8ld %10lu\n", i, svp[i].tag, svp[i].conf, svp[i].retired_value, svp[i].stride, svp[i].instance);
    }
    fprintf(fp, "\n");
}

void vp::debugVPQ(FILE* fp) {
    bool j = true;
    uint64_t i = vpq_h;
    uint64_t index_n;
    uint64_t tag_n;

    fprintf(fp, "VPQ entry #:   PC(hex)   PCtag(hex)   PCindex(hex)\n");
    while(j) {
        index_n = (vpq[i].PC & ((1<<(index+2))-1))>>2;
        tag_n = (vpq[i].PC & ((1<<(tag+index+2))-1))>>(index+2);

        fprintf(fp, "%11lu: %9lx %12lx %14lu\n", i, vpq[i].PC, tag_n, index_n);
        i++;
        if(i == size)
            i = 0;
        if(i == vpq_t)
            j = false;
    }
}

// Search the SVP using the PC tag (if there is one) and check confidence.
// Return true if the predictor is confident. If the predictor is not confident or the instruction
// is not yet in the SVP, return false.
bool vp::get_confidence(uint64_t PC) {
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);

    if((svp[index_n].tag == tag_n || tag==0) && svp[index_n].conf == confmax)
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

    svp[index_n].instance++;
    prediction = svp[index_n].retired_value + (svp[index_n].instance * svp[index_n].stride);
    return prediction;
}

bool vp::get_miss(uint64_t PC) {
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);

    if(svp[index_n].tag == tag_n || tag==0) {
        miss = false;
    }
    else {
        miss = true;
    }
    return miss;
}

void vp::train(uint64_t PC, uint64_t val) {
    uint64_t index_n = (PC & ((1<<(index+2))-1))>>2;
    uint64_t tag_n = (PC & ((1<<(tag+index+2))-1))>>(index+2);
    int64_t new_stride;
    uint64_t tmp_tail;
    uint64_t i;
//    printf("%lu tag train %lu\n", tag_n, tag);
    if(svp[index_n].tag == tag_n || tag==0) {
//        printf("%lu does it go here?\n", index_n);
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
                //saturate the confidence
                if(confdec > svp[index_n].conf)
                    svp[index_n].conf = 0;
                else
                    svp[index_n].conf -= confdec;
            }
            else
                svp[index_n].conf = 0;
        }
        svp[index_n].retired_value = val;
        if(svp[index_n].instance > 0)
            svp[index_n].instance--;
    }
    else if(svp[index_n].conf <= replace) {
//        printf("%lu or go here?\n", index_n);
        // Initialize
        if(tag != 0){
            svp[index_n].tag = tag_n;
        }
        svp[index_n].conf = 0;
        svp[index_n].retired_value = val;
        svp[index_n].stride = val;
        svp[index_n].instance = 0;
        i = vpq_h+1;
        if(i == size)
            i = 0;
        while(i != vpq_t) {
            if(vpq[i].PC == PC) {
                svp[index_n].instance++;
            }
            i++;
            if(i == size)
                i = 0;
        }
    }
    vpq_h++;
    if(vpq_h == size) {
        vpq_h = 0;
        vpq_hp = !vpq_hp;
    }
    // printf("\nHEAD MOVED FORWARD\nhead: %lu\nhead pointer: %d\ntail: %lu\ntail pointer: %d\n", vpq_h, vpq_hp, vpq_t, vpq_tp);
}

// This function allocates a spot for the eligible instruction into the vpq
// and increments the tail pointer for future allocations.
uint64_t vp::vpq_allocate(uint64_t PC) {
    uint64_t foo = vpq_t;
    vpq[vpq_t].PC = PC;
    vpq[vpq_t].val = 0;
    vpq_t++;
    if(vpq_t == size) {
        vpq_t = 0;
        vpq_tp = !vpq_tp;
    }
    // printf("\nTAIL MOVED FORWARD\nhead: %lu\nhead pointer: %d\ntail: %lu\ntail pointer: %d\n", vpq_h, vpq_hp, vpq_t, vpq_tp);
    return foo;
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
   uint64_t index_n;
   uint64_t tag_n;

   while(vpq_t != vpq_h || vpq_tp != vpq_hp){

       if(vpq_t == 0){
           vpq_t = size - 1;
           vpq_tp = vpq_hp;
       }else{
           --vpq_t;
       }
    //    printf("\nTAIL MOVED BACK\nhead: %lu\nhead pointer: %d\ntail: %lu\ntail pointer: %d\n", vpq_h, vpq_hp, vpq_t, vpq_tp);

       index_n = (vpq[vpq_t].PC & ((1<<(index+2))-1))>>2;
       tag_n = (vpq[vpq_t].PC & ((1<<(tag+index+2))-1))>>(index+2);

       if(svp[index_n].tag == tag_n || tag == 0){
           if(svp[index_n].instance > 0){
               svp[index_n].instance--;
           }
       }
   }
   vpq_tp = vpq_hp;

   assert(vpq_h == vpq_t);
}

void vp::restore(uint64_t tail, bool t_phase){
   vpq_tp = t_phase;

   uint64_t index_n;
   uint64_t tag_n;

   while(vpq_t != tail){
       if(vpq_t == 0){
           vpq_t = size - 1;
       }else{
           --vpq_t;
       }
    //    printf("\nTAIL MOVED BACK\nhead: %lu\nhead pointer: %d\ntail: %lu\ntail pointer: %d\n", vpq_h, vpq_hp, vpq_t, vpq_tp);

       index_n = (vpq[vpq_t].PC & ((1<<(index+2))-1))>>2;
       tag_n = (vpq[vpq_t].PC & ((1<<(tag+index+2))-1))>>(index+2);

       if(svp[index_n].tag == tag_n || tag == 0){
           if(svp[index_n].instance > 0){
               svp[index_n].instance--;
           }
       }
   }
}

void vp::check_full() {
    printf("\nTAIL MOVED BACK\nhead: %lu\nhead pointer: %d\ntail: %lu\ntail pointer: %d\n", vpq_h, vpq_hp, vpq_t, vpq_tp);
}

void vp::vpq_settings(FILE* file){

    fprintf(file, "\n=== VALUE PREDICTOR =============================================================\n\n");
    fprintf(file, "VALUE PREDICTOR = ");
    if(perf){
        fprintf(file, "perfect\n");
    }else if(enable){
        fprintf(file, "stride (Project 4 spec. implementation)\n");
        fprintf(file, "   VPQsize             = %lu \n", size);
        fprintf(file, "   oracleconf          = %d ", oracle);
        if(oracle)
            fprintf(file, "(oracle confidence)\n");
        else
            fprintf(file, "(real confidence)\n");
        fprintf(file, "   # index bits        = %lu \n", index);
        fprintf(file, "   # tag bits          = %lu \n", tag);
        fprintf(file, "   confmax             = %lu \n", confmax);
        fprintf(file, "   confinc             = %lu \n", confinc);
        fprintf(file, "   confdec             = %lu ", confdec);
        if(confdec == 0)
            fprintf(file, "(reset)\n");
        else
            fprintf(file, "\n");
        fprintf(file, "   replace_stride      = %lu \n", replace_stride);
        fprintf(file, "   replace             = %lu \n", replace);
        fprintf(file, "   predINTALU          = %d \n", predINTALU);
        fprintf(file, "   predFPALU           = %d \n", predFPALU);
        fprintf(file, "   predLOAD            = %d \n", predLOAD);
        fprintf(file, "   VPQ_full_policy     = %d ", full_policy);
        if(full_policy)
            fprintf(file, "(don’t allocate VPQ entries)\n");
        else
            fprintf(file, "(stall bundle)\n");
    }else
        fprintf(file, "none\n");


}

void vp::cost(FILE* file){
    fprintf(file, "\nCOST ACCOUNTING\n");
    if(!perf && enable){
        uint64_t SVP_bits = tag + (uint64_t)ceil(log2((double)(confmax+1))) + (uint64_t)ceil(log2((double)size)) +
        sizeof(svp[0].retired_value) * 8 + sizeof(svp[0].stride) * 8;
        uint64_t VPQ_bits = tag + index + sizeof(vpq[0].val) * 8;

        fprintf(file, "\tOne SVP entry:\n");
        fprintf(file, "\t\ttag              :%4lu bits\n", tag);
        fprintf(file, "\t\tconf             :%4lu bits\n", (uint64_t)ceil(log2((double)(confmax+1))));
        fprintf(file, "\t\tretired_value    :%4lu bits\n", sizeof(svp[0].retired_value) * 8);
        fprintf(file, "\t\tstride           :%4lu bits\n", sizeof(svp[0].stride) * 8);
        fprintf(file, "\t\tinstance ctr     :%4lu bits\n", (uint64_t)ceil(log2((double)size)));
        fprintf(file, "\t\t---------------------------\n");
        fprintf(file, "\t\tbits/SVP entry   :%4lu bits\n", SVP_bits);
        fprintf(file, "\tOne VPQ entry:\n");
        fprintf(file, "\t\tPC_tag           :%4lu bits\n", tag);
        fprintf(file, "\t\tPC_index         :%4lu bits\n", index);
        fprintf(file, "\t\tvalue            :%4lu bits\n", sizeof(vpq[0].val) * 8);
        fprintf(file, "\t\t---------------------------\n");
        fprintf(file, "\t\tbits/VPQ entry   :%4lu bits\n", VPQ_bits);
        uint64_t SVP_entries = pow(2, index);
        uint64_t total_SVP = SVP_bits * SVP_entries;
        uint64_t total_VPQ = size * VPQ_bits;
        fprintf(file, "\tTotal storage cost (bits) = %lu (%lu SVP entries x %lu bits/SVP entry) ", total_SVP, SVP_entries, SVP_bits);
        fprintf(file, "+ %lu (%lu VPQ entries x %lu bits/VPQ entry) = %lu bits\n", total_VPQ, size, VPQ_bits, total_VPQ + total_SVP);
        double total_in_bytes = double(total_SVP + total_VPQ)/8;
        fprintf(file, "\tTotal storage cost (bytes) = %.2f B (%.2f KB)\n", total_in_bytes, total_in_bytes/1024);
    }else
        fprintf(file, "\tImpossible.\n");

}
