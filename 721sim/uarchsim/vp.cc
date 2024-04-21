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

    if((svp[index_n].tag == tag_n || tag==0) && svp[index_n].conf == confmax && svp[index_n].valid)
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

    if((svp[index_n].tag == tag_n || tag==0) && svp[index_n].valid) {
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

    // printf("Index in SVP: %lu\n", index_n);
    // printf("Tag of SVP: %lx\n", svp[index_n].tag);
    // printf("Tag of instruction: %lx\n", tag_n);
    // printf("Value of instruction: %lu\n", val);
    if(svp[index_n].tag == tag_n || tag==0) {
        // printf("TAGS MATCH IN THE TRAIN\n");
//        if(index_n == 21)
//            printf("21: before svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 32)
//            printf("32: before svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 40)
//            printf("40: before svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 71)
//            printf("71: before svp instance train: %lu\n", svp[index_n].instance);

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

//        if(index_n == 21)
//            printf("21: after svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 32)
//            printf("32: after svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 40)
//            printf("40: after svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 71)
//            printf("71: after svp instance train: %lu\n", svp[index_n].instance);

        // printf("SVP conf: %lu\n", svp[index_n].conf);
        // printf("SVP instance: %lu\n", svp[index_n].instance);
        // printf("SVP retired_value: %lu\n", svp[index_n].retired_value);
        // printf("SVP stride: %lu\n", svp[index_n].stride);
        // printf("SVP tag: %lx\n", svp[index_n].tag);
    }
    else if(svp[index_n].conf <= replace) {
        // printf("In the else if of train\n");
        // Initialize
        svp[index_n].tag = tag_n;
        svp[index_n].conf = 0;
        svp[index_n].retired_value = val;
        svp[index_n].stride = val;
        svp[index_n].instance = 0;
        svp[index_n].valid = 1;
//        if(index_n == 21)
//            printf("21: from %lu to %lu\n", vpq_h, vpq_t);
//        if(index_n == 32)
//            printf("32: from %lu to %lu\n", vpq_h, vpq_t);
//        if(index_n == 40)
//            printf("40: from %lu to %lu\n", vpq_h, vpq_t);
//        if(index_n == 71)
//            printf("71: from %lu to %lu\n", vpq_h, vpq_t);
        i = vpq_h+1;
        if(i == size)
            i = 0;
        while(i != vpq_t) {
//            if(index_n == 21)
//                printf("%lu: 21 svp instance train: %lu\n", i, svp[index_n].instance);
//            if(index_n == 32)
//                printf("%lu: 32 svp instance train: %lu\n", i, svp[index_n].instance);
//            if(index_n == 40)
//                printf("%lu: 40 svp instance train: %lu\n", i, svp[index_n].instance);
//            if(index_n == 71)
//                printf("%lu: 71 svp instance train: %lu\n", i, svp[index_n].instance);

            if(vpq[i].PC == PC) {
                svp[index_n].instance++;
            }
            i++;
            if(i == size)
                i = 0;
        }
//        if(index_n == 21)
//            printf("21: svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 32)
//            printf("32: svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 40)
//            printf("40: svp instance train: %lu\n", svp[index_n].instance);
//        if(index_n == 71)
//            printf("71: svp instance train: %lu\n", svp[index_n].instance);
        // printf("SVP conf: %lu\n", svp[index_n].conf);
        // printf("SVP instance: %lu\n", svp[index_n].instance);
        // printf("SVP retired_value: %lu\n", svp[index_n].retired_value);
        // printf("SVP stride: %lu\n", svp[index_n].stride);
        // printf("SVP tag: %lx\n", svp[index_n].tag);
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
    uint64_t foo = vpq_t;
    vpq[vpq_t].PC = PC;
    vpq[vpq_t].val = 0;
    vpq_t++;
    if(vpq_t == size) {
        vpq_t = 0;
        vpq_tp = !vpq_tp;
    }

    // printf("\nVPQ HEAD: %lu\n", vpq_h);
    // printf("VPQ HEAD phase: %lu\n", vpq_hp);
    // printf("VPQ TAIL: %lu\n", vpq_t);
    // printf("VPQ TAIL phase: %lu\n", vpq_tp);

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
    // printf("\nIndex in deposit: %lu\n", index);
    // printf("Value in deposit: %lu\n", value);
    vpq[index].val = value;
}

void vp::squash(){
//    printf("\nWe are in the squash of the vpq!!\n");
   uint64_t index_n;
   uint64_t tag_n;
    
//     for(int i=0; i<entries; i++) {
//         if(i == 21)
//             printf("BEFORE | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//         if(i == 32)
//             printf("BEFORE | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//         if(i == 40)
//             printf("BEFORE | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//         if(i == 71)
//             printf("BEFORE | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//    }

   while(vpq_t != vpq_h || vpq_tp != vpq_hp){

    // TODO: implement checking phase bits in case vpq is full
       if(vpq_t == 0){
           vpq_t = size - 1;
           vpq_tp = vpq_hp;
       }else{
           --vpq_t;
       }

       index_n = (vpq[vpq_t].PC & ((1<<(index+2))-1))>>2;
       tag_n = (vpq[vpq_t].PC & ((1<<(tag+index+2))-1))>>(index+2);

        // if(index_n == 21)
        //     printf("21: calculated tag: %lu\n", tag_n);
        // if(index_n == 32)
        //     printf("32: calculated tag: %lu\n", tag_n);
        // if(index_n == 40)
        //     printf("40: calculated tag: %lu\n", tag_n);
        // if(index_n == 71)
        //     printf("71: calculated tag: %lu\n", tag_n);

       if(svp[index_n].tag == tag_n){
           if(svp[index_n].instance > 0){
               svp[index_n].instance--;
           }
        //     if(index_n == 21)
        //         printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
        //     if(index_n == 32)
        //         printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
        //     if(index_n == 40)
        //         printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
        //     if(index_n == 71)
        //         printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
       }
   }
   vpq_tp = vpq_hp;

//    for(int i=0; i<entries; i++) {
//     if(i == 21)
//         printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//     if(i == 32)
//         printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//     if(i == 40)
//         printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//     if(i == 71)
//         printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//    }

   assert(vpq_h == vpq_t);
}

void vp::cost(){
    uint64_t total = tag + (uint64_t)ceil(log2((double)(confmax+1))) + (uint64_t)ceil(log2((double)size)) +
    sizeof(svp[0].retired_value) + sizeof(svp[0].stride);

    printf("COST ACCOUNTING\n");
    printf("\tOne SVP entry:\n");
    printf("\t\ttag              :   %lu bits\n", tag);
    printf("\t\tconf             :   %lu bits\n", (uint64_t)ceil(log2((double)(confmax+1))));
    printf("\t\tretired_value    :   %lu bits\n", sizeof(svp[0].retired_value));
    printf("\t\tstride           :   %lu bits\n", sizeof(svp[0].stride));
    printf("\t\tinstance ctr     :   %lu bits\n", (uint64_t)ceil(log2((double)size)));
    printf("\t\t-----------------------------\n");
    printf("\t\tbits/SVP entry   :   %lu bits\n", total);
    printf("\tOne VPQ entry      :   %lu bits\n", tag);
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

       index_n = (vpq[vpq_t].PC & ((1<<(index+2))-1))>>2;
       tag_n = (vpq[vpq_t].PC & ((1<<(tag+index+2))-1))>>(index+2);

//       if(index_n == 21)
//           printf("21: calculated tag: %lu\n", tag_n);
//       if(index_n == 32)
//           printf("32: calculated tag: %lu\n", tag_n);
//       if(index_n == 40)
//           printf("40: calculated tag: %lu\n", tag_n);
//       if(index_n == 71)
//           printf("71: calculated tag: %lu\n", tag_n);

       if(svp[index_n].tag == tag_n || tag == 0){
           if(svp[index_n].instance > 0){
               svp[index_n].instance--;
            //    printf("%lx instance: %lu\n", vpq[vpq_t].PC, svp[index_n].instance);
           }
//           if(index_n == 21)
//           printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
//           if(index_n == 32)
//                 printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
//           if(index_n == 40)
//                 printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
//           if(index_n == 71)
//           printf("SQUASH DEC %lu: %lu\n", index_n, svp[index_n].instance);
       }
   }
//   for(int i=0; i<entries; i++) {
//       if(i == 21)
//           printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//       if(i == 32)
//           printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//       if(i == 40)
//           printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//       if(i == 71)
//           printf("AFTER | %d: %lu TAG: %lu\n", i, svp[i].instance, svp[i].tag);
//   }
}
